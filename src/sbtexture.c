#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "dds.h"

#include "swizzle.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

#define DXT_GET_R(COLOR) (COLOR & 0x001F)
#define DXT_GET_G(COLOR) ((COLOR & (0x003F << 5)) >> 5)
#define DXT_GET_B(COLOR) ((COLOR & (0x001F << 11)) >> 11)

#define FORMAT_DIMENSIONS(FORMAT_INFO) ((FORMAT_INFO & 0x00F0) >> 4)
#define FORMAT_TYPE(FORMAT_INFO) ((FORMAT_INFO & 0xFF00) >> 8)

#define FORMAT_LEVELS(FORMAT_DATA) (FORMAT_DATA & 0x000F)
#define FORMAT_WIDTH(FORMAT_DATA)  (1 << ((FORMAT_DATA & 0x00F0) >>  4))
#define FORMAT_HEIGHT(FORMAT_DATA) (1 << ((FORMAT_DATA & 0x0F00) >>  8))
#define FORMAT_DEPTH(FORMAT_DATA)  (1 << ((FORMAT_DATA & 0xF000) >> 12))
// 1F 0FF 1FF
#define FORMAT_MISC_WIDTH(FORMAT_MISC)   ((FORMAT_MISC & 0x00000FFF) + 1)
#define FORMAT_MISC_HEIGHT(FORMAT_MISC) (((FORMAT_MISC & 0x00FFF000) >> 12) + 1)
#define FORMAT_MISC_DEPTH(FORMAT_MISC)  (((FORMAT_MISC & 0xFF000000) >> 24) + 1)

#define FORMAT_ARGB     0x06
#define FORMAT_DXT1     0x0C
#define FORMAT_DXT3     0x0E
#define FORMAT_LIN_ARGB 0x12

char * progname;
bool objtex_patch;
bool dds_output;

#define PATH_OUT_MAX 256
char out_path[PATH_OUT_MAX];

void dxt1_decode(struct dxt1_block * dxt, uint8_t rgba[64], int isDXT1) {
    float c0[3] = {
        ((dxt->color0 & 0xF800) >> 11) / 32.0f,
        ((dxt->color0 & 0x07E0) >>  5) / 64.0f,
         (dxt->color0 & 0x001F) / 32.0f
    };
    
    float c1[3] = {
        ((dxt->color1 & 0xF800) >> 11) / 32.0f,
        ((dxt->color1 & 0x07E0) >>  5) / 64.0f,
         (dxt->color1 & 0x001F) / 32.0f
    };
    
    for (int i = 0; i < 16; i++) {
        uint8_t code = (dxt->codes >> (i * 2)) & 3;

        float c[3];
        uint8_t alpha = 0xFF;
        for (int j = 0; j < 3; j++) {
            switch (code) {
            case 0: c[j] = c0[j]; break;
            case 1: c[j] = c1[j]; break;
            case 2:
                if (!isDXT1 || dxt->color0 > dxt->color1) {
                    c[j] = (2.0f * c0[j] + c1[j]) / 3.0f;
                } else {
                    c[j] = (c0[j] + c1[j]) / 2.0f;
                }
                break;
            case 3:
                if (!isDXT1 || dxt->color0 > dxt->color1) {
                    c[j] = (c0[j] + 2.0f * c1[j]) / 3.0f;
                } else {
                    c[j] = 0.0f;
                    if (isDXT1) alpha = 0;
                }
            }
        }
        
        int out_pos = i * 4;
        
        rgba[out_pos] = c[0] * 255.0f;
        rgba[out_pos + 1] = c[1] * 255.0f;
        rgba[out_pos + 2] = c[2] * 255.0f;
        rgba[out_pos + 3] = alpha;
    }
}

void dxt3_decode(struct dxt3_block * dxt, uint8_t rgba[64]) {
    dxt1_decode(&dxt->dxt1, rgba, 0);
    
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t a = (dxt->alpha >> i) & 0xF;
        rgba[i + 3] = (a << 4) | a;
    }
}

