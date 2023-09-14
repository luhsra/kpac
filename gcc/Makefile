TARGET_GCC = $(CROSS_COMPILE)gcc

PLUGIN = pac_sw_plugin.so
SRC = main.cc

TESTS = tests

ASMS = $(shell find -name '*.S')
ASMS_PP = $(ASMS:.S=.s)

GCC_PLUGINS_DIR = $(shell $(TARGET_GCC) -print-file-name=plugin)

# GCC CPP flags (we need pic and no-rtti)
CXXFLAGS += -g -I$(GCC_PLUGINS_DIR)/include -fPIC -fno-rtti -DDEBUG -Wall -O2

.PHONY: all clean run_tests

all: $(PLUGIN) $(ASMS_PP)

$(ASMS_PP): %.s: %.S
	$(TARGET_GCC) -E $< -o $@

$(PLUGIN): $(SRC)
	$(CXX) -shared $(CXXFLAGS) $^ -o $@

run_tests:
	$(MAKE) -C $(TESTS) all

clean:
	$(RM) $(PLUGIN) $(ASMS_PP)
	$(MAKE) -C $(TESTS) clean
