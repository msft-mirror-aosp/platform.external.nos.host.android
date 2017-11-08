/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* From Nugget OS */
#include <application.h>
#include <app_nugget.h>

#include <nos/device.h>
#include <nos/transport.h>

/* Opened here for use in transport.c */
int verbose;

/* Our big transfer buffer. Apps may have smaller size limits. */
static uint8_t buf[0x4000];

enum {
	BOARD_BINDER,
	BOARD_PROTO1,
	BOARD_EVT,
};

/* Global options */
static struct option_s {
	/* program-specific options */
	uint8_t app_id;
	uint16_t param;
	int more;
	int ascii;
	int binary;
	int verbose;
	int buttons;
	int board;
	/* generic connection options */
	const char *device;
} option = {
	.board = BOARD_EVT,
};

enum no_short_opts_for_these {
	OPT_DEVICE = 1000,
	OPT_BUTTONS,
	OPT_BINDER,
	OPT_PROTO1,
	OPT_EVT,
};

static char *short_opts = ":hi:p:m:abv";
static const struct option long_opts[] = {
	/* name    hasarg *flag val */
	{"id",          1, NULL, 'i'},
	{"param",       1, NULL, 'p'},
	{"more",        1, NULL, 'm'},
	{"ascii",       0, NULL, 'a'},
	{"binary",      0, NULL, 'b'},
	{"verbose",     0, NULL, 'v'},
	{"buttons",     0, NULL, OPT_BUTTONS},
	{"binder",      0, &option.board, BOARD_BINDER},
	{"proto1",      0, &option.board, BOARD_PROTO1},
	{"evt",         0, &option.board, BOARD_EVT},
	{"device",      1, NULL, OPT_DEVICE},
	{"help",        0, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

static void usage(const char *progname)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s\n"
		"Usage: %s test\n\n"
		"  Quick test to see if Citadel is responsive\n"
		"\n"
		"  Options:\n"
		"    --buttons                        Prompt to press buttons\n"
		"    --binder | --proto1 | --evt      Specify the board\n\n",
		progname, progname);
	fprintf(stderr, "Usage: %s tpm COMMAND [BYTE ...]\n"
		"\n"
		"  Transmit the COMMAND and possibly any BYTEs using the\n"
		"  TPM Wait mode driver. COMMAND and BYTEs are hex values.\n"
		"\n"
		"  Options:\n"
		"    -m, --more NUM         Exchange NUM additional bytes\n\n",
		progname);
	fprintf(stderr, "Usage: %s app [BYTE [BYTE...]]\n"
		"\n"
		"  Call an application function, passing any BYTEs as args\n"
		"\n"
		"  Options:\n"
		"    -i  --id HEX           App ID (default 0x00)\n"
		"    -p, --param HEX        Set the command Param field to HEX"
		" (default 0x0000)\n\n",
		progname);
	fprintf(stderr, "Usage: %s rw ADDRESS [VALUE]\n"
		"\n"
		"  Read or write a memory address on Citadel. Both ADDRESS\n"
		"  and VALUE are 32-bit hex numbers.\n\n",
		progname);
	fprintf(stderr, "Common options:\n\n"
		"  -a, --ascii            Print response as ASCII string\n"
		"  -b, --binary           Dump binary response to stdout\n"
		"  -v, --verbose          Increase verbosity. More is noisier\n"
		"      --device PATH      spidev device file to open\n"
		"  -h, --help             Show this message\n"
		"\n\n");
}

/****************************************************************************/
/* Handy stuff */

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static int errorcnt;
static void Error(const char *format, ...)
{
	va_list ap;

        va_start(ap, format);
        fprintf(stderr, "ERROR: ");
        vfprintf(stderr, format, ap);
        fprintf(stderr, "\n");
        va_end(ap);

	errorcnt++;
}

static void debug(int lvl, const char *format, ...)
{
	va_list ap;

	if (lvl > option.verbose)
		return;

        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
}

