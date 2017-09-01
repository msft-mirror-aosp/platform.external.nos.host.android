/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver.h"

/* From Nugget OS */
#include "application.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define FAIL do								\
	{ fprintf(stderr, "FAIL at %s:%d\n", __FILE__, __LINE__);	\
		exit(1);						\
	} while (0)

/****************************************************************************/
extern device_t *dev;				/* assume open */
extern int verbose;
/****************************************************************************/

/* Return non-zero on error */
static void get_status(uint8_t app_id, uint32_t *status, uint16_t *ulen)
{
	uint8_t buf[6];
	uint32_t command = CMD_ID(app_id) | CMD_IS_READ | CMD_TRANSPORT;

	if (0 != ReadDev(dev, command, buf, sizeof(buf)))
		FAIL;

	*status = *(uint32_t *)buf;
	*ulen = *(uint16_t *)(buf + 4);
}

static void clear_status(uint8_t app_id)
{
	uint32_t command = CMD_ID(app_id) | CMD_TRANSPORT;

	if (0 != WriteDev(dev, command, 0, 0))
		FAIL;
}


uint32_t call_application(uint8_t app_id, uint16_t params,
			  uint8_t *args, uint32_t arg_len,
			  uint8_t *reply, uint32_t *reply_len)
{
	uint32_t command;
	uint8_t buf[MAX_DEVICE_TRANSFER];
	uint32_t status;
	uint16_t ulen;
	uint32_t poll_count = 0;

	/* Make sure it's idle */
	get_status(app_id, &status, &ulen);
	if (verbose)
		printf("%d: query status 0x%08x  ulen 0x%04x\n",
		       __LINE__, status, ulen);

	/* It's not idle, but we're the only ones telling it what to do, so it
	 * should be. */
	if (status != APP_STATUS_IDLE) {

		/* Try clearing the status */
		if (verbose)
			printf("clearing previous status\n");
		clear_status(app_id);

		/* Check again */
		get_status(app_id, &status, &ulen);
		if (verbose)
			printf("%d: query status 0x%08x  ulen 0x%04x\n",
			       __LINE__, status, ulen);

		/* It's ignoring us and is still not ready, so it's broken */
		if (status != APP_STATUS_IDLE)
			FAIL;
	}

	/* Send args data */
	command = CMD_ID(app_id) | CMD_TRANSPORT | CMD_IS_DATA;
	do {
		/*
		 * We can't send more than the device can take. For
		 * Citadel using the TPM Wait protocol on SPS, this is
		 * a constant. For other buses, it may not be.
		 *
		 * For each Write, Citadel requires that we send the length of
		 * what we're about to send in the params field.
		 */
		ulen = MIN(arg_len, MAX_DEVICE_TRANSFER);
		CMD_SET_PARAM(command, ulen);
		if (args && ulen)
			memcpy(buf, args, ulen);

		if (verbose)
			printf("Write command 0x%08x, bytes 0x%x\n",
			       command, ulen);

		if (0 != WriteDev(dev, command, buf, ulen))
			FAIL;

		/* Additional data needs the additional flag set */
		command |= CMD_MORE_TO_COME;

		if (args)
			args += ulen;
		if (arg_len)
			arg_len -= ulen;
	} while (arg_len);

	/* See if we had any errors while sending the args */
	get_status(app_id, &status, &ulen);
	if (verbose)
		printf("%d: query status 0x%08x  ulen 0x%04x\n",
		       __LINE__, status, ulen);
	if (status & APP_STATUS_DONE)
		/* Yep, problems. It should still be idle. */
		goto reply;

	/* Now tell the app to do whatever */
	command = CMD_ID(app_id) | CMD_PARAM(params);
	if (verbose)
		printf("Write command 0x%08x\n", command);
	if (0 != WriteDev(dev, command, 0, 0))
		FAIL;

	/* Poll the app status until it's done */
	do {
		get_status(app_id, &status, &ulen);
		if (verbose > 1)
			printf("%d:  poll status 0x%08x  ulen 0x%04x\n",
			       __LINE__, status, ulen);
		poll_count++;
	} while (!(status & APP_STATUS_DONE));
	if (verbose)
		printf("polled %d times, status 0x%08x  ulen 0x%04x\n",
		       poll_count, status, ulen);

reply:
	/* Read any result only if there's a place with room to put it */
	if (reply && reply_len && *reply_len) {
		uint16_t left = MIN(*reply_len, ulen);
		uint16_t gimme, got;

		command = CMD_ID(app_id) | CMD_IS_READ |
			CMD_TRANSPORT | CMD_IS_DATA;

		got = 0 ;
		while (left) {

			/*
			 * We can't read more than the device can send. For
			 * Citadel using the TPM Wait protocol on SPS, this is
			 * a constant. For other buses, it may not be.
			 */
			gimme = MIN(left, MAX_DEVICE_TRANSFER);
			if (verbose)
				printf("Read command 0x%08x, bytes 0x%x\n",
				       command, gimme);
			if (0 != ReadDev(dev, command, buf, gimme))
				FAIL;

			memcpy(reply, buf, gimme);
			reply += gimme;
			left -= gimme;
			got += gimme;
		}
		/* got it all */
		*reply_len = got;
	}

	/* Clear the reply manually for the next caller */
	command = CMD_ID(app_id) | CMD_TRANSPORT;
	if (0 != WriteDev(dev, command, 0, 0))
		FAIL;

	return APP_STATUS_CODE(status);
}
