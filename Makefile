all: ufontconvert

CC     = gcc
CFLAGS = -Wall -I/usr/local/include/freetype2 -I/usr/include/freetype2 -I/usr/include
LIBS   = -lfreetype

ufontconvert: ufontconvert.c
	$(CC) $(CFLAGS) $< $(LIBS) -o $@
	strip $@

clean:
	rm -f ufontconvert
