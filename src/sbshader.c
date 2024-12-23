#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __linux__
const char separator = '/';
#else
const char separator = '\\';
#endif

struct section_header {
    uint32_t flags;
    // Virtual Address
    uint32_t vaddr;
    uint32_t vsize;
    // File Address
    uint32_t faddr;
    uint32_t fsize;
    // Name Address
    uint32_t name_addr;
    uint32_t ref_count;
    // Shared Reference Count Address
    uint32_t head_ref_count_addr;
    uint32_t tail_ref_count_addr;
    // SHA-1 checksum
    uint8_t checksum[20];
};

struct D3DPIXELSHADERDEF {
    uint32_t PSAlphaInputs[8]; // Alpha inputs for each stage
    uint32_t PSFinalCombinerInputsABCD; // Final combiner inputs
    uint32_t PSFinalCombinerInputsEFG; // Final combiner inputs (continued)
    uint32_t PSConstant0[8]; // C0 for each stage
    uint32_t PSConstant1[8]; // C1 for each stage
    uint32_t PSAlphaOutputs[8]; // Alpha output for each stage
    uint32_t PSRGBInputs[8]; // RGB inputs for each stage
    uint32_t PSCompareMode; // Compare modes for clipplane texture mode
    uint32_t PSFinalCombinerConstant0; // C0 in final combiner
    uint32_t PSFinalCombinerConstant1; // C1 in final combiner
    uint32_t PSRGBOutputs[8]; // Stage 0 RGB outputs
    uint32_t PSCombinerCount; // Active combiner count (Stages 0-7)
    uint32_t PSTextureModes; // Texture addressing modes
    uint32_t PSDotMapping; // Input mapping for dot product modes
    uint32_t PSInputTexture; // Texture source for some texture modes
    uint32_t PSC0Mapping; // Mapping of c0 regs to D3D constants
    uint32_t PSC1Mapping; // Mapping of c1 regs to D3D constants
    uint32_t PSFinalCombinerConstants; // Final combiner constant mapping
};

char * progname;
char out_path[512];

#define VERTEX_SHADER_COUNT 71
#define FRAGMENT_SHADER_COUNT 79

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Shader Tool - By QuantX\n");

    if (!argc) {
        fprintf(stderr, "Please specify a cockpit file: %s <path/cockpit_file>\n", progname);
        return 1;
    }
    
    char * path = *argv++; argc--;
    
    strncpy(out_path, path, sizeof(out_path));
    char * sep = strrchr(out_path, separator);
    
    if (sep) strcpy(sep + 1, ".data.hdr");
    else strcpy(out_path, ".data.hdr");
    
    FILE * hdrf = fopen(out_path, "rb");
    if (!hdrf) {
        fprintf(stderr, "Failed to open file: %s\n", out_path);
        return 1;
    }
    
    struct section_header hdr_data;
    fread(&hdr_data, sizeof(struct section_header), 1, hdrf);
    
    fclose(hdrf);
    
    if (sep) strcpy(sep + 1, ".data.seg");
    else strcpy(out_path, ".data.seg");
    
    FILE * datf = fopen(out_path, "rb");
    if (!datf) {
        fprintf(stderr, "Failed to open file: %s\n", out_path);
        return 1;
    }
    
    uint32_t vertexShaderPointers[VERTEX_SHADER_COUNT];
    fseek(datf, 0xADE0, SEEK_SET); // Brainbox offset
    fread(vertexShaderPointers, sizeof(uint32_t), VERTEX_SHADER_COUNT, datf);
    
    for (int i = 0; i < VERTEX_SHADER_COUNT; i++) {
        fseek(datf, vertexShaderPointers[i] - hdr_data.vaddr, SEEK_SET);
        
        uint16_t header;
        uint16_t length;
        fread(&header, sizeof(uint16_t), 1, datf);
        fread(&length, sizeof(uint16_t), 1, datf);
        
        //length = length * 16 + ((length * 16 + 0x7F) >> 7) * 4; // This is ripped from the decompiler, optimize it
        
        if (!length) continue;
        
        printf("Extracting vertex shader %02d, header: %04X, instruction count: %d\n", i, header, length);
        
        length *= 4;
        
        char out_fname[16];
        snprintf(out_fname, sizeof(out_fname), "shader%02d.vrt", i);
        
        if (sep) strncpy(sep + 1, out_fname, sizeof(out_fname));
        else strncpy(out_path, out_fname, sizeof(out_fname));
        
        FILE * outf = fopen(out_path, "w");
        if (!outf) {
            fprintf(stderr, "Failed to open output file: %s\n", out_path);
            fclose(datf);
            return 1;
        }
        
        for (int j = 0; j < length; j++) {
            uint32_t word;
            fread(&word, sizeof(uint32_t), 1, datf);
            
            if (j && j % 4 == 0) fprintf(outf, "\n");
            fprintf(outf, "0x%08X", word);
            if (j < length - 1) fprintf(outf, ", ");
        }
        
        fclose(outf);
    }
    
    uint32_t fragmentShaderPointers[FRAGMENT_SHADER_COUNT];
    fseek(datf, 0xAF00, SEEK_SET); // Brainbox offset
    fread(fragmentShaderPointers, sizeof(uint32_t), FRAGMENT_SHADER_COUNT, datf);
    
    for (int i = 0; i < FRAGMENT_SHADER_COUNT; i++) {
        fseek(datf, vertexShaderPointers[i] - hdr_data.vaddr, SEEK_SET);
        
        struct D3DPIXELSHADERDEF psh;
        fread(&psh, sizeof(struct D3DPIXELSHADERDEF), 1, datf);
        
        printf("Extracting fragment shader %02d\n", i);
        
        char out_fname[16];
        snprintf(out_fname, sizeof(out_fname), "shader%02d.frg", i);
        
        if (sep) strncpy(sep + 1, out_fname, sizeof(out_fname));
        else strncpy(out_path, out_fname, sizeof(out_fname));
        
        FILE * outf = fopen(out_path, "w");
        if (!outf) {
            fprintf(stderr, "Failed to open output file: %s\n", out_path);
            fclose(datf);
            return 1;
        }
        
        fwrite(&psh, sizeof(struct D3DPIXELSHADERDEF), 1, outf);
        
        fclose(outf);
    }
    
    return 0;
}
