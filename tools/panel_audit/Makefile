# dump_panels links the plugin's build objects: run `make` at the repo root first.
RACK_DIR ?= /home/gg/dl/audio/Rack-SDK-2.6.6

CXXFLAGS += -O1 -std=c++17 -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include
LDFLAGS  += -L$(RACK_DIR) -lRack -Wl,-rpath,$(RACK_DIR)

OBJS := $(wildcard ../../build/src/*.o)

dump_panels: dump_panels.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

clean:
	rm -f dump_panels

.PHONY: clean
