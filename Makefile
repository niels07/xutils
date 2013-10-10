CC=gcc
CFLAGS=-g -Wall -std=c99

all: xdf xls xpwd

clean: 
	rm xdf xls xpwd

xpwd: xpwd.c xlib.c xlib.h
	$(CC) $(CFLAGS) xpwd.c xlib.c -o xpwd

xdf: xdf.c xlib.c xlib.h
	$(CC) $(CFLAGS) xdf.c xlib.c -o xdf

xls: xls.c xlib.c xlib.h
	$(CC) $(CFLAGS) xls.c xlib.c -o xls




