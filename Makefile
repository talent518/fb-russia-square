CC := gcc
CFLAGS := $(CFLAGS) -Wall -O3
LFLAGS := $(LFLAGS) -lm -pthread

all: fbrussia
	@echo -n

fbrussia: fb.o game.o
	@echo LD $@
	@$(CC) -o $@ $^ $(LFLAGS)

fb.o game.o: fb.h

%.o: %.c
	@echo CC $@
	@$(CC) -o $@ -c $< $(CFLAGS)

clean:
	@echo $@
	@rm -vf *.o fbrussia

