/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <check.h>

#include "ringfs.h"
#include "flashsim.h"

START_TEST(test_flashsim)
{
    printf("# test_flashsim\n");

    struct flashsim *sim = flashsim_open("test.sim", 1024, 16);
    uint8_t buf[48];
    uint8_t data[16];

    flashsim_sector_erase(sim, 0);
    flashsim_sector_erase(sim, 16);
    flashsim_sector_erase(sim, 32);

    memset(data, 0x5a, 16);
    flashsim_program(sim, 16, data, 16);

    flashsim_read(sim, 0, buf, 48);
    for (int i=0; i<16; i++)
        ck_assert_int_eq(buf[i], 0xff);
    for (int i=16; i<32; i++)
        ck_assert_int_eq(buf[i], 0x5a);
    for (int i=32; i<48; i++)
        ck_assert_int_eq(buf[i], 0xff);

    memset(data, 0x01, 16);
    flashsim_program(sim, 0, data, 16);
    memset(data, 0x10, 16);
    flashsim_program(sim, 32, data, 16);
    flashsim_sector_erase(sim, 16);

    flashsim_read(sim, 0, buf, 48);
    for (int i=0; i<16; i++)
        ck_assert_int_eq(buf[i], 0x01);
    for (int i=16; i<32; i++)
        ck_assert_int_eq(buf[i], 0xff);
    for (int i=32; i<48; i++)
        ck_assert_int_eq(buf[i], 0x10);

    free(sim);
}
END_TEST

static struct flashsim *sim;

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

static const struct ringfs_flash_partition flash = {
    .sector_size = 65536,
    .sector_offset = 3,
    .sector_count = 13,

    .sector_erase = op_sector_erase,
    .program = op_program,
    .read = op_read,
};

START_TEST(test_ringfs_format)
{
    printf("# test_ringfs_format\n");
    sim = flashsim_open("ringfs_format.sim", 65536*16, 65536);

    struct ringfs fs1;
    printf("## ringfs_init()\n");
    ringfs_init(&fs1, &flash, 0x00000042, 4);
    printf("## ringfs_format()\n");
    ringfs_format(&fs1);
}
END_TEST

START_TEST(test_ringfs_scan)
{
    printf("# test_ringfs_scan\n");
    sim = flashsim_open("ringfs_scan.sim", 65536*16, 65536);

    /* first format a filesystem */
    struct ringfs fs1;
    printf("## ringfs_init()\n");
    ringfs_init(&fs1, &flash, 0x00000042, 4);
    printf("## ringfs_format()\n");
    ringfs_format(&fs1);

    /* now try to scan it */
    struct ringfs fs2;
    printf("## ringfs_init()\n");
    ringfs_init(&fs2, &flash, 0x00000042, 4);
    printf("## ringfs_scan()\n");
    ck_assert(ringfs_scan(&fs2) == 0);

    /* this is an empty FS, should start with this: */
    ck_assert_int_eq(fs2.slots_per_sector, (65536-8)/(4+4));
    ck_assert_int_eq(fs2.read.sector, 0);
    ck_assert_int_eq(fs2.read.slot, 0);
    ck_assert_int_eq(fs2.write.sector, 0);
    ck_assert_int_eq(fs2.write.slot, 0);
    ck_assert_int_eq(fs2.cursor.sector, 0);
    ck_assert_int_eq(fs2.cursor.slot, 0);

    /* now insert some objects */
    ck_assert(ringfs_append(&fs2, (int[]) { 0x11 }) == 0);
    ck_assert(ringfs_append(&fs2, (int[]) { 0x22 }) == 0);
    ck_assert(ringfs_append(&fs2, (int[]) { 0x33 }) == 0);

    /* rescan */
    printf("## ringfs_scan()\n");
    ck_assert(ringfs_scan(&fs2) == 0);

    /* make sure the objects are there */
    ck_assert(ringfs_count_exact(&fs2) == 3);

    /* scan should fail if we supply a different version */
    struct ringfs fs3;
    printf("## ringfs_init()\n");
    ringfs_init(&fs3, &flash, 0x00000043, 4);
    printf("## ringfs_scan()\n");
    ck_assert(ringfs_scan(&fs3) != 0);
}
END_TEST

