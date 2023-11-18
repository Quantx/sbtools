#!/bin/bash
# Ensure all binaries are up to date
make -C binarize
make win -C binarize

make -C segment
make win -C segment

make -C sbmodel
make win -C sbmodel

make -C sbtexture
make win -C sbtexture

make -C sbterrain
make win -C sbterrain

make -C sbstage
make win -C sbstage

# Create output directories
mkdir -p sbtools
mkdir -p sbtools/linux
mkdir -p sbtools/windows

# Copy programs to output directory
cp binarize/binarize sbtools/linux/binarize
cp binarize/binarize.exe sbtools/windows/binarize.exe

cp segment/segment sbtools/linux/segment
cp segment/segment.exe sbtools/windows/segment.exe

cp sbmodel/sbmodel.exe sbtools/windows/sbmodel.exe
cp sbmodel/sbmodel sbtools/linux/sbmodel

cp sbtexture/sbtexture.exe sbtools/windows/sbtexture.exe
cp sbtexture/sbtexture sbtools/linux/sbtexture

cp sbterrain/sbterrain.exe sbtools/windows/sbterrain.exe
cp sbterrain/sbterrain sbtools/linux/sbterrain

cp sbstage/sbstage.exe sbtools/windows/sbstage.exe
cp sbstage/sbstage sbtools/linux/sbstage

# Zip output directory
rm sbtools.zip
zip sbtools.zip sbtools/* sbtools/linux/* sbtools/windows/*
