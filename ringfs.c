/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

/**
 * @defgroup ringfs_impl RingFS implementation
 * @details
 *
 * @{
 */

#include <ringfs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>


/**
 * @defgroup sector
 * @{
 */

enum sector_status {
    SECTOR_ERASED     = 0xFFFFFFFF, /**< Default state after NOR flash erase. */
    SECTOR_FREE       = 0xFFFFFF00, /**< Sector erased. */
    SECTOR_IN_USE     = 0xFFFF0000, /**< Sector contains valid data. */
    SECTOR_ERASING    = 0xFF000000, /**< Sector should be erased. */
    SECTOR_FORMATTING = 0x00000000, /**< The entire partition is being formatted. */
};

struct sector_header {
    uint32_t status;
    uint32_t version;
};

static int _sector_address(struct ringfs *fs, int sector_offset)
{
    return (fs->flash->sector_offset + sector_offset) * fs->flash->sector_size;
}

static int _sector_get_status(struct ringfs *fs, int sector, uint32_t *status)
{
    return fs->flash->read(_sector_address(fs, sector) + offsetof(struct sector_header, status),
            status, sizeof(*status));
}

static int _sector_set_status(struct ringfs *fs, int sector, uint32_t status)
{
    return fs->flash->program(_sector_address(fs, sector) + offsetof(struct sector_header, status),
            &status, sizeof(status));
}

static int _sector_free(struct ringfs *fs, int sector)
{
    int sector_addr = _sector_address(fs, sector);
    _sector_set_status(fs, sector, SECTOR_ERASING);
    fs->flash->sector_erase(sector_addr);
    fs->flash->program(sector_addr + offsetof(struct sector_header, version), &fs->version, sizeof(fs->version));
    _sector_set_status(fs, sector, SECTOR_FREE);
    return 0;
}

/**
 * @}
 * @defgroup slot
 * @{
 */

enum slot_status {
    SLOT_ERASED   = 0xFFFFFFFF, /**< Default state after NOR flash erase. */
    SLOT_RESERVED = 0xFFFFFF00, /**< Write started but not yet committed. */
    SLOT_VALID    = 0xFFFF0000, /**< Write committed, slot contains valid data. */
    SLOT_GARBAGE  = 0xFF000000, /**< Slot contents discarded and no longer valid. */
};

struct slot_header {
    uint32_t status;
};

static int _slot_address(struct ringfs *fs, struct ringfs_loc *loc)
{
    return _sector_address(fs, loc->sector) +
           sizeof(struct sector_header) +
           (sizeof(struct slot_header) + fs->object_size) * loc->slot;
}

static int _slot_get_status(struct ringfs *fs, struct ringfs_loc *loc, uint32_t *status)
{
    return fs->flash->read(_slot_address(fs, loc) + offsetof(struct slot_header, status),
            status, sizeof(*status));
}

static int _slot_set_status(struct ringfs *fs, struct ringfs_loc *loc, uint32_t status)
{
    return fs->flash->program(_slot_address(fs, loc) + offsetof(struct slot_header, status),
            &status, sizeof(status));
}

/**
 * @}
 * @defgroup loc
 * @{
 */

static bool _loc_equal(struct ringfs_loc *a, struct ringfs_loc *b)
{
    return (a->sector == b->sector) && (a->slot == b->slot);
}

/** Advance a location to the beginning of the next sector. */
static void _loc_advance_sector(struct ringfs *fs, struct ringfs_loc *loc)
{
    loc->slot = 0;
    loc->sector++;
    if (loc->sector >= fs->flash->sector_count)
        loc->sector = 0;
}

/** Advance a location to the next slot, advancing the sector too if needed. */
static void _loc_advance_slot(struct ringfs *fs, struct ringfs_loc *loc)
{
    loc->slot++;
    if (loc->slot >= fs->slots_per_sector)
        _loc_advance_sector(fs, loc);
}

/**
 * @}
 */

/* And here we go. */