// The DXT library can't handle DXT1 with alpha, so we need to help it out 
void dxt1_encode(struct dxt1_block * dxt, uint8_t rgba[64]) {
    // Find the average color of all visible pixels
    uint16_t r = 0, g = 0, b = 0, opaque = 0;
    for (int i = 0; i < 64; i += 4) {
        if (rgba[i + 3] <= 127) continue;
        
        r += rgba[i    ];
        g += rgba[i + 1];
        b += rgba[i + 2];
        opaque++;
    }
    
    // No visible pixels, generate a non-visible block
    if (!opaque) {
        dxt->color0 = 0;
        dxt->color1 = 0xFFFF;
        dxt->codes = 0xFFFFFFFFul;
        return;
    }

    r /= opaque;
    g /= opaque;
    b /= opaque;
    
    // Force all non-visible pixels to be the average color
    uint8_t data[64];
    memcpy(data, rgba, 64);
    for (int i = 0; i < 64; i += 4) {
        if (data[i + 3] <= 127) {
            data[i    ] = r;
            data[i + 1] = g;
            data[i + 2] = b;
        }
        data[i + 3] = 0xFF;
    }

    // Don't let this library do the alpha computation, it's doing it wrong!
    stb_compress_dxt_block((unsigned char *)dxt, data, 0, STB_DXT_HIGHQUAL);
    
    // All pixesl are visible, so skip alpha computation
    if (opaque == 16) return;
    
    // Force transparent mode for this block
    uint32_t wasOpaque = dxt->color0 > dxt->color1;
    if (wasOpaque) {
        uint16_t temp = dxt->color0;
        dxt->color0 = dxt->color1;
        dxt->color1 = temp;
    }
    
    uint32_t codes = dxt->codes;
    dxt->codes = 0;
    for (int i = 60; i >= 0; i -= 4) {
        uint32_t c = (codes >> (i / 2)) & 3;
        
        if (rgba[i + 3] <= 127) { // Check if pixel is opaque
            c = 3;
        } else if (c == 3) { // Fix conflicting codes
            c = wasOpaque ? 2 : 0;
        }
        
        dxt->codes |= c << (i / 2);
    }
}

void dxt3_encode(struct dxt3_block * dxt, uint8_t rgba[64]) {
    uint8_t data[64];
    memcpy(data, rgba, 64);
    // Do a small optimization for the library
    for (int i = 3; i < 64; i += 4) data[i] = 0xFF;

    // Don't let this library do the alpha computation, it's doing it wrong!
    stb_compress_dxt_block((unsigned char *)&dxt->dxt1, data, 0, STB_DXT_HIGHQUAL);

    dxt->alpha = 0;
    for (int i = 60; i >= 0; i -= 4) {
        uint64_t a = (rgba[i + 3] >> 4) & 0xF;
    
        dxt->alpha |= a << i;
    }
}

void objtex_blit(uint8_t * img_data, int width, int height, int src_x, int src_y, int dest_x, int dest_y, int dest_w, int dest_h) {
    int off_x = dest_x - src_x;
    int off_y = dest_y - src_y;
    for (int y = dest_y; y < dest_y + dest_h; y++) {
        for (int x = dest_x; x < dest_x + dest_w; x++) {
            int dst_idx = (y * width + x) * 4;
            int src_idx = ((y - off_y) * width + x - off_x) * 4;
            
            for (int i = 0; i < 4; i++) {
                img_data[dst_idx + i] = img_data[src_idx + i];
            }
        }
    }
}

