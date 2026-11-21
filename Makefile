CC = gcc
CFLAGS = -Wall -Wextra -Iinc

BINS = bin/sysmond bin/mem_sensor bin/cpu_sensor bin/proc_sensor

all: $(BINS)

bin/%: src/%.c inc/sysmon_ipc.h
	@mkdir -p bin
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf bin /tmp/sysmon_*.fifo
