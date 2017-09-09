CFLAGS := $(CFLAGS) `pkg-config jansson ncursesw libcurl libbsd --cflags` \
	   -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS := $(LDFLAGS) `pkg-config jansson ncursesw libcurl libbsd --libs`

OPTLEVEL ?= -O3 -march=native
prefix ?= /usr/local

all: cttv.c
	$(CC) $(CFLAGS) $(OPTLEVEL) -o cttv cttv.c $(LDFLAGS)
	strip -s cttv

install: all
	install -m 0755 cttv $(prefix)/bin

clean:
	$(RM) cttv