int readXPRHeader(FILE * xprf, uint32_t * data_start, uint32_t * data_size, uint8_t * format, uint16_t * width, uint16_t * height, uint16_t * levels) {
    if (!xprf) return 1;
    
    // Verify file format
    char xpr_magic[4];
    fread(xpr_magic, sizeof(uint8_t), 4, xprf);
    
    if (strncmp(xpr_magic, "XPR0", 4)) {
        fprintf(stderr, "Not an XPR0 texture file\n");
        return 1;
    }
    
    // Read file size
    uint32_t file_size;
    fread(&file_size, sizeof(uint32_t), 1, xprf);
    
    // Read data start position
    fread(data_start, sizeof(uint32_t), 1, xprf);
    
    uint16_t ref_count;
    fread(&ref_count, sizeof(uint16_t), 1, xprf);
    
    // Validate XPR type
    uint16_t xpr_type;
    fread(&xpr_type, sizeof(uint16_t), 1, xprf);
    
    if ((xpr_type & 7) != 4) {
        fprintf(stderr, "XPR file does not contain a texture, type: %d\n", xpr_type & 7);
        return 1;
    }
    
    if (file_size == *data_start) {
        // File contains no data so we're done (this is not an error)
        printf("File does not contain any image data, nothing to convert\n");
        return -1;
    }
    
    // Read format
    fseek(xprf, 8, SEEK_CUR);
    
    uint16_t format_info;
    fread(&format_info, sizeof(uint16_t), 1, xprf);
    uint16_t format_data;
    fread(&format_data, sizeof(uint16_t), 1, xprf);
    
    uint32_t format_misc;
    fread(&format_misc, sizeof(uint32_t), 1, xprf);
    
    *format = FORMAT_TYPE(format_info);
    
    char * format_str;
    if (*format == FORMAT_DXT1)  format_str = "DXT1";
    else if (*format == FORMAT_DXT3) format_str = "DXT3";
    else if (*format == FORMAT_LIN_ARGB) format_str = "RGBA";
    else if (*format == FORMAT_ARGB) format_str = "RGBA_SWIZZLE";
    else {
        fprintf(stderr, "Unknown texture format %02X for file\n", *format);
        return 1;
    }
    
    uint16_t dimensions = FORMAT_DIMENSIONS(format_info);
    if (dimensions != 2) {
        fprintf(stderr, "Texture had %d dimensions instead of 2\n", dimensions);
        return 1;
    }
    
    *width  = FORMAT_WIDTH(format_data);
    *height = FORMAT_HEIGHT(format_data);
    uint16_t depth  = FORMAT_DEPTH(format_data);
    *levels = FORMAT_LEVELS(format_data);
    
    if (*format == FORMAT_LIN_ARGB) {
        *width = FORMAT_MISC_WIDTH(format_misc);
        *height = FORMAT_MISC_HEIGHT(format_misc);
        depth = FORMAT_MISC_DEPTH(format_misc);
    }
    
    printf("Format: %s, Width: %d, Height: %d, Depth %d, Levels: %d\n", format_str, *width, *height, depth, *levels);
    
    if (*format == FORMAT_DXT1) *data_size = (*width * *height) / 2;
    else if (*format == FORMAT_DXT3) *data_size = *width * *height;
    else if (*format == FORMAT_LIN_ARGB || *format == FORMAT_ARGB) *data_size = *width * *height * 4;
    
    if (*data_size + *data_start > file_size) {
        fprintf(stderr, "Computed data size %d exceeds file size %d\n", *data_size + *data_start, file_size);
        return 1;
    } else if (*data_size + *data_start < file_size) {
//        printf("%d bytes of extra data is present in the file\n", file_size - (*data_size + *data_start));
    }
    
    return 0;
}

int makeTGA(char * path) {
    // Open XPR file
    FILE * sbtex = fopen(path, "rb");
    if (!sbtex) {
        fprintf(stderr, "Could not open file: %s\n", path);
        return 1;
    }
    
    printf("Converting texture %s to TGA\n", path);

    uint32_t data_start, dxt_data_size;
    uint16_t width, height, levels;
    uint8_t format;
    int xpr_res = readXPRHeader(sbtex, &data_start, &dxt_data_size, &format, &width, &height, &levels);
    if (xpr_res) {
        fclose(sbtex);
        return xpr_res > 0 ? xpr_res : 0;
    }
    
    // Read data from XPR
    fseek(sbtex, data_start, SEEK_SET);
    
    void * dxt_data = malloc(dxt_data_size);
    uint32_t dxt_data_read = fread(dxt_data, sizeof(uint8_t), dxt_data_size, sbtex);
    if (dxt_data_read != dxt_data_size) {
        fprintf(stderr, "Unexpected end-of-file while reading image data\n");
        fclose(sbtex);
        return 1;
    }
    
    uint8_t * out_data = malloc(width * height * 4);
    
    printf("Converting %d bytes\n", dxt_data_read);
    
    int dxt_pos = 0, bx = 0, by = 0;
    while (dxt_pos < dxt_data_size) {
        uint8_t rgba[64];
        
        if (format == FORMAT_DXT1) {
            dxt1_decode((struct dxt1_block *)(dxt_data + dxt_pos), rgba, 1);
            dxt_pos += sizeof(struct dxt1_block);
        } else if (format == FORMAT_DXT3) {
            dxt3_decode((struct dxt3_block *)(dxt_data + dxt_pos), rgba);
            dxt_pos += sizeof(struct dxt3_block);
        } else if (format == FORMAT_LIN_ARGB || format == FORMAT_ARGB) {
            out_data[dxt_pos] = ((uint8_t *)dxt_data)[dxt_pos];
            dxt_pos++;
            continue;
        }
        
        int bp = 0;
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                int out_pos = ((by + y) * width + (bx + x)) * 4;
                
                // Copy the RGBA components of this pixel
                for (int p = 0; p < 4; p++) {
                    out_data[out_pos + p] = rgba[bp + p];
                }
                
                bp += 4;
            }
        }
        
        // Increment block position
        bx += 4;
        if (bx >= width) { 
            bx = 0;
            by += 4;
        }
    }
    
    free(dxt_data);
    
    if (objtex_patch) {
        objtex_blit(out_data, width, height, 1075, 1820, 1685, 1732, 15, 101);
        objtex_blit(out_data, width, height, 1075, 1820, 1685, 1220, 15, 101);
    }
    
    char * ext = strrchr(path, '.') + 1;
    strcpy(ext, "tga");
    
    if (!stbi_write_tga(path, width, height, 4, out_data)) {
        fprintf(stderr, "Failed to output TGA file\n");
        return 1;
    }
    
    free(out_data);
    fclose(sbtex); 
    return 0;
}

