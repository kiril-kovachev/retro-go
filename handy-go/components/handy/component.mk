#
# Component Makefile
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

COMPONENT_ADD_INCLUDEDIRS := .
COMPONENT_SRCDIRS := .

CFLAGS += -O3 -DLSB_FIRST=1 -Wall -Wno-comment -Wno-error=comment
CXXFLAGS += -O3 -DLSB_FIRST=1 -Wall -Wno-comment -Wno-error=comment