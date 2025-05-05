#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

use std::ffi::{CStr, CString, c_void};

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

    pub fn get_value(&self) -> CString {
        let value: CString;
        unsafe {
            let value_ffi_cstr = work_factors_value_to_str(self.w_factors, std::ptr::null_mut());
            value = CStr::from_ptr(value_ffi_cstr).to_owned();
            libc::free(value_ffi_cstr as *mut c_void);
        }
        value
    }

    pub fn get_factors(&self) -> CString {
        let factors: CString;
        unsafe {
            let factors_ffi_cstr =
                work_factors_factors_to_str(self.w_factors, std::ptr::null_mut());
            factors = CStr::from_ptr(factors_ffi_cstr).to_owned();
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

pub fn generate_value_and_factors_strings(digits: u64) -> (CString, CString) {
    let wf = WorkFactorsWrapper::new(digits);
    (wf.get_value(), wf.get_factors())
}
