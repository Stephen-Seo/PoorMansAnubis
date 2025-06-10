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

mod args;
mod constants;
mod error;
mod ffi;
mod json_types;
mod salvo_compat;
mod sql_types;

use std::{collections::HashMap, path::Path};

use mysql_async::{
    Pool, Row, params,
    prelude::{Query, WithParams},
};
use salvo::{http::ResBody, prelude::*};
use sql_types::AllowedIPs;
use tokio::{fs::File, io::AsyncReadExt};

use error::Error;
use uuid::Uuid;

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

    r"DROP TABLE IF EXISTS CHALLENGE_FACTORS"
        .ignore(&mut conn)
        .await?;

    r"CREATE TABLE IF NOT EXISTS CHALLENGE_FACTORS2 (
        UUID CHAR(36) CHARACTER SET ascii NOT NULL PRIMARY KEY,
        FACTORS CHAR(128) CHARACTER SET ascii NOT NULL,
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

async fn req_to_url(
    url: String,
    real_ip: Option<&str>,
) -> Result<(ResBody, u16, reqwest::header::HeaderMap), Error> {
    let req: reqwest::Response = if let Some(ip) = real_ip {
        reqwest::Client::new()
            .get(url)
            .header("x-real-ip", ip)
            .header("accept", "text/html,application/xhtml+xml,*/*")
            .send()
            .await?
    } else {
        reqwest::Client::new()
            .get(url)
            .header("accept", "text/html,application/xhtml+xml,*/*")
            .send()
            .await?
    };
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
            return Err("Failed to get client addr (invalid header)".into());
        } else {
            //eprintln!("GET from ip {}", &addr_string);
        }
    } else {
        //eprintln!("GET from ip {}", req.remote_addr());
        if let Some(ipv4) = req.remote_addr().as_ipv4() {
            //eprintln!(" ipv4: {}", ipv4.ip());
            addr_string = format!("{}", ipv4.ip());
        } else if let Some(ipv6) = req.remote_addr().as_ipv6() {
            //eprintln!(" ipv6: {}", ipv6.ip());
            addr_string = format!("{}", ipv6.ip());
        } else {
            return Err("Failed to get client addr".into());
        }
    }

    Ok(addr_string)
}

async fn set_up_factors_challenge(depot: &Depot) -> Result<(String, String), Error> {
    let args = depot.obtain::<args::Args>().unwrap();

    let (value, factors) = ffi::generate_value_and_factors_strings2(if args.factors.is_some() {
        args.factors.unwrap()
    } else {
        constants::DEFAULT_FACTORS_DIGITS
    });

    let seq: u32;

    let pool = get_db_pool(args).await?;
    {
        let mut conn = pool.get_conn().await?;

        r"LOCK TABLE SEQ_ID WRITE"
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

        let seq_row: Option<Row> = r"SELECT ID, SEQ_ID FROM SEQ_ID"
            .with(())
            .first(&mut conn)
            .await
            .map_err(Error::from)?;

        if let Some(seq_r) = seq_row {
            let id: u32 = seq_r.get(0).expect("Row should have ID");
            seq = seq_r.get(1).expect("Row should have SEQ_ID");
            r"UPDATE SEQ_ID SET SEQ_ID = :seq_id WHERE ID = :id_seq_id"
                .with(params! {"seq_id" => (seq + 1), "id_seq_id" => id})
                .ignore(&mut conn)
                .await
                .map_err(Error::from)?;
        } else {
            seq = 1;
            r"INSERT INTO SEQ_ID (SEQ_ID) VALUES (:seq_id)"
                .with(params! {"seq_id" => (seq + 1)})
                .ignore(&mut conn)
                .await
                .map_err(Error::from)?;
        }

        r"UNLOCK TABLES"
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;
    }

    let uuid = Uuid::new_v5(
        &Uuid::NAMESPACE_DNS,
        format!("{}.pma.seodisparate.com", seq).as_bytes(),
    )
    .to_string();

    let factors_hash = blake3::hash(factors.as_bytes()).to_string();

    {
        let mut conn = pool.get_conn().await?;

        r"LOCK TABLE CHALLENGE_FACTORS2 WRITE"
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

        r"INSERT INTO CHALLENGE_FACTORS2 (UUID, FACTORS) VALUES (:uuid, :factors)"
            .with(params! {"uuid" => &uuid, "factors" => factors_hash})
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

        r"UNLOCK TABLES"
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;
    }

    pool.disconnect().await?;

    Ok((value, uuid))
}

