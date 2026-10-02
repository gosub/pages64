# Offline tests. They link against libRack from the SDK (RACK_DIR as for the
# plugin build). `make check` builds and runs everything, nonzero on failure.
RACK_DIR ?= /home/gg/dl/audio/Rack-SDK-2.6.6

CXXFLAGS += -O1 -std=c++17 -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include
LDFLAGS  += -L$(RACK_DIR) -lRack -Wl,-rpath,$(RACK_DIR)

# Every module source except plugin.cpp; Base64.cpp defines struct Base.
MODULES := $(filter-out plugin,$(basename $(notdir $(wildcard ../src/*.cpp))))
structOf = $(if $(filter Base64,$1),Base,$1)

PERSIST_BINS := $(addprefix persist_,$(MODULES))

# Single-purpose checks, run by `check` alongside the persistence ones.
CHECK_BINS := mlr_snapshot poly16

# The kits' seed contract, as rendered sound against fixtures/<Kit>.txt.
# `make fixtures` rewrites them, for an intentional change only.
KITS := Drums64 Objects64 Grains64 Micro64
GOLDEN_BINS := $(addprefix kit_golden_,$(KITS))

all: $(PERSIST_BINS) $(CHECK_BINS) $(GOLDEN_BINS)

stubs.o: stubs.cpp ../src/plugin.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

persist_%: persistence.cpp stubs.o ../src/%.cpp $(wildcard ../src/*.hpp)
	$(CXX) $(CXXFLAGS) -DMODULE_SRC='"../src/$*.cpp"' -DMODULE_T=$(call structOf,$*) \
	    $< stubs.o -o $@ $(LDFLAGS)

mlr_snapshot: mlr_snapshot.cpp stubs.o ../src/Mlr64.cpp $(wildcard ../src/*.hpp)
	$(CXX) $(CXXFLAGS) $< stubs.o -o $@ $(LDFLAGS)

poly16: poly16.cpp stubs.o ../src/Poly16.cpp $(wildcard ../src/*.hpp)
	$(CXX) $(CXXFLAGS) $< stubs.o -o $@ $(LDFLAGS)

rack_host.o: rack_host.cpp
	$(CXX) $(CXXFLAGS) -Wno-cpp -c $< -o $@

kit_golden_%: kit_golden.cpp stubs.o rack_host.o ../src/%.cpp ../src/KitModule.hpp $(wildcard ../src/*.hpp)
	$(CXX) $(CXXFLAGS) -DKIT_SRC='"../src/$*.cpp"' -DKIT_T=$* $< stubs.o rack_host.o -o $@ $(LDFLAGS)

fixtures: $(GOLDEN_BINS)
	for t in $(GOLDEN_BINS); do ./$$t --write; done

check: $(PERSIST_BINS) $(CHECK_BINS) $(GOLDEN_BINS)
	@echo "module,check,pass"; \
	rc=0; \
	for t in $(PERSIST_BINS) $(CHECK_BINS) $(GOLDEN_BINS); do ./$$t --no-header || rc=1; done; \
	exit $$rc

clean:
	rm -f stubs.o rack_host.o $(PERSIST_BINS) $(CHECK_BINS) $(GOLDEN_BINS)

.PHONY: all check clean fixtures
