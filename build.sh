#!/bin/sh
gcc -Wall -Werror -g -std=gnu99 -fno-strict-aliasing \
    -lX11 \
    -o wm snap.c wm.c clients.c buffer.c
