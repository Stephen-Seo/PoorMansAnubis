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

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

use std::{
    ffi::{CStr, CString},
    str::FromStr,
};

include!(concat!(env!("OUT_DIR"), "/msql_bindings.rs"));

pub struct MSQLParamsWrapper {
    params: MSQL_Params,
}

impl Drop for MSQLParamsWrapper {
    fn drop(&mut self) {
        unsafe {
            MSQL_cleanup_params(&mut self.params as *mut MSQL_Params);
        }
    }
}

impl MSQLParamsWrapper {
    pub fn new() -> Self {
        unsafe {
            Self {
                params: MSQL_create_params(),
            }
        }
    }

    pub fn append_null(&mut self) {
        unsafe {
            MSQL_append_param_null(self.params);
        }
    }

    pub fn append_int64(&mut self, value: i64) {
        unsafe {
            MSQL_append_param_int64(self.params, value);
        }
    }

    pub fn append_uint64(&mut self, value: u64) {
        unsafe {
            MSQL_append_param_uint64(self.params, value);
        }
    }

    pub fn append_str(&mut self, string: &str) -> Result<(), ()> {
        let c_string: CString = CString::from_str(string).map_err(|_| ())?;
        unsafe {
            MSQL_append_param_str(self.params, c_string.as_ptr());
        }

        Ok(())
    }

    pub fn append_double(&mut self, value: f64) {
        unsafe {
            MSQL_append_param_double(self.params, value);
        }
    }

    pub fn get_params(&self) -> MSQL_Params {
        self.params
    }
}

#[derive(Copy, Clone, Debug)]
pub enum MSQLValueType {
    Error,
    Null,
    Int64,
    UInt64,
    String,
    Double_f64,
}

pub struct MSQLValueWrapper {
    value: MSQL_Value,
}

impl Drop for MSQLValueWrapper {
    fn drop(&mut self) {
        unsafe {
            MSQL_cleanup_value(&mut self.value as *mut MSQL_Value);
        }
    }
}

impl MSQLValueWrapper {
    pub fn get_type(&self) -> MSQLValueType {
        unsafe {
            match MSQL_get_type(self.value) {
                1 => MSQLValueType::Null,
                2 => MSQLValueType::Int64,
                3 => MSQLValueType::UInt64,
                4 => MSQLValueType::String,
                5 => MSQLValueType::Double_f64,
                _ => MSQLValueType::Error,
            }
        }
    }

    pub fn get_i64(&self) -> Option<i64> {
        let ret: i64;
        unsafe {
            let ptr = MSQL_get_int64(self.value);
            if ptr.is_null() {
                return None;
            }
            ret = *ptr;
        }

        Some(ret)
    }

    pub fn get_u64(&self) -> Option<u64> {
        let ret: u64;

        unsafe {
            let ptr = MSQL_get_uint64(self.value);
            if ptr.is_null() {
                return None;
            }
            ret = *ptr;
        }

        Some(ret)
    }

    pub fn get_f64(&self) -> Option<f64> {
        let ret: f64;

        unsafe {
            let ptr = MSQL_get_double(self.value);
            if ptr.is_null() {
                return None;
            }
            ret = *ptr;
        }

        Some(ret)
    }

    pub fn get_string(&self) -> Option<String> {
        let ret: String;

        unsafe {
            let ptr = MSQL_get_str(self.value);
            if ptr.is_null() {
                return None;
            }
            let c_str: &CStr = CStr::from_ptr(ptr);

            let str = c_str.to_str();
            if str.is_err() {
                return None;
            }
            ret = str.unwrap().to_owned();
        }

        Some(ret)
    }
}

pub struct MSQLRowsWrapper {
    rows: MSQL_Rows,
}

impl Drop for MSQLRowsWrapper {
    fn drop(&mut self) {
        unsafe {
            MSQL_cleanup_rows(&mut self.rows as *mut MSQL_Rows);
        }
    }
}

impl MSQLRowsWrapper {
    pub fn get_row_count(&self) -> usize {
        unsafe { MSQL_row_count(self.rows) }
    }

    pub fn get_col_count(&self) -> usize {
        unsafe { MSQL_col_count(self.rows) }
    }

    pub fn fetch(&self, row: usize, col: usize) -> Option<MSQLValueWrapper> {
        let ret: Option<MSQLValueWrapper>;
        unsafe {
            let raw_value: MSQL_Value = MSQL_fetch(self.rows, row, col);
            if raw_value.is_null() {
                return None;
            }

            ret = Some(MSQLValueWrapper { value: raw_value });
        }

        ret
    }
}

