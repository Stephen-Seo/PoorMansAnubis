mod args;
mod error;
mod ffi;
mod sql_types;

use std::{collections::HashMap, path::Path};

use mysql_async::{
    Pool, Row, params,
    prelude::{Query, WithParams},
};
use salvo::{http::ResBody, prelude::*};
use sql_types::AllowedIPs;
use time::OffsetDateTime;
use tokio::{fs::File, io::AsyncReadExt};

use error::Error;

const DEFAULT_FACTORS_DIGITS: u64 = 17000;
const ALLOWED_IP_TIMEOUT_MINUTES: i64 = 60;

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

    let pool = mysql_async::Pool::from_url(format!(
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

async fn get_client_ip_addr(depot: &Depot, req: &mut Request) -> Result<String, Error> {
    let args = depot.obtain::<args::Args>().unwrap();
    let addr_string: String;

    let real_ip_header = req.headers().get("x-real-ip");

    if args.enable_x_real_ip_header && real_ip_header.is_some() {
        addr_string = real_ip_header
            .unwrap()
            .to_str()
            .map_err(Error::from)?
            .to_owned();
        if addr_string.is_empty() {
            return Err(Error::from(
                "Failed to get client addr (invalid header)".to_owned(),
            ));
        } else {
            eprintln!("GET from ip {}", &addr_string);
        }
    } else {
        eprintln!("GET from ip {}", req.remote_addr());
        if let Some(ipv4) = req.remote_addr().as_ipv4() {
            eprintln!(" ipv4: {}", ipv4.ip());
            addr_string = format!("{}", ipv4.ip());
        } else if let Some(ipv6) = req.remote_addr().as_ipv6() {
            eprintln!(" ipv6: {}", ipv6.ip());
            addr_string = format!("{}", ipv6.ip());
        } else {
            return Err(Error::from("Failed to get client addr".to_owned()));
        }
    }

    Ok(addr_string)
}

#[handler]
async fn api_fn(depot: &Depot, req: &mut Request, res: &mut Response) {
    let _args = depot.obtain::<args::Args>().unwrap();
    eprintln!("API: {}", req.uri().path_and_query().unwrap());
    res.render("API");
}

#[handler]
async fn handler_fn(depot: &Depot, req: &mut Request, res: &mut Response) -> salvo::Result<()> {
    let args = depot.obtain::<args::Args>().unwrap();

    let addr_string = get_client_ip_addr(depot, req).await?;

    {
        let pool = get_db_pool(args).await?;
        let mut conn = pool.get_conn().await.map_err(Error::from)?;

        let mut ip_entry: Option<AllowedIPs> = None;

        r"LOCK TABLE ALLOWED_IPS READ"
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

        let ip_entry_row: Option<Row> = r"SELECT IP, ON_TIME FROM ALLOWED_IPS WHERE IP = :ipaddr"
            .with(params! {"ipaddr" => &addr_string})
            .first(&mut conn)
            .await
            .map_err(Error::from)?;
        if let Some(row) = ip_entry_row {
            ip_entry = Some(AllowedIPs::try_from(row)?);
        }

        r"UNLOCK TABLES"
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

        if let Some(ip_ent) = &mut ip_entry {
            let duration = OffsetDateTime::now_local().map_err(Error::from)? - ip_ent.time;
            if duration.whole_minutes() >= ALLOWED_IP_TIMEOUT_MINUTES {
                r"LOCK TABLE ALLOWED_IPS WRITE"
                    .ignore(&mut conn)
                    .await
                    .map_err(Error::from)?;
                r"DELETE FROM ALLOWED_IPS WHERE IP = :ipaddr"
                    .with(params! {"ipaddr" => ip_ent.ip.to_string()})
                    .ignore(&mut conn)
                    .await
                    .map_err(Error::from)?;
                r"UNLOCK TABLES"
                    .ignore(&mut conn)
                    .await
                    .map_err(Error::from)?;
                ip_entry = None;
                eprintln!("ip timed out");
            }
        }

        if let Some(ip_ent) = &mut ip_entry {
            eprintln!("ip existed:");
            eprintln!("{:?}", ip_ent);
        } else {
            eprintln!("ip did not exist or timed out");
        }

        drop(conn);
        pool.disconnect().await.map_err(Error::from)?;
    }

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

    Ok(())
}

#[tokio::main]
async fn main() {
    let mut parsed_args = args::parse_args();
    if parsed_args.factors.is_none() {
        parsed_args.factors = Some(DEFAULT_FACTORS_DIGITS);
        println!(
            "\"--factors=<digits>\" not specified, defaulting to \"{}\"",
            DEFAULT_FACTORS_DIGITS
        );
    }

    init_db(&parsed_args)
        .await
        .expect("Should be able to init database");

    // {
    //     let pool = get_db_pool(&parsed_args).await.unwrap();
    //     let mut conn = pool.get_conn().await.map_err(Error::from).unwrap();
    //     let ip_entry_row: Option<Row> = r"SELECT IP, ON_TIME FROM ALLOWED_IPS WHERE IP = :ipaddr"
    //         .with(params! {"ipaddr" => "127.0.0.1"})
    //         .first(&mut conn)
    //         .await
    //         .unwrap();
    //     if let Some(row) = ip_entry_row {
    //         let ip_entry_res = AllowedIPs::try_from(row);
    //         eprintln!("{:?}", ip_entry_res);
    //     } else {
    //         eprintln!("No row with 127.0.0.1!");
    //     }
    //     drop(conn);
    //     pool.disconnect().await.map_err(Error::from).unwrap();
    // }

    eprintln!("URL: {}", &parsed_args.dest_url);
    eprintln!("Listening: {}", &parsed_args.addr_port_str);

    let router = Router::new()
        .hoop(affix_state::inject(parsed_args.clone()))
        .push(Router::new().path("/pma_api").post(api_fn))
        .push(Router::new().path("{**}").get(handler_fn));
    let acceptor = TcpListener::new(&parsed_args.addr_port_str).bind().await;
    Server::new(acceptor).serve(router).await;
}
