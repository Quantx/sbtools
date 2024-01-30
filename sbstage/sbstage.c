#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "jWrite.h"

char * progname;
char * basepath;
char path[256];
char json_buffer[1<<20]; // 1MB

void emit_float_array(float * vals, size_t len) {
    for (size_t i = 0; i < len; i++) {
        jwArr_double(vals[i]);
    }
}

int unpackRST(long map) {
    snprintf(path, sizeof(path), "%srstart%02ld.rst", basepath, map);
    FILE * rstf = fopen(path, "rb");
    if (!rstf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    printf("Unpacking %s\n", path);
    
    uint32_t magic;
    fread(&magic, sizeof(uint32_t), 1, rstf);
    if (magic != 1) {
        fprintf(stderr, "Warning magic was not 1, %08X\n", magic);
        return 1;
    }
    
    jwObj_array("rstart");
    
    for (int i = 0; i < 10; i++) {
        float pos[3];
        fread(pos, sizeof(float), 3, rstf);
        float dir[3];
        fread(dir, sizeof(float), 3, rstf);
        
        uint32_t unknown;
        fread(&unknown, sizeof(uint32_t), 1, rstf);
        if (unknown) {
            fprintf(stderr, "Unknown1 %d value was %08X\n", i, unknown);
            return 1;
        }
        fread(&unknown, sizeof(uint32_t), 1, rstf);
        if (unknown) {
            fprintf(stderr, "Unknown2 %d value was %08X\n", i, unknown);
            return 1;
        }

        jwArr_object();
            jwObj_array("position");
            emit_float_array(pos, 3);
            jwEnd();

            jwObj_array("rotation");
            emit_float_array(dir, 3);
            jwEnd();
        jwEnd();
    }
    
    jwEnd();
    
    fclose(rstf);
    return 0;
}
    
int unpackSEG(long map) {
    snprintf(path, sizeof(path), "%sseg%02ld.seg", basepath, map);
    FILE * segf = fopen(path, "rb");
    if (!segf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    printf("Unpacking %s\n", path);
    
    //  798 -  805 | Debug map objects
    //  806 - 1175 | Common map objects
    // 1176 - 1185 | Objective specific objects
    
    jwObj_array("stage");
    
    int obj_count;
    for (obj_count = 0; 1; obj_count++) {
        int16_t obj_arg;
        fread(&obj_arg, sizeof(int16_t), 1, segf);
        int16_t obj_id;
        fread(&obj_id, sizeof(uint16_t), 1, segf);
        
        if (obj_id == -1) break;
        
        int16_t attr0[4];
        fread(attr0, sizeof(int16_t), 4, segf);
        
        float pos[3];
        fread(pos, sizeof(float), 3, segf);
        
        float dir[3];
        fread(dir, sizeof(float), 3, segf);
        
        int16_t attr1[4];
        fread(attr1, sizeof(int16_t), 4, segf);        
        
        uint32_t attr2;
        fread(&attr2, sizeof(int32_t), 1, segf);
        
        for (int i = 0; i < 48; i++) {
            uint8_t pad;
            fread(&pad, sizeof(int8_t), 1, segf);
            
            if (pad) {
                fprintf(stderr, "Pad at %d was %02X\n", i, pad);
                return 1;
            }
        }

        jwArr_object();
        
        jwObj_int("id", obj_id);
        jwObj_int("arg", obj_arg);

        jwObj_array("position");
        emit_float_array(pos, 3);
        jwEnd();
    
        jwObj_array("rotation");
        emit_float_array(dir, 3);
        jwEnd();
        
        jwObj_array("attr0");
        for (int i = 0; i < 4; i++) jwArr_int(attr0[i]);
        jwEnd();
        
        jwObj_array("attr1");
        for (int i = 0; i < 4; i++) jwArr_int(attr1[i]);
        jwEnd();
        
        jwObj_int("attr2", attr2);
        
        jwEnd();
    }
    
    jwEnd();
    
    printf("Unpacked %d objects\n", obj_count);

    fclose(segf);
    return 0;
}

int unpackSTG(long map, int tod) {
    snprintf(path, sizeof(path), "%sstd%d_%02ld.stg", basepath, tod, map);
    FILE * stgf = fopen(path, "rb");
    if (!stgf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    printf("Unpacking %s\n", path);
    
    float fval, fvec[4];
    uint32_t ival;

    jwObj_array("world_light");
    fread(fvec, sizeof(float), 4, stgf);
    emit_float_array(fvec, 4);
    jwEnd();
    
    jwObj_array("world_specular");
    fread(fvec, sizeof(float), 4, stgf);
    emit_float_array(fvec, 4);
    jwEnd();
    
    jwObj_array("world_ambient");
    fread(fvec, sizeof(float), 4, stgf);
    emit_float_array(fvec, 4);
    jwEnd();
    
    jwObj_array("world_fog");
    fread(fvec, sizeof(float), 4, stgf);
    emit_float_array(fvec, 4);
    jwEnd();
    
    jwObj_array("sun_color");
    fread(fvec, sizeof(float), 4, stgf);
    emit_float_array(fvec, 4);
    jwEnd();
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    if (ival) {
        fprintf(stderr, "Magic value at 0x50 was non-zero %08X\n", ival);
        return 1;
    }
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("sky_height", fval);
    
    jwObj_array("top_cloud_velocity");
    fread(fvec, sizeof(float), 2, stgf);
    emit_float_array(fvec, 2);
    jwEnd();
    
    jwObj_array("bottom_cloud_velocity");
    fread(fvec, sizeof(float), 2, stgf);
    emit_float_array(fvec, 2);
    jwEnd();
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("water_height", fval);

    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("water_speed", fval);

    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_bool("draw_rain", ival);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("integer_0x74", ival);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("integer_0x78", ival);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_bool("draw_shadows", ival);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_bool("draw_sky", ival);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_bool("draw_terrain", ival);

    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_bool("draw_water", ival);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("float_0x8C", fval);
    
    // Skip value at 0x90 as it's just the World Fog Color again
    fseek(stgf, sizeof(uint32_t), SEEK_CUR);

    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("fog_start", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("fog_end", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("sky_fog_start", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("sky_fog_end", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("shadow_offset", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("shadow_scale", fval);
    
    jwObj_array("position_0xAC");
    fread(fvec, sizeof(float), 3, stgf);
    emit_float_array(fvec, 3);
    jwEnd();
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("shadow_yaw", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("shadow_pitch", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("sun_flash_power", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("sun_back_size", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("sun_front_size", fval);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("water_texture", ival);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("float_0xD0", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("float_0xD4", fval);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("integer_0xD8", ival);

    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("integer_0xDC", ival);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("clip_ex", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("clip_ex_sub", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("clip_side_ex", fval);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("integer_0xEC", ival);

    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("integer_0xF0", ival);

    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("integer_0xF4", ival);
    
    jwObj_array("color_0xF8");
    fread(fvec, sizeof(float), 4, stgf);
    emit_float_array(fvec, 4);
    jwEnd();
    
    if (ftell(stgf) != 264) {
        fprintf(stderr, "Stage data file was %ld (instead of 264) bytes long\n", ftell(stgf));
        return 1;
    }
    
    fclose(stgf);
    return 0;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Stage Tool - By QuantX\n");

    if (!argc) {
        fprintf(stderr, "Usage: %s <stage_number> (path)\n", progname);
        return 1;
    }
    
    long map = strtol(*argv, NULL, 10);
    argv++; argc--;
    
    if (map < 0 || map > 24) {
        fprintf(stderr, "Invalid map number %ld, should be between 0 and 24\n", map);
        return 1;
    }

    if (argc) {
        basepath = *argv++; argc--;
    } else {
        basepath = "";
    }

    snprintf(path, sizeof(path), "%smap%02ld.json", basepath, map);
    FILE * outf = fopen(path, "w");
    if (!outf) {
        fprintf(stderr, "Failed to open output file: %s\n", path);
        return 1;
    }

    jwOpen(json_buffer, sizeof(json_buffer), JW_OBJECT, JW_PRETTY);

    jwObj_array("stage");
    for (int i = 0; i < 4; i++) {
        jwArr_object();
        if (unpackSTG(map, i)) return 1;
        jwEnd();
    }
    jwEnd();

    if (unpackRST(map)) return 1;
    if (unpackSEG(map)) return 1;
    
    int jw_err = jwClose();
    if (jw_err) {
        fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
        return 1;
    }
    
    fwrite(json_buffer, sizeof(char), strlen(json_buffer), outf);
    fclose(outf);

    return 0;
}
