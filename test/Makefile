# Offline tests. They link against libRack from the SDK (RACK_DIR as for the
# plugin build). `make check` builds and runs everything, nonzero on failure.
RACK_DIR ?= /home/gg/dl/audio/Rack-SDK-2.6.6

CXXFLAGS += -O1 -std=c++17 -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include
LDFLAGS  += -L$(RACK_DIR) -lRack -Wl,-rpath,$(RACK_DIR)

# Every module source except plugin.cpp; Base64.cpp defines struct Base.
MODULES := $(filter-out plugin,$(basename $(notdir $(wildcard ../src/*.cpp))))
structOf = $(if $(filter Base64,$1),Base,$1)

PERSIST_BINS := $(addprefix persist_,$(MODULES))

all: $(PERSIST_BINS)

stubs.o: stubs.cpp ../src/plugin.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

persist_%: persistence.cpp stubs.o ../src/%.cpp $(wildcard ../src/*.hpp)
	$(CXX) $(CXXFLAGS) -DMODULE_SRC='"../src/$*.cpp"' -DMODULE_T=$(call structOf,$*) \
	    $< stubs.o -o $@ $(LDFLAGS)

check: $(PERSIST_BINS)
	@echo "module,check,pass"; \
	rc=0; \
	for t in $(PERSIST_BINS); do ./$$t --no-header || rc=1; done; \
	exit $$rc

clean:
	rm -f stubs.o $(PERSIST_BINS)

.PHONY: all check clean
