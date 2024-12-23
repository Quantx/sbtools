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

struct vector2 {
    float x, y;
};

struct os_subdraw_command {
    struct vector2 start;
    struct vector2 end;
};

struct os_subdraw_pointer {
    uint32_t count;
    uint32_t pointer;
};

struct os_entry_0 { // Null entry
    uint32_t unknown[14]; // All 0s?
};

struct os_entry_text {
    struct vector2 pos;
    uint32_t id;
    uint32_t arg;
    uint32_t unknown[9]; // All 0s?
    uint32_t color; // Always 0x0000FFAA
};

struct os_entry_quad { // ID: 0x2
    struct vector2 points[4];
    uint32_t colors[4]; // BGRA
    uint32_t unknown[2];
};

struct os_entry_line { // ID: 0x3
    struct vector2 start;
    struct vector2 end;
    uint32_t color; // BGRA 
    uint32_t unknown[9]; // All 0s?
};

struct os_entry_sprite { // Sprite
    float x;
    float y;
    float w;
    float h;
    float x2;
    float y2;
    float w2;
    float h2;
    uint32_t color; // BGRA
    uint32_t id;
    uint32_t unknown[4]; // All 0s?
};

struct os_entry_5 {
    float x;
    float y;
    uint32_t color;
    float x2;
    float y2;
    uint32_t value;
    uint32_t unknown[8];
};

struct os_entry_8 {
    uint32_t value0; // Always 1?
    uint32_t color; // Always 0x0000FFAA or 0xB900FFAA might be color? 
    uint32_t unknown[12]; // All 0s?
};

struct os_entry_10 {
    uint32_t value;
    uint32_t colors[4];
    uint32_t unknown[9];
};

struct os_entry_20 {
    uint32_t value; // 1, 70
    uint32_t unknown[13];
};

struct os_entry_40 {
    uint32_t value;
    float x; // Probably more floats after this
    uint32_t unknown[12];
};

struct os_entry { // 64 bytes (16 x 4)
    uint32_t type;
    int32_t frame; // always 0xF, might be frame time? (-1 indicates unused)
    union {
        struct os_entry_0 os0;
        struct os_entry_text text; // 0x1
        struct os_entry_quad quad; // 0x2
        struct os_entry_line line; // 0x3
        struct os_entry_sprite sprite; // 0x4
        struct os_entry_5 os5;
        struct os_entry_8 os8;
        struct os_entry_10 os10;
        struct os_entry_20 os20;
        struct os_entry_40 os40;
    };
};

char * progname;
char out_path[512];

char text_buffer[1024];

char json_buffer[1<<20]; // 1MB

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
            
            jwObj_int("frame", bootAmb.frame); 
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
                
                jwArr_object();
                    jwObj_int("frame", plight.frame);
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
                
                printf("Frame %d, Pos (%.2f, %.2f, %.2f), Color (%.2f, %.2f, %.2f) Energy %.2f\n",
                    plight.frame, plight.x, plight.y, plight.z, plight.r, plight.g, plight.b, plight.energy);
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

