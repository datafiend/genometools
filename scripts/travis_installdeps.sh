#!/bin/bash

echo $TRAVIS_OS_NAME
if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
  sudo apt-get update -q
  sudo apt-get install libpango1.0-dev libcairo2-dev
  sudo apt-get install ncbi-blast+ gcc-multilib -y
  sudo apt-get install binutils-mingw-w64-i686 gcc-mingw-w64-i686 -y
  sudo apt-get install binutils-mingw-w64-x86-64 gcc-mingw-w64-x86-64 -y
else
  brew update
  brew install pango cairo 
  brew unlink proj
  brew install blast --force
fi