int makeXPR(char * path) {
    int width, height, channels;
    uint8_t * img = stbi_load(path, &width, &height, &channels, 4);
    if (!img) {
        fprintf(stderr, "Failed to open image %s\n", path);
        return 1;
    }
    
    // Remove file extension
    char * ext = strrchr(path, '.');
    *ext = '\0';
    
    int pr = snprintf(out_path, PATH_OUT_MAX, "%s.xpr", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %s.xpr path\n", path);
        stbi_image_free(img);
        return 1;
    }
    
    *ext = '.';
    
    FILE * sbtex = fopen(out_path, "rb+");
    if (!sbtex) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        stbi_image_free(img);
        return 1;
    }
    
    uint32_t data_start, data_size;
    uint16_t xpr_width, xpr_height, xpr_levels;
    uint8_t format;
    int xpr_res = readXPRHeader(sbtex, &data_start, &data_size, &format, &xpr_width, &xpr_height, &xpr_levels);
    if (xpr_res) {
        fclose(sbtex);
        stbi_image_free(img);
        return xpr_res > 0 ? xpr_res : 0;
    }
    
    if (xpr_width != width || xpr_height != height) {
        fprintf(stderr, "XPR texture is %dx%d, source image is %dx%d, cannot convert!\n", xpr_width, xpr_height, width, height);
        fclose(sbtex);
        stbi_image_free(img);
        return 1;
    }

    printf("Converting %s to XPR texture\n", path);
    
    // Write data to XPR
    fseek(sbtex, data_start, SEEK_SET);
    
    // RGBA doesn't have mipmaps, just write the data and finish
    if (format == FORMAT_LIN_ARGB) {
        fwrite(img, sizeof(uint8_t), data_size, sbtex);
        fclose(sbtex);
        stbi_image_free(img);
        return 0;
    }
    
    for (int l = xpr_levels; l > 0; l--) {
        printf("Converting mipmap level %d, width %d, height %d\n", l, width, height);
        int data_pos = 0, bx = 0, by = 0;
        while (data_pos < data_size) {
            uint8_t rgba[64];
            
            int bp = 0;
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int in_pos = ((by + y) * width + (bx + x)) * 4;
                    
                    // Copy the RGBA components of this pixel
                    for (int p = 0; p < 4; p++) {
                        rgba[bp + p] = img[in_pos + p];
                    }
                    
                    bp += 4;
                }
            }
            
            // Increment block position
            bx += 4;
            if (bx >= width) { 
                bx = 0;
                by += 4;
            }
            
            if (format == FORMAT_DXT1) {
                struct dxt1_block dxt1 = {0};
                dxt1_encode(&dxt1, rgba);
                fwrite(&dxt1, sizeof(struct dxt1_block), 1, sbtex);
                
                data_pos += sizeof(struct dxt1_block);
            } else if (format == FORMAT_DXT3) {
                struct dxt3_block dxt3 = {0};
                dxt3_encode(&dxt3, rgba);
                fwrite(&dxt3, sizeof(struct dxt3_block), 1, sbtex);
                
                data_pos += sizeof(struct dxt3_block);
            }
        }
        
        // Enforce minimum block size
        uint32_t dw = width >= 8 ? 2 : 1;
        uint32_t dh = height >= 8 ? 2 : 1;
        
        if (dw == 2 || dh == 2) {
            uint32_t mipmap_w = width / dw;
            uint32_t mipmap_h = height / dh;
            
            if (mipmap_w < 4 || mipmap_h < 4) {
                fprintf(stderr, "Mipmap %dx%d less than 4x4!\n", mipmap_w, mipmap_h);
                return 1;
            }
            
            uint8_t * mipmap = malloc(mipmap_w * mipmap_h * 4);
            int mipmap_y = 0;
            for (int y = 0; y < height; y += dh) {
                int mipmap_x = 0;
                for (int x = 0; x < width; x += dw) {
                
                    // Get average color of pixels in region
                    int rgba[4], cc = 0;
                    for (int py = 0; py < dh; py++) {
                        for (int px = 0; px < dw; px++) {
                            int in_pos = ((y + py) * width + (x + px)) * 4;
                            for (int pc = 0; pc < 4; pc++) {
                                rgba[pc] += img[in_pos + pc];
                            }
                            cc++;
                        }
                    }
                    
                    rgba[0] /= cc;
                    rgba[1] /= cc;
                    rgba[2] /= cc;
                    rgba[3] /= cc;
                    
                    int out_pos = (mipmap_y * mipmap_w + mipmap_x) * 4;
                    for (int pc = 0; pc < 4; pc++) {
                        if (out_pos + pc > mipmap_w * mipmap_h * 4) {
                            fprintf(stderr, "Out of bounds [%d, %d], (%d, %d)!\n", mipmap_w, mipmap_h, mipmap_x, mipmap_y);
                            return 1;
                        }
                        mipmap[out_pos + pc] = rgba[pc];
                    }
                    
                    mipmap_x++;
                }
                mipmap_y++;
            }

            width = mipmap_w;
            height = mipmap_h;
            
            if (l == xpr_levels) stbi_image_free(img);
            else free(img);
            
            img = mipmap;
            
            if (format == FORMAT_DXT1) data_size = (width * height) / 2;
            else if (format == FORMAT_DXT3) data_size = width * height;
        }
    }
    
    int padding = ftell(sbtex) % 2048;
    if (padding) {
        padding = 2048 - padding;
        for (int p = 0; p < padding; p++) fputc(0xAD, sbtex);
    }
    
    printf("Wrote %ld bytes\n", ftell(sbtex));
    
    free(img);
    fclose(sbtex);
    return 0;
}

