CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2
# Always ensure -Iinclude is present, even when CFLAGS is overridden by dpkg-buildpackage
CFLAGS += -Iinclude

ifeq ($(OS),Windows_NT)
    TARGET = nexfetch.exe
    LDFLAGS ?=
    PLATFORM_SRC = platform/windows/platform_windows.c
else
    UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
    ifeq ($(UNAME_S),Darwin)
        TARGET = nexfetch
        LDFLAGS ?=
        PLATFORM_SRC = platform/macos/platform_macos.c
    else ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
        TARGET = nexfetch.exe
        LDFLAGS ?=
        PLATFORM_SRC = platform/windows/platform_windows.c
    else ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
        TARGET = nexfetch.exe
        LDFLAGS ?=
        PLATFORM_SRC = platform/windows/platform_windows.c
    else ifeq ($(findstring CYGWIN,$(UNAME_S)),CYGWIN)
        TARGET = nexfetch.exe
        LDFLAGS ?=
        PLATFORM_SRC = platform/windows/platform_windows.c
    else
        TARGET = nexfetch
        LDFLAGS ?= -ldl
        PLATFORM_SRC = platform/linux/os.c \
                       platform/linux/kernel.c \
                       platform/linux/host.c \
                       platform/linux/uptime.c \
                       platform/linux/packages.c \
                       platform/linux/shell.c \
                       platform/linux/de.c \
                       platform/linux/cpu.c \
                       platform/linux/gpu.c \
                       platform/linux/memory.c \
                       platform/linux/disk.c \
                       platform/linux/battery.c \
                       platform/linux/network.c \
                       platform/linux/locale.c \
                       platform/linux/swap.c \
                       platform/linux/display.c
    endif
endif

# Plugin extension depends on platform
ifeq ($(OS),Windows_NT)
    PLUGIN_EXT = dll
else ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
    PLUGIN_EXT = dll
else ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
    PLUGIN_EXT = dll
else ifeq ($(findstring CYGWIN,$(UNAME_S)),CYGWIN)
    PLUGIN_EXT = dll
else
    PLUGIN_EXT = so
endif

PLUGIN_SRC = $(wildcard plugins/*.c)
PLUGIN_TARGETS = $(patsubst plugins/%.c, plugins/%.$(PLUGIN_EXT), $(PLUGIN_SRC))

SRC = src/main.c \
      src/util.c \
      src/config.c \
      src/module_manager.c \
      src/presenter.c \
      modules/os.c \
      modules/kernel.c \
      modules/host.c \
      modules/uptime.c \
      modules/packages.c \
      modules/shell.c \
      modules/de.c \
      modules/cpu.c \
      modules/gpu.c \
      modules/memory.c \
      modules/disk.c \
      modules/battery.c \
      modules/network.c \
      modules/color.c \
      modules/ansi.c \
      modules/custom.c \
      modules/logo.c \
      modules/locale.c \
      modules/swap.c \
      modules/display.c \
      $(PLATFORM_SRC)

OBJ = $(SRC:.c=.o)

all: $(TARGET) $(PLUGIN_TARGETS)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -rdynamic -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

plugins/%.so: plugins/%.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

plugins/%.dll: plugins/%.c
	$(CC) $(CFLAGS) -shared -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET) nexfetch.exe platform/*/*.o plugins/*.so plugins/*.dll

run: all
	./$(TARGET)

# Installation directories (overridable for packaging)
PREFIX  ?= /usr
BINDIR  ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/nexfetch
SYSCONFDIR ?= /etc/nexfetch

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(DATADIR)/logos
	install -d $(DESTDIR)$(DATADIR)/config
	install -d $(DESTDIR)$(DATADIR)/plugins
	install -d $(DESTDIR)$(SYSCONFDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	cp -r logos/* $(DESTDIR)$(DATADIR)/logos/
	cp config/config.json $(DESTDIR)$(DATADIR)/config/
	cp plugins/*.$(PLUGIN_EXT) $(DESTDIR)$(DATADIR)/plugins/ 2>/dev/null || true
	if [ ! -f $(DESTDIR)$(SYSCONFDIR)/config.json ]; then \
		cp config/config.json $(DESTDIR)$(SYSCONFDIR)/config.json; \
	fi

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -rf $(DESTDIR)$(DATADIR)
	rm -f $(DESTDIR)$(SYSCONFDIR)/config.json
	rmdir $(DESTDIR)$(SYSCONFDIR) 2>/dev/null || true

.PHONY: all clean run install uninstall