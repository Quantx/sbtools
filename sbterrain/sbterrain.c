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

#define PATH_OUT_MAX 256
char out_path[PATH_OUT_MAX];

char * helpmsg = "Used pack and unpack SB bumpmap terrain files\n"
"Show help: sbterrain -h\n"
"Unpack map: sbterrain -u map00\n"
"Pack map: sbterrain -p map00\n";

char * progname;

int pack(char * path) {
    int pr = snprintf(out_path, PATH_OUT_MAX, "%s_texture.tga", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %s_texture.tga path\n", path);
        return 1;
    }
    
    int check_width, check_height, channels;
    uint8_t * texture = stbi_load(out_path, &check_width, &check_height, &channels, 4);
    if (!texture) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 1;
    }
    
    uint32_t width = check_width;
    uint32_t height = check_height;
    uint32_t size = width * height;
    
    printf("Packing map: width %d, height %d, size %d\n", width, height, size);
    
    if (width != MAP_WIDTH) printf("Warning non-standard map width %d, should be 280\n", width);
    if (height != MAP_HEIGHT) printf("Warning non-standard map height %d, should be 280\n", height);
    
    pr = snprintf(out_path, PATH_OUT_MAX, "%s_height.data", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %s_height.data path\n", path);
        return 1;
    }
    
    FILE * hmdf = fopen(out_path, "rb");
    if (!hmdf) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 1;
    }
    
    int hmp = 0;
    while (fgetc(hmdf) >= 0) hmp++;
    fseek(hmdf, 0, SEEK_SET);
    
    if (hmp == size) {
        hmp = 0;
        printf("8-bit precision heightmap\n");
    } else if (hmp == size * sizeof(uint16_t)) {
        hmp = 1;
        printf("16-bit precision heightmap\n");
    } else if (hmp == size * sizeof(float)) {
        hmp = 2;
        printf("32-bit float heightmap\n");
    } else {
        fprintf(stderr, "Heightmap size was %d bytes, expected %d for 8-bit percision or %d for 16-bit percision\n", hmp, size, size * 2);
        return 1;
    }
    
    pr = snprintf(out_path, PATH_OUT_MAX, "%s.gnd", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %s.gnd path\n", path);
        return 1;
    }
    
    FILE * gndf = fopen(out_path, "wb");
    if (!hmdf) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 1;
    }
    
    for (int i = 0; i < size; i++) {
        float elv;
        
        if (hmp == 0) {
            uint8_t sei;
            fread(&sei, sizeof(uint8_t), 1, hmdf);
            elv = sei * 100.0f;
        } else if (hmp == 1) {
            int16_t sei;
            fread(&sei, sizeof(int16_t), 1, hmdf);
            elv = sei * 100.0f;
        } else if (hmp == 2) {
            fread(&elv, sizeof(float), 1, hmdf);
        }
        
        fwrite(&elv, sizeof(float), 1, gndf);
    }
    
    fclose(hmdf);
    
    // Fill with zeros
    uint32_t magic = 0;
    for (int i = 0; i < size; i++) {
        fwrite(&magic, sizeof(uint32_t), 1, gndf);
    }
    
    // Swizzle the R and B channels
    for (int i = 0; i < size * 4; i += 4) {
        uint8_t temp = texture[i];
        texture[i] = texture[i + 2];
        texture[i + 2] = temp;
        
        fwrite(texture + i, sizeof(uint8_t), 4, gndf);
    }
    
    stbi_image_free(texture);
    fclose(gndf);
    
    pr = snprintf(out_path, PATH_OUT_MAX, "%s_torque.tga", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %s_torque.tga path\n", path);
        return 1;
    }
    
    uint8_t * gad_torque = stbi_load(out_path, &check_width, &check_height, &channels, 4);
    if (!texture) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 1;
    }
    
    if (check_width != width || check_height != height) {
        fprintf(stderr, "%s image dimensions %dx%d do not match map size of %dx%d\n", out_path, check_width, check_height, width, height);
        return 1;
    }
    
    pr = snprintf(out_path, PATH_OUT_MAX, "%s_magic.tga", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %s_magic.tga path\n", path);
        return 1;
    }
    
    uint8_t * gad_magic = stbi_load(out_path, &check_width, &check_height, &channels, 4);
    if (!texture) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 1;
    }
    
    if (check_width != width || check_height != height) {
        fprintf(stderr, "%s image dimensions %dx%d do not match map size of %dx%d\n", out_path, check_width, check_height, width, height);
        return 1;
    }
    
    pr = snprintf(out_path, PATH_OUT_MAX, "%shit.gad", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %shit.gad path\n", path);
        return 1;
    }
    
    FILE * gadf = fopen(out_path, "wb");
    if (!hmdf) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 1;
    }
    
    // Output size
    fwrite(&width,  sizeof(uint16_t), 1, gadf);
    fwrite(&height, sizeof(uint16_t), 1, gadf);
    
    for (int i = 0; i < size; i++) {
        uint8_t tv = gad_torque[i * 4];
        uint8_t mv = gad_magic[i * 4];
        
        tv /= 8;
        mv /= 64;
        
        uint32_t torque = 1 << tv;

        uint32_t magic;
        switch (mv) {
        case 0: magic = 0xC8000000; break;
        case 1: magic = 0xC9000000; break;
        case 2: magic = 0xCC000000; break;
        case 3: magic = 0xC8400000; break;
        default:
            fprintf(stderr, "Unknown magic value %d, %02X\n", mv, gad_magic[i * 4]);
            return 1;
        }
        
        fwrite(&torque, sizeof(uint32_t), 1, gadf);
        fwrite(&magic, sizeof(uint32_t), 1, gadf);
    }
    
    fclose(gadf);
    stbi_image_free(gad_torque);
    stbi_image_free(gad_magic);
    
    return 0;
}