static void debug_buf(int lvl, uint8_t *buf, int bufsize)
{
	int i;

	if (lvl > option.verbose)
		return;

	if (bufsize <= 0)
		return;

	if (option.binary) {
		fwrite(buf, bufsize, 1, stdout);
		return;
	}

	if (option.ascii) {
		for (i = 0; i < bufsize; i++)
			printf("%c", isprint(buf[i]) ? buf[i] : '.');
		printf("\n");
		return;
	}

	for (i = 0; i < bufsize;) {
		if (!(i % 16))
			printf("0x%06x: ", i);
		printf(" %02x", buf[i]);
		i++;
		if (!(i % 16))
			printf("\n");
	}
	if (i % 16)
		printf("\n");
}

static void debug_retval(int lvl, uint32_t retval, uint32_t replycount)
{
	if (lvl > option.verbose)
		return;

	printf("retval 0x%08x (", retval);
	switch (retval) {
	case APP_SUCCESS:
		printf("success");
		break;
	case APP_ERROR_BOGUS_ARGS:
		printf("bogus args");
		break;
	case APP_ERROR_INTERNAL:
		printf("app is being stupid");
		break;
	case APP_ERROR_TOO_MUCH:
		printf("caller sent too much data");
		break;
	default:
		if (retval >= APP_SPECIFIC_ERROR &&
			 retval < APP_LINE_NUMBER_BASE)
			printf("app-specific error #%d",
				retval - APP_SPECIFIC_ERROR);
		else if (retval >= APP_LINE_NUMBER_BASE)
			printf("error at line %d",
				retval - APP_LINE_NUMBER_BASE);
		else
			printf("unknown");
	}
	printf("), replycount 0x%x (%d)\n", replycount, replycount);
}

/****************************************************************************/
/* Functionality */

static void do_tpm(int argc, char *argv[])
{
	char *e = 0;
	int i, rv, argcount, optind = 1;
	uint32_t command, buflen;
	struct nos_device dev;

	/* Must have a command */
	if (optind < argc) {
		command = strtoul(argv[optind], &e, 16);
		if (e && *e)
			Error("%s: Invalid COMMAND: \"%s\"",
			      argv[0], argv[optind]);
		optind++;
	} else {
		Error("%s: Missing required COMMAND", argv[0]);
		return;
	}

	/* how many bytes to exchange? */
	argcount = argc - optind;

	buflen = option.more + argcount;
	if (buflen > MAX_DEVICE_TRANSFER) {
		Error("%s: Too much to send", argv[0]);
		return;
	}

	/* preload BYTEs from command line */
	for (i = 0; i < argcount; i++) {
		buf[i] = 0xff & strtoul(argv[optind], &e, 16);
		if (e && *e) {
			Error("%s: Invalid byte value: \"%s\"",
			      argv[0], argv[optind]);
			return;
		}
		optind++;
	}

	/* Okay, let's do something */
	if (nos_device_open(option.device, &dev) != 0) {
		Error("Unable to connect");
		return;
	}

	debug(1, "Command 0x%08x, buflen 0x%x\n", command, buflen);

	if (command & 0x80000000)
		rv = dev.ops.read(dev.ctx, command, buf, buflen);
	else
		rv = dev.ops.write(dev.ctx, command, buf, buflen);

	if (rv != 0)
		Error("%s: nuts", argv[0]);
	else if (command & 0x80000000)
		debug_buf(0, buf, buflen);

	/* Done */
	dev.ops.close(dev.ctx);
}

static void do_app(int argc, char *argv[])
{
	char *e = 0;
	int optind = 1;
	uint32_t i, buflen, replycount, retval;
	struct nos_device dev;

	/* preload BYTEs from command line */
	buflen = argc - optind;
	for (i = 0; i < buflen; i++) {
		buf[i] = 0xff & strtoul(argv[optind], &e, 16);
		if (e && *e) {
			Error("%s: Invalid byte value: \"%s\"",
			      argv[0], argv[optind]);
			return;
		}
		optind++;
	}

	/* Okay, let's do something */
	if (nos_device_open(option.device, &dev) != 0) {
		Error("Unable to connect");
		return;
	}

	debug(1, "AppID 0x%02x, App param 0x%04x, buflen 0x%x\n",
	      option.app_id, option.param, buflen);

	replycount = sizeof(buf);
	retval = nos_call_application(&dev, option.app_id, option.param,
				  buf, buflen,
				  buf, &replycount);
	debug_retval(1, retval, replycount);
	debug_buf(0, buf, replycount);

	/* Done */
	dev.ops.close(dev.ctx);
}



