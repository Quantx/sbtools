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

# Zip output directory
rm sbtools.zip
zip sbtools.zip sbtools/* sbtools/linux/* sbtools/windows/*
