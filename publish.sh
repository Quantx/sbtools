#!/bin/bash
sbtools_version="0.4.0.$(date +%s)"

echo "Publishing sbtools version: $sbtools_version"

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

cp README.md sbtools/README.md
cp 00-sbc.rules sbtools/00-sbc.rules
cp package.py sbtools/package.py
cp export_presets.cfg sbtools/export_presets.cfg
cp package.bat sbtools/DRAG_ONTO_ME.bat

sed -i -e "s/<SBTOOLS_VERSION>/$sbtools_version/g" sbtools/package.py

# Zip output directory
rm sbtools.zip
zip "sbtools-$sbtools_version.zip" sbtools/* sbtools/linux/* sbtools/windows/*
