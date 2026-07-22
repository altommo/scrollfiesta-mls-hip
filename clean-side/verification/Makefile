CXX ?= g++
CPPFLAGS ?= -Iinclude
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror

BUILD := build
CORE_SOURCES := src/mls_oracle.cpp src/fixtures.cpp src/golden_io.cpp src/comparator.cpp
CORE_OBJECTS := $(CORE_SOURCES:src/%.cpp=$(BUILD)/%.o)

.PHONY: all test integration-test hip-interface-test goldens clean

all: $(BUILD)/mls_verify

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.cpp include/mls_verify.hpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/main.o: src/main.cpp include/mls_verify.hpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/mls_verify: $(CORE_OBJECTS) $(BUILD)/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/test_mls.o: tests/test_mls.cpp include/mls_verify.hpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/test_mls: $(CORE_OBJECTS) $(BUILD)/test_mls.o
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(BUILD)/test_mls
	./$(BUILD)/test_mls

integration-test: $(BUILD)/mls_verify
	mkdir -p $(BUILD)/integration
	./$(BUILD)/mls_verify generate plane_oblique $(BUILD)/integration/golden.mlsg
	./$(BUILD)/mls_verify oracle $(BUILD)/integration/golden.mlsg $(BUILD)/integration/actual.mlsg
	./$(BUILD)/mls_verify compare $(BUILD)/integration/golden.mlsg $(BUILD)/integration/actual.mlsg

hip-interface-test: $(BUILD)/mls_verify
	mkdir -p $(BUILD)/integration
	./$(BUILD)/mls_verify generate plane_axis $(BUILD)/integration/hip-golden.mlsg
	./scripts/run_hip_vs_oracle.sh $(BUILD)/integration/hip-golden.mlsg ./tests/mock_hip_runner.sh

goldens: $(BUILD)/mls_verify
	mkdir -p goldens
	./$(BUILD)/mls_verify generate-all goldens

clean:
	rm -rf $(BUILD)
