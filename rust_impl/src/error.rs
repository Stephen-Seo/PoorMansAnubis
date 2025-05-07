use std::{error, fmt::Display};

#[derive(Debug)]
pub enum Error {
    Generic(String),
    MySQL(mysql_async::Error),
    IO(std::io::Error),
    Reqwest(reqwest::Error),
}

impl error::Error for Error {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self {
            Error::Generic(_) => None,
            Error::MySQL(error) => error.source(),
            Error::IO(error) => error.source(),
            Error::Reqwest(error) => error.source(),
        }
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::Generic(s) => f.write_str(&s),
            Error::MySQL(error) => error.fmt(f),
            Error::IO(error) => error.fmt(f),
            Error::Reqwest(error) => error.fmt(f),
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
