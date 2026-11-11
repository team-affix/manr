# ==============================================================================
# Toolchain
# ==============================================================================

MAKEFLAGS += -j$(shell nproc)

CXX      = g++
DEPFLAGS = -MMD -MP
AR       = ar
ARFLAGS  = rcs

MODULE_INCS = $(patsubst %,-I%,$(sort $(wildcard modules/*/hpp solvers/*/hpp)))
CXXFLAGS    = -std=c++20 -Wall -Wextra -I. -Icore/hpp -Imcts/core/hpp \
              -Imultiprecision/include -Iboost_config/include $(MODULE_INCS) $(DEPFLAGS)

DEBUG_CXXFLAGS      = -DDEBUG -g
DEBUG_FAST_CXXFLAGS = -DDEBUG -g -O3
RELEASE_CXXFLAGS    = -O3

GTEST_CPPFLAGS = \
    -Igoogletest/googletest/include -Igoogletest/googlemock/include \
    -Igoogletest/googletest -Igoogletest/googlemock
GTEST_SRC = googletest/googletest/src/gtest-all.cc googletest/googlemock/src/gmock-all.cc

# ==============================================================================
# Sources  (discovered, so a new file never needs registering here)
# ==============================================================================

CORE_SRC         = $(shell find core/cpp mcts/core/cpp -name '*.cpp' | sort)
MODULES_SRC      = $(shell find modules solvers -path '*/cpp/*' -name '*.cpp' | sort)
CORE_TEST_SRC    = $(shell find core/test -name '*.cpp' | sort)
MODULES_TEST_SRC = $(shell find modules solvers -path '*/test/*' -name '*.cpp' | sort) modules/test_main.cpp

# $(call objs,<variant>,<sources>) -> build/obj/<variant>/<source path>.o
objs = $(patsubst %,build/obj/$(1)/%.o,$(basename $(2)))

# ==============================================================================
# Targets
# ==============================================================================

.PHONY: all core core_debug core_debug_fast modules modules_debug modules_debug_fast run_tests clean

all: core modules core_debug_fast modules_debug_fast

core:    build/libmanr_core.a
modules: build/libmanr_modules.a

core_debug:         build/core_debug
core_debug_fast:    build/core_debug_fast
modules_debug:      build/modules_debug
modules_debug_fast: build/modules_debug_fast

run_tests: core_debug_fast modules_debug_fast
	./build/core_debug_fast
	./build/modules_debug_fast

clean:
	rm -rf build

# ==============================================================================
# Libraries and test binaries, per variant
# ==============================================================================

# $(1) variant name, $(2) library suffix, $(3) variant flags
define VARIANT
build/libmanr_core$(2).a: $(call objs,$(1),$(CORE_SRC))
	$(AR) $(ARFLAGS) $$@ $$^

build/libmanr_modules$(2).a: $(call objs,$(1),$(MODULES_SRC))
	$(AR) $(ARFLAGS) $$@ $$^

build/obj/$(1)/%.o: %.cpp
	@mkdir -p $$(dir $$@)
	$(CXX) $(CXXFLAGS) $(3) -c $$< -o $$@

build/obj/$(1)/%.o: %.cc
	@mkdir -p $$(dir $$@)
	$(CXX) $(CXXFLAGS) $(3) $(GTEST_CPPFLAGS) -c $$< -o $$@

build/obj/$(1)/core/test/%.o: core/test/%.cpp
	@mkdir -p $$(dir $$@)
	$(CXX) $(CXXFLAGS) $(3) $(GTEST_CPPFLAGS) -c $$< -o $$@

build/obj/$(1)/modules/%.o: modules/%.cpp
	@mkdir -p $$(dir $$@)
	$(CXX) $(CXXFLAGS) $(3) $(GTEST_CPPFLAGS) -c $$< -o $$@

build/obj/$(1)/solvers/%.o: solvers/%.cpp
	@mkdir -p $$(dir $$@)
	$(CXX) $(CXXFLAGS) $(3) $(GTEST_CPPFLAGS) -c $$< -o $$@

-include $(patsubst %.o,%.d,$(call objs,$(1),$(CORE_SRC) $(MODULES_SRC) $(CORE_TEST_SRC) $(MODULES_TEST_SRC) $(GTEST_SRC)))
endef

# $(1) variant name, $(2) library suffix
define TEST_BINARIES
build/core_$(1): build/libmanr_core$(2).a $(call objs,$(1),$(CORE_TEST_SRC) $(GTEST_SRC))
	$(CXX) $(CXXFLAGS) -o $$@ $(call objs,$(1),$(CORE_TEST_SRC) $(GTEST_SRC)) \
	    -Lbuild -lmanr_core$(2) -lpthread

build/modules_$(1): build/libmanr_modules$(2).a build/libmanr_core$(2).a \
                    $(call objs,$(1),$(MODULES_TEST_SRC) $(GTEST_SRC))
	$(CXX) $(CXXFLAGS) -o $$@ $(call objs,$(1),$(MODULES_TEST_SRC) $(GTEST_SRC)) \
	    -Lbuild -lmanr_modules$(2) -lmanr_core$(2) -lpthread
endef

