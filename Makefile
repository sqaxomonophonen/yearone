PKGS=sdl2 glew gl libpng16
CC=clang
CFLAGS=-Ofast -Wall -std=c99 $(shell pkg-config $(PKGS) --cflags)
LINK=$(shell pkg-config $(PKGS) --libs) -lm

all: main

a.o: a.c
	$(CC) $(CFLAGS) -c a.c

shader.o: shader.c
	$(CC) $(CFLAGS) -c shader.c

mud.o: mud.c
	$(CC) $(CFLAGS) -c mud.c

path.glsl.inc: path.glsl
	./glsl2inc.pl path.glsl

sun.glsl.inc: sun.glsl
	./glsl2inc.pl sun.glsl

body.glsl.inc: body.glsl
	./glsl2inc.pl body.glsl

main.o: main.c path.glsl.inc sun.glsl.inc body.glsl.inc
	$(CC) $(CFLAGS) -c main.c

sol.o: sol.c
	$(CC) $(CFLAGS) -c sol.c

main: main.o a.o shader.o mud.o sol.o
	$(CC) $(LINK) main.o a.o shader.o mud.o sol.o -o main

clean:
	rm -rf *.o main *.glsl.inc

