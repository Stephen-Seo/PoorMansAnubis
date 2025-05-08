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

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

use std::ffi::{CStr, c_void};

include!(concat!(env!("OUT_DIR"), "/work_bindings.rs"));

struct WorkFactorsWrapper {
    w_factors: Work_Factors,
}

impl WorkFactorsWrapper {
    pub fn new(digits: u64) -> Self {
        Self {
            w_factors: unsafe { work_generate_target_factors(digits) },
        }
    }

    pub fn get_value(&self) -> String {
        let value;
        unsafe {
            let value_ffi_cstr = work_factors_value_to_str(self.w_factors, std::ptr::null_mut());
            value = CStr::from_ptr(value_ffi_cstr).to_str().unwrap().to_owned();
            libc::free(value_ffi_cstr as *mut c_void);
        }
        value
    }

    pub fn get_factors(&self) -> String {
        let factors;
        unsafe {
            let factors_ffi_cstr =
                work_factors_factors_to_str(self.w_factors, std::ptr::null_mut());
            factors = CStr::from_ptr(factors_ffi_cstr)
                .to_str()
                .unwrap()
                .to_owned();
            libc::free(factors_ffi_cstr as *mut c_void);
        }
        factors
    }
}

impl Drop for WorkFactorsWrapper {
    fn drop(&mut self) {
        unsafe {
            work_cleanup_factors(&mut self.w_factors as *mut Work_Factors);
        }
    }
}

pub fn generate_value_and_factors_strings(digits: u64) -> (String, String) {
    let wf = WorkFactorsWrapper::new(digits);
    (wf.get_value(), wf.get_factors())
}
