flags=-Wall -g -Werror -std=gnu99
cc=gcc

all : wm test_buffer test_snap

wm : wm.o snap.o windowbuffer.o clientbuffer.o clients.o
	$(cc) $(flags) -lX11 -o $@ $^

windowbuffer.c windowbuffer.h clientbuffer.c clientbuffer.h &: buffer.c.template buffer.h.template expand.sh
	./expand.sh

%.o : %.c %.h
	$(cc) $(flags) -c -o $@ $<

%.o : %.c
	$(cc) $(flags) -c -o $@ $<

wm.o : windowbuffer.h clientbuffer.h

test_buffer : test_buffer.c windowbuffer.c
	$(cc) $(flags) -lX11 -o $@ $^

test_snap : test_snap.c snap.c
	$(cc) $(flags) -lX11 -o $@ $^

check-syntax :
	$(cc) -fsyntax-only -Iglad/include $(CHK_SOURCES)
