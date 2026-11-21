/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate mozglue_static;

use std::ffi::{c_char, c_uint, c_void};
use std::ptr::NonNull;

use rav1d::include::dav1d::common::Dav1dDataProps;
use rav1d::include::dav1d::data::Dav1dData;
use rav1d::include::dav1d::dav1d::{Dav1dContext, Dav1dEventFlags, Dav1dSettings};
use rav1d::include::dav1d::headers::Dav1dSequenceHeader;
use rav1d::include::dav1d::picture::Dav1dPicture;
use rav1d::send_sync_non_null::SendSyncNonNull;
use rav1d::Dav1dResult;

type FreeCallback = unsafe extern "C" fn(*const u8, Option<SendSyncNonNull<c_void>>);

macro_rules! forward_dav1d {
    ($($name:ident($($arg:ident: $ty:ty),* $(,)?) $(-> $ret:ty)?;)*) => {
        $(
            #[unsafe(export_name = concat!("moz_", stringify!($name)))]
            pub unsafe extern "C" fn $name($($arg: $ty),*) $(-> $ret)? {
                // SAFETY: The wrapper has the same safety requirements as rav1d's C API.
                unsafe { rav1d::$name($($arg),*) }
            }
        )*
    };
}

forward_dav1d! {
    dav1d_apply_grain(
        context: Option<Dav1dContext>,
        out: Option<NonNull<Dav1dPicture>>,
        input: Option<NonNull<Dav1dPicture>>,
    ) -> Dav1dResult;
    dav1d_close(context: Option<NonNull<Option<Dav1dContext>>>);
    dav1d_data_create(data: Option<NonNull<Dav1dData>>, size: usize) -> *mut u8;
    dav1d_data_props_unref(props: Option<NonNull<Dav1dDataProps>>);
    dav1d_data_unref(data: Option<NonNull<Dav1dData>>);
    dav1d_data_wrap(
        data: Option<NonNull<Dav1dData>>,
        bytes: Option<NonNull<u8>>,
        size: usize,
        free_callback: Option<FreeCallback>,
        cookie: Option<SendSyncNonNull<c_void>>,
    ) -> Dav1dResult;
    dav1d_data_wrap_user_data(
        data: Option<NonNull<Dav1dData>>,
        user_data: Option<NonNull<u8>>,
        free_callback: Option<FreeCallback>,
        cookie: Option<SendSyncNonNull<c_void>>,
    ) -> Dav1dResult;
    dav1d_default_settings(settings: NonNull<Dav1dSettings>);
    dav1d_flush(context: Dav1dContext);
    dav1d_get_decode_error_data_props(
        context: Option<Dav1dContext>,
        out: Option<NonNull<Dav1dDataProps>>,
    ) -> Dav1dResult;
    dav1d_get_event_flags(
        context: Option<Dav1dContext>,
        flags: Option<NonNull<Dav1dEventFlags>>,
    ) -> Dav1dResult;
    dav1d_get_frame_delay(settings: Option<NonNull<Dav1dSettings>>) -> Dav1dResult;
    dav1d_get_picture(
        context: Option<Dav1dContext>,
        out: Option<NonNull<Dav1dPicture>>,
    ) -> Dav1dResult;
    dav1d_open(
        context: Option<NonNull<Option<Dav1dContext>>>,
        settings: Option<NonNull<Dav1dSettings>>,
    ) -> Dav1dResult;
    dav1d_parse_sequence_header(
        out: Option<NonNull<Dav1dSequenceHeader>>,
        bytes: Option<NonNull<u8>>,
        size: usize,
    ) -> Dav1dResult;
    dav1d_picture_unref(picture: Option<NonNull<Dav1dPicture>>);
    dav1d_send_data(context: Option<Dav1dContext>, data: Option<NonNull<Dav1dData>>) -> Dav1dResult;
}

#[unsafe(no_mangle)]
pub extern "C" fn moz_dav1d_version() -> *const c_char {
    rav1d::dav1d_version()
}

#[unsafe(no_mangle)]
pub extern "C" fn moz_dav1d_version_api() -> c_uint {
    rav1d::dav1d_version_api()
}

#[unsafe(no_mangle)]
pub extern "C" fn moz_dav1d_set_cpu_flags_mask(mask: c_uint) {
    rav1d::cpu::dav1d_set_cpu_flags_mask(mask)
}
