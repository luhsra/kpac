COMMON_SRCS = qarma.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)

TARGETS = pac-sw test

TESTS =
LIBS =

ASAN_FLAGS = $(if $(ASAN), -fsanitize=address -fno-omit-frame-pointer -Wno-format-security,)
DEBUG_FLAGS = $(if $(RELEASE), -O2, -g -DDEBUG)

CFLAGS += $(DEBUG_FLAGS) $(ASAN_FLAGS)
CFLAGS += -Wall -Wextra --std=gnu11 #-Werror
CFLAGS += -D_GNU_SOURCE

LDFLAGS += $(ASAN_FLAGS)
LDFLAGS += $(LIBS)

ALL_SRCS = $(wildcard *.c) $(wildcard */*.c)
ALL_OBJS = $(ALL_SRCS:%.c=%.o)
ALL_DEPS = $(ALL_SRCS:%.c=%.d)

# include tests and targets
ALL_BINS = $(TARGETS) $(TESTS:%=tests/%)

.PHONY: all
all: $(ALL_BINS)

.PHONY: release
release: RELEASE=1
release: all

$(ALL_BINS): %: %.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

.PHONY: clean
clean:
	@$(RM) $(ALL_OBJS) $(ALL_DEPS) $(ALL_BINS)

-include $(ALL_DEPS)
