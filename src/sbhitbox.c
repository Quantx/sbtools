#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define HEADER_SIZE 16

struct entry {
    int32_t pos[3];
    int16_t verts[3][3];
    uint16_t padding;
} __attribute__((__packed__));

char * progname;
char out_path[256];
char * ppd_path;
char * gltf_path;

const float scale_factor = 100.0f;

uint32_t vert_count = 0;
float verts[20000];

uint32_t quad_count = 0;
uint32_t quad_flags0[4000];
uint32_t quad_flags1[4000];

void write_verts(FILE * outf) {
    printf("Writing %d quad attributes\n", quad_count);
    
    fwrite(&quad_count, sizeof(uint32_t), 1, outf);
    fwrite(quad_flags0, sizeof(uint32_t), quad_count, outf);
    fwrite(quad_flags1, sizeof(uint32_t), quad_count, outf);
    quad_count = 0;

    printf("Writing %d verts\n", vert_count);
    
    fwrite(&vert_count, sizeof(uint32_t), 1, outf);
    fwrite(verts, 3 * sizeof(float), vert_count, outf);
    vert_count = 0;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Hitbox Tool - By QuantX\n");

    if (argc < 2) {
        fprintf(stderr, "Please specify a PPD file: %s <path/example.ppd> <path/example.gltf>\n", progname);
        return 1;
    }
    
    ppd_path = *argv++; argc--;
    gltf_path = *argv++; argc--;
    
    FILE * ppd = fopen(ppd_path, "rb");
    if (!ppd) {
        fprintf(stderr, "Failed to open PPD file: %s\n", ppd_path);
        return 1;
    }
    
    cgltf_options options = {0};
    cgltf_data * data = NULL;
    cgltf_result result = cgltf_parse_file(&options, gltf_path, &data);
    if (result != cgltf_result_success) {
        fclose(ppd);
        fprintf(stderr, "Failed to open glTF file: %s\n", gltf_path);
        return 1;
    }

    uint32_t code;
    fread(&code, sizeof(uint32_t), 1, ppd);
    printf("Code: %08X | %d\n", code, code);
    
    uint32_t parts;
    fread(&parts, sizeof(uint32_t), 1, ppd);

    printf("Processing %d hitbox parts\n", parts);

    uint32_t zero;
    fread(&zero, sizeof(uint32_t), 1, ppd);
    if (zero) printf("Non-zero value at 0x8: %08X\n", zero);
    
    fread(&zero, sizeof(uint32_t), 1, ppd);
    if (zero) printf("Non-zero value at 0x12: %08X\n", zero);
    
    uint32_t offsets[parts];
    fread(offsets, sizeof(uint32_t), parts, ppd);
    
    uint32_t bone_offset;
    fread(&bone_offset, sizeof(uint32_t), 1, ppd);
    
    fseek(ppd, HEADER_SIZE + offsets[0], SEEK_SET);
    
    strncpy(out_path, ppd_path, sizeof(out_path));

    char * ext = strrchr(out_path, '.');
    if (!ext) {
        fprintf(stderr, "File path is missing extension: %s\n", ppd_path);
        return 1;
    }
    strcpy(ext + 1, "hbx");
    
    FILE * outf = fopen(out_path, "wb");
    if (!outf) {
        fprintf(stderr, "Failed to open output file: %s\n", out_path);
        return 1;
    }
    
    fwrite(&parts, sizeof(uint32_t), 1, outf);
    
    int current_part = 0;
    
    bool done = false;
    while (true) {
        long current_offset = ftell(ppd) - HEADER_SIZE;
        if (bone_offset && current_offset >= bone_offset) {
            done = true;
            break;
        }
        
        if (current_part < parts - 1 && current_offset >= offsets[current_part + 1]) {
            write_verts(outf);
            current_part++;
        }

        while (true) {
            struct entry test;        
            if (fread(&test, sizeof(struct entry), 1, ppd) != 1) {
                done = true;
                break;
            }
            
            // This is such a dumb hack
            if (test.pos[0] == -1 &&
                (test.padding
                || test.pos[1] < -1000000
                || test.pos[1] >  1000000
                || test.pos[2] < -1000000
                || test.pos[2] >  1000000)) break;
            
            printf("POS (%d, %d, %d)", test.pos[0], test.pos[1], test.pos[2]);
            for (int i = 0; i < 3; i++) {
                printf(" | VERT%d (%d, %d, %d)", i, test.verts[i][0], test.verts[i][1], test.verts[i][2]);
            }
            
            if (test.padding) {
                printf(" | PADDING: %04X", test.padding);
            }
            
            printf("\n");
        }
        
        if (done) break;

        fseek(ppd, sizeof(uint32_t) - sizeof(struct entry), SEEK_CUR);

        float * vptr = verts + (vert_count * 3);
        fread(vptr, sizeof(float), 12, ppd);
        
        for (int i = 0; i < 12; i++) {
            vptr[i] /= scale_factor;
        }
        
        fread(&zero, sizeof(uint32_t), 1, ppd);
        if (zero) printf("Non-zero 0 value: %08X\n", zero);
        
        uint32_t * flags0 = quad_flags0 + quad_count;
        fread(flags0, sizeof(uint32_t), 1, ppd);
        printf("Flags 0: %08X\n", *flags0);
        
        uint32_t * flags1 = quad_flags1 + quad_count;
        fread(flags1, sizeof(uint32_t), 1, ppd);
        printf("Flags 1: %08X\n", *flags1);
        
        quad_count++;
        
        float norm[3];
        fread(norm, sizeof(float), 3, ppd);
        printf("NORM (%f, %f, %f)\n", norm[0], norm[1], norm[2]);
        
        fread(&zero, sizeof(uint32_t), 1, ppd);
        if (zero) printf("Non-zero 1 value: %08X\n", zero);
        
        // Finish 4th triangle
        vptr[12] = vptr[3];
        vptr[13] = vptr[4];
        vptr[14] = vptr[5];
        
        vptr[15] = vptr[6];
        vptr[16] = vptr[7];
        vptr[17] = vptr[8];
    
        // Reverse the winding order
        float x = vptr[0];
        float y = vptr[1];
        float z = vptr[2];
        
        vptr[0] = vptr[3];
        vptr[1] = vptr[4];
        vptr[2] = vptr[5];
        
        vptr[3] = x;
        vptr[4] = y;
        vptr[5] = z;
        
        vert_count += 6;
        //fprintf(outf, "f %d %d %d\n", verts, verts + 1, verts + 2);
        //fprintf(outf, "f %d %d %d\n\n", verts + 3, verts + 2, verts + 1);
        
        printf("\n");
    }
    
    write_verts(outf);
    
    if (bone_offset) {
        fseek(ppd, HEADER_SIZE + bone_offset, SEEK_SET);
        
        uint8_t bone_count, bone_parts;
        fread(&bone_count, sizeof(uint8_t), 1, ppd);
        fread(&bone_parts, sizeof(uint8_t), 1, ppd);
        
        printf("Reading data for %d bones, %d parts\n", bone_count, bone_parts);
        
        if (bone_parts != parts) {
            printf("Wrong number of parts in bone data: %d != %d\n", bone_parts, parts);
        }
        
        fwrite(&bone_parts, sizeof(uint8_t), 1, outf);

        if (!data->skins_count) {
            fprintf(stderr, "GLTF does not contain any skins");
            return 1;        
        }
        
        cgltf_skin * skin = data->skins;
        if (skin->joints_count < bone_count) {
            fprintf(stderr, "GLTF has %ld bones, not %d\n", skin->joints_count, bone_count);
            return 1;
        }
        
        int part_count = 0;
        for (int i = 0; i < bone_count; i++) {
            uint8_t bone_id, part_id;
            fread(&bone_id, sizeof(uint8_t), 1, ppd);
            fread(&part_id, sizeof(uint8_t), 1, ppd);

            printf("Index %d, Bone ID %d, Part ID %d\n", i, bone_id, part_id);
            
            if (part_id == 99) continue;

            // There are technically "bone_count + 1" bones including the root bone
            if (bone_id > bone_count) {
                fprintf(stderr, "Bone ID %d exceeded bone count of %d\n", bone_id, bone_count);
                return 1;
            }
            if (part_id >= bone_parts) {
                fprintf(stderr, "Part ID %d exceeded part count of %d\n", part_id, bone_parts);
                return 1;
            }

            cgltf_node * bone = skin->joints[bone_id];
            
            uint32_t name_len = strlen(bone->name);
            
            fwrite(&part_id, sizeof(uint8_t), 1, outf);
            fwrite(&name_len, sizeof(uint32_t), 1, outf);
            fwrite(bone->name, sizeof(char), name_len, outf);
            
            part_count++;
        }
        
        if (part_count != bone_parts) {
            fprintf(stderr, "Bone part count missmatch %d != %d\n", bone_parts, part_count);
            return 1;
        }
    } else {
        uint8_t no_bones = 0;
        fwrite(&no_bones, sizeof(uint8_t), 1, outf);
    }
    
    cgltf_free(data);
    fclose(ppd);
    fclose(outf);

    return 0;
}

