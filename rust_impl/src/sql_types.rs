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

use crate::error::Error;

use std::{net::IpAddr, str::FromStr};

#[cfg(feature = "mysql")]
use mysql_async::{Row, Value};
#[cfg(feature = "sqlite")]
use rusqlite::Row as SqliteRow;
use time::{
    Date, Month, OffsetDateTime, PrimitiveDateTime, Time, UtcOffset, macros::format_description,
};

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct AllowedIPs {
    pub ip: IpAddr,
    pub time: OffsetDateTime,
}

#[cfg(feature = "mysql")]
impl TryFrom<Row> for AllowedIPs {
    type Error = crate::error::Error;

    fn try_from(value: Row) -> Result<Self, Self::Error> {
        let ip_string: String = value.get(0).ok_or("Failed to get ip string".to_owned())?;
        let ip_addr: IpAddr = IpAddr::from_str(&ip_string)?;

        let time_offset =
            UtcOffset::current_local_offset().map_err(|e| Error::from(time::Error::from(e)))?;

        let time_value: Value = value.get(1).ok_or("Failed to get time string".to_owned())?;

        let date: Date;
        let time: Time;
        if let Value::Date(year, month, day, hour, minute, second, _micros) = time_value {
            let month: Month =
                Month::try_from(month).map_err(|_| "Failed to parse sql month".to_owned())?;
            date = Date::from_calendar_date(year as i32, month, day)?;
            time = Time::from_hms(hour, minute, second)?;
        } else {
            return Err("Failed to parse datetime from sql query".into());
        }

        let offset_time: OffsetDateTime = OffsetDateTime::new_in_offset(date, time, time_offset);

        Ok(Self {
            ip: ip_addr,
            time: offset_time,
        })
    }
}

#[cfg(feature = "sqlite")]
impl TryFrom<SqliteRow<'_>> for AllowedIPs {
    type Error = crate::error::Error;

    fn try_from(value: SqliteRow) -> Result<Self, Self::Error> {
        let ip_string: String = value
            .get(0)
            .map_err(|_| "Failed to get ip string".to_owned())?;
        let ip_addr: IpAddr = IpAddr::from_str(&ip_string)?;

        let time_value: String = value
            .get(1)
            .map_err(|_| "Failed to get time string".to_owned())?;

        let primitive_time: PrimitiveDateTime = PrimitiveDateTime::parse(
            &time_value,
            format_description!("[year]-[month]-[day] [hour]:[minute]:[second]"),
        )?;

        let offset_time = primitive_time.assume_utc();

        Ok(Self {
            ip: ip_addr,
            time: offset_time,
        })
    }
}
