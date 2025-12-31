use std::sync::atomic::AtomicBool;

pub static SIGNAL_HANDLED: AtomicBool = AtomicBool::new(false);

extern "C" fn handle_signal(s: std::ffi::c_int) {
    if s == libc::SIGINT || s == libc::SIGHUP || s == libc::SIGTERM {
        SIGNAL_HANDLED.store(true, std::sync::atomic::Ordering::Relaxed);
    }
}

pub fn register_signal_handlers() {
    unsafe {
        let mut sigaction = std::mem::MaybeUninit::<libc::sigaction>::zeroed();
        (*sigaction.as_mut_ptr()).sa_sigaction = handle_signal as *mut std::ffi::c_void as usize;
        libc::sigemptyset(&mut (*sigaction.as_mut_ptr()).sa_mask as *mut libc::sigset_t);
        libc::sigaction(libc::SIGINT, sigaction.as_ptr(), std::ptr::null_mut());
        libc::sigaction(libc::SIGHUP, sigaction.as_ptr(), std::ptr::null_mut());
        libc::sigaction(libc::SIGTERM, sigaction.as_ptr(), std::ptr::null_mut());
    }
}
