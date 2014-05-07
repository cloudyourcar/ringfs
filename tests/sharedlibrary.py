#!/usr/bin/env python
# -*- coding: utf-8 -*-

from ctypes import CDLL

class GenericLibrary(CDLL):

    def __init__(self, *args, **kwargs):
        super(GenericLibrary, self).__init__(self.dllname, *args, **kwargs)

        for name, argtypes, restype in self.functions:
            getattr(self, name).argtypes = argtypes
            getattr(self, name).restype = restype
