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
mod helpers;
mod json_types;
mod salvo_compat;

use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

#[cfg(feature = "mysql")]
use std::path::Path;

#[cfg(feature = "mysql")]
use mysql_async::{
    Pool, Row, params,
    prelude::{Query, WithParams},
};
#[cfg(feature = "sqlite")]
use rusqlite::Connection;
use salvo::{http::ResBody, prelude::*};
#[cfg(feature = "mysql")]
use tokio::{fs::File, io::AsyncReadExt};

use error::Error;

const GETRANDOM_BUF_SIZE: usize = 64;
const CACHED_TIMEOUT: Duration = Duration::from_secs(120);
const CACHED_CLEANUP_TIMEOUT: Duration = Duration::from_secs(3600);

#[derive(Clone, Debug)]
struct CachedAllow {
    allowed: Arc<Mutex<RefCell<HashMap<String, Instant>>>>,
    inst: Arc<Mutex<Cell<Instant>>>,
}

impl CachedAllow {
    pub fn new() -> Self {
        Self {
            allowed: Default::default(),
            inst: Arc::new(Mutex::new(Cell::new(Instant::now()))),
        }
    }

    pub fn get_allowed(&self, addr_port: &str, timeout: Duration) -> Result<bool, Error> {
        let l = self.allowed.lock();
        let l = l.map_err(|_| Error::Generic("Failed to lock CachedAllow".into()))?;
        let mut b = l.borrow_mut();
        {
            let entry = b.get(addr_port);
            if let Some(v) = entry
                && v.elapsed() < timeout
            {
                return Ok(true);
            }
        }
        b.remove(addr_port);

        Ok(false)
    }

    pub fn add_allowed(&self, addr_port: &str) -> Result<(), Error> {
        let l = self.allowed.lock();
        l.map_err(|_| Error::Generic("Failed to lock CachedAllow".into()))?
            .borrow_mut()
            .insert(addr_port.to_owned(), Instant::now());

        Ok(())
    }

    pub fn check_cleanup(&self) -> Result<(), Error> {
        let il = self.inst.lock();
        let il = il.map_err(|_| Error::Generic("Failed to lock CachedAllow.inst".into()))?;
        if il.get().elapsed() > CACHED_CLEANUP_TIMEOUT {
            il.set(Instant::now());
            let l = self.allowed.lock();
            let l = l.map_err(|_| Error::Generic("Failed to lock CachedAllow".into()))?;
            l.borrow_mut().clear();
        }

        Ok(())
    }
}

#[cfg(feature = "mysql")]
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

#[cfg(feature = "mysql")]
async fn get_mysql_db_pool(args: &args::Args) -> Result<Pool, Error> {
    if args.mysql_has_priority {
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
    } else {
        Err(String::from("Prioritizing sqlite over MySQL").into())
    }
}

#[cfg(feature = "mysql")]
async fn init_mysql_db(args: &args::Args) -> Result<(), Error> {
    let pool = get_mysql_db_pool(args).await?;

    let mut conn = pool.get_conn().await?;

    r"CREATE TABLE IF NOT EXISTS RUST_SEQ_ID (
        ID INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
        SEQ_ID INT UNSIGNED NOT NULL
    )"
    .ignore(&mut conn)
    .await?;

    r"DROP TABLE IF EXISTS RUST_CHALLENGE_FACTORS"
        .ignore(&mut conn)
        .await?;

    r"DROP TABLE IF EXISTS RUST_CHALLENGE_FACTORS_2"
        .ignore(&mut conn)
        .await?;

    r"DROP TABLE IF EXISTS RUST_CHALLENGE_FACTORS_3"
        .ignore(&mut conn)
        .await?;

    r"CREATE TABLE IF NOT EXISTS RUST_CHALLENGE_FACTORS_4 (
        ID CHAR(64) CHARACTER SET ascii NOT NULL PRIMARY KEY,
        IP VARCHAR(45) NOT NULL,
        FACTORS CHAR(64) CHARACTER SET ascii NOT NULL,
        PORT INT UNSIGNED NOT NULL,
        GEN_TIME DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
        INDEX ON_TIME_INDEX USING BTREE (GEN_TIME)
    )"
    .ignore(&mut conn)
    .await?;

    r"CREATE TABLE IF NOT EXISTS RUST_ALLOWED_IPS (
        ID INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
        IP VARCHAR(45) NOT NULL,
        PORT INT UNSIGNED NOT NULL,
        ON_TIME DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
        INDEX IP_PORT_INDEX USING HASH (IP, PORT),
        INDEX ON_TIME_INDEX USING BTREE (ON_TIME)
    )"
    .ignore(&mut conn)
    .await?;

    r"DROP TABLE IF EXISTS RUST_ID_TO_PORT"
        .ignore(&mut conn)
        .await?;

    r"DROP TABLE IF EXISTS RUST_ID_TO_PORT_2"
        .ignore(&mut conn)
        .await?;

    r"CREATE TABLE IF NOT EXISTS RUST_ID_TO_PORT_3 (
        ID CHAR(64) CHARACTER SET ascii NOT NULL PRIMARY KEY,
        PORT INT UNSIGNED NOT NULL,
        ON_TIME DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
        INDEX ON_TIME_INDEX USING BTREE (ON_TIME)
    )"
    .ignore(&mut conn)
    .await?;

    drop(conn);

    pool.disconnect().await?;

    Ok(())
}

