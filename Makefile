CC :=
CXX :=
LDLIBS := 
PORTAUDIOINC := portaudio/install/include
PORTAUDIOLIBS := portaudio/install/lib/libportaudio.a
SRCDIR := src


LDFLAGS := -g
CFLAGS := -g
CXXFLAGS += -g

ifeq ($(shell uname), Darwin)
  # By default Mac uses clang++ as g++, but people may have changed their
  # default configuration.
  CC := clang
  CXX := clang++
  CFLAGS += -I$(SRCDIR) -Wall -I$(PORTAUDIOINC)
  CXXFLAGS += -I$(SRCDIR) -Wall -Wno-sign-compare -Winit-self \
      -DHAVE_POSIX_MEMALIGN -DHAVE_CLAPACK -I$(PORTAUDIOINC)
  LDLIBS += -ldl -lm -framework Accelerate -framework CoreAudio \
      -framework AudioToolbox -framework AudioUnit -framework CoreServices \
      $(PORTAUDIOLIBS)
else ifeq ($(shell uname), Linux)
  CC := gcc
  CXX := g++
  CFLAGS += -I$(SRCDIR) -Wall
  CXXFLAGS += -I$(SRCDIR) -std=c++0x -Wall -Wno-sign-compare \
      -Wno-unused-local-typedefs -Winit-self -rdynamic \
      -DHAVE_POSIX_MEMALIGN
  LDLIBS += -ldl -lm -Wl,-Bstatic -Wl,-Bdynamic -lrt -lpthread \
      -lasound $(shell pkg-config --libs speexdsp)
endif

# Suppress clang warnings...
COMPILER = $(shell $(CXX) -v 2>&1 )
ifeq ($(findstring clang,$(COMPILER)), clang)
  CXXFLAGS += -Wno-mismatched-tags -Wno-c++11-extensions
endif

# Set optimization level.
CFLAGS += -O3
CXXFLAGS += -O3

BINFILE = ec

OBJFILES = src/ec.o src/audio.o src/fifo.o src/pa_ringbuffer.o

all: $(BINFILE)

%.a:
	$(MAKE) -C ${@D} ${@F}

# We have to use the C++ compiler to link.
$(BINFILE): $(OBJFILES)
	$(CXX) $(OBJFILES) $(LDLIBS) -o $(BINFILE)

$(PORTAUDIOLIBS):
	@-./install_portaudio.sh

clean:
	-rm -f *.o *.a $(BINFILE) $(OBJFILES)