fn get_local_port_from_req(req: &Request) -> Result<u16, Error> {
    let local = req.local_addr();
    if local.is_ipv4() {
        Ok(local.as_ipv4().unwrap().port())
    } else if local.is_ipv6() {
        Ok(local.as_ipv6().unwrap().port())
    } else {
        Err("Failed to get local port, not ipv4 or ipv6!".into())
    }
}

fn get_mapped_port_to_dest(args: &args::Args, req: &Request) -> Result<String, Error> {
    let port = get_local_port_from_req(req)?;
    args.port_to_dest_urls
        .get(&port)
        .ok_or(Error::from(format!(
            "Failed to get dest-url from port {}",
            port
        )))
        .map(|s| s.to_owned())
}

#[handler]
async fn factors_js_fn(
    depot: &mut Depot,
    req: &mut Request,
    res: &mut Response,
) -> salvo::Result<()> {
    let args = depot.obtain::<args::Args>().unwrap();
    let addr_string = get_client_ip_addr(depot, req).await?;

    eprintln!("Requested challenge from {}", &addr_string);

    let (value, uuid) = set_up_factors_challenge(depot).await?;
    let js = constants::JAVASCRIPT_FACTORS_WORKER;
    let js = js
        .replacen("{API_URL}", &args.api_url, 1)
        .replacen("{LARGE_NUMBER}", &value, 1)
        .replacen("{UUID}", &uuid, 1);
    res.add_header("content-type", "text/javascript", true)?
        .write_body(js)?;

    Ok(())
}

#[handler]
async fn api_fn(depot: &Depot, req: &mut Request, res: &mut Response) -> salvo::Result<()> {
    let args = depot.obtain::<args::Args>().unwrap();
    let addr_string = get_client_ip_addr(depot, req).await?;
    //eprintln!("API: {}", &addr_string);
    let factors_response: json_types::FactorsResponse = req
        .parse_json_with_max_size(constants::DEFAULT_JSON_MAX_SIZE)
        .await
        .map_err(Error::from)?;

    let pool = get_db_pool(args).await?;

    let correct: bool;
    {
        let mut conn = pool.get_conn().await.map_err(Error::from)?;

        r"LOCK TABLE CHALLENGE_FACTORS2 WRITE"
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

        let factors_row: Option<Row> = r"SELECT FACTORS FROM CHALLENGE_FACTORS2 WHERE UUID = :uuid"
            .with(params! {"uuid" => &factors_response.id})
            .first(&mut conn)
            .await
            .map_err(Error::from)?;

        if let Some(factors_r) = factors_row {
            let factors: String = factors_r.get(0).expect("Row should have factors");
            if factors == blake3::hash(factors_response.factors.as_bytes()).to_string() {
                correct = true;
                r"DELETE FROM CHALLENGE_FACTORS2 WHERE UUID = :uuid"
                    .with(params! {"uuid" => &factors_response.id})
                    .ignore(&mut conn)
                    .await
                    .map_err(Error::from)?;
            } else {
                correct = false;
            }
        } else {
            correct = false;
        }

        r"DELETE FROM CHALLENGE_FACTORS2 WHERE TIMESTAMPDIFF(MINUTE, GEN_TIME, NOW()) >= :minutes"
            .with(params! {"minutes" => args.challenge_timeout_mins})
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

        r"UNLOCK TABLES"
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;
    }

    if correct {
        let mut conn = pool.get_conn().await.map_err(Error::from)?;
        r"INSERT INTO ALLOWED_IPS (IP) VALUES (:ip)"
            .with(params! { "ip" => &addr_string })
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;
    }

    pool.disconnect().await.map_err(Error::from)?;

    if correct {
        eprintln!("Challenge response accepted from {}", &addr_string);
        res.body("Correct")
            .add_header("content-type", "text/plain", true)?
            .status_code(StatusCode::OK);
    } else {
        eprintln!("Challenge response DENIED from {}", &addr_string);
        res.body("Incorrect")
            .add_header("content-type", "text/plain", true)?
            .status_code(StatusCode::BAD_REQUEST);
    }

    Ok(())
}

