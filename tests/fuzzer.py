#!/usr/bin/env python
# -*- coding: utf-8 -*-

import random

from pyflashsim import FlashSim
from pyringfs import RingFSFlashPartition, RingFS


class FuzzRun(object):

    def __init__(self, name, version, object_size, sector_size, total_sectors, sector_offset, sector_count):

        sim = FlashSim(name, total_sectors*sector_size, sector_size)

        def op_sector_erase(address):
            sim.sector_erase(address)
            return 0

        def op_program(address, data):
            sim.program(address, data)
            return len(data)

        def op_read(address, size):
            return sim.read(address, size)

        self.version = version
        self.object_size = object_size

        self.flash = RingFSFlashPartition(sector_size, sector_offset, sector_count,
                op_sector_erase, op_program, op_read)
        self.fs = RingFS(self.flash, version, object_size)

    def run(self):

        self.fs.format()

        def do_append():
            self.fs.append('x'*self.object_size)

        def do_fetch():
            self.fs.fetch()

        def do_rewind():
            self.fs.rewind()

        def do_discard():
            self.fs.discard()

        for i in xrange(1000):
            fun = random.choice([do_append]*100 + [do_fetch]*100 + [do_rewind]*10 + [do_discard]*10)
            print i, fun.__name__
            fun()


f = FuzzRun('tests/fuzzer.sim', 0x42, 16, 1024, 16, 0, 16)
f.run()
