#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

use std::ffi::{CStr, CString, c_void};

include!(concat!(env!("OUT_DIR"), "/work_bindings.rs"));

pub fn generate_value_and_factors_strings(digits: u64) -> Result<(CString, CString), String> {
    let value: CString;
    let factors: CString;

    unsafe {
        let mut work_factors = work_generate_target_factors(digits);
        let value_ffi_cstr = work_factors_value_to_str(work_factors, std::ptr::null_mut());
        value = CStr::from_ptr(value_ffi_cstr).to_owned();
        libc::free(value_ffi_cstr as *mut c_void);
        let factors_ffi_cstr = work_factors_factors_to_str(work_factors, std::ptr::null_mut());
        factors = CStr::from_ptr(factors_ffi_cstr).to_owned();
        libc::free(factors_ffi_cstr as *mut c_void);
        work_cleanup_factors(&mut work_factors as *mut Work_Factors);
    }

    Ok((value, factors))
}
