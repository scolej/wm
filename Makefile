flags=-Wall -std=c99 -g
cc=gcc

all : wm

wm : wm.o iter.o
	$(cc) $(flags) -lX11 -o $@ $^

wm.o : wm.c iter.h
	$(cc) $(flags) -c $@ $<

iter.o : iter.c iter.h
	$(cc) $(flags) -c $@ $<

check-syntax :
	$(cc) -fsyntax-only -Iglad/include $(CHK_SOURCES)