/****************************************************************************/
/* Available for bringup/debug only. See b/65067435 */

static uint32_t read32(const struct nos_device *dev, uint32_t address,
                       uint32_t *valptr)
{
	uint32_t buflen, replycount, retval;

	debug(2, "read from 0x%08x\n", address);
	buflen = sizeof(address);
	memcpy(buf, &address, buflen);
	replycount = sizeof(buf);
	retval = nos_call_application(dev, APP_ID_NUGGET, NUGGET_PARAM_READ32,
				  buf, buflen,
				  buf, &replycount);
	debug_retval(2, retval, replycount);
	if (replycount == sizeof(*valptr)) {
		memcpy(valptr, buf, sizeof(*valptr));
		debug(2, "value is 0x%08x\n", *valptr);
	}

	return retval;
}

static uint32_t write32(const struct nos_device *dev, uint32_t address,
                        uint32_t value)
{
	uint32_t buflen, replycount, retval;
	struct nugget_app_write32 w32;

	/* Writing to address */
	debug(2, "write to 0x%08x with value 0x%08x\n", address, value);
	w32.address = address;
	w32.value = value;
	buflen = sizeof(w32);
	memcpy(buf, &w32, buflen);
	replycount = sizeof(buf);
	retval = nos_call_application(dev, APP_ID_NUGGET, NUGGET_PARAM_WRITE32,
				  buf, buflen,
				  buf, &replycount);
	debug_retval(2, retval, replycount);

	return retval;
}

static void do_rw(int argc, char *argv[])
{
	char *e = 0;
	uint32_t retval, address, value;
	struct nos_device dev;

	argc = MIN(argc, 3);			/* ignore any extra args */
	switch (argc) {
	case 3:
		value = strtoul(argv[2], &e, 16);
		if (e && *e) {
			Error("%s: Invalid value: \"%s\"", argv[0], argv[2]);
			return;
		}
		/* fall through */
	case 2:
		address = strtoul(argv[1], &e, 16);
		if (e && *e) {
			Error("%s: Invalid address: \"%s\"", argv[0], argv[1]);
			return;
		}
		break;
	default:
		Error("%s: Missing required address", argv[0]);
		return;
	}

	/* Okay, let's do something */
	if (nos_device_open(option.device, &dev) != 0) {
		Error("Unable to connect");
		return;
	}

	if (argc == 2) {
		retval = read32(&dev, address, &value);
		if (APP_SUCCESS != retval)
			Error("%s: Read failed", argv[0]);
		else
			printf("0x%08x\n", value);
	} else {
		retval = write32(&dev, address, value);
		if (APP_SUCCESS != retval)
			Error("%s: Write failed", argv[0]);
	}

	dev.ops.close(dev.ctx);
}

/****************************************************************************/
/*
 * This stuff is a quick dead-or-alive test for SMT. We assume that the Citadel
 * chip itself will work because it's passed its own manufacturing tests, but
 * we need to know that the chip is powered up and the SPI bus and GPIOs are
 * working. UART passthrough will have to be tested externally.
 */

/* TPM Register magic commands */
#define RD4_TPM_DID_VID 0x83d40f00
#define WR1_TPM_FW_VER  0x00d40f90

/* ARM GPIO config registers */
#define GPIO_DATA     0x40550000
#define GPIO_OUTENSET 0x40550010


