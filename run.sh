#!/usr/bin/env bash
# Star Catcher: install deps, compile and run.
# Usage: bash run.sh
set -e
cd "$(dirname "$0")"

need=""
for pkg in build-essential libglfw3-dev libglew-dev libgl1-mesa-dev; do
    dpkg -s "$pkg" >/dev/null 2>&1 || need="$need $pkg"
done
if [ -n "$need" ]; then
    echo ">> Installing:$need"
    sudo apt update
    sudo apt install -y $need
fi

echo ">> Compiling..."
g++ -std=c++17 -O2 -Wall main.cpp -o game -lglfw -lGLEW -lGL

echo ">> Starting game"
./game || {
    echo ">> Failed, retrying with software rendering..."
    LIBGL_ALWAYS_SOFTWARE=1 ./game
}
