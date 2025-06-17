#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "jWrite.h"

struct vector3 {
    float x, y, z;
} __attribute__((packed));

struct stage_object {
    int16_t life;
    int16_t id;
    int16_t motion;
    int16_t lsq;
    int16_t hitbox;
    uint16_t attr0;
    struct vector3 pos;
    struct vector3 dir;
    int8_t alpha_clip;
    int8_t shadow_flag;
    uint16_t npc_spawn_idx;
    int8_t team_id;
    int8_t ticket_value;
    uint16_t attr1;
    uint32_t flags;
    uint8_t padding[48];
} __attribute__((packed));

char * progname;
char * basepath;
char path[256];
char json_buffer[1<<24]; // 1MB

void emit_vector3(struct vector3 * v) {
    jwArr_double(v->x);
    jwArr_double(v->y);
    jwArr_double(v->z);
}

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
        struct vector3 pos;
        fread(&pos, sizeof(struct vector3), 1, rstf);
        struct vector3 dir;
        fread(&dir, sizeof(struct vector3), 1, rstf);
        
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
            emit_vector3(&pos);
            jwEnd();

            jwObj_array("rotation");
            emit_vector3(&dir);
            jwEnd();
        jwEnd();
    }
    
    jwEnd();
    
    fclose(rstf);
    return 0;
}
    
int unpackSEG(long map) {
    if (sizeof(struct stage_object) != 96) {
        fprintf(stderr, "stage_object was not 96 bytes\n");
        return 1;
    }

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
    
    jwObj_array("objects");
    
    int obj_count;
    for (obj_count = 0; 1; obj_count++) {
        struct stage_object obj;
        fread(&obj, sizeof(struct stage_object), 1, segf);
        
        if (obj.id == -1) break;

        if (obj.attr0) {
            fprintf(stderr, "Stage object attr0 was non-zero: %04X\n", obj.attr0);
            return 1;
        }
        
        if (obj.attr1) {
            fprintf(stderr, "Stage object attr1 was non-zero: %04X\n", obj.attr1);
            return 1;
        }

        jwArr_object();
        
        jwObj_int("id", obj.id);
        jwObj_int("life", obj.life);
        
        jwObj_int("motion", obj.motion);
        jwObj_int("lsq", obj.lsq);
        jwObj_int("hitbox", obj.hitbox);
        
        jwObj_array("position");
        emit_vector3(&obj.pos);
        jwEnd();
    
        jwObj_array("rotation");
        emit_vector3(&obj.dir);
        jwEnd();
        
        jwObj_int("alpha_clip", obj.alpha_clip);
        jwObj_int("shadow_flag", obj.shadow_flag);
        jwObj_int("npc_spawn_idx", obj.npc_spawn_idx);
        
        jwObj_int("team_id", obj.team_id);
        jwObj_int("ticket_value", obj.ticket_value);
        
        jwObj_int("flags", obj.flags);
        
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
    
    fread(&ival, sizeof(uint32_t), 1, stgf); // Shadow Draw Mode
    if (ival) {
        fprintf(stderr, "Magic value at 0x50 was non-zero %08X\n", ival);
        return 1;
    }
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("sky_height", fval);
    
    jwObj_array("sky_scroll_0");
    fread(fvec, sizeof(float), 2, stgf);
    emit_float_array(fvec, 2);
    jwEnd();
    
    jwObj_array("sky_scroll_1");
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
    jwObj_int("tactics_time", ival);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_bool("do_predraw", ival);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_bool("draw_shadows", ival);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_bool("draw_sky", ival);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_bool("draw_terrain", ival);

    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_bool("draw_water", ival);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("point_light_rate", fval);
    
    // Skip value at 0x90 as it's just the World Fog Color again
    fseek(stgf, sizeof(uint32_t), SEEK_CUR); // World Fog uint32

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
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("shadow_angle", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("shadow_start", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("shadow_end", fval);
    
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
    jwObj_double("flash_shadow_rate", fval);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_double("area_over_size", ival);
    
    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("ticket_a", ival);

    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("ticket_b", ival);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("clip_ex", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("clip_ex_sub", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("clip_side_ex", fval);
    
    fread(&fval, sizeof(float), 1, stgf);
    jwObj_double("float_0xEC", fval);

    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("integer_0xF0", ival);

    fread(&ival, sizeof(uint32_t), 1, stgf);
    jwObj_int("integer_0xF4", ival);
    
    jwObj_array("water_color");
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
    
    long map;
    if (!strcmp(*argv, "data")) {
        map = -1;
    } else {
        map = strtol(*argv, NULL, 10);
        if (map < 0 || map > 26) {
            fprintf(stderr, "Invalid map number %ld, should be between 0 and 26\n", map);
            return 1;
        }
    }
    
    argv++; argc--;
    
    if (argc) {
        basepath = *argv++; argc--;
    } else {
        basepath = "";
    }

    snprintf(path, sizeof(path), "%s.data.seg", basepath);
    FILE * datf = fopen(path, "rb");
    if (!datf) {
        fprintf(stderr, "Failed to open .data.seg, ensure you've copied it after running: segment -u default.xbe\n");
        return 1;
    }
    
    if (map < 0) {
        printf("Extracting 64 terrain factors\n");
        float surfaceFactors[64];
        fseek(datf, 0x55728, SEEK_SET);
        fread(surfaceFactors, sizeof(float), 64, datf);
        
        fclose(datf);
        
        snprintf(path, sizeof(path), "%sterrain_resistances.data", basepath);
        FILE * outf = fopen(path, "wb");
        if (!outf) {
            fprintf(stderr, "Failed to open output file: %s\n", path);
            return 1;
        }
        
        fwrite(surfaceFactors, sizeof(float), 64, outf);
        
        fclose(outf);
        return 0;
    }
    
    int32_t missionIDs[27];
    fseek(datf, 0x59518, SEEK_SET);
    fread(missionIDs, sizeof(int32_t), 27, datf);
    
    fclose(datf);

    snprintf(path, sizeof(path), "%smap%02ld.json", basepath, map);
    FILE * outf = fopen(path, "w");
    if (!outf) {
        fprintf(stderr, "Failed to open output file: %s\n", path);
        return 1;
    }

    jwOpen(json_buffer, sizeof(json_buffer), JW_OBJECT, JW_PRETTY);

    // All default maps are this size
    jwObj_int("width", 280);
    jwObj_int("height", 280);

    jwObj_int("id", missionIDs[map]);

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
