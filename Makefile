

ec: ec.o ring.o decimate.o
	gcc -o $@ $^ -lspeexdsp -lmosquitto -lm

all: ec


clean:
	rm -rf *.o ec
