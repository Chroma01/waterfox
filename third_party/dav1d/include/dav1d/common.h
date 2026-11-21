/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DAV1D_COMMON_H
#define DAV1D_COMMON_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#ifdef MOZ_BUNDLED_RAV1D
#define dav1d_apply_grain moz_dav1d_apply_grain
#define dav1d_close moz_dav1d_close
#define dav1d_data_create moz_dav1d_data_create
#define dav1d_data_props_unref moz_dav1d_data_props_unref
#define dav1d_data_unref moz_dav1d_data_unref
#define dav1d_data_wrap moz_dav1d_data_wrap
#define dav1d_data_wrap_user_data moz_dav1d_data_wrap_user_data
#define dav1d_default_settings moz_dav1d_default_settings
#define dav1d_flush moz_dav1d_flush
#define dav1d_get_decode_error_data_props moz_dav1d_get_decode_error_data_props
#define dav1d_get_event_flags moz_dav1d_get_event_flags
#define dav1d_get_frame_delay moz_dav1d_get_frame_delay
#define dav1d_get_picture moz_dav1d_get_picture
#define dav1d_open moz_dav1d_open
#define dav1d_parse_sequence_header moz_dav1d_parse_sequence_header
#define dav1d_picture_unref moz_dav1d_picture_unref
#define dav1d_send_data moz_dav1d_send_data
#define dav1d_version moz_dav1d_version
#define dav1d_version_api moz_dav1d_version_api
#ifdef FUZZING_INTERFACES
#define dav1d_set_cpu_flags_mask moz_dav1d_set_cpu_flags_mask
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DAV1D_API
    #if defined _WIN32
      #if defined DAV1D_BUILDING_DLL
        #define DAV1D_API __declspec(dllexport)
      #else
        #define DAV1D_API
      #endif
    #elif defined __OS2__
        #define DAV1D_API __declspec(dllexport)
    #else
      #if __GNUC__ >= 4
        #define DAV1D_API __attribute__ ((visibility ("default")))
      #else
        #define DAV1D_API
      #endif
    #endif
#endif

#if EPERM > 0
#define DAV1D_ERR(e) (-(e)) ///< Negate POSIX error code.
#else
#define DAV1D_ERR(e) (e)
#endif

/**
 * A reference-counted object wrapper for a user-configurable pointer.
 */
typedef struct Dav1dUserData {
    const uint8_t *data; ///< data pointer
    struct Dav1dRef *ref; ///< allocation origin
} Dav1dUserData;

/**
 * Input packet metadata which are copied from the input data used to
 * decode each image into the matching structure of the output image
 * returned back to the user. Since these are metadata fields, they
 * can be used for other purposes than the documented ones, they will
 * still be passed from input data to output picture without being
 * used internally.
 */
typedef struct Dav1dDataProps {
    int64_t timestamp; ///< container timestamp of input data, INT64_MIN if unknown (default)
    int64_t duration; ///< container duration of input data, 0 if unknown (default)
    int64_t offset; ///< stream offset of input data, -1 if unknown (default)
    size_t size; ///< packet size, default Dav1dData.sz
    struct Dav1dUserData user_data; ///< user-configurable data, default NULL members
} Dav1dDataProps;

/**
 * Release reference to a Dav1dDataProps.
 */
DAV1D_API void dav1d_data_props_unref(Dav1dDataProps *props);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DAV1D_COMMON_H */
