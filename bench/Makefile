TARGET = timing.so
SRC = $(TARGET:.so=.c)

CFLAGS = -I/usr/include/python3.11 -Wall -Wextra -fPIC

.PHONY: all
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -shared $(CFLAGS) $^ -o $@

.PHONY: clean
clean:
	$(RM) $(TARGET)
