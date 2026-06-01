RACK_DIR ?= /home/gg/dl/audio/Rack-SDK-2.6.6

include $(RACK_DIR)/arch.mk

FLAGS +=
CFLAGS +=
CXXFLAGS +=

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

include $(RACK_DIR)/plugin.mk
