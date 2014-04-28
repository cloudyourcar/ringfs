/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef RINGFS_H
#define RINGFS_H

/**
 * @defgroup ringfs_api RingFS API
 * @{
 */

#include <stdint.h>
#include <unistd.h>

/**
 * Flash memory+parition descriptor.
 */
struct ringfs_flash_partition
{
    int sector_size;            /**< Sector size, in bytes. */
    int sector_offset;          /**< Partition offset, in sectors. */
    int sector_count;           /**< Partition size, in sectors. */

    /**
     * Erase a sector.
     * @param address Any address inside the sector.
     * @returns Zero on success, -1 on failure.
     */
    int (*sector_erase)(int address);
    /**
     * Program flash memory bits by toggling them from 1 to 0.
     * @param address Start address, in bytes.
     * @param data Data to program.
     * @param size Size of data.
     * @returns size on success, -1 on failure.
     */
    ssize_t (*program)(int address, const void *data, size_t size);
    /**
     * Read flash memory.
     * @param address Start address, in bytes.
     * @param data Buffer to store read data.
     * @param size Size of data.
     * @returns size on success, -1 on failure.
     */
    ssize_t (*read)(int address, void *data, size_t size);
};

/** @private */
struct ringfs_loc {
    int sector;
    int slot;
};

/**
 * RingFS instance. Should be initialized with ringfs_init() befure use.
 * Structure fields should not be accessed directly.
 * */
struct ringfs {
    /* Constant values, set once at ringfs_init(). */
    const struct ringfs_flash_partition *flash;
    uint32_t version;
    int object_size;
    /* Cached values. */
    int slots_per_sector;

    /* Read/write pointers. Modified as needed. */
    struct ringfs_loc read;
    struct ringfs_loc write;
    struct ringfs_loc cursor;
};

/**
 * Initialize a RingFS instance. Must be called before the instance can be used
 * with the other ringfs_* functions.
 *
 * @param fs RingFS instance to be initialized.
 * @param flash Flash memory interface. Must be implemented externally.
 * @param version Object version. Should be incremented whenever the object's
 *                semantics or size change in a backwards-incompatible way.
 * @param object_size Size of one stored object, in bytes.
 * @returns Zero on success, -1 on failure.
 */
int ringfs_init(struct ringfs *fs, const struct ringfs_flash_partition *flash, uint32_t version, int object_size);

/**
 * Format the flash memory.
 *
 * @param fs Initialized RingFS instance.
 * @returns Zero on success, -1 on failure.
 */
int ringfs_format(struct ringfs *fs);

/**
 * Scan the flash memory for a valid filesystem.
 *
 * @param fs Initialized RingFS instance.
 * @returns Zero on success, -1 on failure.
 */
int ringfs_scan(struct ringfs *fs);

/**
 * Calculate maximum RingFS capacity.
 *
 * @param fs Initialized RingFS instance.
 * @returns Maximum capacity on success, -1 on failure.
 */
int ringfs_capacity(struct ringfs *fs);

/**
 * Calculate approximate object count. Completes in O(1).
 *
 * @param fs Initialized RingFS instance.
 * @returns Estimated object count on success, -1 on failure.
 */
int ringfs_count_estimate(struct ringfs *fs);

/**
 * Calculate exact object count. Completes in O(n) because the entire allocated
 * memory must be scanned.
 *
 * @param fs Initialized RingFS instance.
 * @returns Exact object count on success, -1 on failure.
 */
int ringfs_count_exact(struct ringfs *fs);

/**
 * Append an object at the end of the ring. Deletes oldest objects as needed.
 *
 * @param fs Initialized RingFS instance.
 * @param object Object to be stored.
 * @returns Zero on success, -1 on failure.
 */
int ringfs_append(struct ringfs *fs, const void *object);

/**
 * Fetch next object from the ring, oldest-first. Advances read cursor.
 *
 * @param fs Initialized RingFS instance.
 * @param object Buffer to store retrieved object.
 * @returns Zero on success, -1 on failure.
 */
int ringfs_fetch(struct ringfs *fs, void *object);

/**
 * Discard all fetched objects up to the read cursor.
 *
 * @param fs Initialized RingFS instance.
 * @returns Zero on success, -1 on failure.
 */
int ringfs_discard(struct ringfs *fs);

/**
 * Rewind the read cursor back to the oldest object.
 *
 * @param fs Initialized RingFS instance.
 * @returns Zero on success, -1 on failure.
 */
int ringfs_rewind(struct ringfs *fs);

/**
 * @}
 */

#endif

/* vim: set ts=4 sw=4 et: */
