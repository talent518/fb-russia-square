CC := gcc
CFLAGS := $(CFLAGS) -Wall -O3
LFLAGS := $(LFLAGS) -lm -pthread

all: fbrussia fbtest
	@echo -n

fbrussia: fb.o game.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

fbtest: fb.o test.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

fb.o game.o test.o: fb.h

fb.o: font_08x14.h font_10x18.h font_12x22.h font_18x32.h

%.o: %.c
	@echo CC $@
	@$(CC) -o $@ -c $< $(CFLAGS)

clean:
	@echo $@
	@rm -vf *.o fbrussia fbtest

