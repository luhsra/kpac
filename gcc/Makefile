ARCH ?= $(shell uname -m)
TARGET_GCC = $(CROSS_COMPILE)gcc

PLUGIN = pac_sw_plugin.so
SRC = main.cc

PROLOGUE ?= asm/$(ARCH)/prologue.S
EPILOGUE ?= asm/$(ARCH)/epilogue.S

ASM_INC = asm.h

TEST = test

SIGN_SCOPE ?= std

INIT_OBJ = #../sw/map_device.o
INIT_FUNC ?= $(basename $(notdir $(INIT_OBJ)))

PLUGIN_ARGS := -fplugin=./$(PLUGIN)
ifneq ($(strip $(INIT_FUNC)),)
PLUGIN_ARGS += -fplugin-arg-pac_sw_plugin-init-func=$(INIT_FUNC)
endif
ifneq ($(strip $(SIGN_SCOPE)),)
PLUGIN_ARGS += -fplugin-arg-pac_sw_plugin-sign-scope=$(SIGN_SCOPE)
endif

# Plugins directory of GCC
GCC_PLUGINS_DIR = $(shell $(TARGET_GCC) -print-file-name=plugin)

# GCC CPP flags (we need pic and no-rtti)
CXXFLAGS += -g -I$(GCC_PLUGINS_DIR)/include -fPIC -fno-rtti -DDEBUG -Wall -O2

.PHONY: all clean

all: $(PLUGIN) $(TEST)

# Preprocess assembler files before embedding
$(ASM_INC): $(PROLOGUE) $(EPILOGUE)
	echo "\
	static const char prologue_s[] = {\n\
	$$(gcc -E $(PROLOGUE) | xxd -i), 0x00\n\
	};\n\n\
	static const char epilogue_s[] = {\n\
	$$(gcc -E $(EPILOGUE) | xxd -i), 0x00\n\
	};" > $@

$(PLUGIN): $(SRC) $(ASM_INC)
	$(CXX) -shared $(CXXFLAGS) $^ -o $@

$(TEST): $(TEST).c $(INIT_OBJ) $(PLUGIN)
	$(TARGET_GCC) $< $(INIT_OBJ) $(PLUGIN_ARGS) -g -O0 -o $@

$(INIT_OBJ): $(INIT_OBJ:%.o=%.c)
	$(TARGET_GCC) $< -c -o $@

# testnostd: gcc_pac_plugin.so
# 	$(TARGET_GCC) -E prologue.S > prologue.S.proc
# 	$(TARGET_GCC) -E epilogue.S > epilogue.S.proc
# 	$(TARGET_GCC) mpeg2.c memmapper.c \
# 	-fplugin=$(shell pwd)/gcc_pac_plugin.so \
# 	-fplugin-arg-gcc_pac_plugin-needpa=y \
# 	-fplugin-arg-gcc_pac_plugin-memmap=y \
# 	-fplugin-arg-gcc_pac_plugin-prologue=prologue.S.proc \
# 	-fplugin-arg-gcc_pac_plugin-epilogue=epilogue.S.proc \
# 	-g -O0 -o bin

clean:
	$(RM) $(PLUGIN) $(TEST) $(ASM_INC) $(INIT_OBJ)
