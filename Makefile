ROOT=/usr/local
ICON_DIR=$(ROOT)/share/icons/hicolor/32x32/apps

acom2000: acom2000.c config.h extern.h lib.c
	cc -g -o acom2000 acom2000.c lib.c -lm -march=native `pkg-config --cflags --libs gtk4`

install: acom2000
	-mkdir $(ICON_DIR)
	install -m 644 acom2000.png $(ICON_DIR)
	install -m 755 acom2000 $(ROOT)/bin

clean:
	-rm *~ acom2000 *.o