/* Return true on success */
static int write_to_file(const char *filename, const char *string)
{
	int fd, rv;
	ssize_t len, num;

	/* Assume valid input */
	len = strlen(string);

	fd = open(filename, O_WRONLY | O_SYNC);
	if (fd < 0) {
		debug(1, "can't open %s for writing: %s", filename,
		      strerror(errno));
		return 0;
	}

	num = write(fd, string, len);

	debug(2, "%s(%s, %s) wrote %d / %d\n", __func__,
	      filename, string, num, len);

	if (len != num) {
		debug(1, "can't write %d bytes to %s: %s", len, filename,
		      strerror(errno));
		rv = close(fd);
		if (rv)
			debug(1, "can't close the file descriptor either: %s",
			      strerror(errno));
		return 0;
	}

	rv = close(fd);
	if (rv) {
		debug(1, "can't close the file descriptor for %s: %s",
		      filename, strerror(errno));
		return 0;
	}

	return 1;
}

/* Return true on success */
static int read_from_file(const char *filename, char *buf, ssize_t bufsize)
{
	int fd, rv;
	ssize_t num;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		debug(2, "can't open %s for reading: %s", filename,
		      strerror(errno));
		return 0;
	}

	num = read(fd, buf, bufsize - 1);	/* leave room for '\0' */

	debug(2, "%s(%s) read %d bytes\n", __func__,
	      filename, num);

	if (num < 0) {
		debug(1, "can't read from %s: %s", filename,
		      strerror(errno));
		rv = close(fd);
		if (rv)
			debug(1, "can't close the file descriptor either: %s",
			      strerror(errno));
		return 0;
	}

	if (num == 0) {
		debug(1, "file %s contains no data", filename);
		rv = close(fd);
		if (rv)
			debug(1, "can't close the file descriptor either: %s",
			      strerror(errno));
		return 0;

	}

	debug_buf(2, (unsigned char *)buf, num);

	rv = close(fd);
	if (rv)
		debug(1, "can't close the file descriptor for %s: %s",
		      filename, strerror(errno));

	return 1;
}

/* Returns true if we're able to export this gpio */
static int is_ap_exported(uint32_t num)
{
	char filename[80];
	char buf[80];

	debug(1, "%s(%d)\n", __func__, num);

	/* It might already be exported. Try to read the value to see. */
	sprintf(filename, "/sys/class/gpio/gpio%d/value", num);
	memset(buf, 0, sizeof(buf));
	if (read_from_file(filename, buf, sizeof(buf)))
		return 1;			/* yep */

	/* Request it */
	sprintf(buf, "%d", num);
	if (!write_to_file("/sys/class/gpio/export", buf)) {
		Error("%s: Can't request export of gpio %d", __func__, num);
		return 0;
	}

	/* Try reading the value again */
	memset(buf, 0, sizeof(buf));
	if (read_from_file(filename, buf, sizeof(buf)))
		return 1;			/* yep */

	Error("%s: Nuts. Can't get export of gpio %d", __func__, num);
	return 0;
}

/* Returns true if we're able to set this gpio to an output */
static int is_ap_output(uint32_t num)
{
	char filename[80];
	char buf[80];

	debug(1, "%s(%d)\n", __func__, num);

	/* It might already be an output. Let's see. */
	sprintf(filename, "/sys/class/gpio/gpio%d/direction", num);
	memset(buf, 0, sizeof(buf));
	if (!read_from_file(filename, buf, sizeof(buf))) {
		debug(1, "Can't determine the direction of gpio %d", num);
		return 0;
	}
	if (!strncmp("out", buf, 3))
		return 1;			/* already an output */

	/* Set the direction */
	sprintf(buf, "%s", "out");
	if (!write_to_file(filename, buf)) {
		Error("%s: Can't set the direction of gpio %d", __func__, num);
		return 0;
	}

	/* Check again */
	memset(buf, 0, sizeof(buf));
	if (!read_from_file(filename, buf, sizeof(buf))) {
		Error("%s: Can't determine the direction of gpio %d",
		      __func__, num);
		return 0;
	}
	if (!strncmp("out", buf, 3))
		return 1;			/* yep, it's an output */

	Error("%s: Nuts. Can't set the direction of gpio %d", __func__, num);
	return 0;
}

