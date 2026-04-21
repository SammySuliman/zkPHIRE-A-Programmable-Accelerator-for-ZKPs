CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wno-unknown-pragmas -pedantic -O2
CPPFLAGS ?= -Iinclude

BUILD_DIR := build
TB_BIN := $(BUILD_DIR)/sumcheck_tb

.PHONY: all test clean vitis-csim vitis-csynth

all: test

$(TB_BIN): src/sumcheck_top.cpp tb/sumcheck_tb.cpp include/*.hpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) src/sumcheck_top.cpp tb/sumcheck_tb.cpp -o $(TB_BIN)

test: $(TB_BIN)
	./$(TB_BIN)

vitis-csim:
	vitis_hls -f scripts/run_hls.tcl

vitis-csynth:
	RUN_CSYNTH=1 vitis_hls -f scripts/run_hls.tcl

clean:
	rm -rf $(BUILD_DIR) sumcheck_round_prj .Xil *.jou *.log