#[cfg(feature = "sqlite")]
async fn init_sqlite_db(args: &args::Args) -> Result<(), Error> {
    use rusqlite::Connection;

    let conn = Connection::open(&args.sqlite_db_file)?;

    conn.execute(
        r#"CREATE TABLE IF NOT EXISTS SEQ_ID
        (ID INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT)"#,
        (),
    )?;

    conn.execute(
        r#"CREATE TABLE IF NOT EXISTS ID_TO_PORT
        (ID TEXT NOT NULL PRIMARY KEY,
         PORT INT UNSIGNED NOT NULL,
         ON_TIME TEXT NOT NULL DEFAULT ( datetime() ) )"#,
        (),
    )?;

    conn.execute(
        r#"CREATE INDEX IF NOT EXISTS ID_TO_PORT_TIME ON ID_TO_PORT (ON_TIME)"#,
        (),
    )?;

    conn.execute(
        r#"CREATE TABLE IF NOT EXISTS CHALLENGE_FACTOR
        (ID TEXT NOT NULL PRIMARY KEY,
         FACTORS TEXT NOT NULL,
         IP TEXT NOT NULL,
         PORT INT NOT NULL,
         ON_TIME TEXT DEFAULT ( datetime() ) )"#,
        (),
    )?;

    conn.execute(
        r#"CREATE INDEX IF NOT EXISTS CHALLENGE_FACTOR_TIME
        ON CHALLENGE_FACTOR (ON_TIME)"#,
        (),
    )?;

    conn.execute(
        r#"CREATE TABLE IF NOT EXISTS ALLOWED_IP
        (ID INTEGER PRIMARY KEY AUTOINCREMENT,
         IP TEXT NOT NULL,
         PORT INTEGER NOT NULL,
         ON_TIME TEXT NOT NULL DEFAULT ( datetime() ) )"#,
        (),
    )?;

    conn.execute(
        r#"CREATE INDEX IF NOT EXISTS ALLOWED_IP_IP ON ALLOWED_IP (IP)"#,
        (),
    )?;

    conn.execute(
        r#"CREATE INDEX IF NOT EXISTS ALLOWED_IP_TIME ON ALLOWED_IP (ON_TIME)"#,
        (),
    )?;

    Ok(())
}

async fn init_db(args: &args::Args) -> Result<(), Error> {
    #[cfg(all(feature = "mysql", feature = "sqlite"))]
    if args.mysql_has_priority {
        init_mysql_db(args).await?;
    } else {
        init_sqlite_db(args).await?;
    }
    #[cfg(all(feature = "mysql", not(feature = "sqlite")))]
    init_mysql_db(args).await?;
    #[cfg(all(feature = "sqlite", not(feature = "mysql")))]
    init_sqlite_db(args).await?;

    Ok(())
}

