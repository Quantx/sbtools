#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define PATH_OUT_MAX 256
char out_path[PATH_OUT_MAX];

char * helpmsg = "Used to pack and unpack SB stage files\n"
"Show help: sbstage -h\n"
"Unpack stage: sbstage -u std00.stg\n"
"Pack stage: sbstage -p std00.stg\n"
"Unpack rstart: sbstage -u rstart00.rst\n"
"Pack rstart: sbstage -p rstart00.rst\n"
"Unpack segment: sbstage -u seg00.seg\n"
"Pack segment: sbstage -p seg00.seg\n";

char * progname;

static inline float rad2deg(float deg) { return (deg * 180) / M_PI; }
static inline float deg2rad(float rad) { return (rad * M_PI) / 180; }

int packRST(char * path) {
    FILE * rstf = fopen(path, "wb");
    if (!rstf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    printf("Packing %s\n", path);
    
    char * ext = strrchr(path, '.') + 1;
    strcpy(ext, "txt");
    
    FILE * inf = fopen(path, "r");
    if (!inf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    uint32_t magic = 1;
    fwrite(&magic, sizeof(uint32_t), 1, rstf);
    magic = 0;
    
    for (int i = 0; i < 10; i++) {
        float pos[3], dir[3];
        if (fscanf(inf, "%f %f %f %f %f %f\n", pos, pos + 1, pos + 2, dir, dir + 1, dir + 2) != 6) {
            fprintf(stderr, "Failed to read line %d, missing arguments\n", i);
            return 1;
        }
        
        fwrite(pos, sizeof(float), 3, rstf);
        fwrite(dir, sizeof(float), 3, rstf);
        fwrite(&magic, sizeof(uint32_t), 1, rstf);
        fwrite(&magic, sizeof(uint32_t), 1, rstf);
    }
    
    fclose(inf);
    fclose(rstf);
    return 0;
}

int packSEG(char * path) {
    FILE * segf = fopen(path, "wb");
    if (!segf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    char * ext = strrchr(path, '.') + 1;
    strcpy(ext, "txt");
    
    FILE * inf = fopen(path, "r");
    if (!inf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    printf("Packing %s\n", path);
    
    int obj_count = 0;
    
    int16_t obj_id, obj_arg;
    while (fscanf(inf, "Object ID: %hd\n, argument: %hd\n", &obj_id, &obj_arg) == 2) {
        float pos[3], dir[3];
        
        if (fscanf(inf, "Object POS: (%f, %f, %f), DIR: (%f, %f, %f)\n", pos, pos + 1, pos + 2, dir, dir + 1, dir + 2) != 6) {
            fprintf(stderr, "Object entry %d malformed position or direction\n", obj_count);
            fclose(inf);
            fclose(segf);
            return 1;
        }
        
        for (int i = 0; i < 3; i++) {
            dir[i] = deg2rad(dir[i]);
        }
        
        int16_t attr0[4], attr1[4];
        uint32_t attr2;
        
        if (fscanf(inf, "Attribute 0: %hd, %hd, %hd, %hd\n", attr0, attr0 + 1, attr0 + 2, attr0 + 3) != 4) {
            fprintf(stderr, "Object entry %d malformed Attribute 0\n", obj_count);
            fclose(inf);
            fclose(segf);
            return 1;
        }
        
        if (fscanf(inf, "Attribute 1: %hd, %hd, %hd, %hd\n", attr1, attr1 + 1, attr1 + 2, attr1 + 3) != 4) {
            fprintf(stderr, "Object entry %d malformed Attribute 1\n", obj_count);
            fclose(inf);
            fclose(segf);
            return 1;
        }
        
        if (fscanf(inf, "Attribute 2: %d\n", &attr2) != 1) {
            fprintf(stderr, "Object entry %d malformed Attribute 2\n", obj_count);
            fclose(inf);
            fclose(segf);
            return 1;
        }
        
        fwrite(&obj_arg, sizeof(int16_t), 1, segf);
        fwrite(&obj_id,  sizeof(int16_t), 1, segf);
        
        fwrite(attr0, sizeof(int16_t), 4, segf);
        
        fwrite(pos, sizeof(float), 3, segf);
        fwrite(dir, sizeof(float), 3, segf);
        
        fwrite(attr1, sizeof(int16_t), 4, segf);
        fwrite(&attr2, sizeof(uint32_t), 1, segf);
        
        // Padding
        for (int i = 0; i < 48; i++) fputc(0, segf);
        
        obj_count++;
    }

    // Output EOF marker    
    obj_arg = 0;
    obj_id = -1;
    
    fwrite(&obj_arg, sizeof(int16_t), 1, segf);
    fwrite(&obj_id,  sizeof(int16_t), 1, segf);
    
    printf("Packed %d objects\n", obj_count);
    
    fclose(inf);
    fclose(segf);
    
    return 0;
}

int packSTG(char * path) {
    FILE * stgf = fopen(path, "wb");
    if (!stgf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    printf("Packing %s\n", path);
    
    char * ext = strrchr(path, '.') + 1;
    strcpy(ext, "txt");
    
    FILE * inf = fopen(path, "r");
    if (!inf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    for (int i = 0; i < 7; i++) {
        int a;
        float set[4];
        if (fscanf(inf, "Setting A%d: %f %f %f %f\n", &a, set, set + 1, set + 2, set + 3) != 5) {
            fprintf(stderr, "Setting A%d is missing an argument\n", i);
            return 1;
        }
        fwrite(set, sizeof(float), 4, stgf);
    }
    
    for (int i = 0; i < 7; i++) {
        int b;
        uint32_t set;
        if (fscanf(inf, "Setting B%d: %d\n", &b, &set) != 2) {
            fprintf(stderr, "Setting B%d is missing an argument\n", i);
            return 1;
        }
        fwrite(&set, sizeof(uint32_t), 1, stgf);
    }
    
    {
        float set0;
        if(fscanf(inf, "Setting C0: %f\n", &set0) != 1) {
            fprintf(stderr, "Setting C0 is missing an argument\n");
            return 1;
        }
        fwrite(&set0, sizeof(float), 1, stgf);
        
        int16_t set1[2];
        if(fscanf(inf, "Setting C1: %hd %hd\n", set1, set1 + 1) != 2) {
            fprintf(stderr, "Setting C1 is missing an argument\n");
            return 1;
        }
        fwrite(set1, sizeof(int16_t), 2, stgf);
    }
    
    {
        float set0[4];
        if (fscanf(inf, "Setting D0: %f %f %f %f\n", set0, set0 + 1, set0 + 2, set0 + 3) != 4) {
            fprintf(stderr, "Setting D0 is missing an argument\n");
            return 1;
        }
        fwrite(set0, sizeof(float), 4, stgf);
        
        float set1[2];
        if (fscanf(inf, "Setting D1: %f %f\n", set1, set1 + 1) != 2) {
            fprintf(stderr, "Setting D1 is missing an argument\n");
            return 1;
        }
        fwrite(set1, sizeof(float), 2, stgf);
        
        float set2[4];
        if (fscanf(inf, "Setting D2: %f %f %f %f\n", set2, set2 + 1, set2 + 2, set2 + 3) != 4) {
            fprintf(stderr, "Setting D2 is missing an argument\n");
            return 1;
        }
        fwrite(set2, sizeof(float), 4, stgf);
        
        float set3[2];
        if (fscanf(inf, "Setting D3: %f %f\n", set3, set3 + 1) != 2) {
            fprintf(stderr, "Setting D3 is missing an argument\n");
            return 1;
        }
        fwrite(set3, sizeof(float), 2, stgf);
        
        float set4[2];
        if (fscanf(inf, "Setting D4: %f %f\n", set4, set4 + 1) != 2) {
            fprintf(stderr, "Setting D4 is missing an argument\n");
            return 1;
        }
        fwrite(set4, sizeof(float), 2, stgf);
    }
    
    {
        uint32_t set0;
        if (fscanf(inf, "Setting E0: %d\n", &set0) != 1) {
            fprintf(stderr, "Setting E0 is missing an argument\n");
            return 1;
        }
        fwrite(&set0, sizeof(uint32_t), 1, stgf);
        
        float set1;
        if (fscanf(inf, "Setting E1: %f\n", &set1) != 1) {
            fprintf(stderr, "Setting E1 is missing an argument\n");
            return 1;
        }
        fwrite(&set1, sizeof(float), 1, stgf);
        
        uint32_t set2;
        if (fscanf(inf, "Setting E2: %d\n", &set2) != 1) {
            fprintf(stderr, "Setting E2 is missing an argument\n");
            return 1;
        }
        fwrite(&set2, sizeof(uint32_t), 1, stgf);
        
        uint32_t set3[2];
        if (fscanf(inf, "Setting E3: %d %d\n", set3, set3 + 1) != 2) {
            fprintf(stderr, "Setting E3 is missing an argument\n");
            return 1;
        }
        fwrite(set3, sizeof(uint32_t), 2, stgf);
        
        float set4[2];
        if (fscanf(inf, "Setting E4: %f %f\n", set4, set4 + 1) != 2) {
            fprintf(stderr, "Setting E4 is missing an argument\n");
            return 1;
        }
        fwrite(set4, sizeof(float), 2, stgf);
        
        float set5;
        if (fscanf(inf, "Setting E5: %f\n", &set5) != 1) {
            fprintf(stderr, "Setting E5 is missing an argument\n");
            return 1;
        }
        fwrite(&set5, sizeof(float), 1, stgf);
        
        float set6;
        if (fscanf(inf, "Setting E6: %f\n", &set6) != 1) {
            fprintf(stderr, "Setting E6 is missing an argument\n");
            return 1;
        }
        fwrite(&set6, sizeof(float), 1, stgf);
        
        uint32_t set7;
        if (fscanf(inf, "Setting E7: %d\n", &set7) != 1) {
            fprintf(stderr, "Setting E7 is missing an argument\n");
            return 1;
        }
        fwrite(&set7, sizeof(uint32_t), 1, stgf);
        
        uint32_t set8;
        if (fscanf(inf, "Setting E8: %d\n", &set8) != 1) {
            fprintf(stderr, "Setting E8 is missing an argument\n");
            return 1;
        }
        fwrite(&set8, sizeof(uint32_t), 1, stgf);
        
        float set9[4];
        if (fscanf(inf, "Setting E9: %f %f %f %f\n", set9, set9 + 1, set9 + 2, set9 + 3) != 4) {
            fprintf(stderr, "Setting E9 is missing an argument\n");
            return 1;
        }
        fwrite(set9, sizeof(float), 4, stgf);
    }
    
    fclose(inf);
    fclose(stgf);

    return 0;
}

int unpackRST(char * path) {    
    FILE * rstf = fopen(path, "rb");
    if (!rstf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    printf("Unpacking %s\n", path);
    
    uint32_t magic;
    fread(&magic, sizeof(uint32_t), 1, rstf);
    
    if (magic != 1) printf("Warning magic was not 1, %08X\n", magic);
    
    char * ext = strrchr(path, '.') + 1;
    strcpy(ext, "txt");
    
    FILE * outf = fopen(path, "w");
    if (!outf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    for (int i = 0; i < 10; i++) {
        float pos[3];
        fread(pos, sizeof(float), 3, rstf);
        float dir[3];
        fread(dir, sizeof(float), 3, rstf);
        
        uint32_t unknown;
        fread(&unknown, sizeof(uint32_t), 1, rstf);
        if (unknown) printf("Unknown1 %d value was %08X\n", i, unknown);
        fread(&unknown, sizeof(uint32_t), 1, rstf);
        if (unknown) printf("Unknown2 %d value was %08X\n", i, unknown);
        
        fprintf(outf, "%f %f %f %f %f %f\n", pos[0], pos[1], pos[2], dir[0], dir[1], dir[2]);
    }
    
    fclose(rstf);
    fclose(outf);
    return 0;
}
    
int unpackSEG(char * path) {
    FILE * segf = fopen(path, "rb");
    if (!segf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    printf("Unpacking %s\n", path);
    
    //  798 -  805 | Debug map objects
    //  806 - 1175 | Common map objects
    // 1176 - 1185 | Objective specific objects
    
    char * ext = strrchr(path, '.') + 1;
    strcpy(ext, "txt");
    
    FILE * outf = fopen(path, "w");
    if (!outf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    int obj_count;
    for (obj_count = 0; 1; obj_count++) {
        int16_t obj_arg;
        fread(&obj_arg, sizeof(int16_t), 1, segf);
        int16_t obj_id;
        fread(&obj_id, sizeof(uint16_t), 1, segf);
        
        if (obj_id == -1) break;
        
        fprintf(outf, "Object ID: %d, argument: %d\n", obj_id, obj_arg);
        
        int16_t attr0[4];
        fread(attr0, sizeof(int16_t), 4, segf);
        
        float pos[3];
        fread(pos, sizeof(float), 3, segf);
        float dir[3];
        fread(dir, sizeof(float), 3, segf);
        
        for (int i = 0; i < 3; i++) {
            dir[i] = rad2deg(dir[i]);
        }
        
        fprintf(outf, "Object POS: (%f, %f, %f), DIR: (%f, %f, %f)\n", pos[0], pos[1], pos[2], dir[0], dir[1], dir[2]);
        
        int16_t attr1[4];
        fread(attr1, sizeof(int16_t), 4, segf);
        
        
        uint32_t attr2;
        fread(&attr2, sizeof(int32_t), 1, segf);
        
        fprintf(outf, "Attribute 0: %d, %d, %d, %d\n", attr0[0], attr0[1], attr0[2], attr0[3]);
        fprintf(outf, "Attribute 1: %d, %d, %d, %d\n", attr1[0], attr1[1], attr1[2], attr1[3]);
        fprintf(outf, "Attribute 2: %d\n", attr2);
        
        for (int i = 0; i < 48; i++) {
            uint8_t pad;
            fread(&pad, sizeof(int8_t), 1, segf);
            
            if (pad) fprintf(outf, "Pad at %d was %02X\n", i, pad);
        }
        
        fprintf(outf, "\n");
    }
    printf("Unpacked %d objects\n", obj_count);
    
    fclose(outf);
    fclose(segf);
    return 0;
}

int unpackSTG(char * path) {
    FILE * stgf = fopen(path, "rb");
    if (!stgf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    printf("Unpacking %s\n", path);
    
    char * ext = strrchr(path, '.') + 1;
    strcpy(ext, "txt");
    
    FILE * outf = fopen(path, "w");
    if (!outf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    for (int i = 0; i < 7; i++) {
        float set[4];
        fread(set, sizeof(float), 4, stgf);
        
        fprintf(outf, "Setting A%d: %f %f %f %f\n", i, set[0], set[1], set[2], set[3]);
    }
    fprintf(outf, "\n");
    
    for (int i = 0; i < 7; i++) {
        uint32_t set;
        fread(&set, sizeof(uint32_t), 1, stgf);
        fprintf(outf, "Setting B%d: %d\n", i, set);
    }
    fprintf(outf, "\n");
    
    {
        float set0;
        fread(&set0, sizeof(float), 1, stgf);
        fprintf(outf, "Setting C0: %f\n", set0);
        
        int16_t set1[2];
        fread(set1, sizeof(int16_t), 2, stgf);
        fprintf(outf, "Setting C1: %d %d\n", set1[0], set1[1]);
    }
    fprintf(outf, "\n");
    
    {
        float set0[4];
        fread(set0, sizeof(float), 4, stgf);
        fprintf(outf, "Setting D0: %f %f %f %f\n", set0[0], set0[1], set0[2], set0[3]);
        
        float set1[2];
        fread(set1, sizeof(float), 2, stgf);
        fprintf(outf, "Setting D1: %f %f\n", set1[0], set1[1]);
        
        float set2[4];
        fread(set2, sizeof(float), 4, stgf);
        fprintf(outf, "Setting D2: %f %f %f %f\n", set2[0], set2[1], set2[2], set2[3]);
        
        float set3[2];
        fread(set3, sizeof(float), 2, stgf);
        fprintf(outf, "Setting D3: %f %f\n", set3[0], set3[1]);
        
        float set4[2];
        fread(set4, sizeof(float), 2, stgf);
        fprintf(outf, "Setting D4: %f %f\n", set4[0], set4[1]);
    }
    fprintf(outf, "\n");
    
    {
        uint32_t set0;
        fread(&set0, sizeof(uint32_t), 1, stgf);
        fprintf(outf, "Setting E0: %d\n", set0);
        
        float set1;
        fread(&set1, sizeof(float), 1, stgf);
        fprintf(outf, "Setting E1: %f\n", set1);
        
        uint32_t set2;
        fread(&set2, sizeof(uint32_t), 1, stgf);
        fprintf(outf, "Setting E2: %d\n", set2);
        
        uint32_t set3[2];
        fread(set3, sizeof(uint32_t), 2, stgf);
        fprintf(outf, "Setting E3: %d %d\n", set3[0], set3[1]);
        
        float set4[2];
        fread(set4, sizeof(float), 2, stgf);
        fprintf(outf, "Setting E4: %f %f\n", set4[0], set4[1]);
        
        float set5;
        fread(&set5, sizeof(float), 1, stgf);
        fprintf(outf, "Setting E5: %f\n", set5);
        
        float set6;
        fread(&set6, sizeof(float), 1, stgf);
        fprintf(outf, "Setting E6: %f\n", set6);
        
        uint32_t set7;
        fread(&set7, sizeof(uint32_t), 1, stgf);
        fprintf(outf, "Setting E7: %d\n", set7);
        
        uint32_t set8;
        fread(&set8, sizeof(uint32_t), 1, stgf);
        fprintf(outf, "Setting E8: %d\n", set8);
        
        float set9[4];
        fread(set9, sizeof(float), 4, stgf);
        fprintf(outf, "Setting E9: %f %f %f %f\n", set9[0], set9[1], set9[2], set9[3]);
    }
    
    fclose(outf);
    fclose(stgf);
    return 0;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Stage Tool - By QuantX\n");

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
        if (progmode == 'p') fprintf(stderr, "Please specify a stage to pack\n");
        else if (progmode == 'u') fprintf(stderr, "Please specify a stage unpack\n");
        return 1;
    }

    char * ext = strrchr(*argv, '.');
    if (!ext) {
        fprintf(stderr, "Missing file extension\n");
        return 1;
    }
    ext++;
    
    if (!strncmp(ext, "rst", 3)) {
        return progmode == 'p' ? packRST(*argv) : unpackRST(*argv);
    } else if (!strncmp(ext, "stg", 3)) {
        return progmode == 'p' ? packSTG(*argv) : unpackSTG(*argv);
    } else if (!strncmp(ext, "seg", 3)) {
        return progmode == 'p' ? packSEG(*argv) : unpackSEG(*argv);
    }
   
    fprintf(stderr, "Unknown file extension: %s\n", ext);
    return 1;
}
