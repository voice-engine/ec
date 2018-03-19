
CC := gcc
CXX := g++
CFLAGS += -Isrc -Wall
CXXFLAGS += -Isrc -std=c++0x -Wall -Wno-sign-compare \
    -Wno-unused-local-typedefs -Winit-self -rdynamic \
    -DHAVE_POSIX_MEMALIGN
LDLIBS += -ldl -lm -Wl,-Bstatic -Wl,-Bdynamic -lrt -lpthread \
    -lasound $(shell pkg-config --libs speexdsp)


# Set optimization level.
CFLAGS += -O3
CXXFLAGS += -O3

BINFILE = ec

OBJFILES = src/ec.o src/audio.o src/fifo.o src/pa_ringbuffer.o

all: $(BINFILE)

$(BINFILE): $(OBJFILES)
	$(CXX) $(OBJFILES) $(LDLIBS) -o $(BINFILE)



clean:
	-rm -f *.o $(BINFILE) $(OBJFILES)
