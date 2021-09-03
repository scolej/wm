#!/bin/sh

set -eu

make


Xnest :1 &
nestpid=$!

sleep 1

export DISPLAY=:1
valgrind --exit-on-first-error=yes ./wm &
wmpid=$!

st &
st &
# st &

wait $wmpid

kill $nestpid
