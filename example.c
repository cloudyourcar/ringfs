/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <stdio.h>
#include <assert.h>

#include "ringfs.h"
#include "flashsim.h"


/* Flash implementation & ops. */

#define FLASH_SECTOR_SIZE       1024
#define FLASH_TOTAL_SECTORS     16
#define FLASH_PARTITION_OFFSET  8
#define FLASH_PARTITION_SIZE    4

static struct flashsim *sim;

static void init_flash_driver(void)
{
    sim = flashsim_open("example.sim",
            FLASH_TOTAL_SECTORS*FLASH_SECTOR_SIZE,
            FLASH_SECTOR_SIZE);
}

static int op_sector_erase(int address)
{
    flashsim_sector_erase(sim, address);
    return 0;
}

static ssize_t op_program(int address, const void *data, size_t size)
{
    flashsim_program(sim, address, data, size);
    return size;
}

static ssize_t op_read(int address, void *data, size_t size)
{
    flashsim_read(sim, address, data, size);
    return size;
}

static struct ringfs_flash_partition flash = {
    .sector_size = FLASH_SECTOR_SIZE,
    .sector_offset = FLASH_PARTITION_OFFSET,
    .sector_count = FLASH_PARTITION_SIZE,

    .sector_erase = op_sector_erase,
    .program = op_program,
    .read = op_read,
};


/* Data record format. */
struct log_entry {
    int level;
    char message[16];
};
#define LOG_ENTRY_VERSION 1


/* Let's roll! */
int main()
{
    /* Initialize your Flash driver before using the filesystem. */
    init_flash_driver();

    /* Always call ringfs_init first. */
    struct ringfs fs;
    ringfs_init(&fs, &flash, LOG_ENTRY_VERSION, sizeof(struct log_entry));

    /* Scan and/or format before any data operations. */
    printf("# scanning for filesystem...\n");
    if (ringfs_scan(&fs) == 0) {
        printf("# found existing filesystem, usage: %d/%d\n",
                ringfs_count_estimate(&fs),
                ringfs_capacity(&fs));
    } else {
        printf("# no valid filesystem found, formatting.\n");
        ringfs_format(&fs);
    }

    /* Append data using ringfs_append. Oldest data is removed as needed. */
    printf("# inserting some objects\n");
    ringfs_append(&fs, &(struct log_entry) { 1, "foo" });
    ringfs_append(&fs, &(struct log_entry) { 2, "bar" });
    ringfs_append(&fs, &(struct log_entry) { 3, "baz" });
    ringfs_append(&fs, &(struct log_entry) { 4, "xyzzy" });
    ringfs_append(&fs, &(struct log_entry) { 5, "test" });
    ringfs_append(&fs, &(struct log_entry) { 6, "hello" });

    /* Objects are retrieved using ringfs_fetch. They are not physically removed
     * until you call ringfs_discard. This is useful, for example, when transmitting
     * queued objects over the network: you don't physically remove the objects
     * from the ring buffer until you receive an ACK. */
    printf("# reading 2 objects\n");
    for (int i=0; i<2; i++) {
        struct log_entry entry;
        assert(ringfs_fetch(&fs, &entry) == 0);
        printf("## level=%d message=%.16s\n", entry.level, entry.message);
    }
    printf("# discarding read objects\n");
    ringfs_discard(&fs);

    /* If you decide you can't remove the objects yet, just call ringfs_rewind() and
     * they will be available again for subsequent reads. */
    printf("# reading 2 objects\n");
    for (int i=0; i<2; i++) {
        struct log_entry entry;
        assert(ringfs_fetch(&fs, &entry) == 0);
        printf("## level=%d message=%.16s\n", entry.level, entry.message);
    }
    printf("# rewinding read head back\n");
    ringfs_rewind(&fs);

    /* ...and here they are again. */
    printf("# reading 2 objects\n");
    for (int i=0; i<2; i++) {
        struct log_entry entry;
        assert(ringfs_fetch(&fs, &entry) == 0);
        printf("## level=%d message=%.16s\n", entry.level, entry.message);
    }

    return 0;
}

/* vim: set ts=4 sw=4 et: */
