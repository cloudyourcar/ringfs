# Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
# This program is free software. It comes without any warranty, to the extent
# permitted by applicable law. You can redistribute it and/or modify it under
# the terms of the Do What The Fuck You Want To Public License, Version 2, as
# published by Sam Hocevar. See the COPYING file for more details.

CFLAGS = -g -Wall -Wextra -Werror -std=c99 -I. -Itests
CFLAGS += -D_POSIX_C_SOURCE=200112L
CFLAGS += -fPIC # needed due to our shared library shenanigans
LDLIBS = -lcheck -lm -lpthread -lrt

all: scan-build test example
	@echo "+++ All good."""

test: unit fuzz

unit: tests/tests
	@echo "+++ Running Check test suite..."
	tests/tests

fuzz: ringfs.so tests/flashsim.so tests/fuzzer.py
	@echo "+++ Running fuzzer..."
	tests/fuzzer.py

scan-build: clean
	@echo "+++ Running Clang Static Analyzer..."
	scan-build $(MAKE) tests

docs:
	doxygen

clean:
	$(RM) *.o tests/*.o tests/tests html/ *.sim tags example

%.so: %.o
	$(LINK.o) -shared $^ $(LOADLIBES) $(LDLIBS) -o $@

ringfs.o: ringfs.c ringfs.h

example: example.o ringfs.o tests/flashsim.o
example.o: example.c ringfs.h tests/flashsim.h

tests/tests: ringfs.o tests/tests.o tests/flashsim.o
tests/tests.o: tests/tests.c ringfs.h

tests/tests: ringfs.o tests/tests.o tests/flashsim.o
tests/tests.o: tests/tests.c ringfs.h
tests/flashsim.o: tests/flashsim.c tests/flashsim.h

ringfs.so: ringfs.o
tests/flashsim.so: tests/flashsim.o

.PHONY: all test unit fuzz scan-build clean docs
