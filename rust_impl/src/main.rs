// ISC License
//
// Copyright (c) 2025-2026 Stephen Seo
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
mod ffi_msql;
mod helpers;
mod json_types;
mod salvo_compat;
mod signal;

use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use std::{path::Path, sync::atomic::AtomicBool};

#[cfg(feature = "sqlite")]
use rusqlite::Connection;
use salvo::{http::ResBody, prelude::*};
use tokio::{
    fs::File,
    io::{AsyncBufReadExt, BufReader},
    sync::Mutex as tMutex,
};

use error::Error;

use crate::ffi_msql::{MSQLParamsWrapper, MSQLValueEnum, MSQLWrapper};

const GETRANDOM_BUF_SIZE: usize = 64;
const CACHED_TIMEOUT: Duration = Duration::from_secs(120);
const CACHED_CLEANUP_TIMEOUT: Duration = Duration::from_secs(3600);

#[allow(unused)]
const MSQL_RUST_SEQ_ID_1_CREATE: &str = r"CREATE TABLE IF NOT EXISTS RUST_SEQ_ID_1 (
        ID INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
        SEQ_ID INT8 UNSIGNED NOT NULL
    )";

#[allow(unused)]
const MSQL_RUST_CHALLENGE_FACTORS_4_CREATE: &str = r"CREATE TABLE IF NOT EXISTS RUST_CHALLENGE_FACTORS_4 (
        ID CHAR(64) CHARACTER SET ascii NOT NULL PRIMARY KEY,
        IP VARCHAR(45) NOT NULL,
        FACTORS CHAR(64) CHARACTER SET ascii NOT NULL,
        PORT INT UNSIGNED NOT NULL,
        GEN_TIME DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
        INDEX ON_TIME_INDEX USING BTREE (GEN_TIME)
    )";

#[allow(unused)]
const MSQL_RUST_ALLOWED_IPS_CREATE: &str = r"CREATE TABLE IF NOT EXISTS RUST_ALLOWED_IPS (
        ID INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
        IP VARCHAR(45) NOT NULL,
        PORT INT UNSIGNED NOT NULL,
        ON_TIME DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
        INDEX IP_PORT_INDEX USING HASH (IP, PORT),
        INDEX ON_TIME_INDEX USING BTREE (ON_TIME)
    )";

#[allow(unused)]
const MSQL_RUST_ID_TO_PORT_3_CREATE: &str = r"CREATE TABLE IF NOT EXISTS RUST_ID_TO_PORT_3 (
        ID CHAR(64) CHARACTER SET ascii NOT NULL PRIMARY KEY,
        PORT INT UNSIGNED NOT NULL,
        ON_TIME DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
        INDEX ON_TIME_INDEX USING BTREE (ON_TIME)
    )";

#[allow(unused)]
const SQLITE_SEQ_ID_CREATE: &str = r"CREATE TABLE IF NOT EXISTS SEQ_ID
        (ID INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT)";

#[allow(unused)]
const SQLITE_ID_TO_PORT_CREATE: &str = r"CREATE TABLE IF NOT EXISTS ID_TO_PORT
        (ID TEXT NOT NULL PRIMARY KEY,
         PORT INT UNSIGNED NOT NULL,
         ON_TIME TEXT NOT NULL DEFAULT ( datetime() ) )";

#[allow(unused)]
const SQLITE_CHALLENGE_FACTOR_CREATE: &str = r"CREATE TABLE IF NOT EXISTS CHALLENGE_FACTOR
        (ID TEXT NOT NULL PRIMARY KEY,
         FACTORS TEXT NOT NULL,
         IP TEXT NOT NULL,
         PORT INT NOT NULL,
         ON_TIME TEXT DEFAULT ( datetime() ) )";

#[allow(unused)]
const SQLITE_ALLOWED_IP_CREATE: &str = r"CREATE TABLE IF NOT EXISTS ALLOWED_IP
        (ID INTEGER PRIMARY KEY AUTOINCREMENT,
         IP TEXT NOT NULL,
         PORT INTEGER NOT NULL,
         ON_TIME TEXT NOT NULL DEFAULT ( datetime() ) )";

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

async fn parse_db_conf(config: &Path) -> Result<HashMap<String, String>, Error> {
    let mut map: HashMap<String, String> = HashMap::new();

    let mut lines = BufReader::new(File::open(config).await?).lines();
    while let Some(line) = lines.next_line().await? {
        if let Some(eq_pos) = line.find('=') {
            let (first, last) = line.split_at(eq_pos);
            let (_, actual_last) = last.split_at(1);
            map.insert(first.to_owned(), actual_last.to_owned());
        } else {
            eprintln!("WARNING: parse_db_conf(): config had invalid entry!");
        }
    }

    Ok(map)
}

async fn get_mysql_db_conn(args: &args::Args) -> Result<MSQLWrapper, Error> {
    if args.mysql_has_priority {
        let config_map = parse_db_conf(&args.mysql_config_file)
            .await
            .expect("Parse config for mysql usage");

        let msql_conn = MSQLWrapper::try_new(
            config_map
                .get("address")
                .ok_or("Address not in msql config")?,
            config_map
                .get("port")
                .ok_or("Port not in msql config")?
                .parse()?,
            config_map.get("user").ok_or("User not in msql config")?,
            config_map
                .get("password")
                .ok_or("Password not in msql config")?,
            config_map
                .get("database")
                .ok_or("Database nto in msql config")?,
        )
        .map_err(|_| "Failed to create msql connection")?;
        Ok(msql_conn)
    } else {
        Err(String::from("Prioritizing sqlite over msql").into())
    }
}

