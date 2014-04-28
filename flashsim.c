/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include "flashsim.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#ifdef COLOR_OUTPUT
#define cprintf(colors, fmt, args...) \
    printf("\x1b[0;1;" colors "m" fmt , ## args)
#define cprintf_end(fmt, args...) \
    printf(fmt "\x1b[0m\n" , ## args)
#else
#define cprintf(colors, fmt, args...) \
    printf(fmt , ## args)
#define cprintf_end(fmt, args...) \
    printf(fmt "\n" , ## args)
#endif

struct flashsim {
    int size;
    int sector_size;

    FILE *fh;
};

struct flashsim *flashsim_open(const char *name, int size, int sector_size)
{
    struct flashsim *sim = malloc(sizeof(struct flashsim));

    sim->size = size;
    sim->sector_size = sector_size;
    sim->fh = fopen(name, "w+");
    assert(sim->fh != NULL);
    assert(ftruncate(fileno(sim->fh), size) == 0);

    return sim;
}

void flashsim_close(struct flashsim *sim)
{
    fclose(sim->fh);
    free(sim);
}

void flashsim_sector_erase(struct flashsim *sim, int addr)
{
    int sector_start = addr & ~(sim->sector_size - 1);
    cprintf("31", "flashsim_erase  (0x%08x) * erasing sector at 0x%08x", addr, sector_start);
    cprintf_end("");

    void *empty = malloc(sim->sector_size);
    memset(empty, 0xff, sim->sector_size);

    assert(fseek(sim->fh, sector_start, SEEK_SET) == 0);
    assert(fwrite(empty, 1, sim->sector_size, sim->fh) == (size_t) sim->sector_size);

    free(empty);
}

void flashsim_read(struct flashsim *sim, int addr, uint8_t *buf, int len)
{
    assert(fseek(sim->fh, addr, SEEK_SET) == 0);
    assert(fread(buf, 1, len, sim->fh) == (size_t) len);

    cprintf("32", "flashsim_read   (0x%08x) = %d bytes [ ", addr, len);
    for (int i=0; i<len; i++) {
        printf("%02x ", buf[i]);
        if (i == 15) {
            printf("... ");
            break;
        }
    }
    cprintf_end("]");
}

void flashsim_program(struct flashsim *sim, int addr, const uint8_t *buf, int len)
{
    cprintf("34", "flashsim_program(0x%08x) + %d bytes [ ", addr, len);
    for (int i=0; i<len; i++) {
        printf("%02x ", buf[i]);
        if (i == 15) {
            printf("... ");
            break;
        }
    }
    cprintf_end("]");

    uint8_t *data = malloc(len);

    assert(fseek(sim->fh, addr, SEEK_SET) == 0);
    assert(fread(data, 1, len, sim->fh) == (size_t) len);

    for (int i=0; i<(int) len; i++)
        data[i] &= buf[i];

    assert(fseek(sim->fh, addr, SEEK_SET) == 0);
    assert(fwrite(data, 1, len, sim->fh) == (size_t) len);

    free(data);
}

/* vim: set ts=4 sw=4 et: */
