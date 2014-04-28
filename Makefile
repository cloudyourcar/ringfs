# Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
# This program is free software. It comes without any warranty, to the extent
# permitted by applicable law. You can redistribute it and/or modify it under
# the terms of the Do What The Fuck You Want To Public License, Version 2, as
# published by Sam Hocevar. See the COPYING file for more details.

CFLAGS = -g -Wall -Wextra -Werror -std=c99 -I.
LDLIBS = -lcheck

all: scan-build test example
	@echo "+++ All good."""

test: tests
	@echo "+++ Running Check test suite..."
	./tests

scan-build: clean
	@echo "+++ Running Clang Static Analyzer..."
	scan-build $(MAKE) tests

docs:
	doxygen

clean:
	$(RM) tests *.o html/ *.sim tags

tests: tests.o ringfs.o flashsim.o
example: example.o ringfs.o flashsim.o
tests.o: tests.c ringfs.h
ringfs.o: ringfs.c ringfs.h
flashsim.o: flashsim.c flashsim.h
example.o: example.c ringfs.h flashsim.h

.PHONY: all test scan-build clean docs
