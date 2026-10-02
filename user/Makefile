# SPDX-License-Identifier: GPL-2.0
CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -I../include/uapi -O3 -pthread

all: kmodring_reader kmodring_bench kmodring_ioctl

kmodring_reader: kmodring_reader.c
	$(CC) $(CFLAGS) kmodring_reader.c -o kmodring_reader

kmodring_bench: kmodring_bench.c
	$(CC) $(CFLAGS) kmodring_bench.c -o kmodring_bench

kmodring_ioctl: kmodring_ioctl.c
	$(CC) $(CFLAGS) kmodring_ioctl.c -o kmodring_ioctl

clean:
	rm -f kmodring_reader kmodring_bench kmodring_ioctl
