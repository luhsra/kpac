TARGET = libkpac.so

DEBUG_FLAGS = $(if $(DEBUG), -g -DDEBUG, -O2)

OBJS = libkpac.o kpacd.o proc.o

CFLAGS = -fPIC -Wall -Wextra -Wno-unused $(DEBUG_FLAGS)
LDFLAGS = $(DEBUG_FLAGS)

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CROSS_COMPILE)$(CC) -shared $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CROSS_COMPILE)$(CC) -c $(CFLAGS) -o $@ $^

%.o: %.S
	$(CROSS_COMPILE)$(CC) -c $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET)
