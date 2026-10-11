#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/../.."
if [ ! -x "./vesbo" ]; then
    make
fi
./vesbo games/breakout/breakout.vsb
