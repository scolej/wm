#!/bin/sh

# Expand templates.

set -e

# expand buffer template into source and header files.
#
# header        name of the header file
# type          type which is contained in the buffer
# prefix        prefix to add to all procedures
# structname    name of the struct
# headerdef     if-not-def macro name
# extraheader   an extra include line if necessary
expand_templates() {
    sed -e "s/{{type}}/$type/g" \
        -e "s/{{prefix}}/$prefix/g" \
        -e "s/{{structname}}/$structname/g" \
        -e "s/{{header}}/$header/g" \
        < buffer.c.template > $src

    sed -e "s/{{type}}/$type/g" \
        -e "s/{{prefix}}/$prefix/g" \
        -e "s/{{structname}}/$structname/g" \
        -e "s/{{header}}/$header/g" \
        -e "s/{{headerdef}}/$headerdef/g" \
        -e "s/{{extraheader}}/$extraheader/g" \
        < buffer.h.template > $header
}

header=clientbuffer.h
headerdef=CLIENTBUFFER_H
extraheader='#include "client.h"'
src=clientbuffer.c
type='Client'
structname=ClientBuffer
prefix=cb
expand_templates

header=windowbuffer.h
headerdef=WINDOWBUFFER_H
extraheader='#include \<X11\/Xlib.h\>'
src=windowbuffer.c
type=Window
structname=WindowBuffer
prefix=wb
expand_templates
