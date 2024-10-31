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

struct file_entry {
    uint32_t offset;
    uint32_t length;
};

struct light_entry {
    int16_t arg0;
    int16_t arg1;
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
    float energy;
};

struct ambient_entry {
    float amb_r;
    float amb_g;
    float amb_b;
    float amb_energy;
    float lit_r;
    float lit_g;
    float lit_b;
    float lit_energy;
    float yaw;
    float pitch;
};

#define BOOT_AMBIENT_COUNT 50

struct boot_ambient_entry { // 44 bytes
    int32_t frame;
    struct ambient_entry ambient;
};

struct boot_plight_entry { // 36 bytes (9 values)
    int32_t frame;
    float unknown;
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
    float energy;
};

char * progname;
char out_path[512];

char json_buffer[1<<20]; // 1MB

const float original_fps = 20.0f;

void writeAmbient(struct ambient_entry * amb) {
    jwObj_array("ambient_color");
        jwArr_double(amb->amb_r);
        jwArr_double(amb->amb_g);
        jwArr_double(amb->amb_b);
    jwEnd();
    jwObj_double("ambient_energy", amb->amb_energy);
    jwObj_array("light_color");
        jwArr_double(amb->lit_r);
        jwArr_double(amb->lit_g);
        jwArr_double(amb->lit_b);
    jwEnd();
    jwObj_double("light_energy", amb->lit_energy);
    jwObj_double("yaw", amb->yaw);
    jwObj_double("pitch", amb->pitch);
}

int unpackCOC(char * path) {
    FILE * coc = fopen(path, "rb");
    if (!coc) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return 1;
    }
    
    uint32_t file_count;
    fread(&file_count, sizeof(uint32_t), 1, coc);
    
    struct file_entry files[file_count];
    fread(files, sizeof(struct file_entry), file_count, coc);
    
    char * sep = strrchr(path, separator);
    if (sep) {
        sep[1] = '\0';
    } else {
        path[0] = '\0';
    }
    
    printf("Unpacking %d light file entries\n", file_count);
    
    for (int i = 0; i < file_count; i++) {
        struct file_entry * file = files + i;
        
        fseek(coc, file->offset, SEEK_SET);
        
        uint32_t light_count;
        fread(&light_count, sizeof(uint32_t), 1, coc);
        
        snprintf(out_path, sizeof(out_path), "%sPLIG_%02d.json", path, i);
        FILE * outf = fopen(out_path, "w");
        if (!outf) {
            fclose(coc);
            fprintf(stderr, "Failed to open output file: %s\n", out_path);
            return 1;
        }
        
        jwOpen(json_buffer, sizeof(json_buffer), JW_ARRAY, JW_PRETTY);
        
        for (int j = 0; j < light_count; j++) {
            struct light_entry light;
            fread(&light, sizeof(struct light_entry), 1, coc);
            
            jwArr_object();
            
            jwObj_int("arg0", light.arg0);
            jwObj_int("arg1", light.arg1);
            jwObj_array("position");
                jwArr_double(light.x);
                jwArr_double(light.y);
                jwArr_double(light.z);
            jwEnd();
            jwObj_array("color");
                jwArr_double(light.r);
                jwArr_double(light.g);
                jwArr_double(light.b);
            jwEnd();
            jwObj_double("energy", light.energy);
            
            jwEnd(); // End this light object
        }
        
        int jw_err = jwClose();
        if (jw_err) {
            fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
            fclose(coc);
            fclose(outf);
            return 1;
        }
        
        fwrite(json_buffer, sizeof(char), strlen(json_buffer), outf);
        fclose(outf);
    }
    
    fclose(coc);
    return 0;
}

int unpackAMB(char * path) {
    FILE * amb = fopen(path, "rb");
    if (!amb) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return 1;
    }

    uint32_t file_count;
    fread(&file_count, sizeof(uint32_t), 1, amb);
    
    struct file_entry files[file_count];
    fread(files, sizeof(struct file_entry), file_count, amb);
    
    char * sep = strrchr(path, separator);
    if (sep) {
        sep[1] = '\0';
    } else {
        path[0] = '\0';
    }
    
    printf("Unpacking %d ambient file entries\n", file_count);

    for (int i = 0; i < file_count; i++) {
        struct file_entry * file = files + i;
        
        fseek(amb, file->offset, SEEK_SET);
        
        snprintf(out_path, sizeof(out_path), "%sAMB_%02d.json", path, i);
        FILE * outf = fopen(out_path, "w");
        if (!outf) {
            fclose(amb);
            fprintf(stderr, "Failed to open output file: %s\n", out_path);
            return 1;
        }
        
        jwOpen(json_buffer, sizeof(json_buffer), JW_OBJECT, JW_PRETTY);
        
        struct ambient_entry ambient;
        fread(&ambient, sizeof(struct ambient_entry), 1, amb);
        
        writeAmbient(&ambient);
        
        int jw_err = jwClose();
        if (jw_err) {
            fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
            fclose(amb);
            fclose(outf);
            return 1;
        }
        
        fwrite(json_buffer, sizeof(char), strlen(json_buffer), outf);
        fclose(outf);
    }

    fclose(amb);
    return 0;
}

