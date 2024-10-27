CC = zcc
TARGET = z80
PRAGMAS = pragma.inc
CFLAGS = -O2 --math32 -vn -clib=new -startup=1 -m
HOSTCC = gcc
SOURCES = boot.c ata.c ext4.c tokenizer.c llm.c crc16.c

all: boot.bin
.PHONY: all

bootloader.bin: bootloader.asm
	z88dk-z80asm -m=z80 -b bootloader.asm
	rm bootloader.o

boot.bin: ${SOURCES} ${PRAGMAS} romtool bootloader.bin
	$(CC) +${TARGET} $(CFLAGS) -pragma-include:${PRAGMAS} ${SOURCES} -o boot -create-app
	./romtool

dump: boot.bin
	z88dk-dis boot.bin -x boot.map -m${TARGET} -o 0 > dump.txt

flash: boot.bin
	brp /dev/ttyUSB0 39SF010 EPROM.bin

eprom: boot.bin
	brp /dev/ttyUSB0 27C1001 EPROM.bin

romtool:
	$(HOSTCC) -o romtool romtool.c

clean:
	rm -f boot.bin romtool bootloader.bin
