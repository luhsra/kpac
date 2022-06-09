ARCH ?= $(shell uname -m)
TARGET_GCC = $(CROSS_COMPILE)gcc

PROLOGUE ?= asm/$(ARCH)/prologue.S
EPILOGUE ?= asm/$(ARCH)/epilogue.S
ASM_INC = asm.h

PLUGIN = pac_sw_plugin.so
SRC = main.cc

GCC_PLUGINS_DIR = $(shell $(TARGET_GCC) -print-file-name=plugin)

# GCC CPP flags (we need pic and no-rtti)
CXXFLAGS += -g -I$(GCC_PLUGINS_DIR)/include -fPIC -fno-rtti -DDEBUG -Wall -O2

.PHONY: all clean

all: $(PLUGIN)

# Preprocess assembler files before embedding
$(ASM_INC): $(PROLOGUE) $(EPILOGUE)
	printf "\
	static const char prologue_s[] = {\n\
	$$(gcc -E $(PROLOGUE) | xxd -i), 0x00\n\
	};\n\n\
	static const char epilogue_s[] = {\n\
	$$(gcc -E $(EPILOGUE) | xxd -i), 0x00\n\
	};" > $@

$(PLUGIN): $(SRC) $(ASM_INC)
	$(CXX) -shared $(CXXFLAGS) $^ -o $@

clean:
	$(RM) $(PLUGIN) $(ASM_INC)
