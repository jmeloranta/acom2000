acom2000: acom2000.c config.h lib.c
	cc -o acom2000 acom2000.c lib.c -lm -march=native `pkg-config --cflags --libs gtk4`

clean:
	-rm *~ acom2000 *.o
