#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "jWrite.h"

#ifdef __linux__
const char separator = '/';
#else
const char separator = '\\';
#endif

#define EFFECT_ENTRY_ATTR_REV_VELO 0x1
#define EFFECT_ENTRY_ATTR_NOT_FOG 0x2
#define EFFECT_ENTRY_ATTR_DIR_SPRT 0x4
#define EFFECT_ENTRY_ATTR_NOT_BILL 0x100
#define EFFECT_ENTRY_ATTR_FLG_COCKPIT 0x8000

struct file_entry {
    uint32_t offset;
    uint32_t length;
};

struct vector3 {
    float x, y, z;
};

struct color4 {
    float b, g, r, a;
};

struct effect_entry {
    int16_t effect_type; // 0 = 2D_SIN, 1 = 2D_REP, 2 = 2D_PAR, 3 = LINPAR, 4 = GK, 5 = 3D_SIN, 6 = 3D_REP, 7 = 3D_PAR
    int8_t blend_type; // 0 = Alpha, 1 = Add, 2 = Subtract
    int8_t jnt;
    int16_t priority;
    int16_t texture;
    int16_t uv;
    int16_t sequence;
    int16_t model;
    uint16_t attrs; // Uses EFFECT_ENTRY_ATTR
    uint8_t vertex_color_b;
    uint8_t vertex_color_g;
    uint8_t vertex_color_r;
    uint8_t vertex_color_a;
    struct color4 color_damping;
    int32_t life;
    int32_t delay;
    struct vector3 position;
    struct vector3 rotation; // In radians, Brainbox2 shows degrees
    struct vector3 scale;
    struct vector3 position_velocity; // 0x58
    struct vector3 rotation_velocity; // 0x64
    struct vector3 scale_velocity; // 0x70
    struct vector3 position_acceleration; // 0x7C
    struct vector3 rotation_acceleration; // 0x88
    struct vector3 scale_acceleration; // 0x94
    struct vector3 gravity_acceleration; // 0xA0
    struct vector3 free0[3]; // 0xAC
    struct vector3 free1[3]; // 0xD0
    struct vector3 free2[3]; // 0xF4 (default: free2[1].y == -20.0f)
    struct vector3 free3[3]; // 0x118
};

struct x86_mov_float {
    uint32_t inst;
    float val;
};

struct mech_light {
    uint32_t type;
    float x;
    float y;
    float z;
};


char * progname;
char out_path[512];

char json_buffer[1<<20]; // 1MB

const float original_fps = 20.0f;

