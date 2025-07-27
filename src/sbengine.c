#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "jWrite.h"

#ifdef __linux__
#define SEPARATOR '/'
#include <termios.h>
#else
#define SEPARATOR '\\'
#endif

#define LOADOUT_SLOT_COUNT 7
char * loadoutSlotNames[LOADOUT_SLOT_COUNT] = {"mwep1", "mwep2", "mwep3", "swep1", "swep2", "swep3", "tank"}; //, "part1", "part2"};

char json_buffer[1<<20]; // 1MB

const float ms2kmh = 3.6f;

char out_path[512];

char * progname;

char * helpmsg = "Interactive tool used to edit SB engine data files\n"
"Show help: sbengine -h\n"
"Export engine data: sbengine -e eng_data.eng\n"
"Unpack engine data: sbengine -u eng_data.eng\n"
"Pack engine data: sbengine -p eng_data\n";

struct __attribute__((packed)) vector3 {
    float x, y, z;
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

struct __attribute__((packed)) loadout_data {
    uint32_t mwep_offset;
    uint32_t swep_offset;
    uint32_t tank_offset;
    uint32_t misc_offset;
    uint8_t padding;
    uint8_t weight_type; // Light/Medium/Heavy
    uint8_t class_type; // 0=Standard, 1=Support, 2=Scout, 3=Assult
    uint8_t profile_description; // Used to determine profile description (based on which VT you play)
    uint32_t mounts; // Number of SWEP shoulder weapons that can be mounted
};

struct __attribute__((packed)) engine_data {
    uint32_t id;
    float weight;
    float tier_r;
    float gears[6]; // 0=Reverse, 1=1st, ... , 5=5th
    float gear_f;
    float brake;
    float rpm_min;
    float override_rpm;
    float override_torque;
    float wheel_torque;      // Output torque = wheel_torque * 50.0
    float wheel_start_speed; // What's shown in brainbox2 = wheel_start_speed * 3.6 (min speed needed to enter 5th gear)
    float drag_coefficient;
    float rpm_rate;
    float height;
    float width;
    float internal_resistance;
    float rpm0;
    float torque0;
    float rpm1;
    float torque1;
    float rpm2;
    float torque2;
    float rpm3;
    float torque3;
    float turn_speed;
    float balancer;
    uint16_t life;
    uint16_t l_leg_life;
    uint16_t r_leg_life;
    uint16_t opt_armor_life;
    uint32_t sub_tank_number;
    float main_tank_capacity;
    float sub_tank_capacity;
    float tank_consumption;
    float battery_capacity;
    float armor_front;
    uint32_t cockpit_type;
    uint32_t use_ticket;
    uint32_t slope_a_gear;
    uint32_t slope_b_gear;
    uint32_t price;
    uint32_t max_loadout;
    uint32_t standard_loadout;
    float armor_side;
    float armor_rear;
};

struct __attribute__((packed)) parts_data {
    uint32_t id;
    float collider_offset_z;
    uint32_t manipulator;
    struct vector3 collider_size;
    uint32_t weapon_mount;
    uint32_t unknown;
};

#define VT_NAME_COUNT 32
char * vt_names[VT_NAME_COUNT] = {
    "Vitzh",
    "m-Vitzh",
    "Vortex",
    "Juggernaut",
    "Dammy",
    "Falchion",
    "Blade",
    "Decider",
    "Scare face",
    "Scare face A1",
    "Prominence M1",
    "Prominence M2",
    "Prominence M3",
    "Scare face II",
    "Rapier",
    "Quasar",
    "Jaralaccs C",
    "Jaralaccs NS-R",
    "Jaralaccs N",
    "Regal dress N",
    "Regal dress A",
    "Behemoth",
    "Maelstrom",
    "Sheepdog",
    "Garpike",
    "Siegeszug",
    "Colt Executive",
    "Colt",
    "Jaralaccs Macabre",
    "Earth Shaker",
    "Yellow Jacket",
    "Decider Volcanic"
};

int export(char * path) {
    FILE * engf = fopen(path, "rb");
    if (!engf) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return 1;
    }
    
    strncpy(out_path, path, sizeof(out_path));
    char * sep = strrchr(out_path, SEPARATOR);
    if (sep) strcpy(sep + 1, "mechdata.json");
    else strcpy(out_path, "mechdata.json");
    