async fn init_mysql_db(args: &args::Args) -> Result<(), Error> {
    let mut conn = get_mysql_db_conn(args).await?;

    conn.query_drop(r"DROP TABLE IF EXISTS RUST_SEQ_ID")?;

    conn.query_drop(MSQL_RUST_SEQ_ID_1_CREATE)?;

    conn.query_drop(r"DROP TABLE IF EXISTS RUST_CHALLENGE_FACTORS")?;

    conn.query_drop(r"DROP TABLE IF EXISTS RUST_CHALLENGE_FACTORS_2")?;

    conn.query_drop(r"DROP TABLE IF EXISTS RUST_CHALLENGE_FACTORS_3")?;

    conn.query_drop(MSQL_RUST_CHALLENGE_FACTORS_4_CREATE)?;

    conn.query_drop(MSQL_RUST_ALLOWED_IPS_CREATE)?;

    conn.query_drop(r"DROP TABLE IF EXISTS RUST_ID_TO_PORT")?;

    conn.query_drop(r"DROP TABLE IF EXISTS RUST_ID_TO_PORT_2")?;

    conn.query_drop(MSQL_RUST_ID_TO_PORT_3_CREATE)?;

    Ok(())
}

#[cfg(feature = "sqlite")]
async fn init_sqlite_db(args: &args::Args) -> Result<(), Error> {
    use rusqlite::Connection;

    let conn = Connection::open(&args.sqlite_db_file)?;

    conn.execute(SQLITE_SEQ_ID_CREATE, ())?;

    conn.execute(SQLITE_ID_TO_PORT_CREATE, ())?;

    conn.execute(
        r#"CREATE INDEX IF NOT EXISTS ID_TO_PORT_TIME ON ID_TO_PORT (ON_TIME)"#,
        (),
    )?;

    conn.execute(SQLITE_CHALLENGE_FACTOR_CREATE, ())?;

    conn.execute(
        r#"CREATE INDEX IF NOT EXISTS CHALLENGE_FACTOR_TIME
        ON CHALLENGE_FACTOR (ON_TIME)"#,
        (),
    )?;

    conn.execute(SQLITE_ALLOWED_IP_CREATE, ())?;

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
    #[cfg(feature = "sqlite")]
    if args.mysql_has_priority {
        init_mysql_db(args).await?;
    } else {
        init_sqlite_db(args).await?;
    }
    #[cfg(not(feature = "sqlite"))]
    init_mysql_db(args).await?;

    Ok(())
}

async fn req_to_url(
    url: String,
    real_ip: Option<&str>,
    body: Option<Vec<u8>>,
    method: &str,
) -> Result<(ResBody, u16, reqwest::header::HeaderMap), Error> {
    let client = reqwest::Client::new();
    let req_builder = match method {
        "GET" => client.get(url),
        "POST" => client.post(url),
        "PUT" => client.put(url),
        "DELETE" => client.delete(url),
        "HEAD" => client.request(reqwest::Method::HEAD, url),
        "OPTIONS" => client.request(reqwest::Method::OPTIONS, url),
        "PATCH" => client.request(reqwest::Method::PATCH, url),
        "TRACE" => client.request(reqwest::Method::TRACE, url),
        _ => return Err(Error::Generic(format!("Invalid HTML method {}!", method))),
    };
    let req: reqwest::Response = if let Some(ip) = real_ip {
        if let Some(body) = body {
            req_builder
                .body(body)
                .header("x-real-ip", ip)
                .header("accept", "text/html,application/xhtml+xml,*/*")
                .send()
                .await?
        } else {
            req_builder
                .header("x-real-ip", ip)
                .header("accept", "text/html,application/xhtml+xml,*/*")
                .send()
                .await?
        }
    } else if let Some(body) = body {
        req_builder
            .body(body)
            .header("accept", "text/html,application/xhtml+xml,*/*")
            .send()
            .await?
    } else {
        req_builder
            .header("accept", "text/html,application/xhtml+xml,*/*")
            .send()
            .await?
    };
    let status = req.status().as_u16();
    let headers = req.headers().to_owned();
    Ok((ResBody::Once(req.bytes().await?), status, headers))
}

pub struct ClientIPAddrRet {
    pub addr: String,
    pub remote_port: Option<u16>,
    pub local_port: Option<u16>,
}