int unpackEFP(char * path) {
    FILE * efp = fopen(path, "rb");
    if (!efp) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return 1;
    }
    
    char * ext = strrchr(path, '.');
    strcpy(ext + 1, "tbl");
    
    FILE * tbl = fopen(path, "rb");
    if (!tbl) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        fclose(efp);
        return 1;
    }
    
    char * sep = strrchr(path, separator);
    if (sep) sep[1] = '\0';
    else *path = '\0';
    
    uint32_t efp_file_count;
    fread(&efp_file_count, sizeof(uint32_t), 1, efp);
    
    struct file_entry efp_files[efp_file_count];
    fread(efp_files, sizeof(struct file_entry), efp_file_count, efp);
    
    /*
    EFP0 = EFE
    EFP1 = SEQ
    EFP2 = UV1 (Used with SEQ entries)
    EFP3 = UV2 (Mostly UI sprite)
    */
    
    const char * file_exts[] = {"efe", "seq", "uv", "uv2"};
    
    for (int i = 0; i < efp_file_count; i++) {
        fseek(efp, efp_files[i].offset, SEEK_SET);
        
        uint32_t file_count;
        fread(&file_count, sizeof(uint32_t), 1, efp);
        
        struct file_entry files[file_count];
        fread(files, sizeof(struct file_entry), file_count, efp);
        
        switch (i) {
        case 0: printf("Unpacking %d effect files\n", file_count); break;
        case 1: printf("Unpacking %d sequence files\n", file_count); break;
        case 2: printf("Unpacking %d uv-coord files\n", file_count); break;
        case 3: printf("Unpacking %d uv2-coord files\n", file_count); break;
        }
        
        for (int j = 0; j < file_count; j++) {
            if (!files[j].length) continue;
            
            uint32_t id = j;
            if (i == 0) {
                if (fread(&id, sizeof(uint32_t), 1, tbl) != 1) {
                    fprintf(stderr, "Unexpected end-of-file when reading EFE IDs\n");
                    fclose(efp);
                    fclose(tbl);
                    return 1;
                }
            }
            
            snprintf(out_path, sizeof(out_path), "%seff%0*d.%s", path, i ? 3 : 4, id, file_exts[i]);
            FILE * out = fopen(out_path, "wb");
            if (!out) {
                fprintf(stderr, "Failed to open output file: %s\n", out_path);
                fclose(efp);
                fclose(tbl);
                return 1;
            }
            
            fseek(efp, efp_files[i].offset + files[j].offset, SEEK_SET);
            for (int b = 0; b < files[j].length; b++) {
                int byte = fgetc(efp);
                if (byte == EOF) {
                    fprintf(stderr, "Unexpected end-of-file\n");
                    fclose(efp);
                    fclose(out);
                    return 1;
                }
                fputc(byte, out);
            }
            
            fclose(out);
        }
    }
    
    fclose(tbl);
    fclose(efp);
    return 0;
}

void jwObj_vector3(char * name, struct vector3 * v) {
    jwObj_array(name);
    jwArr_double(v->x);
    jwArr_double(v->y);
    jwArr_double(v->z);
    jwEnd();
}

void jwObj_color4(char * name, struct color4 * c) {
    jwObj_array(name);
    jwArr_double(c->r);
    jwArr_double(c->g);
    jwArr_double(c->b);
    jwArr_double(c->a);
    jwEnd();
}

void jwObj_vector3a(char * name, struct vector3 * va, size_t vs) {
    jwObj_array(name);
    for (size_t i = 0; i < vs; i++) {
        jwArr_array();
        jwArr_double(va[i].x);
        jwArr_double(va[i].y);
        jwArr_double(va[i].z);
        jwEnd();
    }
    jwEnd();
}

