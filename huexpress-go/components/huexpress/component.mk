#
# Component Makefile
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

COMPONENT_ADD_INCLUDEDIRS := . includes engine netplay
COMPONENT_SRCDIRS := . engine odroid

CFLAGS += -Ofast -Wno-all -Wno-error
CPPFLAGS += -Wno-all -Wno-error