pub struct MSQLWrapper {
    connection: MSQL_Connection,
}

impl Drop for MSQLWrapper {
    fn drop(&mut self) {
        unsafe {
            MSQL_cleanup(&mut self.connection as *mut MSQL_Connection);
        }
    }
}

impl MSQLWrapper {
    pub fn try_new(
        addr: &str,
        port: u16,
        user: &str,
        pass: &str,
        dbname: &str,
    ) -> Result<Self, ()> {
        let addr_c: CString = CString::from_str(addr).map_err(|_| ())?;
        let user_c: CString = CString::from_str(user).map_err(|_| ())?;
        let pass_c: CString = CString::from_str(pass).map_err(|_| ())?;
        let dbname_c: CString = CString::from_str(dbname).map_err(|_| ())?;

        let connection;
        unsafe {
            connection = MSQL_new(
                addr_c.as_ptr(),
                port,
                user_c.as_ptr(),
                pass_c.as_ptr(),
                dbname_c.as_ptr(),
            );
            if MSQL_is_valid(connection) != 0 {
                return Err(());
            }
        }

        Ok(Self { connection })
    }

    pub fn is_valid(&mut self) -> Result<(), ()> {
        unsafe {
            if MSQL_is_valid(self.connection) == 0 {
                Ok(())
            } else {
                Err(())
            }
        }
    }

    pub fn ping(&mut self) -> Result<(), ()> {
        unsafe {
            if MSQL_ping(self.connection) == 0 {
                Ok(())
            } else {
                Err(())
            }
        }
    }

    pub fn query_drop(&mut self, stmt: &str) -> Result<(), ()> {
        let mut is_ok = true;

        unsafe {
            let mut params = MSQL_create_params();

            let stmt_c = CString::from_str(stmt).map_err(|_| ())?;

            let mut rows = MSQL_query(self.connection, stmt_c.as_ptr(), params);

            if rows.is_null() {
                is_ok = false;
            }

            MSQL_cleanup_rows(&mut rows as *mut MSQL_Rows);

            MSQL_cleanup_params(&mut params as *mut MSQL_Params);
        }

        if is_ok { Ok(()) } else { Err(()) }
    }

    pub fn query_with_params_drop(
        &mut self,
        stmt: &str,
        params: &MSQLParamsWrapper,
    ) -> Result<(), ()> {
        let mut is_ok = true;

        unsafe {
            let stmt_c = CString::from_str(stmt).map_err(|_| ())?;

            let mut rows = MSQL_query(self.connection, stmt_c.as_ptr(), params.get_params());

            if rows.is_null() {
                is_ok = false;
            }

            MSQL_cleanup_rows(&mut rows as *mut MSQL_Rows);
        }

        if is_ok { Ok(()) } else { Err(()) }
    }

    pub fn query_rows(&mut self, stmt: &str) -> Result<Option<MSQLRowsWrapper>, ()> {
        let mut rows_ret: Option<MSQLRowsWrapper> = None;
        unsafe {
            let mut params = MSQL_create_params();

            let stmt_c = CString::from_str(stmt).map_err(|_| ())?;

            let mut rows = MSQL_query(self.connection, stmt_c.as_ptr(), params);

            if !rows.is_null() && MSQL_row_count(rows) != 0 {
                rows_ret = Some(MSQLRowsWrapper { rows });
            } else {
                MSQL_cleanup_rows(&mut rows as *mut MSQL_Rows);
            }

            MSQL_cleanup_params(&mut params as *mut MSQL_Params);
        }

        Ok(rows_ret)
    }

    pub fn query_with_params_rows(
        &mut self,
        stmt: &str,
        params: &MSQLParamsWrapper,
    ) -> Result<Option<MSQLRowsWrapper>, ()> {
        let mut rows_ret: Option<MSQLRowsWrapper> = None;
        unsafe {
            let stmt_c = CString::from_str(stmt).map_err(|_| ())?;

            let mut rows = MSQL_query(self.connection, stmt_c.as_ptr(), params.get_params());

            if !rows.is_null() && MSQL_row_count(rows) != 0 {
                rows_ret = Some(MSQLRowsWrapper { rows });
            } else {
                MSQL_cleanup_rows(&mut rows as *mut MSQL_Rows);
            }
        }

        Ok(rows_ret)
    }
}