int unpackEFE(char * path) {
    FILE * efe = fopen(path, "rb");
    if (!efe) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return 1;
    }
    
    uint32_t entry_count;
    fread(&entry_count, sizeof(uint32_t), 1, efe);
    
    uint32_t entry_offsets[entry_count];
    fread(entry_offsets, sizeof(uint32_t), entry_count, efe);
    
    jwOpen(json_buffer, sizeof(json_buffer), JW_ARRAY, JW_PRETTY);
    
    printf("Converting effect with %d parts\n", entry_count);
    
    for (int i = 0; i < entry_count; i++) {
        fseek(efe, entry_offsets[i], SEEK_SET);
        
        if (i < entry_count - 1) {
            uint32_t length = entry_offsets[i + 1] - entry_offsets[i];
            //printf("Len %d, %d - %d\n", length, entry_offsets[i + 1], entry_offsets[i]);
            if (length != sizeof(struct effect_entry)) {
                fprintf(stderr, "Effect entry %d size missmatch, got %d expected %ld\n",
                    i, length, sizeof(struct effect_entry));
                fclose(efe);
                return 1;
            }
        }
        
        struct effect_entry effect;
        fread(&effect, sizeof(struct effect_entry), 1, efe);
        
        jwArr_object();
        
        jwObj_int("effect_type", effect.effect_type);
        jwObj_int("blend_type", effect.blend_type);
        jwObj_int("jnt", effect.jnt);
        jwObj_int("priority", effect.priority);
        jwObj_int("texture", effect.texture);
        jwObj_int("uv", effect.uv);
        jwObj_int("sequence", effect.sequence);
        jwObj_int("model", effect.model);
        
        // TODO flags
        jwObj_bool("attr_rev_velo", effect.attrs & EFFECT_ENTRY_ATTR_REV_VELO);
        jwObj_bool("attr_not_fog",  effect.attrs & EFFECT_ENTRY_ATTR_NOT_FOG);
        jwObj_bool("attr_not_bill", effect.attrs & EFFECT_ENTRY_ATTR_NOT_BILL);
        jwObj_bool("attr_dir_sprt", effect.attrs & EFFECT_ENTRY_ATTR_DIR_SPRT);
        jwObj_bool("attr_flg_cockpit", effect.attrs & EFFECT_ENTRY_ATTR_FLG_COCKPIT);
        
        if (effect.attrs & ~(EFFECT_ENTRY_ATTR_REV_VELO | EFFECT_ENTRY_ATTR_NOT_FOG | EFFECT_ENTRY_ATTR_DIR_SPRT | EFFECT_ENTRY_ATTR_NOT_BILL | EFFECT_ENTRY_ATTR_FLG_COCKPIT)) {
            fprintf(stderr, "Unknown attribute flag %04X in entry %02d\n", effect.attrs, i);
            fclose(efe);
            return 1;
        }
        
        jwObj_array("vertex_color");
            jwArr_int(effect.vertex_color_r);
            jwArr_int(effect.vertex_color_g);
            jwArr_int(effect.vertex_color_b);
            jwArr_int(effect.vertex_color_a);
        jwEnd();
        
        jwObj_color4("damping_color", &effect.color_damping);
        
        jwObj_int("life", effect.life);
        jwObj_int("delay", effect.delay);
        
        jwObj_vector3("position", &effect.position);
        jwObj_vector3("rotation", &effect.rotation);
        jwObj_vector3("scale", &effect.scale);
        
        jwObj_vector3("position_velocity", &effect.position_velocity);
        jwObj_vector3("rotation_velocity", &effect.rotation_velocity);
        jwObj_vector3("scale_velocity", &effect.scale_velocity);
        
        jwObj_vector3("position_acceleration", &effect.position_acceleration);
        jwObj_vector3("rotation_acceleration", &effect.rotation_acceleration);
        jwObj_vector3("scale_acceleration", &effect.scale_acceleration);
        
        jwObj_vector3("gravity_acceleration", &effect.gravity_acceleration);
        
        jwObj_vector3a("free0", effect.free0, 3);
        jwObj_vector3a("free1", effect.free1, 3);
        jwObj_vector3a("free2", effect.free2, 3);
        jwObj_vector3a("free3", effect.free3, 3);
        
        jwEnd();
    }
    
    fclose(efe);
    
    int jw_err = jwClose();
    if (jw_err) {
        fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
        return 1;
    }
    
    strncpy(out_path, path, sizeof(out_path));
    char * ext = strrchr(out_path, '.');
    
    strcpy(ext + 1, "json");
    FILE * out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "Failed to open output file: %s\n", out_path);
        return 1;
    }
    
    fwrite(json_buffer, sizeof(char), strlen(json_buffer), out);
    
    fclose(out);
    
    return 0;
}

int unpackUV(char * path) {
    FILE * uv = fopen(path, "rb");
    if (!uv) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 2;
    }
    
    uint32_t uv_count;
    fread(&uv_count, sizeof(uint32_t), 1, uv);
    
    uint32_t texture;
    fread(&texture, sizeof(uint32_t), 1, uv);
    jwObj_int("texture", texture);
    
    float offset[2];
    fread(offset, sizeof(float), 2, uv);
    jwObj_array("offset");
        jwArr_double(offset[0]);
        jwArr_double(offset[1]);
    jwEnd();
    
    printf("Converting UV file: %d frames, texture ID %d, offset (%.3f, %.3f)\n",
        uv_count, texture, offset[0], offset[1]);
    
    jwObj_array("frames");
    for (int i = 0; i < uv_count; i++) {
        jwArr_object();
        
        float vals[6];
        fread(vals, sizeof(float), 6, uv);
        
        jwObj_array("start");
            jwArr_double(vals[0]);
            jwArr_double(vals[1]);
        jwEnd();
        
        jwObj_array("end");
            jwArr_double(vals[2]);
            jwArr_double(vals[3]);
        jwEnd();
        
        jwObj_array("size");
            jwArr_double(vals[4]);
            jwArr_double(vals[5]);
        jwEnd();
        
        jwEnd();
    }
    jwEnd();
    
    fclose(uv);
    return 0;
}

