mod args;
mod ffi;

use salvo::prelude::*;

const DEFAULT_FACTORS_DIGITS: u64 = 17000;

#[handler]
async fn handler_fn(req: &mut Request, res: &mut Response) {
    let path_str = req.uri().path_and_query().unwrap().as_str().to_owned();
    res.render(Text::Plain(path_str));
}

#[tokio::main]
async fn main() {
    let mut parsed_args = args::parse_args();
    if parsed_args.factors == None {
        parsed_args.factors = Some(DEFAULT_FACTORS_DIGITS);
        println!(
            "\"--factors=<digits>\" not specified, defaulting to \"{}\"",
            DEFAULT_FACTORS_DIGITS
        );
    }

    let router = Router::new().path("{**}").get(handler_fn);
    let acceptor = TcpListener::new(&parsed_args.addr_port_str).bind().await;
    Server::new(acceptor).serve(router).await;
}
