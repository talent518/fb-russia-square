CC := gcc
CFLAGS := $(CFLAGS) -Wall -O3
LFLAGS := $(LFLAGS) -lm -pthread

all: fbrussia

fbrussia: fb.o game.o
	echo LD $@
	@$(CC) $(LFLAGS) -o $@ $^

%.o: %.c
	echo CC $@
	@$(CC) $(CFLAGS) -o $@ -c $^

fb.c game.c: fb.h

clean:
	@echo $@
	@rm -vf *.o fbrussia

