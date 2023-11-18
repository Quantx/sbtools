#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#ifdef __linux__
#include <termios.h>
#endif

const float ms2kmh = 5.0f / 18.0f;

char * progname;

char * helpmsg = "Interactive tool used to edit SB engine data files\n"
"Show help: sbengine -h\n"
"Edit engine data: sbengine eng_data.eng\n"
"Unpack engine data: sbengine -u eng_data.eng\n"
"Pack engine data: sbengine -p eng_data\n";

struct engine_data {
    uint32_t id;
    float weight;
    float tier_r;
    float gears[6]; // 0=Reverse, 1=1st, ... , 5=5th
    float gear_f;
    float brake;
    float min_rpm;
    float override_rpm;
    float override_torque;
    float shift5_torque;
    float shift5_start_speed; // What's shown in brainbox2 = shift5_start_speed * 3.6
    float cd;
    float rpm_rate;
    float width;
    float height;
    float ir;
    uint32_t zero;
    float start_torque;
    float rpm1;
    float torque1;
    float rpm2;
    float torque2;
    float max_rpm;
    float torque3;
    float max_rot;
    float balancer;
    uint16_t life;
    uint16_t l_leg_life;
    uint16_t r_leg_life;
    uint16_t t_soukou_life;
    uint32_t sub_tank_number;
    float main_tank_capacity;
    float sub_tank_capacity;
    float tank_consumption;
    float energy_max;
    float armor;
    uint32_t cockpit_type;
    uint32_t use_ticket;
    uint32_t saka_a;
    uint32_t saka_b;
    uint32_t price;
    uint32_t max_loadout;
    uint32_t standard_loadout;
    float armor_side;
    float armor_rear;
};

int unpack(char * path) {
    FILE * engf = fopen(path, "rb");
    if (!engf) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return 1;
    }
    
    // Read file header
    uint32_t file_count;
    fread(&file_count, sizeof(uint32_t), 1, engf);
    
    printf("Unpacking %d engine data files\n", file_count);
    
    uint32_t file_sizes[file_count];
    fread(&file_sizes, sizeof(uint32_t), file_count, engf);
    
    // Read each engine data entry
    for (int i = 0; i < file_count; i++) {
        char out_name[16];
        snprintf(out_name, 16, "engdat%02d.eng", i);
        
        FILE * outf = fopen(out_name, "wb");
        if (!outf) {
            fprintf(stderr, "Failed to open %s\n", out_name);
            return 1;
        }
        
        uint8_t buf[file_sizes[i]];
        fread(buf, sizeof(uint8_t), file_sizes[i], engf);
        fwrite(buf, sizeof(uint8_t), file_sizes[i], outf);
        
        fclose(outf);
    }

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
    
    if (progmode == 'u') return unpack(path);
    else if (progmode == 'p') return pack(path);
    
    // Check file extension
    char * ext = strrchr(path, '.');
    if (!ext) {
        fprintf(stderr, "Missing file extension: %s\n", path);
        return 1;
    }
    ext++;
    if (strncmp(ext, "eng", 3)) {
        fprintf(stderr, "Incorrect file extension: %s\n", path);
        return 1;
    }
    
    FILE * engf = fopen(path, "rb");
    if (!engf) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return 1;
    }
    
    // Read file header
    uint32_t file_count;
    fread(&file_count, sizeof(uint32_t), 1, engf);
    
    printf("Reading %d engine data files\n", file_count);
    
    uint32_t file_sizes[file_count];
    fread(&file_sizes, sizeof(uint32_t), file_count, engf);
    
    // Read file contents
    struct engine_data engdat[file_count];
    bzero(engdat, sizeof(engdat));
    
    for (int i = 0; i < file_count; i++) {
        fread(engdat + i, file_sizes[i], 1, engf);
    }
    
    // Done loading data
    fclose(engf);
    
#ifdef __linux__
    struct termios attr;
    tcgetattr(STDIN_FILENO, &attr);
    attr.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &attr);
#else

