flags=-Wall -g -Werror -std=gnu99 -fno-strict-aliasing
cc:=gcc

all : wm test

test : test_buffer test_snap

wm : wm.o snap.o buffer.c clients.o
	$(cc) $(flags) -lX11 -o $@ $^

%.o : %.c %.h
	$(cc) $(flags) -c -o $@ $<

test_buffer : test_buffer.c buffer.c
	$(cc) $(flags) -lX11 -o $@ $^

test_snap : test_snap.c snap.c
	$(cc) $(flags) -lX11 -o $@ $^

check-syntax :
	$(cc) -fsyntax-only -Iglad/include $(CHK_SOURCES)

clean :
	rm *.o test_buffer test_snap wm