int unpackCBT(char * path) {
    FILE * cbt = fopen(path, "rb");
    if (!cbt) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return 1;
    }

    uint32_t file_count;
    fread(&file_count, sizeof(uint32_t), 1, cbt);
    
    struct file_entry files[file_count];
    fread(files, sizeof(struct file_entry), file_count, cbt);
    
    char * sep = strrchr(path, separator);
    if (sep) {
        sep[1] = '\0';
    } else {
        path[0] = '\0';
    }
    
    printf("Unpacking %d boot light file entries\n", file_count);
    
    for (int i = 0; i < file_count; i++) {
        struct file_entry * file = files + i;
        printf("File entry: %d, length: %d\n", i, file->length);
        
        // These files seem to be unused anyway
        if (file->length == 40000) {
            printf("Skipping non-standard entry\n\n");
            continue;
        }
        
        fseek(cbt, file->offset, SEEK_SET);
        
        snprintf(out_path, sizeof(out_path), "%sBTAMB_%02d.json", path, i);
        FILE * outf = fopen(out_path, "w");
        if (!outf) {
            fclose(cbt);
            fprintf(stderr, "Failed to open output file: %s\n", out_path);
            return 1;
        }
        
        jwOpen(json_buffer, sizeof(json_buffer), JW_ARRAY, JW_PRETTY);
        
        bool endReached = false;
        for (int j = 0; j < BOOT_AMBIENT_COUNT; j++) {
            struct boot_ambient_entry bootAmb;
            fread(&bootAmb, sizeof(struct boot_ambient_entry), 1, cbt); // 44
            
            if (bootAmb.frame < 0 || endReached) {
                endReached = true;
                continue;
            };
            
            jwArr_object();
            
            jwObj_double("time", (float)(bootAmb.frame) / original_fps); 
            writeAmbient(&bootAmb.ambient);
            
            jwEnd();
        }
        
        int jw_err = jwClose();
        if (jw_err) {
            fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
            fclose(cbt);
            fclose(outf);
            return 1;
        }
        
        fwrite(json_buffer, sizeof(char), strlen(json_buffer), outf);
        fclose(outf);
        
        snprintf(out_path, sizeof(out_path), "%sBTLIG_%02d.json", path, i);
        outf = fopen(out_path, "w");
        if (!outf) {
            fclose(cbt);
            fprintf(stderr, "Failed to open output file: %s\n", out_path);
            return 1;
        }
        
        jwOpen(json_buffer, sizeof(json_buffer), JW_ARRAY, JW_PRETTY);
        
        
        for (int j = 0; j < 10; j++) {
            bool isEmpty = true;
            endReached = false;
            for (int k = 0; k < BOOT_AMBIENT_COUNT; k++) {
                struct boot_plight_entry plight;
                fread(&plight, sizeof(struct boot_plight_entry), 1, cbt); // 36
                
                if (plight.frame < 0 || endReached ||
                  ((isnan(plight.x) || plight.x == 0.0f)
                && (isnan(plight.y) || plight.y == 0.0f)
                && (isnan(plight.z) || plight.z == 0.0f))) {
                    endReached = true;
                    continue;
                }
                
                if (k == 0) {
                    printf("Track %d\n", j);
                    isEmpty = false;
                    jwArr_array();
                }
                
                float time = (float)(plight.frame) / original_fps;
                
                jwArr_object();
                    jwObj_double("time", time);
                    jwObj_array("position");
                        jwArr_double(plight.x);
                        jwArr_double(plight.y);
                        jwArr_double(plight.z);
                    jwEnd();
                    jwObj_array("color");
                        jwArr_double(plight.r);
                        jwArr_double(plight.g);
                        jwArr_double(plight.b);
                    jwEnd();
                    jwObj_double("energy", plight.energy);
                jwEnd();
                
                printf("Time %.3f, Pos (%.2f, %.2f, %.2f), Color (%.2f, %.2f, %.2f) Energy %.2f\n",
                    time, plight.x, plight.y, plight.z, plight.r, plight.g, plight.b, plight.energy);
            }
            
            if (!isEmpty) {
                printf("\n");
                jwEnd();
            }
        }
        
        jw_err = jwClose();
        if (jw_err) {
            fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
            fclose(cbt);
            fclose(outf);
            return 1;
        }
        
        fwrite(json_buffer, sizeof(char), strlen(json_buffer), outf);
        fclose(outf);
    }

    fclose(cbt);
    return 0;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Cockpit Tool - By QuantX\n");

    if (!argc) {
        fprintf(stderr, "Please specify a cockpit file: %s <path/cockpit_file>\n", progname);
        return 1;
    }
    
    char * path = *argv++; argc--;
    
    char * ext = strrchr(path, '.');
    if (!ext) {
        fprintf(stderr, "No extension present on path: %s\n", path);
        return 1;
    }
    
    ext++;
    if (!strncmp(ext, "coc", 3)) unpackCOC(path);  
    else if (!strncmp(ext, "amb", 3)) unpackAMB(path);
    else if (!strncmp(ext, "cbt", 3)) unpackCBT(path);
    else {
        fprintf(stderr, "Unknown file extension: %s\n", path);
        return 1;
    }
    return 0;
}