int makeDDS(char * path) {
    // Open XPR file
    FILE * xpr = fopen(path, "rb");
    if (!xpr) {
        fprintf(stderr, "Could not open file: %s\n", path);
        return 1;
    }
    
    // Verify file format
    char xpr_magic[4];
    fread(xpr_magic, sizeof(char), 4, xpr);
    
    if (strncmp(xpr_magic, "XPR0", 4)) {
        fclose(xpr);
        fprintf(stderr, "Not an XPR0 texture file\n");
        return 1;
    }
    
    uint32_t file_end;
    fread(&file_end, sizeof(uint32_t), 1, xpr);
    uint32_t file_start;
    fread(&file_start, sizeof(uint32_t), 1, xpr);
    
    if (file_end <= file_start) {
        printf("XPR file is empty, nothing to convert\n");
        return 0;
    }
    
    uint32_t file_size = file_end - file_start;
    
    uint16_t ref_count;
    fread(&ref_count, sizeof(uint16_t), 1, xpr);
    if (ref_count != 1) {
        fclose(xpr);
        fprintf(stderr, "XPR ref count was not 1\n");
        return 1;
    }
    
    uint16_t xpr_type;
    fread(&xpr_type, sizeof(uint16_t), 1, xpr);
    if (xpr_type != 4) {
        fclose(xpr);
        fprintf(stderr, "XPR type was not 4\n");
        return 1;
    }
    
    uint32_t zero;
    fread(&zero, sizeof(uint32_t), 1, xpr);
    if (zero) {
        fclose(xpr);
        fprintf(stderr, "XPR non-zero value %08X at 0x14\n", zero);
        return 1;
    }
    
    fread(&zero, sizeof(uint32_t), 1, xpr);
    if (zero) {
        fclose(xpr);
        fprintf(stderr, "XPR non-zero value %08X at 0x14\n", zero);
        return 1;
    }
    
    uint16_t xpr_info;
    fread(&xpr_info, sizeof(uint16_t), 1, xpr);
    
    uint16_t xpr_format = FORMAT_TYPE(xpr_info);
    if (FORMAT_DIMENSIONS(xpr_info) != 2) {
        fclose(xpr);
        fprintf(stderr, "XPR isn't 2 dimensional\n");
        return 1;
    }
    
    uint16_t xpr_data;
    fread(&xpr_data, sizeof(uint16_t), 1, xpr);
    uint32_t xpr_misc;
    fread(&xpr_misc, sizeof(uint32_t), 1, xpr);
    
    int32_t terminator;
    fread(&terminator, sizeof(int32_t), 1, xpr);
    if (terminator != -1) {
        fclose(xpr);
        fprintf(stderr, "Expected XPR terminator at 0x20, got %08X\n", terminator);
        return 1;
    }
    
    char * ext = strrchr(path, '.') + 1;
    strcpy(ext, "dds");
    
    FILE * dds = fopen(path, "wb");
    if (!dds) {
        fclose(xpr);
        fprintf(stderr, "Coult not open output file: %s\n", path);
        return 1;
    }
    
    printf("Converting texture %s to DDS\n", path);
    
    struct dds_header header = DDS_HEADER_INIT;
    
    header.levels = FORMAT_LEVELS(xpr_data);
    
    uint32_t block_w;
    uint32_t block_h;
    
    if (xpr_format == FORMAT_LIN_ARGB || xpr_format == FORMAT_ARGB) {
        header.flags |= 0x8; // Uncompressed texture pitch
        header.format.flags |= 0x1 | 0x40; // Alpha Channel | Uncompressed Texture
    
        if (xpr_format == FORMAT_LIN_ARGB) {
            header.height = FORMAT_MISC_HEIGHT(xpr_misc);
            header.width = FORMAT_MISC_WIDTH(xpr_misc);
            header.depth = FORMAT_MISC_DEPTH(xpr_misc);
        } else {
            header.height = FORMAT_HEIGHT(xpr_data);
            header.width = FORMAT_WIDTH(xpr_data);
            header.depth = FORMAT_DEPTH(xpr_data);
        }
        
        block_w = header.width;
        block_h = header.height;
        
        header.format.rgbBitCount = 32;
        
        header.pitch = (header.width * header.height * header.format.rgbBitCount + 7) / 8;

        header.format.aBitMask = 0xff000000;
        header.format.rBitMask = 0x00ff0000;
        header.format.gBitMask = 0x0000ff00;
        header.format.bBitMask = 0x000000ff;
    } else if (xpr_format == FORMAT_DXT1 || xpr_format == FORMAT_DXT3) {
        header.flags |= 0x80000; // Compressed texture linear size
        header.format.flags |= 0x4; // Compressed Texture
        
        header.height = FORMAT_HEIGHT(xpr_data);
        header.width = FORMAT_WIDTH(xpr_data);
        header.depth = FORMAT_DEPTH(xpr_data);
        
        block_w = (header.width + 3) / 4;
        block_h = (header.height + 3) / 4;
        
        header.pitch = block_w * block_h * 16;
        
        // DXT1 will be converted to DXT3
        memcpy(header.format.codeStr, "DXT3", 4);
    } else {
        fprintf(stderr, "Unknown XPR image format: %02X\n", xpr_format);
        fclose(xpr);
        fclose(dds);
        return 1;
    }

/*    
    if (header.depth > 1) {
        header.flags |= 0x800000; // Depth texture
//        fclose(xpr);
//        fclose(dds);
//        fprintf(stderr, "XPR file is a depth texture\n");
//        return 1;
    } else {
        header.depth = 0; // Not a depth texture
    }
*/    
    
    uint32_t data_size = 0;
    uint32_t levels = 1;
    uint32_t level_w = block_w;
    uint32_t level_h = block_h;
    while (true) {
        uint32_t level_size;
        if (xpr_format == FORMAT_LIN_ARGB || xpr_format == FORMAT_ARGB) {
            level_size = level_w * level_h * 4;
        } else if (xpr_format == FORMAT_DXT1) {
            level_size = level_w * level_h * 8;
        } else if (xpr_format == FORMAT_DXT3) {
            level_size = level_w * level_h * 16;
        }

        data_size += level_size;
        
        // Last mip map size
        if (level_w == 1 && level_h == 1) break;
        
        if (level_w > 1) level_w /= 2;
        if (level_h > 1) level_h /= 2;
        levels++;
    }

    // Not enough data
    if (data_size > file_size) {
        printf("Not enough data to satisfy all mipmap sizes for %dx%d, claimed %d, actual %d\n", header.width, header.height, header.levels, levels);
        
        // Disable mipmaps for this texture
        if (xpr_format == FORMAT_DXT1) data_size = block_w * block_h * 8;
        else data_size = header.pitch;
        
        header.levels = 1;
    }

    if (header.levels != levels) {
        printf("Mipmap level count missmatch: claimed %d, actual %d\n", header.levels, levels);
//        return 1;
    }

    // Only square textures should have mipmaps for some reason    
    if (header.width != header.height) header.levels = 1;

    printf("Format %02X, Size %dx%d depth %d, pitch %d, mipmaps: %d\n",
        xpr_format, header.width, header.height, header.depth, header.pitch, header.levels);


    if (header.levels > 1) header.flags |= 0x20000; // Mipmaps are present

    // Write magic string and header
    fputs("DDS ", dds);
    fwrite(&header, sizeof(struct dds_header), 1, dds);
    fseek(xpr, file_start, SEEK_SET);
    
    if (xpr_format == FORMAT_DXT1) {
        // Convert to DXT3
        struct dxt3_block block;
        for (uint32_t pos = 0; pos < data_size; pos += sizeof(struct dxt1_block)) {
            if (fread(&block.dxt1, sizeof(struct dxt1_block), 1, xpr) != 1) {
                fclose(xpr);
                fclose(dds);
                fprintf(stderr, "Unexpected EOF while reading XPR data\n");
                return 1;
            }
            
            if (block.dxt1.color0 > block.dxt1.color1) { // Opaque
                block.alpha = UINT64_MAX;
            } else {
                block.alpha = 0;
                for (int c = 0, a = 0; c < 32; c += 2, a += 4) {
                    // Set opaque pixels as needed
                    if (((block.dxt1.codes >> c) & 3u) != 3u) {
                        block.alpha |= 0xFULL << a;
                    }
                }
            }
            
            fwrite(&block, sizeof(struct dxt3_block), 1, dds);
        }
    } else if (xpr_format == FORMAT_ARGB) {
        uint32_t texture_size = header.width * header.height * 4;
        // Unswizzle data
        uint8_t * xpr_texture = malloc(texture_size);
        uint8_t * dds_texture = malloc(texture_size);
        
        fread(xpr_texture, sizeof(uint8_t), texture_size, xpr);
        
        unswizzle_rect(
            xpr_texture, header.width, header.height,
            dds_texture, header.width * 4, 4
        );
        
        fwrite(dds_texture, sizeof(uint8_t), texture_size, dds);
        
        free(xpr_texture);
        free(dds_texture);
    } else {
        // Copy data
        for (uint32_t pos = 0; pos < data_size; pos++) {
            int byte = fgetc(xpr);
            if (byte == EOF) {
                fclose(xpr);
                fclose(dds);
                fprintf(stderr, "Unexpected EOF while reading XPR data\n");
                return 1;
            }
            fputc(byte, dds);
        }
    }
    
    fclose(xpr);
    fclose(dds);
    return 0;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Texture Tool - By QuantX\n");

    if (!argc) {
        fprintf(stderr, "Please specify either a .tgx or .xpr file: %s <path/example.xpr>\n", progname);
        return 1;
    }
    
    if (argc == 2) {
        char * opt = *argv++; argc--;
        if (*opt++ != '-') {
            fprintf(stderr, "Invalid argument flag: %s\n", opt);
            return 1;
        }
        
        if (*opt == 'd') {
            dds_output = true;
        } else if (*opt == 'p') {
            objtex_patch = true;
        }
    }
    
    char * path = *argv++; argc--;
    
    char * ext = strrchr(path, '.');
    
    if (!ext) {
        fprintf(stderr, "Missing file extension in path: %s\n", path);
        return 1;
    }
    
    ext++;
    
    if (!strncmp(ext, "xpr", 3)) {
        if (dds_output) return makeDDS(path);
        return makeTGA(path);
    }
    return makeXPR(path);
}