/* Return true on success */
static int set_ap_value(uint32_t num, int val)
{
	char filename[80];
	char buf[80];

	debug(1, "%s(%d, %d)\n", __func__, num, val);

	sprintf(filename, "/sys/class/gpio/gpio%d/value", num);
	sprintf(buf, "%d", val);
	if (!write_to_file(filename, buf)) {
		Error("%s: can't set gpio %d to %d", __func__, num, val);
		return 0;
	}

	return 1;
}

static void ap_wiggle(const struct nos_device *dev, uint32_t cit_gpio,
		      uint32_t ap_gpio)
{
	uint32_t prev, curr;
	uint32_t cit_bit;

	cit_bit = (1 << cit_gpio);

	printf("Test GPIO AP %d => Citadel %d\n", ap_gpio, cit_gpio);

	/* Configure AP for output */
	if (!is_ap_exported(ap_gpio))
		return;
	if (!is_ap_output(ap_gpio))
		return;

	debug(1, "cit_bit 0x%08x\n", cit_bit);

	/* drive low, confirm low */
 	if (!set_ap_value(ap_gpio, 0))
		return;
	if (0 != read32(dev, GPIO_DATA, &curr)) {
		Error("%s: can't read Citadel GPIOs", __func__);
		return;
	}
	debug(1, "citadel 0x%08x\n", curr);
	if (curr & cit_bit)
		Error("%s: expected cit_gpio %d low, was high", __func__,
		      cit_gpio);
	prev = curr;

	/* Drive high, confirm high, no other bits changed */
 	if (!set_ap_value(ap_gpio, 1))
		return;
	if (0 != read32(dev, GPIO_DATA, &curr)) {
		Error("%s: can't read Citadel GPIOs", __func__);
		return;
	}
	debug(1, "citadel 0x%08x\n", curr);
	if (!(curr & cit_bit))
		Error("%s: expected cit_gpio %d high, was low", __func__,
		      cit_gpio);
	if (prev != curr && (prev ^ curr) != cit_bit)
		Error("%s: unexpected GPIO change: prev 0x%08x cur 0x%08x",
		      __func__, prev, curr);
	prev = curr;

	/* Drive Low, confirm low again, no other bits changed */
 	if (!set_ap_value(ap_gpio, 0))
		return;
	if (0 != read32(dev, GPIO_DATA, &curr)) {
		Error("%s: can't read Citadel GPIOs", __func__);
		return;
	}
	debug(1, "citadel 0x%08x\n", curr);
	if (curr & cit_bit)
		Error("%s: expected cit_gpio %d low, was high", __func__,
		      cit_gpio);
	if (prev != curr && (prev ^ curr) != cit_bit)
		Error("%s: unexpected GPIO change: prev 0x%08x cur 0x%08x",
		      __func__, prev, curr);
}

static int stopped;
static void sig_handler(int sig)
{
	stopped = 1;
	printf("Signal %d recognized\n", sig);
}

static void phys_wiggle(const struct nos_device *dev, uint32_t cit_gpio,
			const char *button)
{
	uint32_t prev, curr;
	uint32_t cit_bit;

	stopped = 0;
	signal(SIGINT, sig_handler);

	cit_bit = (1 << cit_gpio);

	debug(1, "%s(%d) cit_bit 0x%08x\n", __func__, cit_gpio, cit_bit);

	/* read initial value */
	if (0 != read32(dev, GPIO_DATA, &curr)) {
		Error("%s: can't read Citadel GPIOs", __func__);
		return;
	}
	debug(1, "initial value 0x%08x\n", curr);
	prev = curr;

	printf("\nPlease PRESS the %s button...\n", button);
	do {
		usleep(100 * 1000);
		if (0 != read32(dev, GPIO_DATA, &curr)) {
			Error("%s: can't read Citadel GPIOs", __func__);
			return;
		}
	} while (prev == curr && !stopped);
	debug(1, "new value 0x%08x\n", curr);
	if ((prev ^ curr) != cit_bit)
		Error("%s: multiple cit_gpios changed: prev 0x%08x cur 0x%08x",
		      __func__, prev, curr);
	prev = curr;

	printf("Please RELEASE the %s button...\n", button);
	do {
		usleep(100 * 1000);
		if (0 != read32(dev, GPIO_DATA, &curr)) {
			Error("%s: can't read Citadel GPIOs", __func__);
			return;
		}
	} while (prev == curr && !stopped);
	debug(1, "new value 0x%08x\n", curr);
	if ((prev ^ curr) != cit_bit)
		Error("%s: multiple cit_gpios changed: prev 0x%08x cur 0x%08x",
		      __func__, prev, curr);

	signal(SIGINT, SIG_DFL);
}

