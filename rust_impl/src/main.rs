mod args;
mod ffi;

use salvo::{http::ResBody, prelude::*};

const DEFAULT_FACTORS_DIGITS: u64 = 17000;

async fn req_to_url(url: String) -> Result<(ResBody, u16), reqwest::Error> {
    let req = reqwest::get(url).await?;
    let status = req.status().as_u16();
    Ok((ResBody::Once(req.bytes().await?), status))
}

#[handler]
async fn handler_fn(depot: &mut Depot, req: &mut Request, res: &mut Response) {
    let args = depot.obtain::<args::Args>().unwrap();

    let path_str = req.uri().path_and_query().unwrap().as_str().to_owned();

    let res_body_res = req_to_url(format!("{}{}", args.dest_url, &path_str)).await;
    if let Ok((res_body, status)) = res_body_res {
        res.replace_body(res_body);
        res.status_code = Some(StatusCode::from_u16(status).unwrap());
    } else {
        res.render("Failed to query");
        res.status_code = Some(StatusCode::INTERNAL_SERVER_ERROR);
    }
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
    println!("URL: {}", &parsed_args.dest_url);
    println!("Listening: {}", &parsed_args.addr_port_str);

    let router = Router::new()
        .hoop(affix_state::inject(parsed_args.clone()))
        .path("{**}")
        .get(handler_fn);
    let acceptor = TcpListener::new(&parsed_args.addr_port_str).bind().await;
    Server::new(acceptor).serve(router).await;
}
