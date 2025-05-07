mod args;
mod error;
mod ffi;

use std::{collections::HashMap, path::Path};

use mysql_async::{Pool, prelude::Query};
use salvo::{http::ResBody, prelude::*};
use tokio::{fs::File, io::AsyncReadExt};

use error::Error;

const DEFAULT_FACTORS_DIGITS: u64 = 17000;

async fn parse_db_conf(config: &Path) -> Result<HashMap<String, String>, Error> {
    let mut file_contents: String = String::new();
    File::open(config)
        .await?
        .read_to_string(&mut file_contents)
        .await?;

    let mut map: HashMap<String, String> = HashMap::new();

    for line in file_contents.lines() {
        let line_parts: Vec<&str> = line.split("=").collect();
        if line_parts.len() == 2 {
            map.insert(line_parts[0].to_owned(), line_parts[1].to_owned());
        } else {
            eprintln!("WARNING: parse_db_conf(): config had invalid entry!");
        }
    }

    Ok(map)
}

async fn get_db_pool(args: &args::Args) -> Result<Pool, Error> {
    let config_map = parse_db_conf(&args.mysql_config_file)
        .await
        .expect("Parse config for mysql usage");

    let pool = mysql_async::Pool::from_url(&format!(
        "mysql://{}:{}@{}:{}/{}",
        config_map
            .get("user")
            .ok_or("User not in mysql config".to_owned())?,
        config_map
            .get("password")
            .ok_or("Password not in mysql config".to_owned())?,
        config_map
            .get("address")
            .ok_or("Address not in mysql config".to_owned())?,
        config_map
            .get("port")
            .ok_or("Port not in mysql config".to_owned())?,
        config_map
            .get("database")
            .ok_or("Database not in mysql config".to_owned())?
    ))?;

    Ok(pool)
}

async fn init_db(args: &args::Args) -> Result<(), Error> {
    let pool = get_db_pool(args).await?;

    let mut conn = pool.get_conn().await?;

    r"CREATE TABLE IF NOT EXISTS SEQ_ID (
        ID INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
        SEQ_ID INT UNSIGNED NOT NULL
    )"
    .ignore(&mut conn)
    .await?;

    r"CREATE TABLE IF NOT EXISTS CHALLENGE_FACTORS (
        UUID CHAR(36) NOT NULL PRIMARY KEY,
        FACTORS MEDIUMTEXT CHARACTER SET ascii NOT NULL,
        GEN_TIME DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
    )"
    .ignore(&mut conn)
    .await?;

    r"CREATE TABLE IF NOT EXISTS ALLOWED_IPS (
        IP VARCHAR(45) NOT NULL PRIMARY KEY,
        ON_TIME DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
    )"
    .ignore(&mut conn)
    .await?;

    drop(conn);

    pool.disconnect().await?;

    Ok(())
}

async fn req_to_url(url: String) -> Result<(ResBody, u16, reqwest::header::HeaderMap), Error> {
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

    init_db(&parsed_args)
        .await
        .expect("Should be able to init database");

    println!("URL: {}", &parsed_args.dest_url);
    println!("Listening: {}", &parsed_args.addr_port_str);

    let router = Router::new()
        .hoop(affix_state::inject(parsed_args.clone()))
        .push(Router::new().path("/pma_api").post(api_fn))
        .push(Router::new().path("{**}").get(handler_fn));
    let acceptor = TcpListener::new(&parsed_args.addr_port_str).bind().await;
    Server::new(acceptor).serve(router).await;
}
