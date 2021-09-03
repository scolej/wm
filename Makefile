flags=-Wall -std=c99 -g -Werror
cc=gcc

all : wm test

wm : wm.c
	$(cc) $(flags) -lX11 -o $@ $^

test : test_buffer.c buffer.c
	$(cc) $(flags) -lX11 -o $@ $^

check-syntax :
	$(cc) -fsyntax-only -Iglad/include $(CHK_SOURCES)
