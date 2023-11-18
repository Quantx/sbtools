#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "sha1.h"

#ifdef __linux__
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
const char separator = '/';
#else
#include <windows.h>
const char separator = '\\';
#endif

#define FLAG_PADDED 0x1

#define XBE_PADDING 4096

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

#define PATH_OUT_MAX 256
char out_path[PATH_OUT_MAX];
#define SEG_NAME_MAX 256

int lengthmatch;
int testmode;
int nopadding;

char * helpmsg = "Used pack and unpack the SB executable\n"
"Show help: segment -h\n"
"Unpack: segment -u <path/default.xbe>\n"
"Pack: segment -p <path/directory/>\n";

char * progname;

int pack(char * path) {
    // Remove path separator from end of path
    char * tail = path + strlen(path) - 1;
    if (*tail == separator) *tail = '\0';
    
    int pr = snprintf(out_path, PATH_OUT_MAX, "%s%cxbe_header.seg", path, separator);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate manifest path\n");
        return 1;
    }
    
    FILE * hdrf = fopen(out_path, "rb");
    if (!hdrf) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 1;
    }
    
    // Validate magic
    char magic[4];
    fread(magic, 1, 4, hdrf);

    if (strncmp(magic, "XBEH", 4)) {
        fprintf(stderr, "Cannot pack XBE, %s had an invalid magic number\n", out_path);
        fclose(hdrf);
        return 1;
    }
    
    printf("Reading XBE headers from %s\n", out_path);
    
    pr = snprintf(out_path, PATH_OUT_MAX, "%s.xbe", path);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate output path\n");
        fclose(hdrf);
        return 1;
    }
    
    FILE * xbef = fopen(out_path, "wb");
    if (!xbef) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        fclose(hdrf);
        return 1;
    }
    
    printf("Packing %s\n", out_path);
    
    // Read XBE header
    fseek(hdrf, 0x104, SEEK_SET);
    uint32_t base_addr;
    fread(&base_addr, sizeof(uint32_t), 1, hdrf);
    
    fseek(hdrf, 0x11C, SEEK_SET);
    uint32_t section_count;
    fread(&section_count, sizeof(uint32_t), 1, hdrf);
    uint32_t section_addr;
    fread(&section_addr, sizeof(uint32_t), 1, hdrf);
    
    printf("Base address %08X, Section address %08X\n", base_addr, section_addr);
    
    // Read section headers
    printf("Reading %d section headers\n", section_count);

    fseek(hdrf, section_addr - base_addr, SEEK_SET);
    struct section_header * sections = malloc(section_count * sizeof(struct section_header));
    fread(sections, sizeof(struct section_header), section_count, hdrf);
    
    uint32_t header_size = sections[0].faddr;
    printf("Header size %d\n", header_size);
    
    // Copy XBE header to output using old section headers for now
    fseek(hdrf, 0, SEEK_SET);
    for (int i = 0; i < header_size; i++) {
        int data = fgetc(hdrf);
        if (data < 0) {
            fprintf(stderr, "Ran out of data while copying header\n");
            fclose(hdrf);
            fclose(xbef);
            return 1;
        }
        fputc(data, xbef);
    }
    
    uint32_t file_pos = header_size;
    for (int i = 0; i < section_count; i++) {
        fseek(hdrf, sections[i].name_addr - base_addr, SEEK_SET);
        char seg_name[SEG_NAME_MAX];
        fread(seg_name, sizeof(char), SEG_NAME_MAX, hdrf);
        
#ifndef __linux__
        // Windows can't handle $ in the pathname
        for (int j = 0; j < SEG_NAME_MAX && seg_name[j]; j++) {
            if (seg_name[j] == '$') seg_name[j] = '_';
        }
#endif
        
        pr = snprintf(out_path, PATH_OUT_MAX, "%s%c%s.seg", path, separator, seg_name);
        if (pr < 0 || pr >= PATH_OUT_MAX) {
            fprintf(stderr, "Ran out of room when trying to allocate %s%c%s.seg path\n", path, separator, seg_name);
            fclose(hdrf);
            fclose(xbef);
            return 1;
        }
        
        FILE * segf = fopen(out_path, "rb");
        if (!segf) {
            fprintf(stderr, "Failed to open %s\n", out_path);
            fclose(hdrf);
            fclose(xbef);
            return 1;
        }
        
        printf("Reading %s\n", out_path);
        
        size_t data_size;
        uint32_t data_size_total = 0;
        
        // Copy data and compute length
        do {
            uint8_t data[256];
            data_size = fread(data, sizeof(uint8_t), 256, segf);
            fwrite(data, sizeof(uint8_t), data_size, xbef);            
            data_size_total += data_size;
        } while (data_size == 256);
        
        if (data_size < 0) {
            fprintf(stderr, "Error while reading segment\n");
            fclose(segf);
            fclose(hdrf);
            fclose(xbef);
            return 1;
        }

        printf("Read %d bytes\n", data_size_total);

        fseek(segf, 0, SEEK_SET);

        SHA1_CTX chk;
        SHA1Init(&chk);

        // Generate checksum
        SHA1Update(&chk, (char *)&data_size_total, sizeof(uint32_t));
        
        do {
            uint8_t data[256];
            data_size = fread(data, sizeof(uint8_t), 256, segf);
            SHA1Update(&chk, data, data_size);
        } while (data_size == 256);
        
        uint8_t checksum[20];
        SHA1Final(checksum, &chk);
        
        fclose(segf);
        
        // Output padding
        int padding = data_size_total % XBE_PADDING;
        if (padding) {
            padding = XBE_PADDING - padding;
            for (int j = 0; j < padding; j++) fputc(0, xbef);
        }
        
        int show_error = 1;
        for (int j = 0; j < 20; j++) {
            if (checksum[j] != sections[i].checksum[j] && show_error) {
                printf("Checksums differ. If you didn't change this file then this is an error!\n");
                
                printf("Old: ");
                for (int k = 0; k < 20; k++) printf("%02X", sections[i].checksum[k]);
                printf("\n");
                
                printf("New: ");
                for (int k = 0; k < 20; k++) printf("%02X", checksum[k]);
                printf("\n");
                
                show_error = 0;
            }
            sections[i].checksum[j] = checksum[j];
        }
        
        sections[i].faddr = file_pos;
        sections[i].fsize = data_size_total;
        
        // Compute new vsize for segXX segments
        if (!strncmp(seg_name, "seg", 3)) {
            sections[i].vsize = data_size_total + 92;
        }
        
        file_pos += data_size_total + padding;
    }
    
    // Output section headers
    fseek(xbef, section_addr - base_addr, SEEK_SET);
    fwrite(sections, sizeof(struct section_header), section_count, xbef);
    free(sections);
    
    fclose(hdrf);
    fclose(xbef);
    return 1;
}

