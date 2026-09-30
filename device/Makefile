CC = avr-gcc
TARGET = test
CLOCK = 20000000
DEVICE = attiny1616
CFLAGS = -Wall -Wextra -Iinclude -Iattiny_example -g -Os -mmcu=$(DEVICE) -DF_CPU=$(CLOCK)
LDFLAGS = -mmcu=$(DEVICE)

HOST = rjp-sao-pi.local

SRC = $(wildcard attiny_example/*.c) $(wildcard src/*.c) src/port/attiny/saod_attiny.c
OBJ = $(SRC:.c=.o)

all: $(TARGET).hex

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.S
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@

$(TARGET).hex: $(TARGET).elf
	avr-objcopy -O ihex $< $@

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).hex

re: clean all

upload: $(TARGET).hex
	scp $< $(HOST):$<
	ssh $(HOST) ./flash.sh $<

reset:
	ssh $(HOST) ./flash.sh

.PHONY: all clean re upload reset
