use crate::error::Error;

use std::{net::IpAddr, str::FromStr};

use mysql_async::{Row, Value};
use time::{Date, Month, OffsetDateTime, Time, UtcOffset};

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct AllowedIPs {
    pub ip: IpAddr,
    pub time: OffsetDateTime,
}

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
            return Err("Failed to parse datetime from sql query".to_owned().into());
        }

        let offset_time: OffsetDateTime = OffsetDateTime::new_in_offset(date, time, time_offset);

        Ok(Self {
            ip: ip_addr,
            time: offset_time,
        })
    }
}
