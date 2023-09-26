TARGET = virtpac.so

OBJS = virtpac.o

CFLAGS = -fPIC -Wall -Wextra -Wno-unused -O2 -g
LDFLAGS = -g

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CROSS_COMPILE)$(CC) -shared $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CROSS_COMPILE)$(CC) -c $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET)
