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

#include <getopt.h>
#include <openssl/sha.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* From Nugget OS */
#include <application.h>
#include <app_nugget.h>
#include <flash_layout.h>
#include <signed_header.h>

#include <nos/device.h>
#include <nos/transport.h>

/* Global options */
static struct option_s {
	/* actions to take */
	int version;
	int ro;
	int rw;
	int reboot;
	/* generic connection options */
	const char *device;
} option;

enum no_short_opts_for_these {
	OPT_DEVICE = 1000,
	OPT_RO,
	OPT_RW,
	OPT_REBOOT,
};

static char *short_opts = ":hv";
static const struct option long_opts[] = {
	/* name    hasarg *flag val */
	{"version",     0, NULL, 'v'},
	{"ro",          0, NULL, OPT_RO},
	{"rw",          0, NULL, OPT_RW},
	{"reboot",      0, NULL, OPT_REBOOT},
	{"device",      1, NULL, OPT_DEVICE},
	{"help",        0, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

static void usage(const char *progname)
{
	fprintf(stderr, "\n");
	fprintf(stderr,
		"Usage: %s [actions] [image.bin]\n"
		"\n"
		"Citadel firmware boots in two stages. The first stage\n"
		"bootloader (aka \"RO\") is provided by the SOC hardware team\n"
		"and seldom changes. The application image (\"RW\") is invoked\n"
		"by the RO image. There are two copies (A/B) of each stage,\n"
		"so that the active copy can be protected while the unused\n"
		"copy may be updated. At boot, the newer (valid) copy of each\n"
		"stage is selected.\n"
		"\n"
		"The Citadel image file is the same size of the internal\n"
		"flash, and contains all four firmware components (RO_A,\n"
		"RW_A, RO_B, RW_B) located at the correct offsets. Only the\n"
		"inactive copy (A/B) of each stage (RO/RW) can be modified.\n"
		"The tool will update the correct copies automatically.\n"
		"\n"
		"You must specify the actions to perform. With no options,\n"
		"this help message is displayed.\n"
		"\n"
		"Actions:\n"
		"\n"
		"  -v, --version     Display the Citadel version info\n"
		"      --rw          Update RW firmware from the image file\n"
		"      --ro          Update RO firmware from the image file\n"
		"      --reboot      Tell Citadel to reboot\n"
		"\n",
		progname);
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

/* Return true on APP_SUCESS, display error message if it's not */
static int is_app_success(uint32_t retval)
{
	if (retval == APP_SUCCESS)
		return 1;

	errorcnt++;

	fprintf(stderr, "Error code 0x%x: ", retval);
	switch (retval) {
	case APP_ERROR_BOGUS_ARGS:
		fprintf(stderr, "bogus args");
		break;
	case APP_ERROR_INTERNAL:
		fprintf(stderr, "app is being stupid");
		break;
	case APP_ERROR_TOO_MUCH:
		fprintf(stderr, "caller sent too much data");
		break;
	default:
		if (retval >= APP_SPECIFIC_ERROR &&
			 retval < APP_LINE_NUMBER_BASE)
			fprintf(stderr, "app-specific error #%d",
				retval - APP_SPECIFIC_ERROR);
		else if (retval >= APP_LINE_NUMBER_BASE)
			fprintf(stderr, "error at line %d",
				retval - APP_LINE_NUMBER_BASE);
		else
			fprintf(stderr, "unknown)");
	}
	fprintf(stderr, "\n");

	return 0;
}

/****************************************************************************/

static uint8_t *read_image_from_file(const char *name, uint32_t *len)
{
	FILE *fp;
	struct stat st;
	uint8_t *buf;

	fp = fopen(name, "rb");
	if (!fp) {
		perror("fopen");
		Error("Can't open file %s", name);
		return 0;
	}

	if (fstat(fileno(fp), &st)) {
		perror("fstat");
		Error("Can't fstat file %s", name);
		fclose(fp);
		return 0;
	}

	if (st.st_size != CHIP_FLASH_SIZE) {
		Error("The firmware image must be exactly %d bytes",
		      CHIP_FLASH_SIZE);
		fclose(fp);
		return 0;
	}

	buf = malloc(st.st_size);
	if (!buf) {
		perror("malloc");
		Error("Can't malloc space for the image file");
		fclose(fp);
		return 0;
	}

	if (1 != fread(buf, st.st_size, 1, fp)) {
		perror("fread");
		Error("Can't read %zd bytes", st.st_size);
		fclose(fp);
		free(buf);
		return 0;
	}

	fclose(fp);

	if (len)
		*len = (uint32_t)st.st_size;

	return buf;
}

static uint32_t compute_digest(struct nugget_app_flash_block *blk)
{
	uint8_t *start_here = ((uint8_t *)blk) +
		offsetof(struct nugget_app_flash_block, offset);
	size_t size_to_hash = sizeof(*blk) -
		offsetof(struct nugget_app_flash_block, offset);
	SHA_CTX ctx;
	uint8_t digest[SHA_DIGEST_LENGTH];
	uint32_t retval;

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, start_here, size_to_hash);
	SHA1_Final(digest, &ctx);

	memcpy(&retval, digest, sizeof(retval));
	return retval;
}

static uint32_t try_update(struct nos_device *dev, uint8_t *image,
			   uint32_t offset, uint32_t imagesize)
{
	struct nugget_app_flash_block fb;
	uint32_t stop = offset + imagesize;
	uint32_t rv;

	printf("Updating image from 0x%05x to 0x%05x, size 0x%05x\n",
	       CHIP_FLASH_BASE + offset, CHIP_FLASH_BASE + stop, imagesize);

	for (; offset < stop; offset += CHIP_FLASH_BANK_SIZE) {
		int retries = 3;

		fb.offset = offset;
		memcpy(fb.payload, image + offset, CHIP_FLASH_BANK_SIZE);
		fb.block_digest = compute_digest(&fb);

		printf("writing 0x%05x / 0x%05x",
		       CHIP_FLASH_BASE + offset, CHIP_FLASH_BASE + stop);
		do {
			rv = nos_call_application(dev, APP_ID_NUGGET,
						  NUGGET_PARAM_FLASH_BLOCK,
						  (uint8_t *)&fb, sizeof(fb),
						  0, 0);
			if (rv == NUGGET_ERROR_RETRY)
				printf(" retrying");
		} while (rv == NUGGET_ERROR_RETRY && retries--);
		printf(" %s\n", rv ? "fail" : "ok");
		if (rv)
			break;
	}

	return rv;
}


static uint32_t do_update(struct nos_device *dev, uint8_t *image,
			  uint32_t offset_A, uint32_t offset_B)
{
	struct SignedHeader *hdr;
	uint32_t rv_A, rv_B;

	/* Try image A first */
	hdr = (struct SignedHeader *)(image + offset_A);
	rv_A = try_update(dev, image, offset_A, hdr->image_size);

	/* If that worked, we're done */
	if (rv_A == APP_SUCCESS)
		return rv_A;

	/* Else try image B */
	hdr = (struct SignedHeader *)(image + offset_B);
	rv_B = try_update(dev, image, offset_B, hdr->image_size);

	return rv_B;
}

static uint32_t do_version(struct nos_device *dev)
{
	uint32_t replycount, retval;
	uint8_t buf[512];

	replycount = sizeof(buf);
	retval = nos_call_application(dev, APP_ID_NUGGET,
				      NUGGET_PARAM_VERSION,
				      0, 0,
				      buf, &replycount);

	if (is_app_success(retval))
		printf("%s\n", buf);

	return retval;
}

static uint32_t do_reboot(struct nos_device *dev)
{
	uint32_t retval;
	uint8_t hard = 0;

	retval = nos_call_application(dev, APP_ID_NUGGET,
				      NUGGET_PARAM_REBOOT,
				      &hard, sizeof(hard),
				      0, 0);

	if (is_app_success(retval))
		printf("Citadel reboot requested\n");

	return retval;
}

int main(int argc, char *argv[])
{
	int i;
	int idx = 0;
	char *this_prog;
	uint8_t *image = 0;
	uint32_t imagesize = 0 ;
	int got_action = 0;
	struct nos_device dev;

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
		case 'v':
			option.version = 1;
			got_action = 1;
			break;
		case OPT_RO:
			option.ro = 1;
			got_action = 1;
			break;
		case OPT_RW:
			option.rw = 1;
			got_action = 1;
			break;
		case OPT_REBOOT:
			option.reboot = 1;
			got_action = 1;
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
		goto out;

	if (!got_action) {
		usage(this_prog);
		goto out;
	}

	if (option.ro || option.rw) {
		if (optind < argc)
			/* Sets errorcnt on failure */
			image = read_image_from_file(argv[optind], &imagesize);
		else
			Error("An image file is required with --ro and --rw");
	}

	if (errorcnt)
		goto out;

	/* Okay, let's do something */
	if (nos_device_open(option.device, &dev) != 0) {
		Error("Unable to connect");
		goto out;
	}

	/* Try all requested actions in reasonable order, bail out on error */

	if (option.version &&
	    do_version(&dev) != APP_SUCCESS)
		goto done;

	if (option.rw &&
	    do_update(&dev, image,
		      CHIP_RW_A_MEM_OFF, CHIP_RW_B_MEM_OFF) != APP_SUCCESS)
		goto done;

	if (option.ro &&
	    do_update(&dev, image,
		      CHIP_RO_A_MEM_OFF, CHIP_RO_B_MEM_OFF) != APP_SUCCESS)
		goto done;

	if (option.reboot)
		(void) do_reboot(&dev);
	/* Rebooting is the last action, so fall through either way */

done:
	dev.ops.close(dev.ctx);

out:
	if (image)
		free(image);

	return !!errorcnt;
}
