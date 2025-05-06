mod args;
mod ffi;

use salvo::{http::ResBody, prelude::*};

const DEFAULT_FACTORS_DIGITS: u64 = 17000;

async fn req_to_url(
    url: String,
) -> Result<(ResBody, u16, reqwest::header::HeaderMap), reqwest::Error> {
    let req = reqwest::get(url).await?;
    let status = req.status().as_u16();
    let headers = req.headers().to_owned();
    Ok((ResBody::Once(req.bytes().await?), status, headers))
}

#[handler]
async fn api_fn(depot: &mut Depot, req: &mut Request, res: &mut Response) {
    let _args = depot.obtain::<args::Args>().unwrap();
    eprintln!("API: {}", req.uri().path_and_query().unwrap());
    res.render("API");
}

#[handler]
async fn handler_fn(depot: &mut Depot, req: &mut Request, res: &mut Response) {
    let args = depot.obtain::<args::Args>().unwrap();

    let path_str = req.uri().path_and_query().unwrap().as_str().to_owned();

    let res_body_res = req_to_url(format!("{}{}", args.dest_url, &path_str)).await;
    if let Ok((res_body, status, headers)) = res_body_res {
        res.replace_body(res_body);
        res.status_code = Some(StatusCode::from_u16(status).unwrap());
        for (k_opt, v) in headers {
            if let Some(k) = k_opt {
                res.headers.append(k, v);
            }
        }
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
        .push(Router::new().path("/pma_api").post(api_fn))
        .push(Router::new().path("{**}").get(handler_fn));
    let acceptor = TcpListener::new(&parsed_args.addr_port_str).bind().await;
    Server::new(acceptor).serve(router).await;
}
