/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef UTIL_NUGGET_DRIVER_H
#define UTIL_NUGGET_DRIVER_H

/*
 * This talks to an SoC using the default Nugget OS protocol. Ideally we'd just
 * open some /dev/FOO and issue commands through that. Until that happens,
 * we'll fake it from userspace.
 */

/* Abstract handle */
#define device_t void

/* Use /dev/citadel0 if devicename is NULL */
device_t *OpenDev(const char *devicename);

/* Guess */
void CloseDev(device_t *);

/* Send command, read fixed size reply. Return zero on success. */
int ReadDev(device_t *, uint32_t command, uint8_t *buf, uint32_t len);

/* Send command and data. Return zero on success. */
int WriteDev(device_t *, uint32_t command, uint8_t *buf, uint32_t len);

/* Max data size for read/write. Yes, it's a magic number. */
#define MAX_DEVICE_TRANSFER 2044

/* Blocking call using Nugget OS' Transport API */
uint32_t call_application(uint8_t app_id, uint16_t params,
			  uint8_t *args, uint32_t arg_len,
			  uint8_t *reply, uint32_t *reply_len);

#endif	/* UTIL_NUGGET_DRIVER_H */
