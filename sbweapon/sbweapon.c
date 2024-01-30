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
    uint16_t time_to_shift_when_firing_at_same_time;
    float horizontal_length_of_simultaneous_firing;
    float vertical_length_of_simultaneous_firing;
    float retraction;
    uint32_t damage_type;
    uint8_t trajectory;
    uint8_t firing_effect;
    uint8_t next_to_simultaneous_firing;
    uint8_t simultaneous_vertical_firing;
};

char * wep_classes[3] = {"mwep", "swep", "cwep"};

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
    
    uint32_t file_count;
    fread(&file_count, sizeof(uint32_t), 1, wdat);
    
    struct file_data * files = malloc(file_count * sizeof(struct file_data));
    fread(files, sizeof(struct file_data), file_count, wdat);
    
    jwOpen(json_buffer, sizeof(json_buffer), JW_OBJECT, JW_PRETTY);
    
    for (int we = 0; we < files[0].count; we++) {
        uint32_t weapon_count;
        fread(&weapon_count, sizeof(uint32_t), 1, wdat);
    
        if (we > 2) {
            fprintf(stderr, "Weapon entry %d > 2\n", we);
            return 1;
        }
    
        jwObj_array(wep_classes[we]);
    
        int weapons_processed = 0;
        for (int i = 0; i < weapon_count; i++) {
            struct weapon_data wep;
            fread(&wep, sizeof(struct weapon_data), 1, wdat);
            
            if (wep.id != i) {
                fprintf(stderr, "Weapon ID does not match! Got %d expected %d\n", wep.id, i);
                return 1;
            }
            
            jwArr_object();
            
            jwObj_int("id", wep.id);
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
            jwObj_int("unknown0", wep.unknown0);
            jwObj_int("impact_effect", wep.impact_effect);
            jwObj_int("number_of_bullets", wep.number_of_bullets);
            jwObj_int("number_of_mags", wep.number_of_mags);
            jwObj_int("ranged_weapon_range_damage", wep.ranged_weapon_range_damage);
            jwObj_int("weight", wep.weight);
            jwObj_int("fire_probability", wep.fire_probability);
            jwObj_int("additional_damage_when_on_fire", wep.additional_damage_when_on_fire);
            jwObj_int("unknown1", wep.unknown1);
            jwObj_int("rear_end", wep.rear_end);
            jwObj_int("time_to_shift_when_firing_at_same_time", wep.time_to_shift_when_firing_at_same_time);
            jwObj_double("horizontal_length_of_simultaneous_firing", wep.horizontal_length_of_simultaneous_firing);
            jwObj_double("vertical_length_of_simultaneous_firing", wep.vertical_length_of_simultaneous_firing);
            jwObj_double("retraction", wep.retraction);
            jwObj_int("damage_type", wep.damage_type);
            jwObj_int("trajectory", wep.trajectory);
            jwObj_int("firing_effect", wep.firing_effect);
            jwObj_int("next_to_simultaneous_firing", wep.next_to_simultaneous_firing);
            jwObj_int("simultaneous_vertical_firing", wep.simultaneous_vertical_firing);
            
            jwEnd();
            
            weapons_processed++;
        }
        
        printf("Weapon entry %d: Processed %d weapons\n", we, weapons_processed);
        
        jwEnd();
    }
    
    fclose(wdat);
    
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