int unpack(char * path) {
    FILE * xbef = fopen(path, "rb");
    if (!xbef) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }
    
    // Validate magic
    char magic[4];
    fread(magic, 1, 4, xbef);

    if (strncmp(magic, "XBEH", 4)) {
        fprintf(stderr, "Cannot unpack file, invalid magic number. Are you sure this is an XBE file?\n");
        fclose(xbef);
        return 1;
    }
    
    char * ext = strrchr(path, '.');
    *ext = '\0';
    
    int pr = snprintf(out_path, PATH_OUT_MAX, "%s%c", path, separator);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %s%c path\n", path, separator);
        return 1;
    }
    
#ifdef __linux__
    if (mkdir(out_path, 0777) < 0 && errno != EEXIST) {
#else
    if (!CreateDirectory(out_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
#endif
        fprintf(stderr, "Failed to create output directory: %s\n", out_path);
        fclose(xbef);
        return 1;
    }

    // Read XBE header
    fseek(xbef, 0x104, SEEK_SET);
    uint32_t base_addr;
    fread(&base_addr, sizeof(uint32_t), 1, xbef);
    
    fseek(xbef, 0x11C, SEEK_SET);
    uint32_t section_count;
    fread(&section_count, sizeof(uint32_t), 1, xbef);
    uint32_t section_addr;
    fread(&section_addr, sizeof(uint32_t), 1, xbef);
    
    printf("Base address %08X, Section address %08X\n", base_addr, section_addr);
    
    // Read section headers
    printf("Reading %d section headers\n", section_count);
    
    fseek(xbef, section_addr - base_addr, SEEK_SET);
    struct section_header * sections = malloc(section_count * sizeof(struct section_header));
    fread(sections, sizeof(struct section_header), section_count, xbef);
    
    uint32_t header_size = sections[0].faddr;
    printf("Header size %d\n", header_size);
    
    pr = snprintf(out_path, PATH_OUT_MAX, "%s%cxbe_header.seg", path, separator);
    if (pr < 0 || pr >= PATH_OUT_MAX) {
        fprintf(stderr, "Ran out of room when trying to allocate %s%cxbe_header.seg path\n", path, separator);
        return 1;
    }
    
    FILE * hdrf = fopen(out_path, "wb");
    if (!hdrf) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        fclose(xbef);
        return 1;
    }
    
    fseek(xbef, 0, SEEK_SET);
    for (int i = 0; i < header_size; i++) {
        int data = fgetc(xbef);
        if (data < 0) {
            fprintf(stderr, "Ran out of data\n");
            fclose(hdrf);
            fclose(xbef);
            return 1;
        }
        fputc(data, hdrf);
    }
    
    printf("Wrote %ld to header segment\n", ftell(hdrf));
    
    fclose(hdrf);

    int total_size = header_size;
    for (int i = 0; i < section_count; i++) {
        int padding = sections[i].fsize % XBE_PADDING;
        if (padding) {
            padding = XBE_PADDING - padding;
        }
        total_size += sections[i].fsize + padding;
    
        fseek(xbef, sections[i].name_addr - base_addr, SEEK_SET);
        char seg_name[SEG_NAME_MAX];
        fread(seg_name, sizeof(char), SEG_NAME_MAX, xbef);
        
#ifndef __linux__
        // Windows can't handle $ in the pathname
        for (int j = 0; j < SEG_NAME_MAX && seg_name[j]; j++) {
            if (seg_name[j] == '$') seg_name[j] = '_';
        }
#endif
        
        pr = snprintf(out_path, PATH_OUT_MAX, "%s%c%s.seg", path, separator, seg_name);
        if (pr < 0 || pr >= PATH_OUT_MAX) {
            fprintf(stderr, "Ran out of room when trying to allocate %s%c%s.seg path\n", path, separator, seg_name);
            return 1;
        }
        
        FILE * segf = fopen(out_path, "wb");
        if (!segf) {
            fprintf(stderr, "Failed to open %s\n", out_path);
            return 1;
        }
        
        printf("Extracting section: %s\n", seg_name);
        printf("Offset %08X, Size %d, Vsize %d, Diff %d\n", sections[i].faddr, sections[i].fsize, sections[i].vsize, sections[i].vsize - sections[i].fsize);
        printf("Head %08X, Tail %08X\n", sections[i].head_ref_count_addr, sections[i].tail_ref_count_addr);
        
        fseek(xbef, sections[i].faddr, SEEK_SET);
        
        for (int j = 0; j < sections[i].fsize; j++) {
            int data = fgetc(xbef);
            if (data < 0) {
                fprintf(stderr, "Ran out of data\n");
                fclose(segf);
                fclose(xbef);
                return 1;
            }
            fputc(data, segf);
        }
        
        fclose(segf);
    }
    printf("Total size %d\n", total_size);

    fclose(xbef);
    return 0;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Segment Tool - By QuantX\n");

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
        if (progmode == 'p') fprintf(stderr, "Please specify a directory to pack\n");
        else if (progmode == 'u') fprintf(stderr, "Please specify a .xbe to unpack\n");
        return 1;
    }

    if (progmode == 'p') return pack(*argv);
    else if (progmode == 'u') return unpack(*argv);

    fprintf(stderr, "Unknown progmode: %c\n", progmode);
    return 1;
}
