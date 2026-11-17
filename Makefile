CC = gcc
CFLAGS = -Wall -Wextra -Iinc -pthread
LDFLAGS = -pthread

TARGET = bin/sysmond
SRC = src/sysmond.c

all: $(TARGET)

$(TARGET): $(SRC) inc/sysmon.h
	@mkdir -p bin
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -rf bin