async fn get_client_ip_addr(depot: &Depot, req: &mut Request) -> Result<ClientIPAddrRet, Error> {
    let args = depot.obtain::<args::Args>().unwrap();
    let addr_string: String;
    let local_port: Option<u16>;
    let remote_port: Option<u16>;

    let real_ip_header = req.headers().get("x-real-ip");

    if args.enable_x_real_ip_header
        && let Some(real_ip_h) = real_ip_header
    {
        addr_string = real_ip_h.to_str().map_err(Error::from)?.to_owned();
        if addr_string.is_empty() {
            return Err("Failed to get client addr (invalid header)".into());
        }

        if let Some(ipv4) = req.local_addr().as_ipv4() {
            local_port = Some(ipv4.port());
        } else if let Some(ipv6) = req.local_addr().as_ipv6() {
            local_port = Some(ipv6.port());
        } else {
            local_port = None;
        }

        if let Some(ipv4) = req.remote_addr().as_ipv4() {
            remote_port = Some(ipv4.port());
        } else if let Some(ipv6) = req.remote_addr().as_ipv6() {
            remote_port = Some(ipv6.port());
        } else {
            remote_port = None;
        }
    } else {
        //eprintln!("GET from ip {}", req.remote_addr());
        if let Some(ipv4) = req.local_addr().as_ipv4() {
            local_port = Some(ipv4.port());
        } else if let Some(ipv6) = req.local_addr().as_ipv6() {
            local_port = Some(ipv6.port());
        } else {
            local_port = None;
        }

        if let Some(ipv4) = req.remote_addr().as_ipv4() {
            //eprintln!(" ipv4: {}", ipv4.ip());
            addr_string = format!("{}", ipv4.ip());
            remote_port = Some(ipv4.port());
        } else if let Some(ipv6) = req.remote_addr().as_ipv6() {
            //eprintln!(" ipv6: {}", ipv6.ip());
            addr_string = format!("{}", ipv6.ip());
            remote_port = Some(ipv6.port());
        } else {
            return Err("Failed to get client addr".into());
        }
    }

    Ok(ClientIPAddrRet {
        addr: addr_string,
        remote_port,
        local_port,
    })
}

async fn get_next_seq_mysql(depot: &Depot) -> Result<u64, Error> {
    let seq: u64;
    let args: &args::Args = depot.obtain().unwrap();
    let locked_bool: Arc<AtomicBool> = depot.obtain::<Arc<AtomicBool>>().unwrap().clone();
    let conn: Arc<tMutex<MSQLWrapper>> = Arc::new(tMutex::new(get_mysql_db_conn(args).await?));

    while locked_bool
        .compare_exchange_weak(
            false,
            true,
            std::sync::atomic::Ordering::SeqCst,
            std::sync::atomic::Ordering::Relaxed,
        )
        .is_err()
    {
        tokio::time::sleep(Duration::from_millis(1)).await;
    }

    let _unlock_scope = helpers::GenericCleanup::new(conn.clone(), |c| {
        //eprintln!("UNLOCK TABLES in get_next_seq_mysql");
        let handle = tokio::runtime::Handle::current();
        let c_clone = c.clone();
        let locked_bool = locked_bool.clone();
        handle.spawn(async move {
            let mut lock = c_clone.lock().await;
            let f = lock.query_drop("UNLOCK TABLES");
            f.ok();
            locked_bool.store(false, std::sync::atomic::Ordering::SeqCst);
        });
    });

    let mut locked = conn.lock().await;

    locked.query_drop("LOCK TABLE RUST_SEQ_ID_1 WRITE")?;

    let seq_rows: Option<Vec<Vec<MSQLValueEnum>>> =
        locked.query_rows("SELECT ID, SEQ_ID FROM RUST_SEQ_ID_1")?;

    if let Some(seq_r) = seq_rows {
        let id: u64 = match seq_r[0][0] {
            MSQLValueEnum::Int64(i) => i as u64,
            MSQLValueEnum::UInt64(u) => u,
            _ => {
                return Err(Error::Generic(String::from(
                    "Failed to get ID from SEQ_ID table!",
                )));
            }
        };

        match seq_r[0][1] {
            MSQLValueEnum::Int64(i) => seq = i as u64,
            MSQLValueEnum::UInt64(u) => seq = u,
            _ => {
                return Err(Error::Generic(String::from(
                    "Failed to get SEQ from SEQ_ID table!",
                )));
            }
        }

        if seq + 1 >= 0x7FFFFFFFFFFFFFFF {
            let mut params = MSQLParamsWrapper::new();
            params.append_uint64(1);
            params.append_uint64(id);

            locked.query_with_params_drop(
                "UPDATE RUST_SEQ_ID_1 SET SEQ_ID = ? WHERE ID = ?",
                &params,
            )?;
        } else {
            let mut params = MSQLParamsWrapper::new();
            params.append_uint64(seq + 1);
            params.append_uint64(id);

            locked.query_with_params_drop(
                "UPDATE RUST_SEQ_ID_1 SET SEQ_ID = ? WHERE ID = ?",
                &params,
            )?;
        }
    } else {
        let mut params = MSQLParamsWrapper::new();
        params.append_uint64(1);
        locked.query_with_params_drop("INSERT INTO RUST_SEQ_ID_1 (SEQ_ID) VALUES (?)", &params)?;
        seq = 1;
    }

    Ok(seq)
}