$(eval $(call VARIANT,release,,$(RELEASE_CXXFLAGS)))
$(eval $(call VARIANT,debug,_debug,$(DEBUG_CXXFLAGS)))
$(eval $(call VARIANT,debug_fast,_debug_fast,$(DEBUG_FAST_CXXFLAGS)))
$(eval $(call TEST_BINARIES,debug,_debug))
$(eval $(call TEST_BINARIES,debug_fast,_debug_fast))

# ==============================================================================
# Benchmarks (release)
# ==============================================================================

.PHONY: poly_bench fit_bench list_bench seq_bench logic_bench prog_bench bench_timing bench_check bench_check_full bench_baseline

poly_bench: build/libmanr_modules.a build/libmanr_core.a
	$(CXX) $(CXXFLAGS) $(RELEASE_CXXFLAGS) -pthread bench/poly_bench.cpp -Lbuild -lmanr_modules -lmanr_core -o build/poly_bench

fit_bench: build/libmanr_modules.a build/libmanr_core.a
	$(CXX) $(CXXFLAGS) $(RELEASE_CXXFLAGS) -pthread bench/fit_bench.cpp -Lbuild -lmanr_modules -lmanr_core -o build/fit_bench

list_bench: build/libmanr_modules.a build/libmanr_core.a
	$(CXX) $(CXXFLAGS) $(RELEASE_CXXFLAGS) -pthread bench/list_bench.cpp -Lbuild -lmanr_modules -lmanr_core -o build/list_bench

seq_bench: build/libmanr_modules.a build/libmanr_core.a
	$(CXX) $(CXXFLAGS) $(RELEASE_CXXFLAGS) -pthread bench/seq_bench.cpp -Lbuild -lmanr_modules -lmanr_core -o build/seq_bench

logic_bench: build/libmanr_modules.a build/libmanr_core.a
	$(CXX) $(CXXFLAGS) $(RELEASE_CXXFLAGS) -pthread bench/logic_bench.cpp -Lbuild -lmanr_modules -lmanr_core -o build/logic_bench

prog_bench: build/libmanr_modules.a build/libmanr_core.a
	$(CXX) $(CXXFLAGS) $(RELEASE_CXXFLAGS) -pthread bench/prog_bench.cpp -Lbuild -lmanr_modules -lmanr_core -o build/prog_bench

# The regression gate (docs/extending.md): every solver's cost on every problem must be no worse than the committed
# baseline, and every result sound. bench_check skips MCTS on the largest problems; bench_check_full runs everything.
bench_check: poly_bench fit_bench list_bench seq_bench logic_bench prog_bench
	@status=0; \
	./build/poly_bench --fast --check bench/baseline/poly.tsv > build/poly_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/poly_bench.txt; \
	./build/fit_bench --fast --check bench/baseline/fit.tsv > build/fit_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/fit_bench.txt; \
	./build/list_bench --fast --check bench/baseline/list.tsv > build/list_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/list_bench.txt; \
	./build/seq_bench --fast --check bench/baseline/seq.tsv > build/seq_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/seq_bench.txt; \
	./build/logic_bench --fast --check bench/baseline/logic.tsv > build/logic_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/logic_bench.txt; \
	./build/prog_bench --fast --check bench/baseline/prog.tsv > build/prog_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/prog_bench.txt; \
	exit $$status

bench_check_full: poly_bench fit_bench list_bench seq_bench logic_bench prog_bench
	@status=0; \
	./build/poly_bench --check bench/baseline/poly.tsv > build/poly_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/poly_bench.txt; \
	./build/fit_bench --check bench/baseline/fit.tsv > build/fit_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/fit_bench.txt; \
	./build/list_bench --check bench/baseline/list.tsv > build/list_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/list_bench.txt; \
	./build/seq_bench --check bench/baseline/seq.tsv > build/seq_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/seq_bench.txt; \
	./build/logic_bench --check bench/baseline/logic.tsv > build/logic_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/logic_bench.txt; \
	./build/prog_bench --check bench/baseline/prog.tsv > build/prog_bench.txt || status=1; \
	sed -n '/^gate against/,$$p' build/prog_bench.txt; \
	exit $$status

# True timings: every problem under every solver, one at a time (the gate runs problems in parallel, so its seconds
# are measured under load). Results go to build/*_bench_serial.txt; nothing is checked or written.
bench_timing: poly_bench fit_bench list_bench seq_bench logic_bench prog_bench
	./build/poly_bench --serial > build/poly_bench_serial.txt
	./build/fit_bench --serial > build/fit_bench_serial.txt
	./build/list_bench --serial > build/list_bench_serial.txt
	./build/seq_bench --serial > build/seq_bench_serial.txt
	./build/logic_bench --serial > build/logic_bench_serial.txt
	./build/prog_bench --serial > build/prog_bench_serial.txt

# Records the current results as the baseline. Run only when a change is accepted; commit the diff with it.
bench_baseline: poly_bench fit_bench list_bench seq_bench logic_bench prog_bench
	./build/poly_bench --write bench/baseline/poly.tsv > build/poly_bench.txt
	./build/fit_bench --write bench/baseline/fit.tsv > build/fit_bench.txt
	./build/list_bench --write bench/baseline/list.tsv > build/list_bench.txt
	./build/seq_bench --write bench/baseline/seq.tsv > build/seq_bench.txt
	./build/logic_bench --write bench/baseline/logic.tsv > build/logic_bench.txt
	./build/prog_bench --write bench/baseline/prog.tsv > build/prog_bench.txt
