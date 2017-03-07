CFLAGS := -Ofast -march=native
DCFLAGS := -Ofast
CFLAGS += `pkg-config jansson ncursesw libcurl libbsd --cflags`
DCFLAGS += `pkg-config jansson ncursesw libcurl libbsd --cflags`
LFLAGS := `pkg-config jansson ncursesw libcurl libbsd --libs`

prefix ?= /usr/local

all: cttv.c
	$(CC) $(CFLAGS) -std=c11 -Wall -Wextra -o cttv cttv.c $(LFLAGS)
	strip -s cttv

dist: cttv.c
	$(CC) $(DCFLAGS) -std=c11 -Wall -Wextra -o cttv cttv.c $(LFLAGS)
	strip -s cttv

install: all
	install -m 0755 cttv $(prefix)/bin

clean:
	$(RM) cttv