static const uint8_t expected_tpm_did_vid[] = {
	0xe0, 0x1a, 0x28, 0x00,
};

static void do_test(void)
{
	int rv;
	uint32_t i, buflen, retval, replycount, value;
	struct nos_device dev;

	printf("\nOpening connection...\n");
	if (nos_device_open(option.device, &dev) != 0) {
		Error("Unable to connect to the SPI driver");
		goto out;
	}

	printf("Simple write...\n");
	/* Clear the version string pointer */
	buf[0] = 0x00;
	rv = dev.ops.write(dev.ctx, WR1_TPM_FW_VER, buf, 1);
	if (rv != 0) {
		Error("Simple write failed with 0x%08x (%d)", rv, rv);
		goto done;
	}

	printf("Simple read...\n");
	/* Read TPM_DID_VID */
	buflen = sizeof(expected_tpm_did_vid);
	memset(buf, 0, buflen);
	rv = dev.ops.read(dev.ctx, RD4_TPM_DID_VID, buf, buflen);
	if (rv != 0) {
		Error("Simple read failed with 0x%08x (%d)", rv, rv);
		goto done;
	} else {
		debug(1, "got:");
		for (i = 0; i < buflen; i++)
			debug(1, " %02x", buf[i]);
		debug(1, "\n");

		if (memcmp(buf, expected_tpm_did_vid,
			   sizeof(expected_tpm_did_vid)))
			Error("Simple read returned unexpected values");
		/* Don't quit - others things might still work */
	}

	/* Transport API */
	printf("Get version string...\n");
	replycount = sizeof(buf);
	retval = nos_call_application(&dev, APP_ID_NUGGET, NUGGET_PARAM_VERSION,
				      buf, 0,
				      buf, &replycount);
	if (retval != 0) {
		Error("Get version failed with 0x%08x", retval);
		debug_retval(0, retval, replycount);
		goto done;
	}
	if (replycount < 4 || replycount > 1024)
		Error("Get version returned %d bytes, which seems wrong",
		      replycount);		/* might be okay, though */
	debug_buf(1, buf, replycount);

	/*
	 * We want to drive each GPIO from the AP side and just check that
	 * Citadel can see it wiggle. Citadel treats them all as inputs for
	 * now. We'll have to update our tests when that changes, of course.
	 */

	printf("Read GPIO direction\n");
	retval = read32(&dev, GPIO_OUTENSET, &value);
	if (retval != 0) {
		Error("Reading GPIO direction failed with 0x%08x", retval);
		goto done;
	}
	/* We expect everything to be an input, but keep going regardless */
	if (value != 0x00000000)
		Error("GPIO direction = 0x%08x\n", value);

	ap_wiggle(&dev, 4, 76);	/* Citadel GPIO 4 AP_SEC_STATE is AP GPIO 76 */
	ap_wiggle(&dev, 5, 69);	/* Citadel GPIO 5 AP_PWR_STATE is AP GPIO 69 */
	ap_wiggle(&dev, 6, 94);	/* Citadel GPIO 6 AP_CTDL_IRQ  is AP GPIO 94 */
	ap_wiggle(&dev, 7, 96);	/* Citadel GPIO 7 CTDL_AP_IRQ  is AP GPIO 96 */

	/* Citadel GPIO 8 CCD_CABLE_DET is AP GPIO 126 */
	if (option.board == BOARD_BINDER)
		// but it's on gpio 127 on the binder board */
		ap_wiggle(&dev, 8, 127);
	else
		ap_wiggle(&dev, 8, 126);

	/*
	 * Citadel should be able to drive all the physical buttons under
	 * certain circumstances, but I don't know how to confirm whether the
	 * AP sees them change. We'll have to prompt the user to poke them to
	 * verify the connectivity. That's probably tested elsewhere, though.
	 */
	if (option.buttons) {

		if (option.board != BOARD_PROTO1)
			/* We had to cut this trace on proto1 (b/66976641) */
			phys_wiggle(&dev, 0, "Power");

		phys_wiggle(&dev, 1, "Volume Down");
		phys_wiggle(&dev, 2, "Volume Up");

		if (option.board == BOARD_BINDER)
			/* There's only a button on the binder board */
			phys_wiggle(&dev, 10, "Forced USB Boot");

	}

	/*
	 * These are harder to test. We'll have to change the UART passthrough
	 * to access the Citadel console and do these manually:
	 *
	 *   Citadel GPIO 3 MSM_RST_OUT_L should wiggle when the phone reboots
	 *   Citadel GPIO 9 PM_MSM_RST_L should force the AP to reboot
	 */

done:
	dev.ops.close(dev.ctx);
out:
	if (errorcnt)
		printf("\nFAIL FAIL FAIL\n\n");
	else
		printf("\nPASS PASS PASS\n\n");
}