int unpackSEQ(char * path) {
    strncpy(out_path, path, sizeof(out_path));
    char * ext = strrchr(out_path, '.');
    
    FILE * seq = fopen(out_path, "rb");
    if (!seq) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 2;
    }
    
    uint32_t seq_count;
    fread(&seq_count, sizeof(uint32_t), 1, seq);
    
    printf("Converting sequence file: %d frames\n", seq_count);
    
    jwOpen(json_buffer, sizeof(json_buffer), JW_OBJECT, JW_PRETTY);
    
    jwObj_array("sequence");
    for (int i = 0; i < seq_count; i++) {
        int16_t seq_frame, seq_time;
        fread(&seq_frame, sizeof(int16_t), 1, seq);
        fread(&seq_time, sizeof(int16_t), 1, seq);
        
        jwArr_object();
        jwObj_int("frame", seq_frame);
        jwObj_int("time", seq_time);
        jwEnd();
    }
    jwEnd();
    
    fclose(seq);
    
    jwObj_object("uv");
    strcpy(ext + 1, "uv");
    if (unpackUV(out_path)) return 1;
    jwEnd();
    
    int jw_err = jwClose();
    if (jw_err) {
        fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
        return 1;
    }
    
    // Rename it from eff000.seq to spr000.json
    memcpy(ext - 6, "spr", 3);
    strcpy(ext + 1, "json");
    FILE * out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "Failed to open output file: %s\n", out_path);
        return 1;
    }
    
    fwrite(json_buffer, sizeof(char), strlen(json_buffer), out);
    
    fclose(out);
    return 0;
}

int unpackUV2(char * path) {
    jwOpen(json_buffer, sizeof(json_buffer), JW_OBJECT, JW_PRETTY);

    if (unpackUV(path)) return 1;
    
    int jw_err = jwClose();
    if (jw_err) {
        fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
        return 1;
    }
    
    // Rename it from eff000.seq to ui000.json
    strncpy(out_path, path, sizeof(out_path)); 
    char * ext = strrchr(out_path, '.');
    
    memmove(ext - 4, ext - 3, 3); // Move ID back 1 character without overwriting self
    memcpy(ext - 6, "ui", 2); // Change name to ui
    strcpy(ext - 1, ".json"); // Add extension
    FILE * out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "Failed to open output file: %s\n", out_path);
        return 1;
    }
    
    fwrite(json_buffer, sizeof(char), strlen(json_buffer), out);
    
    fclose(out);
    return 0;
}

