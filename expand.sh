#!/bin/sh

# Expand templates.

set -e

# todo a loop

header=clientbuffer.h
headerdef=CLIENTBUFFER_H
typeheader='"client.h"'
src=clientbuffer.c
type='Client'
typename=ClientBuffer
prefix=cb

sed -e "s/{{type}}/$type/g" \
    -e "s/{{prefix}}/$prefix/g" \
    -e "s/{{typename}}/$typename/g" \
    -e "s/{{header}}/$header/g" \
    < buffer.c.template > $src

sed -e "s/{{type}}/$type/g" \
    -e "s/{{prefix}}/$prefix/g" \
    -e "s/{{typename}}/$typename/g" \
    -e "s/{{header}}/$header/g" \
    -e "s/{{headerdef}}/$headerdef/g" \
    -e "s/{{typeheader}}/$typeheader/g" \
    < buffer.h.template > $header


header=windowbuffer.h
headerdef=WINDOWBUFFER_H
typeheader='\<X11\/Xlib.h\>'
src=windowbuffer.c
type=Window
typename=WindowBuffer
prefix=wb

sed -e "s/{{type}}/$type/g" \
    -e "s/{{prefix}}/$prefix/g" \
    -e "s/{{typename}}/$typename/g" \
    -e "s/{{header}}/$header/g" \
    < buffer.c.template > $src

sed -e "s/{{type}}/$type/g" \
    -e "s/{{prefix}}/$prefix/g" \
    -e "s/{{typename}}/$typename/g" \
    -e "s/{{header}}/$header/g" \
    -e "s/{{headerdef}}/$headerdef/g" \
    -e "s/{{typeheader}}/$typeheader/g" \
    < buffer.h.template > $header

# todo how to not specify an extra header, eg to make an int buffer?
