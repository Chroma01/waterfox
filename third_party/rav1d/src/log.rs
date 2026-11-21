use std::ffi::{c_int, c_void};
use std::fmt;
use std::fmt::Write as _;
use std::io::{stderr, stdout, Write as _};
use std::ptr::fn_addr_eq;

use crate::send_sync_non_null::SendSyncNonNull;

/// An opaque pointer to a C `void (*)(void *, const char *, va_list)`.
/// Only the C shim may call it; Rust does not model the target's `va_list`.
pub type Dav1dLoggerCallback = unsafe extern "C" fn();

unsafe extern "C" {
    fn rav1d_log_byte(logger: &Dav1dLogger, byte: c_int);
    static rav1d_stdout_logger: Dav1dLogger;
    static rav1d_stderr_logger: Dav1dLogger;
}

#[derive(Clone)]
#[repr(C)]
pub struct Dav1dLogger {
    /// A cookie that's passed as the first argument to the callback below.
    ///
    /// # Safety
    ///
    /// All accesses to [`Self::cookie`] must be thread-safe
    /// (i.e. [`Self::cookie`] must be [`Send`]` + `[`Sync`]).
    ///
    /// If used from Rust, [`Self::cookie`] is a [`SendSyncNonNull`],
    /// whose constructors ensure this [`Send`]` + `[`Sync`] safety.
    cookie: Option<SendSyncNonNull<c_void>>,

    /// A `vprintf`-style callback with `cookie` as its first argument.
    callback: Option<Dav1dLoggerCallback>,
}

impl Dav1dLogger {
    /// # Safety
    ///
    /// `callback`, if non-[`None`]/`NULL`, must be a C `va_list` callback
    /// valid to call with `cookie`, a format string, and its arguments.
    ///
    /// See [`Self::cookie`]'s safety requirements.
    pub const unsafe fn new(
        cookie: Option<SendSyncNonNull<c_void>>,
        callback: Option<Dav1dLoggerCallback>,
    ) -> Self {
        Self { cookie, callback }
    }
}

impl fmt::Write for Dav1dLogger {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        if self.callback.is_none() {
            return Ok(());
        }

        // Print bytes individually because `s` may contain embedded NULs.
        for &byte in s.as_bytes() {
            // SAFETY: The callback is non-null and valid per `Self::new`.
            unsafe { rav1d_log_byte(self, c_int::from(byte)) };
        }
        Ok(())
    }
}

#[derive(Clone, Default)]
pub(crate) enum Rav1dLogger {
    Dav1d(Dav1dLogger),
    Stdout,
    #[default]
    Stderr,
}

/// Any type implementing [`Rav1dLog`] can be used with [`write!`].
///
/// [`Rav1dLog`] is very similar to [`fmt::Write`] and [`io::Write`]
/// in that they can both be called by [`write!`] as `.write_fmt(format_args!(...))`.
///
/// The difference, and the reason we don't use either of those,
/// is that [`Rav1dLog::write_fmt`] takes `&self` instead of `&mut self`,
/// and this makes ownership and borrowing much simpler and more flexible.
/// Furthermore, this returns `()` instead of a [`Result`]
/// so that call sites don't have to propagate or `.unwrap()` it,
/// bloating call sites for non-essential logging code.
///
/// [`io::Write`]: std::io::Write
pub trait Rav1dLog {
    fn write_fmt(&self, args: fmt::Arguments);
}

impl Rav1dLog for Rav1dLogger {
    /// Logging doesn't have to be fast when it's on,
    /// but we don't want it slow things down when it's off,
    /// so ensure the logging code isn't inlined everywhere, bloating call sites.
    #[inline(never)]
    fn write_fmt(&self, args: fmt::Arguments) {
        match self {
            // The `dav1d.clone()` is because [`fmt::Write::write_fmt`] takes `&mut`
            // even though we don't need it to.
            // [`Dav1dLogger`] is trivial to [`Clone`], though, so we can just do that.
            Self::Dav1d(dav1d) => dav1d.clone().write_fmt(args).unwrap(),
            Self::Stdout => stdout().write_fmt(args).unwrap(),
            Self::Stderr => stderr().write_fmt(args).unwrap(),
        }
    }
}

impl Rav1dLog for Option<Rav1dLogger> {
    /// When a logger isn't set, we don't want to have to run any code here,
    /// so force this to be inlined so a [`None`] can be seen.
    #[inline(always)]
    fn write_fmt(&self, args: fmt::Arguments) {
        if let Some(logger) = self {
            logger.write_fmt(args);
        }
    }
}

impl From<Dav1dLogger> for Option<Rav1dLogger> {
    fn from(logger: Dav1dLogger) -> Self {
        let callback = logger.callback?;
        // SAFETY: The C shim defines both immutable loggers with non-null callbacks.
        let (stdout_cb, stderr_cb) = unsafe {
            (
                rav1d_stdout_logger.callback.unwrap(),
                rav1d_stderr_logger.callback.unwrap(),
            )
        };
        Some(if fn_addr_eq(callback, stdout_cb) {
            Rav1dLogger::Stdout
        } else if fn_addr_eq(callback, stderr_cb) {
            Rav1dLogger::Stderr
        } else {
            Rav1dLogger::Dav1d(logger)
        })
    }
}

impl From<Option<Rav1dLogger>> for Dav1dLogger {
    fn from(logger: Option<Rav1dLogger>) -> Self {
        // SAFETY: The C shim defines both immutable loggers.
        match logger {
            Some(Rav1dLogger::Stdout) => unsafe { rav1d_stdout_logger.clone() },
            Some(Rav1dLogger::Stderr) => unsafe { rav1d_stderr_logger.clone() },
            Some(Rav1dLogger::Dav1d(dav1d)) => dav1d,
            None => Self {
                cookie: None,
                callback: None,
            },
        }
    }
}
