#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/../.."
if [ ! -x "./vesbo" ]; then
    make
fi
./vesbo -c games/breakout/breakout.vsb -o games/breakout/breakout.vbo
./vesbo games/breakout/breakout.vbo
