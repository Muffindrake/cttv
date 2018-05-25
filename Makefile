.PHONY: all clean install

LIBS += jansson ncursesw libcurl glib-2.0
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
CFLAGS += $(shell pkg-config $(LIBS) --cflags)
LDFLAGS ?= -Wl,-O1 -Wl,-flto -Wl,--as-needed
LDFLAGS += $(shell pkg-config $(LIBS) --libs)

PROG = cttv
DIR_BUILD = build
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,$(DIR_BUILD)/%.o,$(SRCS))
DEPS = $(patsubst %.c,$(DIR_BUILD)/%.d,$(SRCS))

OPTLEVEL ?= -O3 -march=native -flto
PREFIX ?= /usr/local

$(info mkdir -p $(DIR_BUILD))
$(shell mkdir -p $(DIR_BUILD))

$(DIR_BUILD)/%.o: %.c
	$(CC) $(OPTLEVEL) $(CFLAGS) $(OPTLEVEL) -o $@ -c -MMD $<

$(PROG): $(OBJS)
	$(CC) $(OPTLEVEL) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

all: $(PROG)

clean:
	$(RM) $(PROG) $(OBJS) $(DEPS)

DESTINATION = $(DESTDIR)$(PREFIX)
install: all
	install -m 0755 $(PROG) $(DESTINATION)/bin
	bzip2 -c $(PROG).1 > $(DESTINATION)/share/man/man1/$(PROG).1.bz2

-include $(DEPS)