#endif
    
    int cur_vt = 0;
    int cur_page = 0;
    int cur_line = 0;
    
    while (1) {
        struct engine_data * vt = engdat + cur_vt;
        
        printf("\x1B[H\x1B[J");

        printf("VT ID: %d", vt->id);
        if (vt->id != cur_vt) printf(" (BAD ID %d)", cur_vt);
        printf("\n");
        printf("Page %d\n", cur_page);
        
        if (cur_page == 0) {
            printf("Weight: %.2fkg\n", vt->weight);
            printf("tier R: %.2fm\n", vt->tier_r);
            printf("Gear R: %.2f\n", vt->gears[0]);
            for (int i = 1; i < 6; i++) printf("Gear %d: %.2f\n", i, vt->gears[i]);
            printf("Gear F: %.2f\n", vt->gear_f);
            printf("Brake: %.2f\n", vt->brake);
            printf("RPM Rate: %.2f\n", vt->rpm_rate);
            printf("CD: %.2f\n", vt->cd);
            printf("Width: %.2fm\n", vt->width);
            printf("Height: %.2fm\n", vt->height);
            printf("IR: %.2f\n", vt->ir);
            printf("Max Rot: %.2fdeg\n", vt->max_rot * 180 / M_PI);
            printf("Min RPM: %.2frpm\n", vt->min_rpm);
            printf("Balancer: %.2f\n", vt->balancer);
        } else if (cur_page == 1) {
            printf("Start Torque: %.2fkgm\n", vt->start_torque);
            printf("RPM 1: %.2frpm\n", vt->rpm1);
            printf("Torque 1: %.2fkgm\n", vt->torque1);
            printf("RPM 2: %.2frpm\n", vt->rpm2);
            printf("Torque 2: %.2fkgm\n", vt->torque2);
            printf("Max RPM: %.2fkgm\n", vt->max_rpm);
            printf("Torque 3: %.2fkgm\n", vt->torque3);
            printf("Shift 5 Torque: %.2fkgm\n", vt->shift5_torque);
            printf("Shift 5 Start Speed: %.2fkm/h\n", vt->shift5_start_speed * 3.6f);
            printf("Override RPM: %.2frpm\n", vt->override_rpm);
            printf("Override Torque Rate: %.2f\n", vt->override_torque);
            printf("Saka A: %d\n", vt->saka_a);
            printf("Saka B: %d\n", vt->saka_b);
        } else if (cur_page == 2) {
            printf("Life: %d\n", vt->life);
            printf("Left Leg Life: %d\n", vt->l_leg_life);
            printf("Right Leg Life: %d\n", vt->r_leg_life);
            printf("T Soukou Life: %d\n", vt->t_soukou_life);
            printf("Sub Tank Number: %d\n", vt->sub_tank_number);
            printf("Main Tank Capcity: %.2f\n", vt->main_tank_capacity);
            printf("Sub Tank Capcity: %.2f\n", vt->sub_tank_capacity);
            printf("Tank Consumption: %.2f\n", vt->tank_consumption);
            printf("Energy Max: %.2f\n", vt->energy_max);
            printf("Armor: %.2f\n", vt->armor);
            printf("Armor Side: %.2f\n", vt->armor_side);
            printf("Armor Rear: %.2f\n", vt->armor_rear);
            printf("Cockpit Type: %d\n", vt->cockpit_type);
            printf("Use Ticket: %d\n", vt->use_ticket);
            printf("Price: %d\n", vt->price);
            printf("Max Loadout: %d\n", vt->max_loadout);
            printf("Standard Loadout: %d\n", vt->standard_loadout);
        }

        printf("\x1B[H");
        
        // Draw charts
        if (vt->tier_r > 0) {
            printf("\x1B[39C Speed  |     Normal |   Override\n");
            for (int g = 0; g < 6; g++) {
                float normal = (vt->max_rpm * vt->tier_r * 0.05f) / (vt->gear_f * vt->gears[g]);
                float override = (vt->override_rpm * vt->tier_r * 0.05f) / (vt->gear_f * vt->gears[g]);
                printf("\x1B[39C Gear %c | %6.2fkm/h | %6.2fkm/h\n", g ? '0' + g : 'R', normal / ms2kmh, override / ms2kmh);
            }
            
            float torques[] = {
                vt->start_torque,
                vt->torque1,
                vt->torque2,
                vt->torque3,
                vt->shift5_torque
            };
            
            printf("\n\x1B[39C Normal | TORQ 0 | TORQ 1 | TORQ 2 | TORQ 3 | TORQ 5\n");
            for (int g = 0; g < 6; g++) {
                printf("\x1B[39C Gear %c", g ? '0' + g : 'R');
                
                for (int t = 0; t < 5; t++) {
                    printf(" | %6.0f", (torques[t] * vt->gear_f * vt->gears[g] * 2.0f) / vt->tier_r);
                }
                printf("\n");
            }
            
            printf("\n\x1B[39C Override | TORQ 0 | TORQ 1 | TORQ 2 | TORQ 3 | TORQ 5\n");
            for (int g = 0; g < 6; g++) {
                printf("\x1B[39C Gear %c  ", g ? '0' + g : 'R');
                
                for (int t = 0; t < 5; t++) {
                    printf(" | %6.0f", (torques[t] * vt->gear_f * vt->gears[g] * 2.0f * vt->override_torque) / vt->tier_r);
                }
                printf("\n");
            }
        }
        
        // Update cursor
        printf("\x1B[%d;0H", cur_line + 1);
        fflush(stdout);
        
        char code;
        read(STDIN_FILENO, &code, 1);
        if (code == 'q' || code == 'Q') break; // Exit
        else if (code == '\x1B') {
            char escape[5];
            read(STDIN_FILENO, escape, 5);
            
            if (escape[0] == '[') {
                if (escape[1] == 'A') { // Up
                    cur_line--;
                } else if (escape[1] == 'B') { // Down
                    cur_line++;
                } else if (escape[1] == 'C') { // Right
                    if (cur_line == 0) cur_vt++;
                    else if (cur_line == 1) cur_page++;                    
                } else if (escape[1] == 'D') { // Left
                    if (cur_line == 0) cur_vt--;
                    else if (cur_line == 1) cur_page--;
                } else if (escape[1] == '5') { // Page Up
                    cur_page++;
                } else if (escape[1] == '6') { // Page Down
                    cur_page--;
                }
                
                if (cur_vt < 0) cur_vt = file_count - 1;
                else if (cur_vt >= file_count) cur_vt = 0;

                if (cur_page < 0) cur_page = 2;                
                else if (cur_page > 2) cur_page = 0;

                if (cur_page == 0) {
                    if (cur_line < 0) cur_line = 19;
                    else if (cur_line > 19) cur_line = 0;
                } else if (cur_page == 1) {
                    if (cur_line < 0) cur_line = 14;
                    else if (cur_line > 14) cur_line = 0;
                } else if (cur_page == 2) {
                    if (cur_line < 0) cur_line = 18;
                    else if (cur_line > 18) cur_line = 0;                
                }
            } 
        }
    }
    
    printf("\x1B[24;0H\n");

#ifdef __linux__
    attr.c_lflag |= ICANON | ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &attr);
#else

#endif
    
    return 0;
}
