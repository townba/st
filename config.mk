# st version
VERSION = 0.7

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11/include
X11LIB = /usr/X11/lib

# includes and libs
OSDEP_LIBS =
ifeq ($(shell uname -s),Darwin)
else ifeq ($(shell uname -s),OpenBSD)
else
        OSDEP_LIBS += -lrt
endif

# flags
CPPFLAGS += -DVERSION=\"${VERSION}\" -D_XOPEN_SOURCE=600 \
       -Wall -Wextra -Wpedantic -Wno-missing-field-initializers \
       -Wmissing-prototypes \
       -Wno-type-limits -Wno-variadic-macros -Wunused-macros \
       -I. -I/usr/include -I${X11INC} \
       $(shell pkg-config --cflags fontconfig) \
       $(shell pkg-config --cflags freetype2)
CFLAGS += -std=c99
CXXFLAGS += -std=c++11
LDFLAGS += -L/usr/lib -L${X11LIB} \
       $(shell pkg-config --libs-only-L fontconfig)  \
       $(shell pkg-config --libs-only-L freetype2)
LDLIBS += -lc -lm ${OSDEP_LIBS} -lX11 -lutil -lXft \
       $(shell pkg-config --libs-only-l fontconfig)  \
       $(shell pkg-config --libs-only-l freetype2)

# TODO(townba): These will fail on any other system.
CLANG_TIDY_CHECKS = *,-clang-analyzer-alpha*,-cert-env33-c
CLANG_TIDY_EXECUTABLE = $(shell brew --prefix)/opt/llvm/bin/clang-tidy
CLANG_TIDY_FLAGS = \
	-I$(shell xcode-select --print-path)/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/usr/include
