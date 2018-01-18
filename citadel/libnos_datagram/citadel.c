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

#include <nos/device.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/types.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/*****************************************************************************/
/* TODO: #include <linux/citadel.h> */
#define CITADEL_IOC_MAGIC               'c'
struct citadel_ioc_tpm_datagram {
    __u64        buf;
    __u32        len;
    __u32        command;
};
#define CITADEL_IOC_TPM_DATAGRAM    _IOW(CITADEL_IOC_MAGIC, 1, \
                         struct citadel_ioc_tpm_datagram)
/*****************************************************************************/

#define DEV_CITADEL "/dev/citadel0"

static uint8_t in_buf[MAX_DEVICE_TRANSFER];
static int read_datagram(void *ctx, uint32_t command, uint8_t *buf, uint32_t len) {
    struct citadel_ioc_tpm_datagram dg = {
        .buf = (unsigned long)in_buf,
        .len = len,
        .command = command,
    };
    int ret;
    int fd;

    if (!ctx) {
        fprintf(stderr, "%s: invalid (NULL) device\n", __func__);
        return -ENODEV;
    }
    fd = *(int *)ctx;
    if (fd < 0) {
        fprintf(stderr, "%s: invalid device\n", __func__);
        return -ENODEV;
    }

    if (len > MAX_DEVICE_TRANSFER) {
        fprintf(stderr, "%s: invalid len (%d > %d)\n", __func__,
            len, MAX_DEVICE_TRANSFER);
        return -E2BIG;
    }

    ret = ioctl(fd, CITADEL_IOC_TPM_DATAGRAM, &dg);
    if (ret < 0) {
        perror("can't send spi message");
        return -errno;
    }

    memcpy(buf, in_buf, len);

    return 0;
}

static uint8_t out_buf[MAX_DEVICE_TRANSFER];
static int write_datagram(void *ctx, uint32_t command, const uint8_t *buf, uint32_t len) {
    struct citadel_ioc_tpm_datagram dg = {
        .buf = (unsigned long)out_buf,
        .len = len,
        .command = command,
    };
    int ret;
    int fd;

    if (!ctx) {
        fprintf(stderr, "%s: invalid (NULL) device\n", __func__);
        return -ENODEV;
    }
    fd = *(int *)ctx;
    if (fd < 0) {
        fprintf(stderr, "%s: invalid device\n", __func__);
        return -ENODEV;
    }

    if (len > MAX_DEVICE_TRANSFER) {
        fprintf(stderr, "%s: invalid len (%d > %d)\n", __func__,
            len, MAX_DEVICE_TRANSFER);
        return -E2BIG;
    }

    memcpy(out_buf, buf, len);

    ret = ioctl(fd, CITADEL_IOC_TPM_DATAGRAM, &dg);
    if (ret < 0) {
        perror("can't send spi message");
        return -errno;
    }

    return 0;
}

static void close_device(void *ctx) {
    int fd;

    if (!ctx) {
        fprintf(stderr, "%s: invalid (NULL) device (ignored)\n", __func__);
        return;
    }
    fd = *(int *)ctx;
    if (fd < 0) {
        fprintf(stderr, "%s: invalid device (ignored)\n", __func__);
        return;
    }

    if (close(fd) < 0)
        perror("Problem closing device (ignored)");
    free(ctx);
}

int nos_device_open(const char *device_name, struct nos_device *dev) {
    int fd, *new_fd;

    fd = open(device_name ? device_name : DEV_CITADEL, O_RDWR);
    if (fd < 0) {
        perror("can't open device");
        return -errno;
    }

    new_fd = (int *)malloc(sizeof(int));
    if (!new_fd) {
        perror("can't malloc new fd");
        close(fd);
        return -ENOMEM;
    }
    *new_fd = fd;

    dev->ctx = new_fd;
    dev->ops.read = read_datagram;
    dev->ops.write = write_datagram;
    dev->ops.close = close_device;
    return 0;
}