int ringfs_init(struct ringfs *fs, const struct ringfs_flash_partition *flash, uint32_t version, int object_size)
{
    /* Copy arguments to instance. */
    fs->flash = flash;
    fs->version = version;
    fs->object_size = object_size;

    /* Precalculate commonly used values. */
    fs->slots_per_sector = (fs->flash->sector_size - sizeof(struct sector_header)) /
                           (sizeof(struct slot_header) + fs->object_size);

    return 0;
}

int ringfs_format(struct ringfs *fs)
{
    /* Mark all sectors to prevent half-erased filesystems. */
    for (int sector=0; sector<fs->flash->sector_count; sector++)
        _sector_set_status(fs, sector, SECTOR_FORMATTING);

    /* Erase, update version, mark as free. */
    for (int sector=0; sector<fs->flash->sector_count; sector++)
        _sector_free(fs, sector);

    /* Start reading & writing at the first sector. */
    fs->read.sector = 0;
    fs->read.slot = 0;
    fs->write.sector = 0;
    fs->write.slot = 0;
    fs->cursor.sector = 0;
    fs->cursor.slot = 0;

    return 0;
}

int ringfs_scan(struct ringfs *fs)
{
    uint32_t previous_sector_status = SECTOR_FREE;
    /* The read sector is the first IN_USE sector *after* a FREE sector
     * (or the first one). */
    int read_sector = 0;
    /* The write sector is the last IN_USE sector *before* a FREE sector
     * (or the last one). */
    int write_sector = fs->flash->sector_count - 1;
    /* There must be at least one FREE sector available at all times. */
    bool free_seen = false;
    /* If there's no IN_USE sector, we start at the first one. */
    bool used_seen = false;

    /* Iterate over sectors. */
    for (int sector=0; sector<fs->flash->sector_count; sector++) {
        int addr = _sector_address(fs, sector);

        /* Read sector header. */
        struct sector_header header;
        fs->flash->read(addr, &header, sizeof(header));

        /* Detect partially-formatted partitions. */
        if (header.status == SECTOR_FORMATTING) {
            printf("ringfs_scan: partially formatted partition\r\n");
            return -1;
        }

        /* Detect and fix partially erased sectors. */
        if (header.status == SECTOR_ERASING || header.status == SECTOR_ERASED) {
            _sector_free(fs, addr);
            header.status = SECTOR_FREE;
        }

        /* Detect corrupted sectors. */
        if (header.status != SECTOR_FREE && header.status != SECTOR_IN_USE) {
            printf("ringfs_scan: corrupted sector %d\r\n", sector);
            return -1;
        }

        /* Detect obsolete versions. We can't do this earlier because the version
         * could have been invalid due to a partial erase. */
        if (header.version != fs->version) {
            printf("ringfs_scan: incompatible version 0x%08x\r\n", header.version);
            return -1;
        }

        /* Record the presence of a FREE sector. */
        if (header.status == SECTOR_FREE)
            free_seen = true;

        /* Update read & write sectors according to the above rules. */
        if (header.status == SECTOR_IN_USE && previous_sector_status == SECTOR_FREE)
            read_sector = sector;
        if (header.status == SECTOR_FREE && previous_sector_status == SECTOR_IN_USE)
            write_sector = sector-1;

        previous_sector_status = header.status;
    }

    /* Detect the lack of a FREE sector. */
    if (!free_seen) {
        printf("ringfs_scan: invariant violated: no FREE sector found\r\n");
        return -1;
    }

    /* Start writing at the first sector if the filesystem is empty. */
    if (!used_seen) {
        write_sector = 0;
    }

    /* Scan the write sector and skip all occupied slots at the beginning. */
    fs->write.sector = write_sector;
    fs->write.slot = 0;
    while (fs->write.sector == write_sector) {
        uint32_t status;
        _slot_get_status(fs, &fs->write, &status);
        if (status == SLOT_ERASED)
            break;

        _loc_advance_slot(fs, &fs->write);
    }
    /* If the sector was full, we're at the beginning of a FREE sector now. */

    /* Position the read head at the start of the first IN_USE sector, then skip
     * over garbage/invalid slots until something of value is found or we reach
     * the write head which means there's no data. */
    fs->read.sector = read_sector;
    fs->read.slot = 0;
    while (!_loc_equal(&fs->read, &fs->write)) {
        uint32_t status;
        _slot_get_status(fs, &fs->read, &status);
        if (status == SLOT_ERASED)
            break;

        _loc_advance_slot(fs, &fs->read);
    }

    /* Move the read cursor to the read head position. */
    fs->cursor = fs->read;

    return 0;
}