async fn req_to_url(
    url: String,
    real_ip: Option<&str>,
    body: Option<Vec<u8>>,
) -> Result<(ResBody, u16, reqwest::header::HeaderMap), Error> {
    let req: reqwest::Response = if let Some(ip) = real_ip {
        if let Some(body) = body {
            reqwest::Client::new()
                .post(url)
                .body(body)
                .header("x-real-ip", ip)
                .header("accept", "text/html,application/xhtml+xml,*/*")
                .send()
                .await?
        } else {
            reqwest::Client::new()
                .get(url)
                .header("x-real-ip", ip)
                .header("accept", "text/html,application/xhtml+xml,*/*")
                .send()
                .await?
        }
    } else if let Some(body) = body {
        reqwest::Client::new()
            .post(url)
            .body(body)
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

#[cfg(feature = "mysql")]
async fn get_next_seq_mysql(args: &args::Args) -> Result<u64, Error> {
    let seq: u64;
    let pool = get_mysql_db_pool(args).await?;
    let mut conn = pool.get_conn().await?;

    r"LOCK TABLE RUST_SEQ_ID WRITE"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    let seq_row: Option<Row> = r"SELECT ID, SEQ_ID FROM RUST_SEQ_ID"
        .with(())
        .first(&mut conn)
        .await
        .map_err(Error::from)?;

    if let Some(seq_r) = seq_row {
        let id: u64 = seq_r.get(0).expect("Row should have ID");
        seq = seq_r.get(1).expect("Row should have SEQ_ID");
        if seq + 1 >= 0x7FFFFFFF {
            r"UPDATE RUST_SEQ_ID SET SEQ_ID = :seq_id WHERE ID = :id_seq_id"
                .with(params! {"seq_id" => (1), "id_seq_id" => id})
                .ignore(&mut conn)
                .await
                .map_err(Error::from)?;
        } else {
            r"UPDATE RUST_SEQ_ID SET SEQ_ID = :seq_id WHERE ID = :id_seq_id"
                .with(params! {"seq_id" => (seq + 1), "id_seq_id" => id})
                .ignore(&mut conn)
                .await
                .map_err(Error::from)?;
        }
    } else {
        seq = 1;
        r"INSERT INTO RUST_SEQ_ID (SEQ_ID) VALUES (:seq_id)"
            .with(params! {"seq_id" => (seq + 1)})
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;
    }

    r"UNLOCK TABLES"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    Ok(seq)
}

#[cfg(feature = "sqlite")]
async fn get_next_seq_sqlite(args: &args::Args) -> Result<u64, Error> {
    let seq: u64;
    let conn = Connection::open(&args.sqlite_db_file)?;

    let query_res = conn.query_one(r#"SELECT ID FROM SEQ_ID"#, (), |r| r.get::<usize, u64>(0));
    match query_res {
        Ok(s) => {
            seq = s;
            if seq + 1 >= 0xFFFFFFFF {
                conn.execute(r#"UPDATE SEQ_ID SET ID = ?1"#, (1,))?;
            } else {
                conn.execute(r#"UPDATE SEQ_ID SET ID = ?1"#, (s + 1,))?;
            }
        }
        Err(rusqlite::Error::QueryReturnedNoRows) => {
            seq = 1;
            conn.execute(r#"INSERT INTO SEQ_ID (ID) VALUES (1)"#, ())?;
        }
        Err(e) => return Err(e.into()),
    }

    Ok(seq)
}

#[cfg(feature = "mysql")]
async fn has_challenge_factor_id_mysql(args: &args::Args, hash: &str) -> Result<bool, Error> {
    let pool = get_mysql_db_pool(args).await?;
    let mut conn = pool.get_conn().await?;

    let with_id: Vec<String> = r"SELECT ID FROM RUST_CHALLENGE_FACTORS_4 WHERE ID = ?"
        .with((hash,))
        .map(&mut conn, |(id,)| id)
        .await?;

    Ok(!with_id.is_empty())
}

#[cfg(feature = "sqlite")]
async fn has_challenge_factor_id_sqlite(args: &args::Args, hash: &str) -> Result<bool, Error> {
    let conn = Connection::open(&args.sqlite_db_file)?;

    match conn.query_one(r"SELECT ID FROM SEQ_ID WHERE ID = ?1", (hash,), |r| {
        r.get::<usize, String>(0)
    }) {
        Ok(_) => Ok(true),
        Err(_) => Ok(false),
    }
}

#[cfg(feature = "mysql")]
async fn set_challenge_factor_mysql(
    args: &args::Args,
    ip: &str,
    hash: &str,
    port: u16,
    factors_hash: &str,
) -> Result<(), Error> {
    let pool = get_mysql_db_pool(args).await?;
    let mut conn = pool.get_conn().await?;

    r"LOCK TABLE RUST_CHALLENGE_FACTORS_4 WRITE"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    r"INSERT INTO RUST_CHALLENGE_FACTORS_4 (ID, IP, PORT, FACTORS) VALUES (:id, :ip, :port, :factors)"
        .with(params! {"id" => hash, "ip" => ip, "port" => port, "factors" => factors_hash})
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    r"UNLOCK TABLES"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    Ok(())
}

#[cfg(feature = "sqlite")]
async fn set_challenge_factor_sqlite(
    args: &args::Args,
    ip: &str,
    hash: &str,
    port: u16,
    factors_hash: &str,
) -> Result<(), Error> {
    let conn = Connection::open(&args.sqlite_db_file)?;

    conn.execute(
        r"INSERT INTO CHALLENGE_FACTOR (ID, FACTORS, IP, PORT) VALUES (?1, ?2, ?3, ?4)",
        (hash, factors_hash, ip, port),
    )?;

    Ok(())
}

async fn set_up_factors_challenge(
    depot: &Depot,
    ip: &str,
    port: u16,
) -> Result<(String, String), Error> {
    let args = depot.obtain::<args::Args>().unwrap();

    let (value, factors) = ffi::generate_value_and_factors_strings2(if args.factors.is_some() {
        args.factors.unwrap()
    } else {
        constants::DEFAULT_FACTORS_DIGITS
    });

    let mut hash: String;

    #[allow(clippy::needless_late_init)]
    let seq: u64;

    #[cfg(all(feature = "mysql", feature = "sqlite"))]
    if args.mysql_has_priority {
        seq = get_next_seq_mysql(args).await?;
    } else {
        seq = get_next_seq_sqlite(args).await?;
    }

    #[cfg(all(feature = "mysql", not(feature = "sqlite")))]
    {
        seq = get_next_seq_mysql(args).await?;
    }

    #[cfg(all(feature = "sqlite", not(feature = "mysql")))]
    {
        seq = get_next_seq_sqlite(args).await?;
    }

    loop {
        let mut hasher = blake3::Hasher::new();
        hasher.update("pma.seodisparate.com".as_bytes());
        hasher.update(&seq.to_ne_bytes());
        let mut buf = [0u8; GETRANDOM_BUF_SIZE];
        getrandom::fill(&mut buf)?;
        hasher.update(&buf);
        let hasher = hasher.finalize();

        hash = hasher.to_string();

        #[cfg(all(feature = "mysql", feature = "sqlite"))]
        if args.mysql_has_priority {
            if has_challenge_factor_id_mysql(args, &hash).await? {
                continue;
            }
        } else if has_challenge_factor_id_sqlite(args, &hash).await? {
            continue;
        }
        #[cfg(all(feature = "mysql", not(feature = "sqlite")))]
        if has_challenge_factor_id_mysql(args, &hash).await? {
            continue;
        }
        #[cfg(all(feature = "sqlite", not(feature = "mysql")))]
        if has_challenge_factor_id_sqlite(args, &hash).await? {
            continue;
        }

        let factors_hash = blake3::hash(factors.as_bytes()).to_string();

        #[cfg(all(feature = "mysql", feature = "sqlite"))]
        if args.mysql_has_priority {
            set_challenge_factor_mysql(args, ip, &hash, port, &factors_hash).await?;
        } else {
            set_challenge_factor_sqlite(args, ip, &hash, port, &factors_hash).await?;
        }
        #[cfg(all(feature = "mysql", not(feature = "sqlite")))]
        {
            set_challenge_factor_mysql(args, ip, &hash, port, &factors_hash).await?;
        }
        #[cfg(all(feature = "sqlite", not(feature = "mysql")))]
        {
            set_challenge_factor_sqlite(args, ip, &hash, port, &factors_hash).await?;
        }
        break;
    }

    Ok((value, hash))
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

#[cfg(feature = "mysql")]
async fn challenge_port_mysql(args: &args::Args, id: &str) -> Result<u16, Error> {
    let mut port: Option<u16> = None;
    let pool = get_mysql_db_pool(args).await?;
    let mut conn = pool.get_conn().await.map_err(Error::from)?;

    r"LOCK TABLE RUST_ID_TO_PORT_3 WRITE"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    {
        let sel_row: Option<Row> = r"SELECT PORT FROM RUST_ID_TO_PORT_3 WHERE ID = :id"
            .with(params! {"id" => id})
            .first(&mut conn)
            .await
            .map_err(Error::from)?;

        if let Some(sel_r) = sel_row {
            port = sel_r.get(0);
        }
    }

    if port.is_some() {
        r"DELETE FROM RUST_ID_TO_PORT_3 WHERE ID = :id"
            .with(params! {"id" => id})
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;
    }

    r"UNLOCK TABLES"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    port.ok_or(Into::<Error>::into(String::from(
        "gen challenge, failed to get port",
    )))
}

#[cfg(feature = "sqlite")]
async fn challenge_port_sqlite(args: &args::Args, id: &str) -> Result<u16, Error> {
    let conn = Connection::open(&args.sqlite_db_file)?;

    match conn.query_one(r"SELECT PORT FROM ID_TO_PORT WHERE ID = ?1", (id,), |r| {
        r.get::<usize, u16>(0)
    }) {
        Ok(p) => {
            conn.execute(r"DELETE FROM ID_TO_PORT WHERE ID = ?1", (id,))?;
            Ok(p)
        }
        Err(e) => Err(e.into()),
    }
}

#[handler]
async fn factors_js_fn(
    depot: &mut Depot,
    req: &mut Request,
    res: &mut Response,
) -> salvo::Result<()> {
    let args = depot.obtain::<args::Args>().unwrap();
    let addr_string = get_client_ip_addr(depot, req).await?;
    let id: String = req.query("id").ok_or(crate::Error::Generic(
        "No id passed to factors_js url!".to_owned(),
    ))?;

    #[allow(unused_assignments)]
    let mut port: Result<u16, Error> = Err(Error::Generic("port uninitialized".into()));
    #[cfg(all(feature = "mysql", feature = "sqlite"))]
    if args.mysql_has_priority {
        port = challenge_port_mysql(args, &id).await;
    } else {
        port = challenge_port_sqlite(args, &id).await;
    }
    #[cfg(all(feature = "mysql", not(feature = "sqlite")))]
    {
        port = challenge_port_mysql(args, &id).await;
    }
    #[cfg(all(feature = "sqlite", not(feature = "mysql")))]
    {
        port = challenge_port_sqlite(args, &id).await;
    }
    let port: u16 = port?;

    eprintln!("Requested challenge from {}:{}", &addr_string, port);

    let (value, uuid) = set_up_factors_challenge(depot, &addr_string, port).await?;
    let js = constants::JAVASCRIPT_FACTORS_WORKER;
    let js = js
        .replacen("{API_URL}", &args.api_url, 1)
        .replacen("{LARGE_NUMBER}", &value, 1)
        .replacen("{UUID}", &uuid, 1);
    res.add_header("content-type", "text/javascript", true)?
        .write_body(js)?;

    Ok(())
}

#[cfg(feature = "mysql")]
async fn validate_client_mysql(
    args: &args::Args,
    factors_response: &json_types::FactorsResponse,
    addr: &str,
) -> Result<u16, Error> {
    let correct;
    let mut port: u16 = 0;
    let pool = get_mysql_db_pool(args).await?;
    let mut conn = pool.get_conn().await.map_err(Error::from)?;

    r"LOCK TABLE RUST_CHALLENGE_FACTORS_4 WRITE"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    r"DELETE FROM RUST_CHALLENGE_FACTORS_4 WHERE TIMESTAMPDIFF(MINUTE, GEN_TIME, NOW()) >= :minutes"
            .with(params! {"minutes" => args.challenge_timeout_mins})
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

    let hashed_factors = blake3::hash(factors_response.factors.as_bytes()).to_string();

    let addr_port_row: Option<Row> =
        r"SELECT IP, PORT FROM RUST_CHALLENGE_FACTORS_4 WHERE ID = :id AND FACTORS = :factors"
            .with(params! {"id" => &factors_response.id, "factors" => hashed_factors})
            .first(&mut conn)
            .await
            .map_err(Error::from)?;

    if let Some(addr_port_r) = addr_port_row {
        let r_addr: String = addr_port_r.get(0).ok_or(Into::<Error>::into(String::from(
            "No IP from ChallengeFactors",
        )))?;
        if r_addr == addr {
            port = addr_port_r.get(1).ok_or(Into::<Error>::into(String::from(
                "No Port from ChallengeFactors",
            )))?;
            correct = true;
            r"DELETE FROM RUST_CHALLENGE_FACTORS_4 WHERE ID = :id"
                .with(params! {"id" => &factors_response.id})
                .ignore(&mut conn)
                .await
                .map_err(Error::from)?;
        } else {
            correct = false;
        }
    } else {
        correct = false;
    }

    r"UNLOCK TABLES"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    if correct && port != 0 {
        r"INSERT INTO RUST_ALLOWED_IPS (IP, PORT) VALUES (:ip, :port)"
            .with(params! { "ip" => addr, "port" => port })
            .ignore(&mut conn)
            .await
            .map_err(Error::from)?;

        Ok(port)
    } else {
        Err(String::from("Incorrect").into())
    }
}

#[cfg(feature = "sqlite")]
async fn validate_client_sqlite(
    args: &args::Args,
    factors_response: &json_types::FactorsResponse,
    addr: &str,
) -> Result<u16, Error> {
    let conn = Connection::open(&args.sqlite_db_file)?;

    let hashed_factors = blake3::hash(factors_response.factors.as_bytes()).to_string();

    conn.execute(&format!(r#"DELETE FROM CHALLENGE_FACTOR WHERE datetime(ON_TIME, '{} minutes') < datetime('now')"#, args.challenge_timeout_mins), ())?;

    let res = conn.query_one(
        r"SELECT IP, PORT FROM CHALLENGE_FACTOR WHERE ID = ?1 AND FACTORS = ?2",
        (&factors_response.id, &hashed_factors),
        |r| Ok((r.get::<usize, String>(0), r.get::<usize, u16>(1))),
    );

    if let Ok((Ok(ip), Ok(port))) = res {
        if ip == addr && port != 0 {
            conn.execute(
                r"DELETE FROM CHALLENGE_FACTOR WHERE ID = ?1",
                (&factors_response.id,),
            )?;
            conn.execute(
                r"INSERT INTO ALLOWED_IP (IP, PORT) VALUES (?1, ?2)",
                (&ip, &port),
            )?;
            Ok(port)
        } else {
            Err(String::from("Invalid entries from ChallengeFactor").into())
        }
    } else {
        Err(String::from("Incorrect").into())
    }
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

    helpers::validate_client_response(&factors_response.factors)?;

    #[allow(unused_assignments)]
    let mut validate_result: Result<u16, Error> = Err(String::from("Invalid state").into());
    #[cfg(all(feature = "mysql", feature = "sqlite"))]
    if args.mysql_has_priority {
        validate_result = validate_client_mysql(args, &factors_response, &addr_string).await;
    } else {
        validate_result = validate_client_sqlite(args, &factors_response, &addr_string).await;
    }
    #[cfg(all(feature = "mysql", not(feature = "sqlite")))]
    {
        validate_result = validate_client_mysql(args, &factors_response, &addr_string).await;
    }
    #[cfg(all(feature = "sqlite", not(feature = "mysql")))]
    {
        validate_result = validate_client_sqlite(args, &factors_response, &addr_string).await;
    }

    if let Ok(port) = validate_result {
        eprintln!("Challenge response accepted from {}:{}", &addr_string, port);
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

#[cfg(feature = "mysql")]
async fn check_is_allowed_mysql(args: &args::Args, addr: &str, port: u16) -> Result<bool, Error> {
    let is_allowed: bool;
    let pool = get_mysql_db_pool(args).await?;
    let mut conn = pool.get_conn().await.map_err(Error::from)?;

    r"LOCK TABLE RUST_ALLOWED_IPS WRITE"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    r"DELETE FROM RUST_ALLOWED_IPS WHERE TIMESTAMPDIFF(MINUTE, ON_TIME, NOW()) >= :minutes"
        .with(params! {"minutes" => args.allowed_timeout_mins})
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    r"UNLOCK TABLES"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    r"LOCK TABLE RUST_ALLOWED_IPS READ"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    let ip_entry_row: Option<Row> =
        r"SELECT IP, ON_TIME FROM RUST_ALLOWED_IPS WHERE IP = :ipaddr AND PORT = :port"
            .with(params! {"ipaddr" => &addr, "port" => port})
            .first(&mut conn)
            .await
            .map_err(Error::from)?;

    r"UNLOCK TABLES"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    if let Some(_ip_ent) = ip_entry_row {
        //eprintln!("ip existed:");
        //eprintln!("{:?}", ip_ent);
        is_allowed = true;
    } else {
        //eprintln!("ip did not exist or timed out");
        is_allowed = false;
    }

    drop(conn);
    pool.disconnect().await.map_err(Error::from)?;
    Ok(is_allowed)
}

#[cfg(feature = "sqlite")]
async fn check_is_allowed_sqlite(args: &args::Args, addr: &str, port: u16) -> Result<bool, Error> {
    let conn = Connection::open(&args.sqlite_db_file)?;

    conn.execute(
        &format!(
            r#"DELETE FROM ALLOWED_IP WHERE datetime(ON_TIME, '{} minutes') < datetime('now')"#,
            args.allowed_timeout_mins
        ),
        (),
    )?;

    let mut stmt = conn.prepare(r"SELECT PORT FROM ALLOWED_IP WHERE IP = ?1 AND PORT = ?2")?;
    let rows = stmt.query_map((addr, port), |r| r.get::<usize, u16>(0));
    let is_allowed: bool = rows?.count() != 0;

    Ok(is_allowed)
}

#[cfg(feature = "mysql")]
async fn init_id_to_port_mysql(args: &args::Args, port: u16) -> Result<String, Error> {
    let mut hash: String;
    let pool = get_mysql_db_pool(args).await?;
    let mut conn = pool.get_conn().await.map_err(Error::from)?;

    r"LOCK TABLE RUST_ID_TO_PORT_3 WRITE"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    r"DELETE FROM RUST_ID_TO_PORT_3 WHERE TIMESTAMPDIFF(MINUTE, ON_TIME, NOW()) >= :minutes"
        .with(params! {"minutes" => args.challenge_timeout_mins})
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    let mut hasher = blake3::Hasher::new();
    let mut buf = [0u8; GETRANDOM_BUF_SIZE];
    getrandom::fill(&mut buf).map_err(Into::<Error>::into)?;
    hasher.update(&buf);
    hash = hasher.finalize().to_string();

    loop {
        let row: Result<Option<Row>, _> = r"SELECT ID FROM RUST_ID_TO_PORT_3 WHERE ID = :id"
            .with(params! {"id" => &hash})
            .first(&mut conn)
            .await;

        if let Ok(Some(r)) = &row
            && let Some(id) = r.get::<String, usize>(0)
            && id == hash
        {
            hasher = blake3::Hasher::new();
            getrandom::fill(&mut buf).map_err(Into::<Error>::into)?;
            hasher.update(&buf);
            hash = hasher.finalize().to_string();
            continue;
        }
        break;
    }

    r"INSERT INTO RUST_ID_TO_PORT_3 (ID, PORT) VALUES (:id, :port)"
        .with(params! {"id" => &hash, "port" => port})
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    r"UNLOCK TABLES"
        .ignore(&mut conn)
        .await
        .map_err(Error::from)?;

    Ok(hash)
}

#[cfg(feature = "sqlite")]
async fn init_id_to_port_sqlite(args: &args::Args, port: u16) -> Result<String, Error> {
    let mut hash: String;

    let conn = Connection::open(&args.sqlite_db_file)?;

    conn.execute(
        &format!(
            r#"DELETE FROM ID_TO_PORT WHERE datetime(ON_TIME, '{} minutes') < datetime('now')"#,
            args.challenge_timeout_mins
        ),
        (),
    )?;

    let mut hasher = blake3::Hasher::new();
    let mut buf = [0u8; GETRANDOM_BUF_SIZE];
    getrandom::fill(&mut buf)?;
    hasher.update(&buf);
    hash = hasher.finalize().to_string();

    while conn
        .query_one(
            r"SELECT PORT FROM ID_TO_PORT WHERE ID = ?1",
            (&hash,),
            |r| r.get::<usize, u16>(0),
        )
        .is_ok()
    {
        hasher.reset();
        getrandom::fill(&mut buf)?;
        hasher.update(&buf);
        hash = hasher.finalize().to_string();
    }

    conn.execute(
        r"INSERT INTO ID_TO_PORT (ID, PORT) VALUES (?1, ?2)",
        (&hash, port),
    )?;

    Ok(hash)
}

#[handler]
async fn handler_fn(depot: &Depot, req: &mut Request, res: &mut Response) -> salvo::Result<()> {
    let args = depot.obtain::<args::Args>().unwrap();
    let cached_allow: &CachedAllow = depot.obtain::<CachedAllow>().unwrap();
    cached_allow.check_cleanup()?;

    let addr_string = get_client_ip_addr(depot, req).await?;

    let port: Option<u16> = match req.local_addr() {
        salvo::conn::SocketAddr::Unknown => None,
        salvo::conn::SocketAddr::IPv4(socket_addr_v4) => Some(socket_addr_v4.port()),
        salvo::conn::SocketAddr::IPv6(socket_addr_v6) => Some(socket_addr_v6.port()),
        salvo::conn::SocketAddr::Unix(_socket_addr) => None,
        _ => None,
    };
    let port: u16 = port.ok_or(crate::Error::Generic(
        "Should have port from request!".to_owned(),
    ))?;

    let mut is_allowed: bool =
        cached_allow.get_allowed(&req.remote_addr().to_string(), CACHED_TIMEOUT)?;
    if !is_allowed {
        #[cfg(all(feature = "mysql", feature = "sqlite"))]
        if args.mysql_has_priority {
            is_allowed = check_is_allowed_mysql(args, &addr_string, port).await?;
            if is_allowed {
                cached_allow.add_allowed(&req.remote_addr().to_string())?;
            }
        } else {
            is_allowed = check_is_allowed_sqlite(args, &addr_string, port).await?;
            if is_allowed {
                cached_allow.add_allowed(&req.remote_addr().to_string())?;
            }
        }
        #[cfg(all(feature = "mysql", not(feature = "sqlite")))]
        {
            is_allowed = check_is_allowed_mysql(args, &addr_string, port).await?;
            if is_allowed {
                cached_allow.add_allowed(&req.remote_addr().to_string())?;
            }
        }
        #[cfg(all(feature = "sqlite", not(feature = "mysql")))]
        {
            is_allowed = check_is_allowed_sqlite(args, &addr_string, port).await?;
            if is_allowed {
                cached_allow.add_allowed(&req.remote_addr().to_string())?;
            }
        }
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

        let payload: Vec<u8> = req.payload().await?.to_vec();
        let res_body_res = if payload.is_empty() {
            req_to_url(format!("{}{}", url, &path_str), Some(&addr_string), None).await
        } else {
            req_to_url(
                format!("{}{}", url, &path_str),
                Some(&addr_string),
                Some(payload),
            )
            .await
        };

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
        #[allow(unused_assignments)]
        let mut hash: Option<String> = None;

        #[cfg(all(feature = "mysql", feature = "sqlite"))]
        if args.mysql_has_priority {
            hash = Some(init_id_to_port_mysql(args, port).await?);
        } else {
            hash = Some(init_id_to_port_sqlite(args, port).await?);
        }
        #[cfg(all(feature = "mysql", not(feature = "sqlite")))]
        {
            hash = Some(init_id_to_port_mysql(args, port).await?);
        }
        #[cfg(all(feature = "sqlite", not(feature = "mysql")))]
        {
            hash = Some(init_id_to_port_sqlite(args, port).await?);
        }

        if let Some(hash) = hash {
            let html = constants::HTML_BODY_FACTORS;
            let html = html.replacen(
                "{JS_FACTORS_URL}",
                &format!("{}?id={}", args.js_factors_url, &hash),
                1,
            );
            res.body(html).status_code(StatusCode::OK);
        } else {
            res.render("Failed to init request challenge");
            res.status_code = Some(StatusCode::INTERNAL_SERVER_ERROR);
        }
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
        .hoop(affix_state::inject(CachedAllow::new()))
        .push(Router::new().path(&parsed_args.api_url).post(api_fn))
        .push(
            Router::new()
                .path(&parsed_args.js_factors_url)
                .get(factors_js_fn),
        )
        .push(Router::new().path("{**}").get(handler_fn).post(handler_fn));
    if parsed_args.addr_port_strs.len() == 1 {
        let addr_port_str = parsed_args.addr_port_strs[0].clone();
        let acceptor = TcpListener::new(addr_port_str).bind().await;
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
        for addr_port_str in parsed_args.addr_port_strs.clone().into_iter() {
            tcp_vector_listener.push(TcpListener::new(addr_port_str));
        }

        Server::new(tcp_vector_listener.bind().await)
            .serve(router)
            .await;
    }
}
