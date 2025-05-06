use std::env::args as args_fn;

pub struct Args {
    pub factors: Option<u64>,
}

pub fn parse_args() -> Args {
    let mut args = Args { factors: None };

    let p_args = args_fn();

    for mut arg in p_args.skip(1) {
        if arg.starts_with("--factors=") {
            let end = arg.split_off(10);
            args.factors = end.parse().ok();
        }
    }

    args
}
