ARCH ?= $(shell uname -m)
TARGET_GCC = $(CROSS_COMPILE)gcc

ASM ?= asm/syscall

PROLOGUE ?= $(ASM)/$(ARCH)/prologue.S
EPILOGUE ?= $(ASM)/$(ARCH)/epilogue.S
ASM_DEP = $(ASM)/common.h
ASM_INC = asm.h

PLUGIN = pac_sw_plugin.so
SRC = main.cc

TESTS = tests

GCC_PLUGINS_DIR = $(shell $(TARGET_GCC) -print-file-name=plugin)

# GCC CPP flags (we need pic and no-rtti)
CXXFLAGS += -g -I$(GCC_PLUGINS_DIR)/include -fPIC -fno-rtti -DDEBUG -Wall -O2

.PHONY: all clean run_tests

all: $(PLUGIN)

# Preprocess assembler files before embedding
$(ASM_INC): $(PROLOGUE) $(EPILOGUE) $(ASM_DEP)
	printf "\
	static const char prologue_s[] = {\n\
	$$(gcc -E $(PROLOGUE) | xxd -i), 0x00\n\
	};\n\n\
	static const char epilogue_s[] = {\n\
	$$(gcc -E $(EPILOGUE) | xxd -i), 0x00\n\
	};" > $@

$(PLUGIN): $(SRC) $(ASM_INC)
	$(CXX) -shared $(CXXFLAGS) $^ -o $@

run_tests:
	$(MAKE) -C $(TESTS) all

clean:
	$(RM) $(PLUGIN) $(ASM_INC)
	$(MAKE) -C $(TESTS) clean
