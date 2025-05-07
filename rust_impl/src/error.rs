use std::{error, fmt::Display};

#[derive(Debug)]
pub enum Error {
    Generic(String),
    MySQL(mysql_async::Error),
    IO(std::io::Error),
    Reqwest(reqwest::Error),
    Time(time::Error),
    TimeCRange(time::error::ComponentRange),
    TimeIOffset(time::error::IndeterminateOffset),
    AddrParse(std::net::AddrParseError),
    ToStrE(reqwest::header::ToStrError),
}

impl error::Error for Error {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self {
            Error::Generic(_) => None,
            Error::MySQL(error) => error.source(),
            Error::IO(error) => error.source(),
            Error::Reqwest(error) => error.source(),
            Error::Time(error) => error.source(),
            Error::AddrParse(error) => error.source(),
            Error::TimeCRange(error) => error.source(),
            Error::TimeIOffset(error) => error.source(),
            Error::ToStrE(error) => error.source(),
        }
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::Generic(s) => f.write_str(s),
            Error::MySQL(error) => error.fmt(f),
            Error::IO(error) => error.fmt(f),
            Error::Reqwest(error) => error.fmt(f),
            Error::Time(error) => error.fmt(f),
            Error::AddrParse(error) => error.fmt(f),
            Error::TimeCRange(error) => error.fmt(f),
            Error::TimeIOffset(error) => error.fmt(f),
            Error::ToStrE(error) => error.fmt(f),
        }
    }
}

impl From<String> for Error {
    fn from(value: String) -> Self {
        Error::Generic(value)
    }
}

impl From<mysql_async::Error> for Error {
    fn from(value: mysql_async::Error) -> Self {
        Error::MySQL(value)
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

impl From<reqwest::header::ToStrError> for Error {
    fn from(value: reqwest::header::ToStrError) -> Self {
        Error::ToStrE(value)
    }
}

impl From<Error> for salvo::Error {
    fn from(value: Error) -> Self {
        salvo::Error::other(value)
    }
}
