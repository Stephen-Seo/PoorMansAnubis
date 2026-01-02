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

use std::{error, fmt::Display};

#[derive(Debug)]
pub enum Error {
    Generic(String),
    #[cfg(feature = "mysql")]
    MySQL(mysql_async::Error),
    #[cfg(feature = "sqlite")]
    Sqlite(rusqlite::Error),
    IO(std::io::Error),
    Reqwest(reqwest::Error),
    Time(time::Error),
    TimeCRange(time::error::ComponentRange),
    TimeIOffset(time::error::IndeterminateOffset),
    TimeParse(time::error::Parse),
    AddrParse(std::net::AddrParseError),
    ToStrE(reqwest::header::ToStrError),
    ReqParse(salvo::http::ParseError),
    IntParse(std::num::ParseIntError),
    GetRand(getrandom::Error),
}

impl error::Error for Error {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self {
            Error::Generic(_) => None,
            #[cfg(feature = "mysql")]
            Error::MySQL(error) => error.source(),
            #[cfg(feature = "sqlite")]
            Error::Sqlite(error) => error.source(),
            Error::IO(error) => error.source(),
            Error::Reqwest(error) => error.source(),
            Error::Time(error) => error.source(),
            Error::AddrParse(error) => error.source(),
            Error::TimeCRange(error) => error.source(),
            Error::TimeIOffset(error) => error.source(),
            Error::TimeParse(error) => error.source(),
            Error::ToStrE(error) => error.source(),
            Error::ReqParse(error) => error.source(),
            Error::IntParse(error) => error.source(),
            Error::GetRand(error) => error.source(),
        }
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::Generic(s) => f.write_str(s),
            #[cfg(feature = "mysql")]
            Error::MySQL(error) => error.fmt(f),
            #[cfg(feature = "sqlite")]
            Error::Sqlite(error) => error.fmt(f),
            Error::IO(error) => error.fmt(f),
            Error::Reqwest(error) => error.fmt(f),
            Error::Time(error) => error.fmt(f),
            Error::AddrParse(error) => error.fmt(f),
            Error::TimeCRange(error) => error.fmt(f),
            Error::TimeIOffset(error) => error.fmt(f),
            Error::TimeParse(error) => error.fmt(f),
            Error::ToStrE(error) => error.fmt(f),
            Error::ReqParse(error) => error.fmt(f),
            Error::IntParse(error) => error.fmt(f),
            Error::GetRand(error) => error.fmt(f),
        }
    }
}

impl From<String> for Error {
    fn from(value: String) -> Self {
        Error::Generic(value)
    }
}

impl From<&str> for Error {
    fn from(value: &str) -> Self {
        Error::Generic(value.to_owned())
    }
}

#[cfg(feature = "mysql")]
impl From<mysql_async::Error> for Error {
    fn from(value: mysql_async::Error) -> Self {
        Error::MySQL(value)
    }
}

#[cfg(feature = "sqlite")]
impl From<rusqlite::Error> for Error {
    fn from(value: rusqlite::Error) -> Self {
        Error::Sqlite(value)
    }
}

impl From<std::io::Error> for Error {
    fn from(value: std::io::Error) -> Self {
        Error::IO(value)
    }
}

impl From<reqwest::Error> for Error {
    fn from(value: reqwest::Error) -> Self {
        Error::Reqwest(value)
    }
}

impl From<time::Error> for Error {
    fn from(value: time::Error) -> Self {
        Error::Time(value)
    }
}

impl From<std::net::AddrParseError> for Error {
    fn from(value: std::net::AddrParseError) -> Self {
        Error::AddrParse(value)
    }
}

impl From<time::error::ComponentRange> for Error {
    fn from(value: time::error::ComponentRange) -> Self {
        Error::TimeCRange(value)
    }
}

impl From<time::error::IndeterminateOffset> for Error {
    fn from(value: time::error::IndeterminateOffset) -> Self {
        Error::TimeIOffset(value)
    }
}

impl From<time::error::Parse> for Error {
    fn from(value: time::error::Parse) -> Self {
        Error::TimeParse(value)
    }
}

impl From<reqwest::header::ToStrError> for Error {
    fn from(value: reqwest::header::ToStrError) -> Self {
        Error::ToStrE(value)
    }
}

impl From<salvo::http::ParseError> for Error {
    fn from(value: salvo::http::ParseError) -> Self {
        Error::ReqParse(value)
    }
}

impl From<std::num::ParseIntError> for Error {
    fn from(value: std::num::ParseIntError) -> Self {
        Error::IntParse(value)
    }
}

impl From<getrandom::Error> for Error {
    fn from(value: getrandom::Error) -> Self {
        Error::GetRand(value)
    }
}

impl From<Error> for salvo::Error {
    fn from(value: Error) -> Self {
        salvo::Error::other(value)
    }
}
