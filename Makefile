
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


COMMON_OBJ = src/audio.o src/fifo.o src/pa_ringbuffer.o src/util.o
EC_OBJ = $(COMMON_OBJ) src/ec.o
EC_LOOPBACK_OBJ = $(COMMON_OBJ) src/ec_loopback.o

all: ec ec_loopback

ec: $(EC_OBJ)
	$(CXX) $(EC_OBJ) $(LDLIBS) -o ec

ec_loopback: $(EC_LOOPBACK_OBJ)
	$(CXX) $(EC_LOOPBACK_OBJ) $(LDLIBS) -o ec_loopback

clean:
	-rm -f src/*.o ec ec_loopback
