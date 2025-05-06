use std::env::args as args_fn;

pub struct Args {
    pub factors: Option<u64>,
    pub addr_port_str: String,
}

pub fn parse_args() -> Args {
    let mut args = Args {
        factors: None,
        addr_port_str: "127.0.0.1:8180".into(),
    };

    let p_args = args_fn();

    for mut arg in p_args.skip(1) {
        if arg.starts_with("--factors=") {
            let end = arg.split_off(10);
            args.factors = end.parse().ok();
        } else if arg.starts_with("--addr_port=") {
            let end = arg.split_off(12);
            args.addr_port_str = end.to_owned();
        }
    }

    args
}
