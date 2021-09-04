flags=-Wall -std=c99 -g -Werror
cc=gcc

all : wm test

test : test_buffer test_snap

wm : wm.c snap.c
	$(cc) $(flags) -lX11 -o $@ $^

test_buffer : test_buffer.c buffer.c
	$(cc) $(flags) -lX11 -o $@ $^

test_snap : test_snap.c snap.c
	$(cc) $(flags) -lX11 -o $@ $^

check-syntax :
	$(cc) -fsyntax-only -Iglad/include $(CHK_SOURCES)
