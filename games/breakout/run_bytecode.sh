#!/usr/bin/env bash
cd "$(dirname "$0")/../.."
./vesbo -c games/breakout/breakout.vsb -o games/breakout/breakout.vbo
./vesbo games/breakout/breakout.vbo
