TEST_SRCS := $(wildcard *.c)
TEST_BINS := $(TEST_SRCS:.c=)

PLUGIN_DIR ?= ..
PLUGIN ?= $(PLUGIN_DIR)/pac_sw_plugin.so

LEAF ?=
SCOPE ?=
INIT ?=

PLUGIN_FLAGS = -fplugin=./$(PLUGIN) \
	$(if $(SCOPE), -fplugin-arg-pac_sw_plugin-scope=$(SCOPE)) \
	$(if $(INIT), -fplugin-arg-pac_sw_plugin-init=$(INIT)) \
	$(if $(LEAF), -fplugin-arg-pac_sw_plugin-leaf=$(LEAF))

LDFLAGS = -pthread

ifdef TERM
ESC_RED   := \033[1;31m
ESC_GREEN := \033[1;32m
ESC_RST   := \033[0m
endif

MSG_OK   := "  $(ESC_GREEN)OK\t%s$(ESC_RST)\n"
MSG_FAIL := "  $(ESC_RED)FAIL\t%s ($$?)$(ESC_RST)\n"

.PHONY: all
all: $(TEST_BINS)

.FORCE:

$(PLUGIN): .FORCE
	$(MAKE) -C $(PLUGIN_DIR)

$(TEST_BINS): %: %.c $(INIT_OBJ) $(PLUGIN) .FORCE
	$(CC) $(CFLAGS) $(PLUGIN_FLAGS) -o $@ $< $(INIT_OBJ) $(LDFLAGS)
	@./$@ && printf $(MSG_OK) $* || printf $(MSG_FAIL) $*;

opt: CFLAGS+=-O2

.PHONY: clean
clean:
	$(RM) $(TEST_BINS)
