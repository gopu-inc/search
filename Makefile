CC = gcc
CFLAGS = -Wall -Wextra -O2
LDLIBS = -lcurl
TARGET = search

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c $(LDLIBS)

clean:
	rm -f $(TARGET) result.html

.PHONY: all clean
