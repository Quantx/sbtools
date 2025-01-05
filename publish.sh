#!/bin/bash
# Ensure all binaries are up to date
make

# Create output directories
mkdir -p sbtools
mkdir -p sbtools/linux
mkdir -p sbtools/windows

# Copy programs to output directory
for file in bin/*; do
	if [[ "$file" == *.exe ]]; then
		cp "$file" sbtools/windows/
	else
		cp "$file" sbtools/linux/
	fi
done

cp package.py sbtools/package.py
cp export_presets.cfg sbtools/export_presets.cfg
cp package.bat sbtools/DRAG_ONTO_ME.bat

# Zip output directory
rm sbtools.zip
zip sbtools.zip sbtools/* sbtools/linux/* sbtools/windows/*