int unpackOS(char * path) {
    if (sizeof(struct os_entry) != 64) {
        fprintf(stderr, "OS Entry struct was %ld bytes not 64\n", sizeof(struct os_entry));
        return 1;
    }
    
    FILE * os = fopen(path, "rb");
    if (!os) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return 1;
    }
    
    strncpy(out_path, path, sizeof(out_path));
    char * ext = strrchr(out_path, separator);
    if (ext) *ext = '\0';
    else *out_path = '\0';
    
    strcat(out_path, "os.str");
    FILE * text_file = fopen(out_path, "r");
    if (!text_file) {
        fprintf(stderr, "Failed to open os text file: %s\n", out_path);
        fclose(os);
        return 1;
    }
    
    if (fgets(text_buffer, sizeof(text_buffer), text_file) == NULL) {
        fprintf(stderr, "Failed to get text line count\n");
        fclose(os);
        fclose(text_file);
        return 1;
    }
    
    long text_lines = strtol(text_buffer, NULL, 10);
    printf("Reading %ld lines of OS text\n", text_lines);
    
    char ** text = malloc(text_lines * sizeof(char *));
    int text_offset = 0;
    
    for (int i = 0; i < text_lines; i++) {
        text[i] = text_buffer + text_offset;
        if (fgets(text[i], sizeof(text_buffer) - text_offset, text_file) == NULL) {
            fprintf(stderr, "Failed to get line %d from OS strings\n", i);
            fclose(os);
            fclose(text_file);
            return 1;
        }
        
        text_offset += strlen(text[i]) - 1;
        (text_buffer + text_offset)[-1] = '\0'; // Remove EOL character
    }
    
    fclose(text_file);
    
    if (ext) *ext = '\0';
    else *out_path = '\0';
    strcat(out_path, ".data.hdr");
    
    FILE * hdrf = fopen(out_path, "rb");
    if (!hdrf) {
        fprintf(stderr, "Failed to open file: %s\n", out_path);
        return 1;
    }
    
    struct section_header hdr_data;
    fread(&hdr_data, sizeof(struct section_header), 1, hdrf);
    
    fclose(hdrf);
    
    if (ext) *ext = '\0';
    else *out_path = '\0';
    strcat(out_path, ".data.seg");
    
    FILE * datf = fopen(out_path, "rb");
    if (!datf) {
        fprintf(stderr, "Failed to open file: %s\n", out_path);
        return 1;
    }
    
    fseek(datf, 0x451B8, SEEK_SET);
    struct os_subdraw_pointer subdraw_ptrs[39];
    fread(subdraw_ptrs, sizeof(struct os_subdraw_pointer), 39, datf);
    
    for (int i = 0; i < 39; i++) {
        printf("%d: %d %08X\n", i, subdraw_ptrs[i].count, subdraw_ptrs[i].pointer);
    }
    
    fclose(datf);
    
    strncpy(out_path, path, sizeof(out_path));
    ext = strrchr(out_path, '.');
    if (ext) *ext = '\0';
    strcat(out_path, ".svg");
    
    FILE * out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "Failed to open output file: %s\n", out_path);
        fclose(out);
        return 1;
    }
    
    fprintf(out, "<svg version=\"1.1\" width=\"512\" height=\"512\" xmlns=\"http://www.w3.org/2000/svg\">\n");
    fprintf(out, "<rect width=\"100%%\" height=\"100%%\" fill=\"black\"/>\n");
    
    uint32_t frame_count;
    fread(&frame_count, sizeof(uint32_t), 1, os);
    
    printf("OS file has %d frames\n", frame_count);
    
    int index = -1;
    while (true) {
        unsigned long pos = ftell(os);
    
        index++;
    
        struct os_entry entry;
        if (fread(&entry, sizeof(struct os_entry), 1, os) != 1) {
            break;
        }
        
        if (entry.frame == -1) {
            continue;
        }
        
        printf("Entry type 0x%02X at 0x%08lX, frame %d: ", entry.type, pos, entry.frame);
        
        switch (entry.type) {
        case 0x0:
            for (int i = 0; i < 14; i++) {
                if (entry.os0.unknown[i]) {
                    printf("unknown non-zero value\n");
                    return 1;
                }
            }
            printf("N/A\n");
        break;
        case 0x1:
            for (int i = 0; i < 9; i++) {
                if (entry.text.unknown[i]) {
                    printf("unknown non-zero value\n");
                    return 1;
                }
            }
            
            if (entry.text.id >= text_lines) {
                printf("ERROR: Text ID %d >= text_lines %ld\n", entry.text.id, text_lines);
                return 1;
            }
            
            printf("SVG\n");
            
            fprintf(out, "<text index=\"%d\" frame=\"%d\" x=\"%.0f\" y=\"%.0f\" color=\"%08X\" fill=\"#%06X\" arg=\"%d\">%s</text>\n",
                index, entry.frame,
                entry.text.pos.x, entry.text.pos.y,
                entry.text.color, entry.text.color & 0xFFFFFF,
                entry.text.arg, text[entry.text.id]);
        break;
        case 0x2:
            if (entry.quad.unknown[0] || entry.quad.unknown[1]) {
                printf("unknown non-zero value\n");
                return 1;
            }
            printf("SVG\n");

fprintf(out, "<polygon index=\"%d\" frame=\"%d\" points=\"%.0f,%.0f %.0f,%.0f %.0f,%.0f %.0f,%.0f\" colors=\"%08X %08X %08X %08X\" fill=\"#%06X\"/>\n",
                index, entry.frame,
                entry.quad.points[0].x, entry.quad.points[0].y,
                entry.quad.points[1].x, entry.quad.points[1].y,
                entry.quad.points[2].x, entry.quad.points[2].y,
                entry.quad.points[3].x, entry.quad.points[3].y,
                entry.quad.colors[0],
                entry.quad.colors[1],
                entry.quad.colors[2],
                entry.quad.colors[3],
                entry.quad.colors[0]);
        break;
        case 0x3:
            for (int i = 0; i < 9; i++) {
                if (entry.line.unknown[i]) {
                    printf("unknown non-zero value\n");
                    return 1;
                }
            }
            printf("SVG\n");
            
            fprintf(out, "<line index=\"%d\" frame=\"%d\" x1=\"%.0f\" y1=\"%.0f\" x2=\"%.0f\" y2=\"%.0f\" color=\"%08X\" stroke=\"#%06X\"/>\n",
                index, entry.frame,
                entry.line.start.x, entry.line.start.y, entry.line.end.x, entry.line.end.y,
                entry.line.color, entry.line.color);
        break;
        case 0x4:
            for (int i = 0; i < 4; i++) {
                if (entry.sprite.unknown[i]) {
                    printf("unknown non-zero value\n");
                    return 1;
                }
            }
            printf("(%.2fX %.2fY %.2fW %.2fH), (%.2fX %.2fY %.2fW %.2fH), color %08X, value %d\n",
                entry.sprite.x, entry.sprite.y, entry.sprite.w, entry.sprite.h,
                entry.sprite.x2, entry.sprite.y2, entry.sprite.w2, entry.sprite.h2,
                entry.sprite.color, entry.sprite.id);
            
        break;
        case 0x5:
            for (int i = 0; i < 8; i++) {
                if (entry.os5.unknown[i]) {
                    printf("unknown non-zero value\n");
                    return 1;
                }
            }
            printf("(%.2fX %.2fY), color %08X, (%.2fX %.2fY), value %d\n",
                entry.os5.x, entry.os5.y, entry.os5.color,
                entry.os5.x2, entry.os5.y2, entry.os5.value);
        break;
        case 0x8:
            for (int i = 0; i < 12; i++) {
                if (entry.os8.unknown[i]) {
                    printf("unknown non-zero value\n");
                    return 1;
                }
            }
            printf("%d, color %08X\n",
                entry.os8.value0, entry.os8.color);
        break;
        case 0x10:  
            for (int i = 0; i < 9; i++) {
                if (entry.os10.unknown[i]) {
                    printf("unknown non-zero value\n");
                    return 1;
                }
            }
            printf("%d, colors (%08X %08X %08X %08X)\n",
                entry.os10.value, entry.os10.colors[0], entry.os10.colors[1], entry.os10.colors[2], entry.os10.colors[3]);
        break;
        case 0x20:
            for (int i = 0; i < 13; i++) {
                if (entry.os20.unknown[i]) {
                    printf("unknown non-zero value\n");
                    return 1;
                }
            }
            printf("%d\n", entry.os20.value);
        break;
        case 0x40:
            for (int i = 0; i < 10; i++) {
                if (entry.os40.unknown[i]) {
                    printf("unknown non-zero value\n");
                    return 1;
                }
            }
            printf("(%.2f), value %d\n", entry.os40.x, entry.os40.value);
        break;
        default:
            printf("UNKNOWN ENTRY TYPE\n");
            return 1;
        }
    }
    
    fprintf(out, "</svg>\n");
    
    free(text);
    
    fclose(out);
    fclose(os);
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
    if (!strcmp(ext, "coc")) return unpackCOC(path);  
    else if (!strcmp(ext, "amb")) return unpackAMB(path);
    else if (!strcmp(ext, "cbt")) return unpackCBT(path);
    else if (!strcmp(ext, "os")) return unpackOS(path);
    else {
        fprintf(stderr, "Unknown file extension: %s\n", path);
        return 1;
    }
    return 0;
}
