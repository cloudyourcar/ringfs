#!/usr/bin/env python
# -*- coding: utf-8 -*-

from pyflashsim import FlashSim
from pyringfs import RingFSFlashPartition, RingFS

def op_sector_erase(address):
    sim.sector_erase(address)
    return 0

def op_program(address, data):
    sim.program(address, data)
    return len(data)

def op_read(address, size):
    return sim.read(address, size)

sim = FlashSim('tests/fuzzer.sim', 16*1024, 1024)
flash = RingFSFlashPartition(1024, 0, 16,
        op_sector_erase, op_program, op_read)
fs = RingFS(flash, 0x42, 16)
