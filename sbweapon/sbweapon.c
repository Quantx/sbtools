#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "jWrite.h"

#ifdef __linux__
#define SEPARATOR '/'
#else
#define SEPARATOR '\\'
#endif

char * progname;

char out_path[265];
char json_buffer[1<<20]; // 1MB

struct file_data {
    uint32_t offset;
    uint32_t length;
    uint32_t count;
};

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

struct __attribute__((__packed__)) model_atribs {
    int16_t model;
    int16_t motion;
    int16_t unknown0;
    int16_t unknown1;
    uint16_t zero;
    uint16_t flags;
};

struct __attribute__((__packed__)) weapon_data {
    uint16_t id;
    uint16_t life;
    float initial_velocity;
    float max_speed;
    float acceleration;
    float gravity_acceleration;
    float gun_barrel_movement_angle;
    float range_at_which_damage_type_applies;
    float max_range;
    float min_range;
    uint16_t offensive_power;
    uint16_t rapid_fire;
    uint16_t fire_interval;
    uint16_t shooting_interval;
    uint16_t number_of_simultaneous_shots;
    uint16_t time_to_shift_when_firing_at_the_same_time;
    uint8_t unknown0;
    uint8_t impact_effect;
    uint16_t number_of_bullets;
    uint16_t number_of_mags;
    uint16_t ranged_weapon_range_damage;
    uint8_t weight;
    uint8_t fire_probability;
    uint16_t additional_damage_when_on_fire;
    uint8_t unknown1;
    uint8_t rear_end;
    uint16_t simultaneous_flight;
    float horizontal_length_of_simultaneous_firing;
    float vertical_length_of_simultaneous_firing;
    float retraction;
    uint32_t damage_type;
    uint8_t trajectory;
    uint8_t firing_effect;
    uint8_t next_to_simultaneous_firing;
    uint8_t simultaneous_vertical_firing;
};

#define WEP_CLASS_COUNT 3
char * wep_classes[WEP_CLASS_COUNT] = {"mwep", "swep", "cwep"};

#define MWEP_DATA_LEN 26
uint32_t mwep_data_ptrs[MWEP_DATA_LEN];

#define SWEP_DATA_LEN 32
uint32_t swep_data_ptrs[SWEP_DATA_LEN];

#define TAMA_DATA_LEN 15
uint32_t tama_data_ptrs[TAMA_DATA_LEN];

#define MWEP_NAME_LEN 26
uint32_t mwep_name_ptrs[MWEP_NAME_LEN];

