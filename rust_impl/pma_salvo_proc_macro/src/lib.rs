// ISC License
//
// Copyright (c) 2025 Stephen Seo
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

use std::str::FromStr;

use proc_macro::TokenStream;

enum State {
    ExpectIdx,
    ExpectAddrPortsIter,
    ExpectListener,
    ExpectRouter,
    End,
}

#[proc_macro]
pub fn combine_tcplisteners(input: TokenStream) -> TokenStream {
    let mut state = State::ExpectIdx;
    let mut output: String = String::new();
    let mut first_item: Option<String> = None;
    let mut second_item: Option<String> = None;
    let mut third_item: Option<String> = None;
    let mut fourth_item: Option<String> = None;

    for tree in input {
        let expr: String;
        match tree {
            proc_macro::TokenTree::Group(group) => expr = group.to_string(),
            proc_macro::TokenTree::Ident(ident) => expr = ident.to_string(),
            proc_macro::TokenTree::Punct(_punct) => continue,
            proc_macro::TokenTree::Literal(literal) => expr = literal.to_string(),
        }

        match state {
            State::ExpectIdx => {
                first_item = Some(expr);
                state = State::ExpectAddrPortsIter;
            }
            State::ExpectAddrPortsIter => {
                second_item = Some(expr);
                state = State::ExpectListener;
            }
            State::ExpectListener => {
                third_item = Some(expr);
                state = State::ExpectRouter;
            }
            State::ExpectRouter => {
                fourth_item = Some(expr);
                state = State::End;
            }
            State::End => panic!("Invalid (End) state"),
        }
    }

    let idx;
    let addr_ports_iter;
    let listener;
    let router;
    if fourth_item.is_none() {
        idx = "0".to_owned();
        addr_ports_iter = first_item.unwrap();
        listener = second_item.unwrap();
        router = third_item.unwrap();
    } else {
        idx = first_item.unwrap();
        addr_ports_iter = second_item.unwrap();
        listener = third_item.unwrap();
        router = fourth_item.unwrap();
    }

    // Parse idx.
    let mut value = 0;
    let mut second_value = 0;
    let mut is_plus_reached = false;
    for c in idx.chars() {
        if is_plus_reached {
            if c.is_digit(10) {
                second_value = second_value * 10 + c.to_digit(10).unwrap();
            }
        } else {
            if c.is_digit(10) {
                value = value * 10 + c.to_digit(10).unwrap();
            } else if c == '+' {
                is_plus_reached = true;
            }
        }
    }
    let idx = value + second_value;

    output.push_str(&format!("if {}.len() != 0", &addr_ports_iter));
    output.push_str("{");
    if idx > 32 {
        output.push_str("panic!(\"Recursion limit of 32 reached!\");");
    } else {
        output.push_str(&format!(
            "let joined = {}.join(TcpListener::new({}.next().unwrap()));",
            &listener, &addr_ports_iter
        ));
        output.push_str(&format!(
            "combine_tcplisteners!(({} + 1) {} joined {})",
            idx, &addr_ports_iter, &router
        ));
    }
    output.push_str("} else {");
    output.push_str(&format!(
        "Server::new({}.bind().await).serve({}).await;",
        &listener, &router
    ));
    output.push_str("}");

    TokenStream::from_str(&output).unwrap()
}