#[handler]
async fn handler_fn(depot: &Depot, req: &mut Request, res: &mut Response) -> salvo::Result<()> {
    let args = depot.obtain::<args::Args>().unwrap();

    let addr_string = get_client_ip_addr(depot, req).await?;

    let is_allowed: bool;
    {
        let pool = get_db_pool(args).await?;
        let mut conn = pool.get_conn().await.map_err(Error::from)?;

        let mut ip_entry: Option<AllowedIPs> = None;

        r"LOCK TABLE ALLOWED_IPS WRITE"
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

        r"DELETE FROM ALLOWED_IPS WHERE TIMESTAMPDIFF(MINUTE, ON_TIME, NOW()) >= :minutes"
            .with(params! {"minutes" => args.allowed_timeout_mins})
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

        r"UNLOCK TABLES"
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

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

        if let Some(_ip_ent) = &mut ip_entry {
            //eprintln!("ip existed:");
            //eprintln!("{:?}", ip_ent);
            is_allowed = true;
        } else {
            //eprintln!("ip did not exist or timed out");
            is_allowed = false;
        }

        drop(conn);
        pool.disconnect().await.map_err(Error::from)?;
    }

    if is_allowed {
        let path_str = req.uri().path_and_query().unwrap().as_str().to_owned();

        let url = if args.enable_override_dest_url {
            let override_url: Option<&str> = req.header("override-dest-url");
            if let Some(dest_url) = override_url {
                dest_url.to_owned()
            } else if let Ok(dest) = get_mapped_port_to_dest(args, req) {
                dest
            } else {
                args.dest_url.clone()
            }
        } else if let Ok(dest) = get_mapped_port_to_dest(args, req) {
            dest
        } else {
            args.dest_url.clone()
        };

        let res_body_res = req_to_url(format!("{}{}", url, &path_str), Some(&addr_string)).await;

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
    } else {
        let html = constants::HTML_BODY_FACTORS;
        let html = html.replacen("{JS_FACTORS_URL}", &args.js_factors_url, 1);
        res.body(html).status_code(StatusCode::OK);
    }

    Ok(())
}

#[tokio::main]
async fn main() {
    let mut parsed_args = args::parse_args().unwrap();
    if parsed_args.factors.is_none() {
        parsed_args.factors = Some(constants::DEFAULT_FACTORS_DIGITS);
        println!(
            "\"--factors=<digits>\" not specified, defaulting to \"{}\"",
            constants::DEFAULT_FACTORS_DIGITS
        );
    }

    init_db(&parsed_args)
        .await
        .expect("Should be able to init database");

    eprintln!("Default Dest URL: {}", &parsed_args.dest_url);
    eprintln!("Listening: {:?}", parsed_args.addr_port_strs.iter());
    eprintln!("Port Mappings: {:?}", parsed_args.port_to_dest_urls.iter());
    if parsed_args.enable_override_dest_url {
        eprintln!(
            "NOTICE: --enable-override-dest-url is active! A well set-up firewall is highly recommended!"
        );
    }

    let router = Router::new()
        .hoop(affix_state::inject(parsed_args.clone()))
        .push(Router::new().path(&parsed_args.api_url).post(api_fn))
        .push(
            Router::new()
                .path(&parsed_args.js_factors_url)
                .get(factors_js_fn),
        )
        .push(Router::new().path("{**}").get(handler_fn));
    if parsed_args.addr_port_strs.len() == 1 {
        let acceptor = TcpListener::new(&parsed_args.addr_port_strs[0])
            .bind()
            .await;
        Server::new(acceptor).serve(router).await;
    } else if parsed_args.addr_port_strs.len() == 2 {
        let first = parsed_args.addr_port_strs[0].clone();
        let second = parsed_args.addr_port_strs[1].clone();
        let acceptor = TcpListener::new(first)
            .join(TcpListener::new(second))
            .bind()
            .await;
        Server::new(acceptor).serve(router).await;
    } else {
        let mut tcp_vector_listener = salvo_compat::TcpVectorListener::new();
        for addr_port_str in &parsed_args.addr_port_strs {
            tcp_vector_listener.push(TcpListener::new(addr_port_str));
        }

        Server::new(tcp_vector_listener.bind().await)
            .serve(router)
            .await;
    }
}