#define SWEP_NAME_LEN 33
uint32_t swep_name_ptrs[SWEP_NAME_LEN];

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Weapon Data Tool - By QuantX\n");

    if (!argc) {
        printf("Usage: %s wepdat.wcb\n", progname);
        return 1;
    }
    
    char * path = *argv++; argc--;
    
    FILE * wdat = fopen(path, "rb");
    if (!wdat) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }

    strncpy(out_path, path, sizeof(out_path));
    char * sep = strrchr(out_path, SEPARATOR);
    if (sep) strcpy(sep + 1, "weapondata.json");
    else strcpy(out_path, "weapondata.json");
    
    FILE * outf = fopen(out_path, "w");
    if (!outf) {
        fprintf(stderr, "failed to open output file: %s\n", out_path);
        return 1;
    }
    
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

    if (sep) strcpy(sep + 1, ".rdata.hdr");
    else strcpy(out_path, ".rdata.hdr");

    hdrf = fopen(out_path, "rb");
    if (!hdrf) {
        fprintf(stderr, "Failed to open file: %s\n", out_path);
        return 1;
    }
    
    struct section_header hdr_rdata;
    fread(&hdr_rdata, sizeof(struct section_header), 1, hdrf);
    
    fclose(hdrf);
    
    if (sep) strcpy(sep + 1, ".data.seg");
    else strcpy(out_path, ".data.seg");
    
    FILE * datf = fopen(out_path, "rb");
    if (!datf) {
        fprintf(stderr, "Failed to open file: %s\n", out_path);
        return 1;
    }
    
    if (sep) strcpy(sep + 1, ".rdata.seg");
    else strcpy(out_path, ".rdata.seg");
    
    FILE * rdatf = fopen(out_path, "rb");
    if (!rdatf) {
        fprintf(stderr, "Failed to open file: %s\n", out_path);
        return 1;
    }
    
    uint32_t file_count;
    fread(&file_count, sizeof(uint32_t), 1, wdat);
    
    struct file_data files[file_count];
    fread(files, sizeof(struct file_data), file_count, wdat);

    fseek(datf, 0x58A90, SEEK_SET);
    fread(tama_data_ptrs, sizeof(uint32_t), TAMA_DATA_LEN, datf);

    fseek(datf, 0x58620, SEEK_SET);
    fread(mwep_data_ptrs, sizeof(uint32_t), MWEP_DATA_LEN, datf);

    fseek(datf, 0x58998, SEEK_SET);
    fread(swep_data_ptrs, sizeof(uint32_t), SWEP_DATA_LEN, datf);
    
    fseek(datf, 0x58AE0, SEEK_SET);
    fread(mwep_name_ptrs, sizeof(uint32_t), MWEP_NAME_LEN, datf);

    fseek(datf, 0x58B48, SEEK_SET);
    fread(swep_name_ptrs, sizeof(uint32_t), SWEP_NAME_LEN, datf);
    
    jwOpen(json_buffer, sizeof(json_buffer), JW_OBJECT, JW_PRETTY);
    
    for (int we = 0; we < files[0].count; we++) {
        uint32_t weapon_count;
        fread(&weapon_count, sizeof(uint32_t), 1, wdat);
    
        if (we >= WEP_CLASS_COUNT) {
            fprintf(stderr, "Weapon entry %d > 2\n", we);
            return 1;
        }

        uint32_t weapon_lookup_ptrs[weapon_count];
        fseek(datf, we ? 0x58790 : 0x58480, SEEK_SET);
        fread(weapon_lookup_ptrs, sizeof(uint32_t), weapon_count, datf);

        jwObj_array(wep_classes[we]);
    
        int weapons_processed = 0;
        for (int i = 0; i < weapon_count; i++) {
            struct weapon_data wep;
            fread(&wep, sizeof(struct weapon_data), 1, wdat);
            
            if (wep.id != i) {
                fprintf(stderr, "Weapon ID does not match! Got %d expected %d\n", wep.id, i);
                return 1;
            }

            uint32_t bullet_offset = 0;
            uint32_t weapon_offset = 0;
            
            if (weapon_lookup_ptrs[i] >= hdr_data.vaddr && weapon_lookup_ptrs[i] < hdr_data.vaddr + hdr_data.vsize) {
                fseek(datf, weapon_lookup_ptrs[i] - hdr_data.vaddr, SEEK_SET);
                fread(&bullet_offset, sizeof(uint32_t), 1, datf);
                fread(&weapon_offset, sizeof(uint32_t), 1, datf);
            }
            
            uint32_t * wep_data_ptrs = we ? swep_data_ptrs : mwep_data_ptrs;
            
            struct model_atribs weapon_atribs;
            fseek(datf, wep_data_ptrs[weapon_offset] - hdr_data.vaddr, SEEK_SET);
            fread(&weapon_atribs, sizeof(struct model_atribs), 1, datf);
            
            int16_t bullet_model;
            fseek(datf, tama_data_ptrs[bullet_offset] - hdr_data.vaddr, SEEK_SET);
            fread(&bullet_model, sizeof(int16_t), 1, datf);
            
            uint32_t * wep_name_ptrs = we ? swep_name_ptrs : mwep_name_ptrs;
            
            char name[16];
            fseek(rdatf, wep_name_ptrs[i] - hdr_rdata.vaddr, SEEK_SET);
            fread(name, sizeof(char), 16, rdatf);
            
            printf("WE %d | ID %02d: Weapon %04d, Bullet %04d, Flags: %04X, Name: \"%s\"\n", we, i, weapon_offset, bullet_offset, weapon_atribs.flags, name);
            
            jwArr_object();
            
            jwObj_int("id", wep.id);
            jwObj_string("name", name);
            jwObj_int("life", wep.life);            
            jwObj_double("initial_velocity", wep.initial_velocity);
            jwObj_double("max_speed", wep.max_speed);
            jwObj_double("acceleration", wep.acceleration);
            jwObj_double("gravity_acceleration", wep.gravity_acceleration);
            jwObj_double("gun_barrel_movement_angle", wep.gun_barrel_movement_angle);
            jwObj_double("range_at_which_damage_type_applies", wep.range_at_which_damage_type_applies);
            jwObj_double("max_range", wep.max_range);
            jwObj_double("min_range", wep.min_range);
            jwObj_int("offensive_power", wep.offensive_power);
            jwObj_int("rapid_fire", wep.rapid_fire);
            jwObj_int("fire_interval", wep.fire_interval);
            jwObj_int("shooting_interval", wep.shooting_interval);
            jwObj_int("number_of_simultaneous_shots", wep.number_of_simultaneous_shots);
            jwObj_int("time_to_shift_when_firing_at_the_same_time", wep.time_to_shift_when_firing_at_the_same_time);
//            jwObj_int("unknown0", wep.unknown0);
            jwObj_int("impact_effect", wep.impact_effect);
            jwObj_int("number_of_bullets", wep.number_of_bullets);
            jwObj_int("number_of_mags", wep.number_of_mags);
            jwObj_int("ranged_weapon_range_damage", wep.ranged_weapon_range_damage);
            jwObj_int("weight", wep.weight);
            jwObj_int("fire_probability", wep.fire_probability);
            jwObj_int("additional_damage_when_on_fire", wep.additional_damage_when_on_fire);
//            jwObj_int("unknown1", wep.unknown1);
            jwObj_int("rear_end", wep.rear_end);
            jwObj_int("simultaneous_flight", wep.simultaneous_flight);
            jwObj_double("horizontal_length_of_simultaneous_firing", wep.horizontal_length_of_simultaneous_firing);
            jwObj_double("vertical_length_of_simultaneous_firing", wep.vertical_length_of_simultaneous_firing);
            jwObj_double("retraction", wep.retraction);
            jwObj_int("damage_type", wep.damage_type);
            jwObj_int("trajectory", wep.trajectory);
            jwObj_int("firing_effect", wep.firing_effect);
            jwObj_int("next_to_simultaneous_firing", wep.next_to_simultaneous_firing);
            jwObj_int("simultaneous_vertical_firing", wep.simultaneous_vertical_firing);
            
            jwObj_int("bullet_model", bullet_model);
            jwObj_int("weapon_model", weapon_atribs.model);
            jwObj_int("weapon_motion", weapon_atribs.motion);

            jwObj_bool("shoulder", weapon_atribs.flags & 0x0001); // Shoulder weapon or SWEP BOX weapon
            jwObj_bool("melee",    weapon_atribs.flags & 0x0080);
            jwObj_bool("mounted",  weapon_atribs.flags & 0x0800); // Whether or not this contributes to "MOUNT OVER"
            jwObj_bool("fixed",    weapon_atribs.flags & 0x1000); // Whether or not this weapon can be unequipped
            
            jwEnd();
            
            weapons_processed++;
        }
        
        printf("Weapon entry %d: Processed %d weapons\n", we, weapons_processed);
        
        jwEnd();
    }
    
    fclose(wdat);
    fclose(datf);
    fclose(rdatf);
    
    int jw_err = jwClose();
    if (jw_err) {
        fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
        fclose(outf);
        return 1;
    }
    
    fwrite(json_buffer, sizeof(char), strlen(json_buffer), outf);
    fclose(outf);
    
    return 0;
}
