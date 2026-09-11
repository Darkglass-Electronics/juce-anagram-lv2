/*
 * Copyright 2026 Robin Gareus <robin@gareus.org>
 * Copyright 2026 Darkglass Electronics Oy
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include "../lv2/license.h"

#if !defined(__cplusplus) && __STDC_VERSION__ < 202311l
#include <stdbool.h>
#endif

#pragma GCC visibility push(hidden)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * NOTE: You need to store a local 'uint32_t run_count' variable on your plugin,
 *       initialized with value 0.
 */

/** Initiates license check for the given plugin uri (plugin or collection)
 *
 * Must be called at instantiate(), one time for each license uri.
 *
 * Returns true if a valid license was found or host doesn't support licensing API.
 * (so that you can stop checking for other license uris)
 */
bool nickel_init(double sample_rate, const LV2_Feature* const* features, const char* license_uri);

/** Begin time calculations for unlicensed silence.
 *
 * Must be called at the beginning of each run().
 * This counts samples (time) to later decide if silence needs to be injected.
 *
 * Returned value must be stored in the local 'run_count'.
 */
uint32_t nickel_license_run_begin(uint32_t run_count, uint32_t n_samples);

/** Periodically silence output buffers if unlicensed.
 *
 * Must be called at the end of each run(), for all audio output buffers.
 * Call this function on each buffer, using @a chn to specify the channel.
 */
void nickel_license_run_silence(uint32_t run_count, float* buf, uint32_t n_samples, uint32_t chn);

/** Get the LV2 interface for the Darkglass license API.
 *
 * Must be called at the end of your lv2 plugin extension_data.
 */
const void* nickel_license_interface(const char* uri);

#ifdef __cplusplus
} /* extern "C" */
#endif

#pragma GCC visibility pop