int unpackSEG(char * path) {
    strncpy(out_path, path, sizeof(out_path));
    char * sep = strrchr(out_path, separator);
    
    if (sep) strcpy(sep + 1, ".data.seg");
    else strcpy(out_path, ".data.seg");
    
    FILE * datf = fopen(out_path, "rb");
    if (!datf) {
        fprintf(stderr, "Failed to open file: %s\n", out_path);
        return 1;
    }
    
    printf("Unpacking engine data\n");
    
    jwOpen(json_buffer, sizeof(json_buffer), JW_OBJECT, JW_PRETTY);
    
    uint32_t cockpitLightTypes[54];
    fseek(datf, 0x36F78, SEEK_SET);
    fread(cockpitLightTypes, sizeof(uint32_t), 54, datf);
    
    float cockpitLightPositions[162];
    fseek(datf, 0x37050, SEEK_SET);
    fread(cockpitLightPositions, sizeof(float), 162, datf);
    
    struct mech_light mechLights[32];
    fseek(datf, 0x56118, SEEK_SET);
    fread(mechLights, sizeof(struct mech_light), 32, datf);
    
    fclose(datf);
    
    jwObj_array("mechs");
    for (int i = 0 ; i < 32; i++) {
        struct mech_light * ml = mechLights + i;
        jwArr_object();
            jwObj_int("type", ml->type);
            jwObj_array("position");
                jwArr_double(ml->x);
                jwArr_double(ml->y);
                jwArr_double(ml->z);
            jwEnd();
        jwEnd();
    }
    jwEnd();
    
    if (sep) strcpy(sep + 1, ".text.seg");
    else strcpy(out_path, ".text.seg");
    
    FILE * txtf = fopen(out_path, "rb");
    if (!txtf) {
        fprintf(stderr, "Failed to open file: %s\n", out_path);
        return 1;
    }
    
    jwObj_array("cockpits");
    for (int c = 0; c < 6; c++) {
        jwArr_object();
        
        jwObj_array("status");
        for (int i = 0; i < 9; i++) {
            int l = c * 9 + i;
            jwArr_object();
                jwObj_int("type", cockpitLightTypes[l]);
                jwObj_array("position");
                    jwArr_double(cockpitLightPositions[l * 3]);
                    jwArr_double(cockpitLightPositions[l * 3 + 1]);
                    jwArr_double(cockpitLightPositions[l * 3 + 2]);
                jwEnd();
            jwEnd();
        }
        jwEnd();
        
        struct x86_mov_float mov;
        fseek(txtf, 0x5C6EE + sizeof(struct x86_mov_float) * 3 * c, SEEK_SET);
        jwObj_object("eject");
            jwObj_int("type", c + 273);
            jwObj_array("position");
            for (int i = 0; i < 3; i++) {
                fread(&mov, sizeof(struct x86_mov_float), 1, txtf);
                jwArr_double(mov.val);
            };
            jwEnd();
        jwEnd();
        
        if (c == 1) {
            fseek(txtf, 0x6A2FC, SEEK_SET);
            
            jwObj_array("comms");
            for (int i = 0; i < 5; i++) {
                jwArr_array();
                for (int j = 0; j < 3; j++) {
                    fread(&mov, sizeof(struct x86_mov_float), 1, txtf);
                    jwArr_double(mov.val);
                }
                jwEnd();
            }
            jwEnd();
        }
        jwEnd();
    }
    jwEnd();
    
    fclose(txtf);
    
    int jw_err = jwClose();
    if (jw_err) {
        fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
        return 1;
    }
    
    if (sep) strcpy(sep + 1, "lightdata.json");
    else strcpy(out_path, "lightdata.json");
    
    FILE * outf = fopen(out_path, "w");
    if (!outf) {
        fprintf(stderr, "Failed to open output file: %s\n", out_path);
        return 1;
    }
    
    fwrite(json_buffer, sizeof(char), strlen(json_buffer), outf);
    
    fclose(outf);
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Effect Tool - By QuantX\n");

    if (!argc) {
        fprintf(stderr, "Please specify an effect file: %s <path/effect.efe>\n", progname);
        return 1;
    }
    
    char * path = *argv++; argc--;
    
    char * ext = strrchr(path, '.');
    if (!ext) {
        fprintf(stderr, "No extension present on path: %s\n", path);
        return 1;
    }
    
    ext++;
    if (!strcmp(ext, "efp")) return unpackEFP(path);
    else if (!strcmp(ext, "efe")) return unpackEFE(path);
    else if (!strcmp(ext, "seq")) return unpackSEQ(path);
    else if (!strcmp(ext, "uv2")) return unpackUV2(path);
    else if (!strcmp(ext, "seg")) return unpackSEG(path);
    else {
        fprintf(stderr, "Unknown file extension: %s\n", path);
        return 1;
    }
    return 0;
}