int unpack(char * path) {
    int pr = snprintf(out_path, PATH_OUT_MAX, "%shit.gad", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate .gad path\n");
        return 1;
    }

    printf("Reading terrain attribute data from %s\n", out_path);
    
    FILE * gadf = fopen(out_path, "rb");
    if (!gadf) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 1;
    }
    
    uint16_t width, height;
    fread(&width, sizeof(uint16_t), 1, gadf);
    fread(&height, sizeof(uint16_t), 1, gadf);
    uint32_t size = (uint32_t)width * (uint32_t)height;
    
    printf("Width %d, Height %d, Size %d\n", width, height, size);
    if (width != MAP_WIDTH || height != MAP_HEIGHT) printf("Warning map was not 280x280 in size!\n");

    uint8_t * gad_data = calloc(size, 3);
    for (int i = 0; i < size; i++ ) {
        int pos = i * 3;
        
        uint32_t torque, magic;
        fread(&torque, sizeof(uint32_t), 1, gadf);
        fread(&magic,  sizeof(uint32_t), 1, gadf);
        
        // Magic
        uint8_t mv;
        switch (magic) {
        case 0xC8000000: mv = 0; break; // -131072.0f
        case 0xC9000000: mv = 1; break; // 2.81661e-43f
        case 0xCC000000: mv = 2; break; // 2.85865e-43f
        case 0xC8400000: mv = 3; break; // 2.32391e-41
        default:
            mv = 4;
            printf("Unknown magic %08X at %d\n", magic, i);
        }
        
        gad_data[pos] = mv * 64;
        
        // Torque
        uint8_t tv;
        for (tv = 0; tv < 32; tv++) {
            if ((torque >> tv) & 1) break;
        }
        
        // Check for invalid torques
        for (int te = tv + 1; te < 32; te++) {
            if ((torque >> te) & 1) {
                printf("Invalid torque %08X at %d\n", torque, i);
                break;
            }
        }

        gad_data[pos + 2] = tv * 8;
    }
    
    fclose(gadf);
    
    pr = snprintf(out_path, PATH_OUT_MAX, "%s_hit.tga", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %s_magic.tga path\n", path);
        return 1;
    }
    
    if (!stbi_write_tga(out_path, width, height, 3, gad_data)) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 1;
    }

    // *** Finished read .gad now try and read the .gnd file

    pr = snprintf(out_path, PATH_OUT_MAX, "%s.gnd", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate .gnd path\n");
        return 1;
    }

    FILE * gndf = fopen(out_path, "rb");
    if (!gndf) {
        printf("No heightmap data for this terrain could be found at %s\n", out_path);
        return 0;
    }
    
    printf("Reading terrain heightmap data from %s\n", out_path);
    
    pr = snprintf(out_path, PATH_OUT_MAX, "%s_height.dds", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %s_height.obj path\n", path);
        return 1;
    }
    
    FILE * hmdf = fopen(out_path, "wb");
    if (!hmdf) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 1;
    }
    
    struct dds_header header = DDS_HEADER_INIT;
    
    header.width = width;
    header.height = height;

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
    
    pr = snprintf(out_path, PATH_OUT_MAX, "%s_texture.tga", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %s_texture.tga path\n", path);
        return 1;
    }
    
    if (!stbi_write_tga(out_path, width, height, 4, texture)) {
        fprintf(stderr, "Failed to open %s\n", out_path);
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
