TARGET = latency

DEBUG_FLAGS = $(if $(DEBUG), -g -DDEBUG, -O2)

OBJS = latency.o

CFLAGS = -fPIC -Wall -Wextra -Wno-unused $(DEBUG_FLAGS)
LDFLAGS = $(DEBUG_FLAGS) -lm

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CROSS_COMPILE)$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CROSS_COMPILE)$(CC) -c $(CFLAGS) -MD -MP -o $@ $<

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET)

-include $(OBJS:%.o=%.d)
