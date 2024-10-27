CC = gcc
LD = gcc

CFLAGS += -O2
LDFLAGS += -lm

all: test

OBJS=main.o tokenizer.o crc16.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(OBJS)
	$(LD) -o test $^ $(LDFLAGS)

clean:
	rm -rf *.o test

.PHONY: all
