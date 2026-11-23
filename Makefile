# ────────────────────── CONFIGURATION ───────────────────────
MCU          = atmega328p
F_CPU        = 16000000UL

CC           = avr-gcc
OBJCOPY      = avr-objcopy
AVRDUDE      = avrdude

PROGRAMMER   = arduino
PORT         = /dev/ttyUSB0
BAUD         = 57600

SRCDIR       = src
INCDIR       = include
OBJDIR       = obj
BINDIR       = bin

CFLAGS += -mmcu=$(MCU) \
           -DF_CPU=$(F_CPU) \
           -Os -Wall -Wextra -std=c11 \
           -I$(INCDIR) \
           -MMD -MP \
           -flto

LDFLAGS += -mmcu=$(MCU) -flto

# ────────────────────── END CONFIGURATION ───────────────────
SRCS  := $(wildcard $(SRCDIR)/*.c)
OBJS   = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
DEPS   = $(OBJS:.o=.d)

TARGET_ELF := $(BINDIR)/main.elf
TARGET_HEX := $(BINDIR)/main.hex

.PHONY: all compile flash clean depend

all: compile

compile: $(TARGET_HEX)

$(OBJDIR):
	mkdir -p $@

$(BINDIR):
	mkdir -p $@

$(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_ELF): $(OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(TARGET_HEX): $(TARGET_ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

flash: $(TARGET_HEX)
	$(AVRDUDE) -c $(PROGRAMMER) \
	           -p $(MCU)  -P $(PORT) -b $(BAUD) \
	           -D -U flash:w:$^:i

clean:
	rm -rf $(OBJDIR) $(BINDIR)

-include $(DEPS)
