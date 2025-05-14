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

use crate::error::Error;
use std::{env::args as args_fn, path::PathBuf};

#[derive(Default, Clone, Debug)]
pub struct Args {
    pub factors: Option<u64>,
    pub dest_url: String,
    pub addr_port_str: String,
    pub mysql_config_file: PathBuf,
    pub enable_x_real_ip_header: bool,
    pub api_url: String,
    pub js_factors_url: String,
    pub challenge_timeout_mins: u64,
    pub allowed_timeout_mins: u64,
    pub enable_override_dest_url: bool,
}

pub fn print_args() {
    println!("Args:");
    println!("  --factors=<digits> : Generate factors challenge with <digits> digits");
    println!("  --dest-url=<url> : Destination URL for verified clients");
    println!("  --addr-port=<addr>:<port> : Listening addr/port");
    println!("  --mysql-conf=<config_file> : Set path to config file for mysql settings");
    println!(
        "  --enable-x-real-ip-header : Enable trusting \"x-real-ip\" header as client ip addr"
    );
    println!("  --api-url=<url> : Set endpoint for client to POST to this software");
    println!(
        "  --js-factors-url=<url> : Set endpoint for client to request factors.js from this software"
    );
    println!(
        "  --challenge-timeout=<minutes> : Set minutes for how long challenge answers are stored in db"
    );
    println!(
        "  --allowed-timeout=<minutes> : Set how long a client is allowed to access before requiring challenge again"
    );
    println!(
        "  --enable-override-dest-url : Enable \"override-dest-url\" request header to determine where to forward"
    );
}

pub fn parse_args() -> Result<Args, Error> {
    let mut args = Args {
        factors: None,
        dest_url: "https://seodisparate.com".into(),
        addr_port_str: "127.0.0.1:8180".into(),
        mysql_config_file: "mysql.conf".into(),
        enable_x_real_ip_header: false,
        api_url: "/pma_api".into(),
        js_factors_url: "/pma_factors.js".into(),
        challenge_timeout_mins: crate::constants::CHALLENGE_FACTORS_TIMEOUT_MINUTES,
        allowed_timeout_mins: crate::constants::ALLOWED_IP_TIMEOUT_MINUTES,
        enable_override_dest_url: false,
    };

    let p_args = args_fn();

    for mut arg in p_args.skip(1) {
        if arg == "-h" || arg == "--help" {
            print_args();
            return Err(Error::from("Printed help text"));
        } else if arg.starts_with("--factors=") {
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
        } else if arg.starts_with("--js-factors-url=") {
            let end = arg.split_off(17);
            args.js_factors_url = end;
        } else if arg.starts_with("--challenge-timeout=") {
            let end = arg.split_off(20);
            args.challenge_timeout_mins = end
                .parse()
                .expect("challenge timeout should be a valid integer");
        } else if arg.starts_with("--allowed-timeout=") {
            let end = arg.split_off(18);
            args.allowed_timeout_mins = end
                .parse()
                .expect("allowed timeout should be a valid integer");
        } else if arg == "--enable-override-dest-url" {
            args.enable_override_dest_url = true;
        }
    }

    Ok(args)
}
