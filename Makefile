CC = gcc
CFLAGS = -Wall -g

TARGETS = bin/sysmond bin/mem_sensor bin/cpu_sensor bin/proc_sensor

all: $(TARGETS)

bin/%: src/%.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf bin/*