int ringfs_count_exact(struct ringfs *fs)
{
    int count = 0;

    /* Use a temporary loc for iteration. */
    struct ringfs_loc loc = fs->read;
    while (!_loc_equal(&loc, &fs->write)) {
        uint32_t status;
        _slot_get_status(fs, &loc, &status);
        
        if (status == SLOT_VALID)
            count++;

        _loc_advance_slot(fs, &loc);
    }

    return count;
}

int ringfs_append(struct ringfs *fs, const void *object)
{
    uint32_t status;

    /*
     * There are three sectors involved in appending a value:
     * - the sector where the append happens: it has to be writable
     * - the next sector: it must be free (invariant)
     * - the next-next sector: read & cursor heads are moved there if needed
     */

    /* Make sure the next sector is free. */
    int next_sector = (fs->write.sector+1) % fs->flash->sector_count;
    _sector_get_status(fs, next_sector, &status);
    if (status != SECTOR_FREE) {
        /* Next sector must be freed. But first... */

        /* Move the read & cursor heads out of the way. */
        if (fs->read.sector == next_sector)
            _loc_advance_sector(fs, &fs->read);
        if (fs->cursor.sector == next_sector)
            _loc_advance_sector(fs, &fs->cursor);

        /* Free the next sector. */
        _sector_free(fs, next_sector);
    }

    /* Now we can make sure the current write sector is writable. */
    _sector_get_status(fs, fs->write.sector, &status);
    if (status == SECTOR_FREE) {
        /* Free sector. Mark as used. */
        _sector_set_status(fs, fs->write.sector, SECTOR_IN_USE);
    } else if (status != SECTOR_IN_USE) {
        printf("ringfs_append: corrupted filesystem\r\n");
        return -1;
    }

    /* Preallocate slot. */
    _slot_set_status(fs, &fs->write, SLOT_RESERVED);

    /* Write object. */
    fs->flash->program(_slot_address(fs, &fs->write) + sizeof(struct slot_header),
            object, fs->object_size);

    /* Commit write. */
    _slot_set_status(fs, &fs->write, SLOT_VALID);

    /* Advance the write head. */
    _loc_advance_slot(fs, &fs->write);

    return 0;
}

int ringfs_fetch(struct ringfs *fs, void *object)
{
    /* Advance forward in search of a valid slot. */
    while (!_loc_equal(&fs->cursor, &fs->write)) {
        uint32_t status;

        _slot_get_status(fs, &fs->cursor, &status);

        if (status == SLOT_VALID) {
            fs->flash->read(_slot_address(fs, &fs->cursor) + sizeof(struct slot_header),
                    object, fs->object_size);
            _loc_advance_slot(fs, &fs->cursor);
            return 0;
        }

        _loc_advance_slot(fs, &fs->cursor);
    }

    return -1;
}

int ringfs_discard(struct ringfs *fs)
{
    while (!_loc_equal(&fs->read, &fs->cursor)) {
        _slot_set_status(fs, &fs->read, SLOT_GARBAGE);
        _loc_advance_slot(fs, &fs->read);
    }

    return 0;
}

int ringfs_rewind(struct ringfs *fs)
{
    fs->cursor = fs->read;
    return 0;
}

/**
 * @}
 */

/* vim: set ts=4 sw=4 et: */
