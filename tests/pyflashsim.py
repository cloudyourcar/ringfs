#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import sys
from sharedlibrary import GenericLibrary
from ctypes import *


class libflashsim(GenericLibrary):
    dllname = 'tests/flashsim.so'
    functions = [
        ['flashsim_open', [c_char_p, c_int, c_int], c_void_p],
        ['flashsim_sector_erase', [c_void_p, c_int], None],
        ['flashsim_read', [c_void_p, c_int, c_void_p, c_int], None],
        ['flashsim_program', [c_void_p, c_int, c_void_p, c_int], None],
        ['flashsim_close', [c_void_p], None],
    ]


class FlashSim(object):

    def __init__(self, name, size, sector_size):
        self.libflashsim = libflashsim()
        self.sim = self.libflashsim.flashsim_open(name ,size, sector_size)

    def sector_erase(self, addr):
        self.libflashsim.flashsim_sector_erase(self.sim, addr)

    def read(self, addr, size):
        buf = create_string_buffer(size)
        self.libflashsim.flashsim_read(self.sim, addr, buf, size)
        return buf.raw

    def program(self, addr, data):
        self.libflashsim.flashsim_program(self.sim, addr, data, len(data))

    def __del__(self):
        self.libflashsim.flashsim_close(self.sim)
