# ==============================================================================
# Toolchain
# ==============================================================================

MAKEFLAGS += -j$(shell nproc)

CXX      = g++
DEPFLAGS = -MMD -MP
AR       = ar
ARFLAGS  = rcs

MODULE_INCS = $(patsubst %,-I%,$(sort $(wildcard modules/*/hpp)))
CXXFLAGS    = -std=c++20 -Wall -Wextra -I. -Icore/hpp -Imcts/core/hpp $(MODULE_INCS) $(DEPFLAGS)

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
MODULES_SRC      = $(shell find modules -path '*/cpp/*' -name '*.cpp' | sort)
CORE_TEST_SRC    = $(shell find core/test -name '*.cpp' | sort)
MODULES_TEST_SRC = $(shell find modules -path '*/test/*' -name '*.cpp' | sort) modules/test_main.cpp

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
