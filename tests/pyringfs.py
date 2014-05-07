#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import sys
from sharedlibrary import GenericLibrary
from ctypes import *


op_sector_erase_t = CFUNCTYPE(c_int, c_int)
op_program_t = CFUNCTYPE(c_ssize_t, c_int, c_void_p, c_size_t)
op_read_t = CFUNCTYPE(c_ssize_t, c_int, c_void_p, c_size_t)


class StructRingFSFlashPartition(Structure):
    _fields_ = [
        ('sector_size', c_int),
        ('sector_offset', c_int),
        ('sector_count', c_int),

        ('sector_erase', op_sector_erase_t),
        ('program', op_program_t),
        ('read', op_read_t),
    ]

class StructRingFSLoc(Structure):
    _fields_ = [
        ('sector', c_int),
        ('slot', c_int),
    ]

class StructRingFS(Structure):
    _fields_ = [
        ('flash', POINTER(StructRingFSFlashPartition)),
        ('version', c_uint32),
        ('object_size', c_int),
        ('slots_per_sector', c_int),

        ('read', StructRingFSLoc),
        ('write', StructRingFSLoc),
        ('cursor', StructRingFSLoc),
    ]


class libringfs(GenericLibrary):
    dllname = 'ringfs.so'
    functions = [
        ['ringfs_init', [POINTER(StructRingFS), POINTER(StructRingFSFlashPartition), c_uint32, c_int], c_int],
        ['ringfs_format', [POINTER(StructRingFS)], c_int],
        ['ringfs_scan', [POINTER(StructRingFS)], c_int],
        ['ringfs_capacity', [POINTER(StructRingFS)], c_int],
        ['ringfs_count_estimate', [POINTER(StructRingFS)], c_int],
        ['ringfs_count_exact', [POINTER(StructRingFS)], c_int],
        ['ringfs_append', [POINTER(StructRingFS), c_void_p], c_int],
        ['ringfs_fetch', [POINTER(StructRingFS), c_void_p], c_int],
        ['ringfs_discard', [POINTER(StructRingFS)], c_int],
        ['ringfs_rewind', [POINTER(StructRingFS)], c_int],
        ['ringfs_dump', [POINTER(StructRingFS)], None],
    ]


class RingFSFlashPartition(object):

    def __init__(self, sector_size, sector_offset, sector_count, sector_erase, program, read):

        def op_sector_erase(address):
            sector_erase(address)
            return 0

        def op_program(address, data, size):
            program(address, string_at(data, size))
            return size

        def op_read(address, buf, size):
            data = read(address, size)
            memmove(buf, data, size)
            return size

        self.struct = StructRingFSFlashPartition(sector_size, sector_offset, sector_count,
                op_sector_erase_t(op_sector_erase),
                op_program_t(op_program),
                op_read_t(op_read))


class RingFS(object):

    def __init__(self, flash, version, object_size):
        self.libringfs = libringfs()
        self.ringfs = StructRingFS()
        self.flash = flash.struct
        self.libringfs.ringfs_init(byref(self.ringfs), byref(self.flash), version, object_size)

    def format(self):
        self.libringfs.ringfs_format(byref(self.ringfs))

    def append(self, obj):
        self.libringfs.ringfs_append(byref(self.ringfs), obj)

__all__ = [
    'StructRingFSFlashPartition',
    'RingFS',
]