/****************************************************************************/

int main(int argc, char *argv[])
{
	int i;
	int idx = 0;
	char *e = 0;
	char *this_prog;

	this_prog= strrchr(argv[0], '/');
        if (this_prog)
		this_prog++;
        else
		this_prog = argv[0];

        opterr = 0;				/* quiet, you */
	while ((i = getopt_long(argc, argv,
				short_opts, long_opts, &idx)) != -1) {
		switch (i) {
			/* program-specific options */
		case 'i':
			option.app_id = (uint8_t)strtoul(optarg, &e, 16);
			if (!*optarg || (e && *e))
				Error("Invalid argument: \"%s\"", optarg);
			break;
		case 'p':
			option.param = (uint16_t)strtoul(optarg, &e, 16);
			if (!*optarg || (e && *e))
				Error("Invalid argument: \"%s\"", optarg);
			break;
		case 'm':
			option.more = (uint32_t)strtoul(optarg, &e, 0);
			if (!*optarg || (e && *e) || option.more < 0)
				Error("Invalid argument: \"%s\"", optarg);
			break;
		case 'a':
			option.ascii = 1;
			option.binary = 0;
			break;
		case 'b':
			option.ascii = 0;
			option.binary = 1;
			break;
		case 'v':
			option.verbose++;
			break;
		case OPT_BUTTONS:
			option.buttons = 1;
			break;

			/* generic options below */
		case OPT_DEVICE:
			option.device = optarg;
			break;
		case 'h':
			usage(this_prog);
			return 0;
		case 0:
			break;
		case '?':
			if (optopt)
				Error("Unrecognized option: -%c", optopt);
			else
				Error("Unrecognized option: %s",
				      argv[optind - 1]);
			usage(this_prog);
			break;
		case ':':
			Error("Missing argument to %s", argv[optind - 1]);
			break;
		default:
			Error("Internal error at %s:%d", __FILE__, __LINE__);
			exit(1);
		}
        }

	if (errorcnt)
		return !!errorcnt;

	/*
	 * We can freely intermingle options and args, so the function should
	 * be the first non-option. Try to pick it out if it exists.
	 */
	if (argc > optind) {
		if (!strcmp("help", argv[optind]))
			usage(this_prog);
		else if (!strcmp("tpm", argv[optind]))
			do_tpm(argc - optind, argv + optind);
		else if (!strcmp("app", argv[optind]))
			do_app(argc - optind, argv + optind);
		else if (!strcmp("rw", argv[optind]))
			do_rw(argc - optind, argv + optind);
		else
			do_test();
		/*
		 * "test" is the default function, but it doesn't take any args
		 * so anything not listed is just silently ignored. Too bad.
		 */
	} else {
		do_test();
	}

	return !!errorcnt;
}
