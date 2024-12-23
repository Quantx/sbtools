#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "dds.h"

char * progname;

int makeDDS(char * path) {
    FILE * raw = fopen(path, "rb");
    if (!raw) {
        fprintf(stderr, "Could not open file: %s\n", path);
        return 1;
    }
    
    // Validate magic
    char magic[4];
    fread(magic, sizeof(char), 4, raw);
    if (strncmp(magic, "XRAW", 4)) {
        fprintf(stderr, "Invalid magic, expected XRAW\n");
        return 1;
    }
    
    char * ext = strrchr(path, '.');
    strcpy(ext + 1, "dds");
    
    FILE * dds = fopen(path, "wb");
    if (!dds) {
        fprintf(stderr, "Could not open file: %s\n", path);
        fclose(raw);
        return 1;
    }
    
    const int tex_size = 64 * 64 * sizeof(struct dxt1_block);
    
    // Skip rest of header
    fseek(raw, 16, SEEK_SET);
    
    struct dds_header header = DDS_HEADER_INIT;
    
    header.flags |= 0x80000; // Compressed texture linear size
    header.format.flags |= 0x4; // Compressed Texture
    
    header.levels = 1;
    header.width = 256;
    header.height = 256;
    header.pitch = tex_size;
    
    memcpy(header.format.codeStr, "DXT1", 4);
    
    fputs("DDS ", dds);
    fwrite(&header, sizeof(struct dds_header), 1, dds);
    
    for (int i = 0; i < tex_size; i++) {
        int byte = fgetc(raw);
        fputc(byte, dds);
    }
    
    fclose(dds);
    fclose(raw);
    return 0;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Bumpmap Tool - By QuantX\n");

    if (!argc) {
        fprintf(stderr, "Please specify either a .raw file: %s <path/example.raw>\n", progname);
        return 1;
    }
    
    char * path = *argv++; argc--;
    
    char * ext = strrchr(path, '.');
    
    if (!ext) {
        fprintf(stderr, "Missing file extension in path: %s\n", path);
        return 1;
    }
    
    ext++;
    
    if (!strncmp(ext, "raw", 3)) {
        return makeDDS(path);
    }

    fprintf(stderr, "Invalid file extension: .%s\n", ext);
    return 1;
}
