flags=-Wall -std=c99 -g
cc=gcc

all : wm

wm : wm.c
	$(cc) $(flags) -lX11 -o $@ $^

check-syntax :
	$(cc) -fsyntax-only -Iglad/include $(CHK_SOURCES)
