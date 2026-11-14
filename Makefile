# GENRA CHOP: a VCV Rack 2 plugin (slug GenraChop). Build:
#   make RACK_DIR=/path/to/Rack-SDK dist
# Self-contained: nothing to fetch (its own knobs, its own look; WARP's libraries are in dep/).
RACK_DIR ?= ../..

SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard src/hh/*.cpp)
SOURCES += $(wildcard src/fx/*.cpp)
# the link hub of the GENRA plugins (Follow GENRA: their instruments can follow GENRA CHOP), copied from the GENRA repo
SOURCES += $(wildcard src/link/*.cpp)
# WARP: Rubber Band (GPL-2.0-or-later) and Signalsmith Stretch (MIT), both in dep/
SOURCES += $(wildcard src/warp/*.cpp)
FLAGS += -Idep -Idep/rubberband

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += licenses

include $(RACK_DIR)/plugin.mk
