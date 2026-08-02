CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -Iinclude
LDFLAGS ?= -ldl

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
      platform/linux/os.c \
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

OBJ = $(SRC:.c=.o)
TARGET = nexfetch

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: all
	./$(TARGET)

.PHONY: all clean run