#[cfg(feature = "sqlite")]
async fn get_next_seq_sqlite(args: &args::Args) -> Result<u64, Error> {
    let seq: i64;
    let conn = Connection::open(&args.sqlite_db_file)?;

    let query_res = conn.query_one(r#"SELECT ID FROM SEQ_ID"#, (), |r| r.get::<usize, i64>(0));
    match query_res {
        Ok(s) => {
            seq = s;
            if seq == 0x7FFFFFFFFFFFFFFF {
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

    Ok(seq as u64)
}

async fn has_challenge_factor_id_mysql(depot: &Depot, hash: &str) -> Result<bool, Error> {
    let args: &args::Args = depot.obtain().unwrap();
    let locked_bool: Arc<AtomicBool> = depot.obtain::<Arc<AtomicBool>>().unwrap().clone();
    let mut conn: MSQLWrapper = get_mysql_db_conn(args).await?;

    while locked_bool
        .compare_exchange_weak(
            false,
            true,
            std::sync::atomic::Ordering::SeqCst,
            std::sync::atomic::Ordering::Relaxed,
        )
        .is_err()
    {
        tokio::time::sleep(Duration::from_millis(1)).await;
    }

    let _scoped_cleanup = helpers::GenericCleanup::new(locked_bool, |b| {
        b.store(false, std::sync::atomic::Ordering::SeqCst);
    });

    let mut params = MSQLParamsWrapper::new();
    params.append_str(hash)?;
    let rows_opt = conn.query_with_params_rows(
        "SELECT ID FROM RUST_CHALLENGE_FACTORS_4 WHERE ID = ?",
        &params,
    )?;

    if let Some(rows) = rows_opt {
        if let MSQLValueEnum::String(_) = &rows[0][0] {
            Ok(true)
        } else {
            Ok(false)
        }
    } else {
        Ok(false)
    }
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

async fn set_challenge_factor_mysql(
    depot: &Depot,
    ip: &str,
    hash: &str,
    port: u16,
    factors_hash: &str,
) -> Result<(), Error> {
    let args: &args::Args = depot.obtain().unwrap();
    let locked_bool: Arc<AtomicBool> = depot.obtain::<Arc<AtomicBool>>().unwrap().clone();
    let conn: Arc<tMutex<MSQLWrapper>> = Arc::new(tMutex::new(get_mysql_db_conn(args).await?));

    while locked_bool
        .compare_exchange_weak(
            false,
            true,
            std::sync::atomic::Ordering::SeqCst,
            std::sync::atomic::Ordering::Relaxed,
        )
        .is_err()
    {
        tokio::time::sleep(Duration::from_millis(1)).await;
    }

    let _unlock_scope = helpers::GenericCleanup::new(conn.clone(), |c| {
        //eprintln!("UNLOCK TABLES in challenge_port_mysql");
        let handle = tokio::runtime::Handle::current();
        let c_clone = c.clone();
        let locked_bool = locked_bool.clone();
        handle.spawn(async move {
            let mut lock = c_clone.lock().await;
            let f = lock.query_drop("UNLOCK TABLES");
            f.ok();
            locked_bool.store(false, std::sync::atomic::Ordering::SeqCst);
        });
    });

    let mut locked = conn.lock().await;

    locked.query_drop("LOCK TABLE RUST_CHALLENGE_FACTORS_4 WRITE")?;

    let mut params = MSQLParamsWrapper::new();
    params.append_str(hash)?;
    params.append_str(ip)?;
    params.append_uint64(port as u64);
    params.append_str(factors_hash)?;

    locked
        .query_with_params_drop(
            "INSERT INTO RUST_CHALLENGE_FACTORS_4 (ID, IP, PORT, FACTORS) VALUES (?, ?, ?, ?)",
            &params,
        )
        .ok();

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

    let (value, factors) = ffi::generate_value_and_factors_strings2(
        args.factors.unwrap_or(constants::DEFAULT_FACTORS_QUADS),
    );

    let mut hash: String;

    #[allow(clippy::needless_late_init)]
    let seq: u64;

    #[cfg(feature = "sqlite")]
    if args.mysql_has_priority {
        seq = get_next_seq_mysql(depot).await?;
    } else {
        seq = get_next_seq_sqlite(args).await?;
    }

    #[cfg(not(feature = "sqlite"))]
    {
        seq = get_next_seq_mysql(depot).await?;
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

        #[cfg(feature = "sqlite")]
        if args.mysql_has_priority {
            if has_challenge_factor_id_mysql(depot, &hash).await? {
                continue;
            }
        } else if has_challenge_factor_id_sqlite(args, &hash).await? {
            continue;
        }
        #[cfg(not(feature = "sqlite"))]
        if has_challenge_factor_id_mysql(depot, &hash).await? {
            continue;
        }

        let factors_hash = blake3::hash(factors.as_bytes()).to_string();

        #[cfg(feature = "sqlite")]
        if args.mysql_has_priority {
            set_challenge_factor_mysql(depot, ip, &hash, port, &factors_hash).await?;
        } else {
            set_challenge_factor_sqlite(args, ip, &hash, port, &factors_hash).await?;
        }
        #[cfg(not(feature = "sqlite"))]
        {
            set_challenge_factor_mysql(depot, ip, &hash, port, &factors_hash).await?;
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

async fn challenge_port_mysql(depot: &Depot, id: &str) -> Result<u16, Error> {
    let mut port: Option<u16> = None;
    let args: &args::Args = depot.obtain().unwrap();
    let locked_bool: Arc<AtomicBool> = depot.obtain::<Arc<AtomicBool>>().unwrap().clone();
    let conn: Arc<tMutex<MSQLWrapper>> = Arc::new(tMutex::new(get_mysql_db_conn(args).await?));

    while locked_bool
        .compare_exchange_weak(
            false,
            true,
            std::sync::atomic::Ordering::SeqCst,
            std::sync::atomic::Ordering::Relaxed,
        )
        .is_err()
    {
        tokio::time::sleep(Duration::from_millis(1)).await;
    }

    let _unlock_scope = helpers::GenericCleanup::new(conn.clone(), |c| {
        //eprintln!("UNLOCK TABLES in challenge_port_mysql");
        let handle = tokio::runtime::Handle::current();
        let c_clone = c.clone();
        let locked_bool = locked_bool.clone();
        handle.spawn(async move {
            let mut lock = c_clone.lock().await;
            let f = lock.query_drop("UNLOCK TABLES");
            f.ok();
            locked_bool.store(false, std::sync::atomic::Ordering::SeqCst);
        });
    });

    let mut locked = conn.lock().await;

    locked.query_drop("LOCK TABLE RUST_ID_TO_PORT_3 WRITE")?;

    let mut params = MSQLParamsWrapper::new();
    params.append_str(id)?;
    {
        let rows_opt = locked
            .query_with_params_rows("SELECT PORT FROM RUST_ID_TO_PORT_3 WHERE ID = ?", &params)?;
        if let Some(rows) = rows_opt {
            match rows[0][0] {
                MSQLValueEnum::Int64(i) => port = Some(i as u16),
                MSQLValueEnum::UInt64(u) => port = Some(u as u16),
                _ => {
                    return Err(Error::Generic(String::from(
                        "Failed to get port from id-to-port",
                    )));
                }
            }
        }
    }

    if port.is_some() {
        locked.query_with_params_drop("DELETE FROM RUST_ID_TO_PORT_3 WHERE ID = ?", &params)?;
    }

    port.ok_or(Error::Generic(String::from(
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
    let client_info_ret = get_client_ip_addr(depot, req).await?;
    let id: String = req.query("id").ok_or(crate::Error::Generic(
        "No id passed to factors_js url!".to_owned(),
    ))?;

    #[allow(unused_assignments)]
    let mut port: Result<u16, Error> = Err(Error::Generic("port uninitialized".into()));
    #[cfg(feature = "sqlite")]
    if args.mysql_has_priority {
        port = challenge_port_mysql(depot, &id).await;
    } else {
        port = challenge_port_sqlite(args, &id).await;
    }
    #[cfg(not(feature = "sqlite"))]
    {
        port = challenge_port_mysql(depot, &id).await;
    }
    if port.is_err() {
        eprintln!(
            "WARNING: Failed to query id-to-port for client {}:{} to {}!",
            &client_info_ret.addr,
            client_info_ret.remote_port.unwrap_or(0),
            client_info_ret.local_port.unwrap_or(0)
        );
    }
    let port: u16 = port?;

    eprintln!(
        "Requested challenge from {}:{} -> {}",
        &client_info_ret.addr,
        client_info_ret.remote_port.unwrap_or(0),
        port
    );

    let (value, uuid) = set_up_factors_challenge(depot, &client_info_ret.addr, port).await?;
    let js = constants::JAVASCRIPT_FACTORS_WORKER;
    let js = js
        .replacen("{API_URL}", &args.api_url, 1)
        .replacen("{LARGE_NUMBER}", &value, 1)
        .replacen("{UUID}", &uuid, 1);
    res.add_header("content-type", "text/javascript", true)?
        .write_body(js)?;

    Ok(())
}

async fn validate_client_mysql(
    args: &args::Args,
    depot: &Depot,
    factors_response: &json_types::FactorsResponse,
    addr: &str,
) -> Result<u16, Error> {
    let correct;
    let mut port: u16 = 0;
    let conn: Arc<tMutex<MSQLWrapper>> = Arc::new(tMutex::new(get_mysql_db_conn(args).await?));
    let locked_bool: Arc<AtomicBool> = depot.obtain::<Arc<AtomicBool>>().unwrap().clone();

    while locked_bool
        .compare_exchange_weak(
            false,
            true,
            std::sync::atomic::Ordering::SeqCst,
            std::sync::atomic::Ordering::Relaxed,
        )
        .is_err()
    {
        tokio::time::sleep(Duration::from_millis(1)).await;
    }

    {
        let _unlock_scope = helpers::GenericCleanup::new(conn.clone(), |c| {
            //eprintln!("UNLOCK TABLES in validate_client_mysql");
            let handle = tokio::runtime::Handle::current();
            let c_clone = c.clone();
            let locked_bool = locked_bool.clone();
            handle.spawn(async move {
                let mut lock = c_clone.lock().await;
                let f = lock.query_drop("UNLOCK TABLES");
                f.ok();
                locked_bool.store(false, std::sync::atomic::Ordering::SeqCst);
            });
        });

        let mut locked = conn.lock().await;

        locked.query_drop("LOCK TABLE RUST_CHALLENGE_FACTORS_4 WRITE")?;

        {
            let mut params = MSQLParamsWrapper::new();
            params.append_uint64(args.challenge_timeout_mins);

            locked.query_with_params_drop("DELETE FROM RUST_CHALLENGE_FACTORS_4 WHERE TIMESTAMPDIFF(MINUTE, GEN_TIME, now()) >= ?", &params)?;
        }

        let hashed_factors = blake3::hash(factors_response.factors.as_bytes()).to_string();

        let mut params = MSQLParamsWrapper::new();
        params.append_str(&factors_response.id)?;
        params.append_str(&hashed_factors)?;

        let addr_port_rows_opt: Option<Vec<Vec<MSQLValueEnum>>> = locked.query_with_params_rows(
            "SELECT IP, PORT FROM RUST_CHALLENGE_FACTORS_4 WHERE ID = ? AND FACTORS = ?",
            &params,
        )?;

        if let Some(rows) = addr_port_rows_opt {
            let client_addr: String = match &rows[0][0] {
                MSQLValueEnum::String(s) => s.to_owned(),
                _ => {
                    return Err(Error::Generic(String::from("No IP from ChallengeFactors")));
                }
            };

            if client_addr == addr {
                port = match rows[0][1] {
                    MSQLValueEnum::Int64(i) => i as u16,
                    MSQLValueEnum::UInt64(u) => u as u16,
                    _ => {
                        return Err(Error::Generic(String::from(
                            "No Port from ChallengeFactors",
                        )));
                    }
                };
                correct = true;

                let mut params = MSQLParamsWrapper::new();
                params.append_str(&factors_response.id)?;

                locked.query_with_params_drop(
                    "DELETE FROM RUST_CHALLENGE_FACTORS_4 WHERE ID = ?",
                    &params,
                )?;
            } else {
                correct = false;
            }
        } else {
            correct = false;
        }
    }

    if correct && port != 0 {
        while locked_bool
            .compare_exchange_weak(
                false,
                true,
                std::sync::atomic::Ordering::SeqCst,
                std::sync::atomic::Ordering::Relaxed,
            )
            .is_err()
        {
            tokio::time::sleep(Duration::from_millis(1)).await;
        }

        let _scoped_cleanup = helpers::GenericCleanup::new(locked_bool, |b| {
            b.store(false, std::sync::atomic::Ordering::SeqCst);
        });

        let mut locked = conn.lock().await;

        let mut params = MSQLParamsWrapper::new();
        params.append_str(addr)?;
        params.append_uint64(port as u64);

        locked.query_with_params_drop(
            "INSERT INTO RUST_ALLOWED_IPS (IP, PORT) VALUES (?, ?)",
            &params,
        )?;

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
    let client_info_ret = get_client_ip_addr(depot, req).await?;
    //eprintln!("API: {}", &addr_string);
    let factors_response: json_types::FactorsResponse = req
        .parse_json_with_max_size(constants::DEFAULT_JSON_MAX_SIZE)
        .await
        .map_err(Error::from)?;

    helpers::validate_client_response(&factors_response.factors)?;

    #[allow(unused_assignments)]
    let mut validate_result: Result<u16, Error> = Err(String::from("Invalid state").into());
    #[cfg(feature = "sqlite")]
    if args.mysql_has_priority {
        validate_result =
            validate_client_mysql(args, depot, &factors_response, &client_info_ret.addr).await;
    } else {
        validate_result =
            validate_client_sqlite(args, &factors_response, &client_info_ret.addr).await;
    }
    #[cfg(not(feature = "sqlite"))]
    {
        validate_result =
            validate_client_mysql(args, depot, &factors_response, &client_info_ret.addr).await;
    }

    if let Ok(port) = validate_result {
        eprintln!(
            "Challenge response accepted from {}:{} -> {}",
            &client_info_ret.addr,
            client_info_ret.remote_port.unwrap_or(0),
            port
        );
        res.body("Correct")
            .add_header("content-type", "text/plain", true)?
            .status_code(StatusCode::OK);
    } else {
        eprintln!(
            "Challenge response DENIED from {}:{} -> {}",
            &client_info_ret.addr,
            client_info_ret.remote_port.unwrap_or(0),
            client_info_ret.local_port.unwrap_or(0)
        );
        res.body("Incorrect")
            .add_header("content-type", "text/plain", true)?
            .status_code(StatusCode::BAD_REQUEST);
    }

    Ok(())
}

async fn check_is_allowed_mysql(
    args: &args::Args,
    depot: &Depot,
    addr: &str,
    port: u16,
) -> Result<bool, Error> {
    let locked_bool: Arc<AtomicBool> = depot.obtain::<Arc<AtomicBool>>().unwrap().clone();
    let conn: Arc<tMutex<MSQLWrapper>> = Arc::new(tMutex::new(get_mysql_db_conn(args).await?));

    while locked_bool
        .compare_exchange_weak(
            false,
            true,
            std::sync::atomic::Ordering::SeqCst,
            std::sync::atomic::Ordering::Relaxed,
        )
        .is_err()
    {
        tokio::time::sleep(Duration::from_millis(1)).await;
    }

    {
        let _unlock_scope = helpers::GenericCleanup::new(conn.clone(), |c| {
            //eprintln!("UNLOCK TABLES in check_is_allowed_mysql 1");
            let handle = tokio::runtime::Handle::current();
            let c_clone = c.clone();
            let locked_bool = locked_bool.clone();
            handle.spawn(async move {
                let mut lock = c_clone.lock().await;
                let f = lock.query_drop("UNLOCK TABLES");
                f.ok();
                locked_bool.store(false, std::sync::atomic::Ordering::SeqCst);
            });
        });

        let mut locked = conn.lock().await;

        locked.query_drop("LOCK TABLE RUST_ALLOWED_IPS WRITE")?;

        let mut params = MSQLParamsWrapper::new();
        params.append_uint64(args.allowed_timeout_mins);

        locked.query_with_params_drop(
            "DELETE FROM RUST_ALLOWED_IPS WHERE TIMESTAMPDIFF(MINUTE, ON_TIME, NOW()) >= ?",
            &params,
        )?;
    }

    while locked_bool
        .compare_exchange_weak(
            false,
            true,
            std::sync::atomic::Ordering::SeqCst,
            std::sync::atomic::Ordering::Relaxed,
        )
        .is_err()
    {
        tokio::time::sleep(Duration::from_millis(1)).await;
    }

    let _unlock_scope = helpers::GenericCleanup::new(conn.clone(), |c| {
        //eprintln!("UNLOCK TABLES in check_is_allowed_mysql 2");
        let handle = tokio::runtime::Handle::current();
        let c_clone = c.clone();
        let locked_bool = locked_bool.clone();
        handle.spawn(async move {
            let mut lock = c_clone.lock().await;
            let f = lock.query_drop("UNLOCK TABLES");
            f.ok();
            locked_bool.store(false, std::sync::atomic::Ordering::SeqCst);
        });
    });

    let mut locked = conn.lock().await;

    locked.query_drop("LOCK TABLE RUST_ALLOWED_IPS READ")?;

    let mut params = MSQLParamsWrapper::new();
    params.append_str(addr)?;
    params.append_uint64(port as u64);

    let ip_entry_row_opt: Option<Vec<Vec<MSQLValueEnum>>> = locked.query_with_params_rows(
        "SELECT IP FROM RUST_ALLOWED_IPS WHERE IP = ? AND PORT = ?",
        &params,
    )?;

    if ip_entry_row_opt.is_some() {
        Ok(true)
    } else {
        Ok(false)
    }
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

async fn init_id_to_port_mysql(
    args: &args::Args,
    depot: &Depot,
    port: u16,
) -> Result<String, Error> {
    let mut hash: String;
    let locked_bool: Arc<AtomicBool> = depot.obtain::<Arc<AtomicBool>>().unwrap().clone();
    let conn: Arc<tMutex<MSQLWrapper>> = Arc::new(tMutex::new(get_mysql_db_conn(args).await?));

    while locked_bool
        .compare_exchange_weak(
            false,
            true,
            std::sync::atomic::Ordering::SeqCst,
            std::sync::atomic::Ordering::Relaxed,
        )
        .is_err()
    {
        tokio::time::sleep(Duration::from_millis(1)).await;
    }

    let _unlock_scope = helpers::GenericCleanup::new(conn.clone(), |c| {
        //eprintln!("UNLOCK TABLES in init_id_to_port_mysql");
        let handle = tokio::runtime::Handle::current();
        let c_clone = c.clone();
        let locked_bool = locked_bool.clone();
        handle.spawn(async move {
            let mut lock = c_clone.lock().await;
            let f = lock.query_drop("UNLOCK TABLES");
            f.ok();
            locked_bool.store(false, std::sync::atomic::Ordering::SeqCst);
        });
    });

    let mut locked = conn.lock().await;

    locked.query_drop("LOCK TABLE RUST_ID_TO_PORT_3 WRITE")?;

    let mut params = MSQLParamsWrapper::new();
    params.append_uint64(args.challenge_timeout_mins);

    locked.query_with_params_drop(
        "DELETE FROM RUST_ID_TO_PORT_3 WHERE TIMESTAMPDIFF(MINUTE, ON_TIME, NOW()) >= ?",
        &params,
    )?;

    let mut hasher = blake3::Hasher::new();
    let mut buf = [0u8; GETRANDOM_BUF_SIZE];
    getrandom::fill(&mut buf).map_err(Into::<Error>::into)?;
    hasher.update(&buf);
    hash = hasher.finalize().to_string();

    let mut params = MSQLParamsWrapper::new();
    params.append_str(&hash)?;

    loop {
        let rows_opt: Option<Vec<Vec<MSQLValueEnum>>> = locked
            .query_with_params_rows("SELECT ID FROM RUST_ID_TO_PORT_3 WHERE ID = ?", &params)?;

        if let Some(rows) = rows_opt {
            let id: String = match &rows[0][0] {
                MSQLValueEnum::String(s) => s.to_owned(),
                _ => {
                    return Err(Error::Generic(String::from(
                        "Failed to fetch ID from id-to-port",
                    )));
                }
            };

            if id == hash {
                hasher = blake3::Hasher::new();
                getrandom::fill(&mut buf).map_err(Into::<Error>::into)?;
                hasher.update(&buf);
                hash = hasher.finalize().to_string();
                continue;
            }
        }
        break;
    }

    let mut params = MSQLParamsWrapper::new();
    params.append_str(&hash)?;
    params.append_uint64(port as u64);

    locked.query_with_params_drop(
        "INSERT INTO RUST_ID_TO_PORT_3 (ID, PORT) VALUES (?, ?)",
        &params,
    )?;

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

    let client_info_ret = get_client_ip_addr(depot, req).await?;

    let port: u16 = client_info_ret.local_port.ok_or(crate::Error::Generic(
        "Should have port from request!".to_owned(),
    ))?;

    let mut is_allowed: bool =
        cached_allow.get_allowed(&req.remote_addr().to_string(), CACHED_TIMEOUT)?;
    if !is_allowed {
        #[cfg(feature = "sqlite")]
        if args.mysql_has_priority {
            is_allowed = check_is_allowed_mysql(args, depot, &client_info_ret.addr, port).await?;
            if is_allowed {
                cached_allow.add_allowed(&req.remote_addr().to_string())?;
            }
        } else {
            is_allowed = check_is_allowed_sqlite(args, &client_info_ret.addr, port).await?;
            if is_allowed {
                cached_allow.add_allowed(&req.remote_addr().to_string())?;
            }
        }
        #[cfg(not(feature = "sqlite"))]
        {
            is_allowed = check_is_allowed_mysql(args, depot, &client_info_ret.addr, port).await?;
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
        let method = req.method();
        let res_body_res = if payload.is_empty() {
            req_to_url(
                format!("{}{}", url, &path_str),
                Some(&client_info_ret.addr),
                None,
                method.as_str(),
            )
            .await
        } else {
            req_to_url(
                format!("{}{}", url, &path_str),
                Some(&client_info_ret.addr),
                Some(payload),
                method.as_str(),
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

        #[cfg(feature = "sqlite")]
        if args.mysql_has_priority {
            hash = Some(init_id_to_port_mysql(args, depot, port).await?);
        } else {
            hash = Some(init_id_to_port_sqlite(args, port).await?);
        }
        #[cfg(not(feature = "sqlite"))]
        {
            hash = Some(init_id_to_port_mysql(args, depot, port).await?);
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
    signal::register_signal_handlers();

    let mut parsed_args = args::parse_args().unwrap();
    if parsed_args.factors.is_none() {
        parsed_args.factors = Some(constants::DEFAULT_FACTORS_QUADS);
        println!(
            "\"--factors=<digits>\" not specified, defaulting to \"{}\"",
            constants::DEFAULT_FACTORS_QUADS
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

    #[allow(clippy::needless_late_init)]
    let router;
    #[cfg(feature = "sqlite")]
    if parsed_args.mysql_has_priority {
        let locked_bool = Arc::new(AtomicBool::new(false));
        router = Router::new()
            .hoop(affix_state::inject(parsed_args.clone()))
            .hoop(affix_state::inject(CachedAllow::new()))
            .hoop(affix_state::inject(locked_bool))
            .push(Router::new().path(&parsed_args.api_url).post(api_fn))
            .push(
                Router::new()
                    .path(&parsed_args.js_factors_url)
                    .get(factors_js_fn),
            )
            .push(Router::new().path("{**}").get(handler_fn).post(handler_fn));
    } else {
        router = Router::new()
            .hoop(affix_state::inject(parsed_args.clone()))
            .hoop(affix_state::inject(CachedAllow::new()))
            .push(Router::new().path(&parsed_args.api_url).post(api_fn))
            .push(
                Router::new()
                    .path(&parsed_args.js_factors_url)
                    .get(factors_js_fn),
            )
            .push(Router::new().path("{**}").get(handler_fn).post(handler_fn));
    }
    #[cfg(not(feature = "sqlite"))]
    {
        let locked_bool = Arc::new(AtomicBool::new(false));
        router = Router::new()
            .hoop(affix_state::inject(parsed_args.clone()))
            .hoop(affix_state::inject(CachedAllow::new()))
            .hoop(affix_state::inject(locked_bool))
            .push(Router::new().path(&parsed_args.api_url).post(api_fn))
            .push(
                Router::new()
                    .path(&parsed_args.js_factors_url)
                    .get(factors_js_fn),
            )
            .push(Router::new().path("{**}").get(handler_fn).post(handler_fn));
    }
    if parsed_args.addr_port_strs.len() == 1 {
        let addr_port_str = parsed_args.addr_port_strs[0].clone();
        let acceptor = TcpListener::new(addr_port_str).bind().await;
        let server = Server::new(acceptor);
        let handle = server.handle();
        tokio::spawn(async move {
            loop {
                if signal::SIGNAL_HANDLED.load(std::sync::atomic::Ordering::Relaxed) {
                    handle.stop_graceful(Some(Duration::from_secs(5)));
                    break;
                }
                tokio::time::sleep(Duration::from_millis(333)).await;
            }
        });
        server.serve(router).await;
    } else if parsed_args.addr_port_strs.len() == 2 {
        let first = parsed_args.addr_port_strs[0].clone();
        let second = parsed_args.addr_port_strs[1].clone();
        let acceptor = TcpListener::new(first)
            .join(TcpListener::new(second))
            .bind()
            .await;
        let server = Server::new(acceptor);
        let handle = server.handle();
        tokio::spawn(async move {
            loop {
                if signal::SIGNAL_HANDLED.load(std::sync::atomic::Ordering::Relaxed) {
                    handle.stop_graceful(Some(Duration::from_secs(5)));
                    break;
                }
                tokio::time::sleep(Duration::from_millis(333)).await;
            }
        });
        server.serve(router).await;
    } else {
        let mut tcp_vector_listener = salvo_compat::TcpVectorListener::new();
        for addr_port_str in parsed_args.addr_port_strs.clone().into_iter() {
            tcp_vector_listener.push(TcpListener::new(addr_port_str));
        }

        let server = Server::new(tcp_vector_listener.bind().await);
        let handle = server.handle();
        tokio::spawn(async move {
            loop {
                if signal::SIGNAL_HANDLED.load(std::sync::atomic::Ordering::Relaxed) {
                    handle.stop_graceful(Some(Duration::from_secs(5)));
                    break;
                }
                tokio::time::sleep(Duration::from_millis(333)).await;
            }
        });
        server.serve(router).await;
    }
}
