DCFLAGS += `pkg-config jansson ncursesw libcurl libbsd --cflags` \
	-std=c11 -Wall -Wextra -Wpedantic -O2
CFLAGS += $(DCFLAGS) -march=native
LFLAGS += `pkg-config jansson ncursesw libcurl libbsd --libs`

prefix ?= /usr/local

all: cttv.c
	$(CC) $(CFLAGS) -o cttv cttv.c $(LFLAGS)
	strip -s cttv

dist: cttv.c
	$(CC) $(DCFLAGS) -o cttv cttv.c $(LFLAGS)
	strip -s cttv

install: all
	install -m 0755 cttv $(prefix)/bin

clean:
	$(RM) cttv
