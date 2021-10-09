#!/bin/sh

sh expand.sh

flags='-Wall -Werror -g -std=gnu99 -fno-strict-aliasing'

gcc $flags \
    -lX11 \
    -o wm \
    snap.c wm.c clients.c \
    clientbuffer.c windowbuffer.c

gcc $flags -o test_buffer test_buffer.c windowbuffer.c
./test_buffer
