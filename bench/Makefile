TARGET = bench.so
SRC = $(TARGET:.so=.c)

CFLAGS = -I/usr/include/python3.9 -Wall -Wextra -fPIC

.PHONY: all
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -shared $(CFLAGS) $^ -o $@

.PHONY: clean
clean:
	$(RM) $(TARGET)
