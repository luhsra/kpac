BENCHMARKS := $(shell grep -v '^#' benchmarks.txt)

PLUGIN = ../gcc/pac_sw_plugin.so
PROLOGUE_IN = ../gcc/prologue.s
EPILOGUE_IN = ../gcc/epilogue.s

INST_FLAGS    = -fplugin-arg-pac_sw_plugin-prologue=$(PROLOGUE_IN) \
		-fplugin-arg-pac_sw_plugin-epilogue=$(EPILOGUE_IN)

PLUGIN_FLAGS  = -fplugin=./$(PLUGIN) \
		-fplugin-arg-pac_sw_plugin-init-function=map_device

# Dump inst stats if specified
ifdef DUMP_FILE
PLUGIN_FLAGS += -fplugin-arg-pac_sw_plugin-dump-stats=$(DUMP_FILE)
endif

INIT_OBJ = ../gcc/map_device.o

CFLAGS = -O0

RUNNER = do_bench

BARE = bare
PAC  = pac

BIN_BARE := $(BENCHMARKS:%=%/$(BARE))
BIN_PAC  := $(BENCHMARKS:%=%/$(PAC))

.PHONY: all
all: $(RUNNER) $(BIN_BARE) $(BIN_PAC)

.SECONDEXPANSION:
%/$(BARE): $$(wildcard %/*.c) $$(wildcard %/*.h)
	gcc $*/*.c $(INIT_OBJ) $(CFLAGS) $(PLUGIN_FLAGS) -o $@

.SECONDEXPANSION:
%/$(PAC): $$(wildcard %/*.c) $$(wildcard %/*.h) $(INIT_OBJ)
	gcc $*/*.c $(INIT_OBJ) $(CFLAGS) $(PLUGIN_FLAGS) $(INST_FLAGS) -o $@

$(RUNNER): $(RUNNER).c
	$(CC) $< -O2 -lm -o $@

.PHONY: clean
clean:
	$(RM) $(BIN_BARE) $(BIN_PAC) $(RUNNER) $(DUMP_FILE)
