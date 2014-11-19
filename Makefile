PKGS=sdl2 glew gl libpng16
CC=clang
CFLAGS=-Ofast -Wall -std=c99 $(shell pkg-config $(PKGS) --cflags)
LINK=$(shell pkg-config $(PKGS) --libs) -lm

all: main

a.o: a.c
	$(CC) $(CFLAGS) -c a.c

bdf2c: bdf2c.c
	$(CC) bdf2c.c -o bdf2c

ter_u24.c: bdf2c ter-u24n.bdf ter-u24b.bdf
	./bdf2c 1024 512 ter_u24.c ter-u24n.bdf ter-u24b.bdf

ter_u24.o: ter_u24.c
	$(CC) -c ter_u24.c

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

text.glsl.inc: text.glsl
	./glsl2inc.pl text.glsl

text.o: text.c text.glsl.inc
	$(CC) $(CFLAGS) -c text.c

main.o: main.c path.glsl.inc sun.glsl.inc body.glsl.inc
	$(CC) $(CFLAGS) -c main.c

sol.o: sol.c
	$(CC) $(CFLAGS) -c sol.c

main: main.o a.o shader.o mud.o sol.o text.o ter_u24.o
	$(CC) $(LINK) main.o a.o shader.o mud.o sol.o text.o ter_u24.o -o main

clean:
	rm -rf *.o main *.glsl.inc bdf2c ter_u24.c