START_TEST(test_ringfs_append)
{
    printf("# test_ringfs_append\n");
    sim = flashsim_open("ringfs_append.sim", 65536*16, 65536);

    /* first format a filesystem */
    struct ringfs fs;
    printf("## ringfs_init()\n");
    ringfs_init(&fs, &flash, 0x00000042, 4);
    printf("## ringfs_format()\n");
    ringfs_format(&fs);

    /* now we're brave and we write some data */
    for (int i=0; i<3; i++) {
        printf("## ringfs_append()\n");
        ringfs_append(&fs, (int[]) { 0x11*(i+1) });

        /* make sure the write head has advanced */
        ck_assert_int_eq(fs.write.slot, i+1);
    }

    /* now we fetch at it. */
    int obj;
    for (int i=0; i<3; i++) {
        printf("## ringfs_fetch()\n");
        ck_assert(ringfs_fetch(&fs, &obj) == 0);
        ck_assert_int_eq(obj, 0x11*(i+1));

        /* make sure the cursor head has advanced */
        ck_assert_int_eq(fs.cursor.slot, i+1);
    }
    /* there should be no data left */
    ck_assert(ringfs_fetch(&fs, &obj) < 0);

    /* test the rewind. */
    ck_assert(ringfs_rewind(&fs) == 0);
    ck_assert_int_eq(fs.cursor.slot, 0);

    /* try to read the objects once again. */
    for (int i=0; i<3; i++) {
        printf("## ringfs_fetch()\n");
        ck_assert(ringfs_fetch(&fs, &obj) == 0);
        ck_assert_int_eq(obj, 0x11*(i+1));
    }
}
END_TEST

START_TEST(test_ringfs_discard)
{
    printf("# test_ringfs_discard\n");
    sim = flashsim_open("ringfs_append.sim", 65536*16, 65536);

    struct ringfs fs;
    printf("## ringfs_init()\n");
    ringfs_init(&fs, &flash, 0x00000042, 4);
    printf("## ringfs_format()\n");
    ringfs_format(&fs);

    /* write some records */
    for (int i=0; i<4; i++) {
        printf("## ringfs_append()\n");
        ringfs_append(&fs, (int[]) { 0x11*(i+1) });
    }
    /* read some of them */
    int obj;
    for (int i=0; i<2; i++) {
        printf("## ringfs_fetch()\n");
        ck_assert(ringfs_fetch(&fs, &obj) == 0);
        ck_assert_int_eq(obj, 0x11*(i+1));
    }
    /* discard whatever was read */
    ck_assert(ringfs_discard(&fs) == 0);
    /* make sure we're consistent */
    ck_assert_int_eq(fs.read.slot, 2);
    ck_assert_int_eq(fs.cursor.slot, 2);
    ck_assert_int_eq(fs.write.slot, 4);

    /* read the rest of the records */
    for (int i=2; i<4; i++) {
        printf("## ringfs_fetch()\n");
        ck_assert(ringfs_fetch(&fs, &obj) == 0);
        ck_assert_int_eq(obj, 0x11*(i+1));
    }
    /* discard them */
    ck_assert(ringfs_discard(&fs) == 0);
    ck_assert_int_eq(fs.read.slot, 4);
    ck_assert_int_eq(fs.cursor.slot, 4);
    ck_assert_int_eq(fs.write.slot, 4);
}
END_TEST

Suite *ringfs_suite(void)
{
    Suite *s = suite_create ("ringfs");
    TCase *tc;

    tc = tcase_create("flashsim");
    tcase_add_test(tc, test_flashsim);
    suite_add_tcase(s, tc);

    tc = tcase_create("ringfs");
    tcase_add_test(tc, test_ringfs_format);
    tcase_add_test(tc, test_ringfs_scan);
    tcase_add_test(tc, test_ringfs_append);
    tcase_add_test(tc, test_ringfs_discard);
    suite_add_tcase(s, tc);

    return s;
}

int main()
{
    int number_failed;
    Suite *s = ringfs_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set ts=4 sw=4 et: */