    FILE * outf = fopen(out_path, "w");
    if (!outf) {
        fprintf(stderr, "Failed to open output file: %s\n", out_path);
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
    
    if (sep) strcpy(sep + 1, ".data.seg");
    else strcpy(out_path, ".data.seg");
    
    FILE * datf = fopen(out_path, "rb");
    if (!datf) {
        fprintf(stderr, "Failed to open file: %s\n", out_path);
        return 1;
    }
    
    // Read file header
    uint32_t file_count;
    fread(&file_count, sizeof(uint32_t), 1, engf);
    
    printf("Unpacking %u engine data files\n", file_count);
    
    uint32_t file_sizes[file_count];
    fread(&file_sizes, sizeof(uint32_t), file_count, engf);

    fseek(datf, 0x345C0, SEEK_SET);    
    uint32_t manufacturers[file_count];
    fread(manufacturers, sizeof(uint32_t), file_count, datf);
    
    fseek(datf, 0x668E0, SEEK_SET); // Start of loadout data
    struct loadout_data loadouts[file_count];
    fread(loadouts, sizeof(struct loadout_data), file_count, datf);
    
    fseek(datf, 0x61D70, SEEK_SET);
    uint32_t loadout_presets[file_count];
    fread(loadout_presets, sizeof(uint32_t), file_count, datf);
    
    fseek(datf, 0x61E00, SEEK_SET);
    uint32_t loadout_fixed_mounts[file_count];
    fread(loadout_fixed_mounts, sizeof(uint32_t), file_count, datf);
    
    fseek(datf, 0x57A40, SEEK_SET);
    struct parts_data parts[file_count];
    fread(parts, sizeof(struct parts_data), file_count, datf);
    
    jwOpen(json_buffer, sizeof(json_buffer), JW_ARRAY, JW_PRETTY);
    
    // Read each engine data entry
    int unpack_count = 0;
    for (int i = 0; i < file_count; i++) {
        // Skip entries with invalid sizes
        if (file_sizes[i] != sizeof(struct engine_data)) {
            fprintf(stderr, "Skipped entry %d due to invalid file size: %d (expected %ld)\n",
                i, file_sizes[i], sizeof(struct engine_data));
            fseek(engf, file_sizes[i], SEEK_CUR);
            continue;
        } 
        
        struct engine_data engdat;
        fread(&engdat, sizeof(struct engine_data), 1, engf);
        
        jwArr_object();
        
        const uint8_t gens[6] = {0, 1, 2, 1, 0, 1};
        uint8_t gen = gens[engdat.cockpit_type];

        jwObj_int("id", engdat.id);
        jwObj_int("manufacturer", manufacturers[i]);
        jwObj_int("gen", gen);
        jwObj_int("class_type", loadouts[i].class_type);
        jwObj_int("weight_type", loadouts[i].weight_type);
        jwObj_int("profile_description", loadouts[i].profile_description);
        jwObj_double("weight", engdat.weight);
        jwObj_double("tier_r", engdat.tier_r);
        
        jwObj_array("gears");
        for (int j = 0; j < 6; j++) {
            jwArr_double(engdat.gears[j]);
        }
        jwEnd();
        
        jwObj_double("gear_f", engdat.gear_f);
        jwObj_double("brake", engdat.brake);
        jwObj_double("rpm_min", engdat.rpm_min);
        jwObj_double("override_rpm", engdat.override_rpm);
        jwObj_double("override_torque", engdat.override_torque);
        jwObj_double("wheel_torque", engdat.wheel_torque);
        jwObj_double("wheel_start_speed", engdat.wheel_start_speed); // What's shown in brainbox2 = wheel_start_speed * 3.6
        jwObj_double("drag_coefficient", engdat.drag_coefficient);
        jwObj_double("rpm_rate", engdat.rpm_rate);
        jwObj_double("width", engdat.width);
        jwObj_double("height", engdat.height);
        jwObj_double("internal_resistance", engdat.internal_resistance);
        
        jwObj_array("rpm");
        jwArr_double(engdat.rpm0);
        jwArr_double(engdat.rpm1);
        jwArr_double(engdat.rpm2);
        jwArr_double(engdat.rpm3);
        jwEnd();
        
        jwObj_array("torque");
        jwArr_double(engdat.torque0);
        jwArr_double(engdat.torque1);
        jwArr_double(engdat.torque2);
        jwArr_double(engdat.torque3);
        jwEnd();
        
        jwObj_double("turn_speed", engdat.turn_speed);
        jwObj_double("balancer", engdat.balancer);
        jwObj_int("life", engdat.life);
        jwObj_int("l_leg_life", engdat.l_leg_life);
        jwObj_int("r_leg_life", engdat.r_leg_life);
        jwObj_int("opt_armor_life", engdat.opt_armor_life);
        jwObj_int("sub_tank_number", engdat.sub_tank_number);
        jwObj_double("main_tank_capacity", engdat.main_tank_capacity);
        jwObj_double("sub_tank_capacity", engdat.sub_tank_capacity);
        jwObj_double("tank_consumption", engdat.tank_consumption);
        jwObj_double("battery_capacity", engdat.battery_capacity);
        jwObj_double("armor_front", engdat.armor_front);
        jwObj_int("cockpit_type", engdat.cockpit_type);
        jwObj_int("use_ticket", engdat.use_ticket);
        jwObj_int("slope_a_gear", engdat.slope_a_gear);
        jwObj_int("slope_b_gear", engdat.slope_b_gear);
        jwObj_int("price", engdat.price);
        jwObj_double("armor_side", engdat.armor_side);
        jwObj_double("armor_rear", engdat.armor_rear);

        jwObj_int("weapon_mount", parts[i].weapon_mount);
        jwObj_int("manipulator", parts[i].manipulator);
        
        jwObj_double("collider_offset_z", parts[i].collider_offset_z / 100.0);
        jwObj_array("collider_size");
            jwArr_double(parts[i].collider_size.x / 100.0);
            jwArr_double(parts[i].collider_size.y / 100.0);
            jwArr_double(parts[i].collider_size.z / 100.0);
        jwEnd();

        jwObj_object("loadout"); { // Start of loadout
            jwObj_int("mounts", loadouts[i].mounts);
            jwObj_int("standard", engdat.standard_loadout);
            jwObj_int("max", engdat.max_loadout);

            int8_t item;
            
            jwObj_array("mweps");
            fseek(datf, loadouts[i].mwep_offset - hdr_data.vaddr, SEEK_SET);
            while ((item = fgetc(datf)) >= -1 ) {
                jwArr_int(item);
            }
            jwEnd();

            jwObj_array("sweps");
            fseek(datf, loadouts[i].swep_offset - hdr_data.vaddr, SEEK_SET);
            while ((item = fgetc(datf)) >= -1) {
                jwArr_int(item);
            }
            jwEnd();
            
            jwObj_array("tanks");
            fseek(datf, loadouts[i].tank_offset - hdr_data.vaddr, SEEK_SET);
            while ((item = fgetc(datf)) >= -1) {
                jwArr_int(item);
            }
            jwEnd();
        
            int8_t preset[LOADOUT_SLOT_COUNT];
            fseek(datf, loadout_presets[i] - hdr_data.vaddr, SEEK_SET);
            fread(preset, sizeof(int8_t), LOADOUT_SLOT_COUNT, datf);
            
            int8_t fixed[LOADOUT_SLOT_COUNT];
            bool fixed_valid = loadout_fixed_mounts[i] - hdr_data.vaddr < hdr_data.fsize;
            
            if (fixed_valid) {
                fseek(datf, loadout_fixed_mounts[i] - hdr_data.vaddr, SEEK_SET);
                fread(fixed, sizeof(int8_t), LOADOUT_SLOT_COUNT, datf);
                printf("VT %d preset with fixed mounts: Preset %08X, Fixed %08X\n", engdat.id, loadout_presets[i] - hdr_data.vaddr, loadout_fixed_mounts[i] - hdr_data.vaddr);
            }

            for (int li = 0; li < LOADOUT_SLOT_COUNT; li++) {
                jwObj_object(loadoutSlotNames[li]);
                jwObj_int("type", preset[li]);
                jwObj_bool("fixed", fixed_valid ? fixed[li] : false);
                jwEnd();
            }

        } jwEnd(); // Loadout object end

        jwEnd(); // Array entry end        
        unpack_count++;
    }

    fclose(engf);
    fclose(datf);
    
    printf("Unpacked %d engine data entries\n", unpack_count);
    
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

int unpack(char * path) {
    FILE * engf = fopen(path, "rb");
    if (!engf) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return 1;
    }
    
    char * sep = strrchr(path, SEPARATOR);
    if (sep) sep[1] = '\0';
    else *path = '\0';
    
    // Read file header
    uint32_t file_count;
    fread(&file_count, sizeof(uint32_t), 1, engf);
    
    printf("Unpacking %u engine data files\n", file_count);
    
    uint32_t file_sizes[file_count];
    fread(&file_sizes, sizeof(uint32_t), file_count, engf);
    
    for (int i = 0; i < file_count; i++) {
        // Skip entries with invalid sizes
        if (file_sizes[i] != sizeof(struct engine_data)) {
            fprintf(stderr, "Skipped entry %d due to invalid file size: %d (expected %ld)\n",
                i, file_sizes[i], sizeof(struct engine_data));
            fseek(engf, file_sizes[i], SEEK_CUR);
            continue;
        }
        
        struct engine_data engdat;
        fread(&engdat, sizeof(struct engine_data), 1, engf);
        
        snprintf(out_path, sizeof(out_path), "%sengdat%02d.eng", path, i);
        
        FILE * outf = fopen(out_path, "wb");
        if (!outf) {
            fprintf(stderr, "Failed to open output file: %s\n", out_path);
            fclose(engf);
            return 1;
        }
        
        fwrite(&engdat, sizeof(struct engine_data), 1, outf);
        fclose(outf);
    }
    
    fclose(engf);
    return 0;
}

int pack(char * path) {
    fprintf(stderr, "Unimplimented\n");
    return 1;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Engine Data Tool - By QuantX\n");

    char progmode = 0;
    while (argc && **argv == '-') {
        (*argv)++;
        switch (**argv) {
        case 'h':
            printf("%s", helpmsg);
            return 0;
        case 'e':
    	case 'u':
    	case 'p':
    	    progmode = **argv;
    	    break;
        }
    	argv++; argc--;
    }
    
    if (!argc) {
        fprintf(stderr, "Please specify a file to edit or run '%s -h' for help\n", progname);
        return 1;
    }

    char * path = *argv;
    
    if (progmode == 'e') return export(path);
    else if (progmode == 'u') return unpack(path);
    else if (progmode == 'p') return pack(path);
    else {
        fprintf(stderr, "Unknown program mode, try: %s -h\n", progname);
        return 1;
    }
    return 0;
}
