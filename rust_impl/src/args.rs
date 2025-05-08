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

use std::{env::args as args_fn, path::PathBuf};

#[derive(Default, Clone, Debug)]
pub struct Args {
    pub factors: Option<u64>,
    pub dest_url: String,
    pub addr_port_str: String,
    pub mysql_config_file: PathBuf,
    pub enable_x_real_ip_header: bool,
    pub api_url: String,
}

pub fn parse_args() -> Args {
    let mut args = Args {
        factors: None,
        dest_url: "https://git.seodisparate.com".into(),
        addr_port_str: "127.0.0.1:8180".into(),
        mysql_config_file: "mysql.conf".into(),
        enable_x_real_ip_header: false,
        api_url: "/pma_api".into(),
    };

    let p_args = args_fn();

    for mut arg in p_args.skip(1) {
        if arg.starts_with("--factors=") {
            let end = arg.split_off(10);
            args.factors = end.parse().ok();
        } else if arg.starts_with("--dest-url=") {
            let end = arg.split_off(11);
            args.dest_url = end;
        } else if arg.starts_with("--addr-port=") {
            let end = arg.split_off(12);
            args.addr_port_str = end;
        } else if arg.starts_with("--mysql-conf=") {
            let end = arg.split_off(13);
            args.mysql_config_file = end.into();
        } else if arg == "--enable-x-real-ip-header" {
            args.enable_x_real_ip_header = true;
        } else if arg.starts_with("--api-url=") {
            let end = arg.split_off(10);
            args.api_url = end;
        }
    }

    args
}
