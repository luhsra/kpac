VARIANTS = kpacd pac-pl
TARGETS = $(VARIANTS:%=libkpac-%.so)

DEBUG_FLAGS = $(if $(DEBUG), -g -DDEBUG, -O2)

OBJS = libkpac.o proc.o

CFLAGS = -fPIC -Wall -Wextra -Wno-unused $(DEBUG_FLAGS)
LDFLAGS = $(DEBUG_FLAGS)

.PHONY: all
all: $(TARGETS)

$(TARGETS): libkpac-%.so: $(OBJS) %.o
	$(CROSS_COMPILE)$(CC) -shared $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CROSS_COMPILE)$(CC) -c $(CFLAGS) -MD -MP -o $@ $<

%.o: %.S
	$(CROSS_COMPILE)$(CC) -c $(CFLAGS) -MD -MP -o $@ $<

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGETS) $(VARIANTS:=.o)

-include $(OBJS:%.o=%.d)
