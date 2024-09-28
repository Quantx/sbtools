#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../dds.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MAP_WIDTH 280
#define MAP_HEIGHT 280

char * helpmsg = "Used pack and unpack SB heightmap terrain files\n"
"Show help: sbterrain -h\n"
"Unpack map: sbterrain -u map00.gnd\n"
"Pack map: sbterrain -p map00.gnd\n";

char * progname;

int pack(char * path) {
    fprintf(stderr, "Not implemented\n");
    return 1;
}

int unpack(char * path) {
    char * ext = strrchr(path, '.');
    if (!ext) {
        fprintf(stderr, "Path is missing extension: %s\n", path);
        return 1;
    }

    FILE * gndf = fopen(path, "rb");
    if (!gndf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    printf("Reading terrain heightmap data from %s\n", path);
    
    // Change path extension to DDS
    strcpy(ext + 1, "dds");
    
    FILE * hmdf = fopen(path, "wb");
    if (!hmdf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    struct dds_header header = DDS_HEADER_INIT;
    
    header.width = MAP_WIDTH;
    header.height = MAP_HEIGHT;
    size_t size = header.width * header.height;

    header.flags |= 0x80000; // Linear size is provided
    header.pitch = size * sizeof(float);

    // Enable DX10 header
    memcpy(header.format.codeStr, "DX10", 4);
    header.format.flags = 0x4;
    
    struct dds_header_dx10 header10 = {
        .format = 16, // R32G32F
        .dimensions = DDS_DX10_DIMENSION_2D,
        .arraySize = 1,
    };
    
    fputs("DDS ", hmdf);
    fwrite(&header, sizeof(struct dds_header), 1, hmdf);
    fwrite(&header10, sizeof(struct dds_header_dx10), 1, hmdf);
    
    float * rChannel = malloc(size * sizeof(float));
    float * gChannel = malloc(size * sizeof(float));
    
    fread(rChannel, sizeof(float), size, gndf); // This channel stores the heightmap data
    fread(gChannel, sizeof(float), size, gndf);
    
    // Stripe the R and G channels
    for (uint32_t px = 0; px < size; px++) {
        fwrite(rChannel + px, sizeof(float), 1, hmdf);
        fwrite(gChannel + px, sizeof(float), 1, hmdf);
    }
    
    free(rChannel);
    free(gChannel);
    
    fclose(hmdf);
    
    uint8_t * texture = malloc(size * 4);
    fread(texture, sizeof(uint8_t), size * 4, gndf);

    fclose(gndf);

    // Swizzle the R and B channels
    for (int i = 0; i < size * 4; i+= 4) {
        uint8_t temp = texture[i];
        texture[i] = texture[i + 2];
        texture[i + 2] = temp;
    }
    
    strcpy(ext + 1, "tga");
    
    if (!stbi_write_tga(path, MAP_WIDTH, MAP_HEIGHT, 4, texture)) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    free(texture);
    
    return 0;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Terrain Tool - By QuantX\n");

    char progmode = 0;
    while (argc && **argv == '-') {
        (*argv)++;
        switch (**argv) {
        case 'h':
            printf("%s", helpmsg);
            return 0;
        case 'p':
        case 'u':
            progmode = **argv;
            break;
        }
    	argv++; argc--;
    }

    if (!progmode) {
        fprintf(stderr, "Please provide either '-h', '-p', or '-u', for help run: %s -h\n", progname);
        return 1;
    }

    if (!argc) {
        if (progmode == 'p') fprintf(stderr, "Please specify a map to pack\n");
        else if (progmode == 'u') fprintf(stderr, "Please specify a map unpack\n");
        return 1;
    }

    if (progmode == 'p') return pack(*argv);
    else if (progmode == 'u') return unpack(*argv);
    
    fprintf(stderr, "Unknown progmode: %c\n", progmode);
    return 1;
}
