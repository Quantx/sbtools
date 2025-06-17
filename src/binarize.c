#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifdef __linux__
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#else
#include <windows.h>
#endif

#define FORMAT_XBO 0x152
#define FORMAT_SHA 0x12

#define FLAG_PADDED 0x1

#define DVD_SECTOR_SIZE 2048

struct file_entry {
    uint32_t offset;
    uint32_t length;
};

#ifdef __linux__
const char separator = '/';
#else
const char separator = '\\';
#endif

#define MAX_OUT_PATH 256
char out_path[MAX_OUT_PATH];

int lengthmatch;
int testmode;
int nopadding;

char * helpmsg = "Used pack and unpack SB .bin files\n"
"Show help: binarize -h\n"
"Unpack: binarize -u <path/package.bin>\n"
"Pack: binarize -p <path/directory/>\n"
"Pack with length matching: binarize -l -p <path/directory/>\n\n"
"Pack without padding: binarize -f -p <path/directory/>\n\n"
"Length matching will ensure that the generated binary is exactly the same size\n"
"as the original. However, this will impose limits on the size of each\n"
"individual file based on how much leftover space was present after file in the\n"
"original binary. The amount of leftover space after any given file will always\n"
"be less than 2048 bytes as this is the sector size of a DVD.\n";

char * progname;

int pack(char * path) {
    // Remove path separator from end of path
    char * tail = path + strlen(path) - 1;
    if (*tail == separator) *tail = '\0';
    
    // Open manifest file
    int pr = snprintf(out_path, MAX_OUT_PATH, "%s%cmanifest.txt", path, separator);
    if (pr < 0 || pr >= MAX_OUT_PATH) {
        fprintf(stderr, "Ran out of room when trying to allocate manifest path\n");
        return 1;
    }

    FILE * manf = fopen(out_path, "r");
    if (!manf) {
        fprintf(stderr, "Failed to open manifest file: %s\n", out_path);
        return 1;
    }
    
    printf("Reading manifest from %s\n", out_path);

    fgets(out_path, MAX_OUT_PATH, manf);
    uint32_t file_count, flags;
    if (fscanf(manf, "%d,%X,%256s\n", &file_count, &flags, out_path) != 3) {
        fprintf(stderr, "Manifest file did not contain output file size and name\n");
        fclose(manf);
        return 1;
    }
    uint32_t man_start = ftell(manf);
    
    if (nopadding) flags &= ~FLAG_PADDED;

    FILE * binf = fopen(out_path, "wb");
    if (!binf) {
        fprintf(stderr, "Failed to open output binary file: %s\n", out_path);
        fclose(manf);
        return 1;
    }
    
    printf("Packing %d files into %s\n", file_count, out_path);
    
    struct file_entry * file_list = malloc(file_count * sizeof(struct file_entry));
    
    fwrite("BIN0", 1, 4, binf);
    fwrite(&file_count, 4, 1, binf);
    
    // Compute file start positions
    uint32_t current_offset = file_count * sizeof(struct file_entry) + 8;
    if (flags & FLAG_PADDED) {
        int alignment = current_offset % DVD_SECTOR_SIZE;
        if (alignment) current_offset += DVD_SECTOR_SIZE - alignment;
    }

    uint32_t i = 0, offset, length = 0;
    while (fscanf(manf, "%X,%X,%256s\n", &offset, &length, out_path) == 3) {
        if (i >= file_count) {
            fprintf(stderr, "Too many files listed in manifest\n");
            free(file_list);
            fclose(manf);
            fclose(binf);
            return 1;
        }
        
        FILE * packf = fopen(out_path, "rb");
        if (!packf) {
            fprintf(stderr, "Failed to open file for length checking: %s\n", out_path);
            free(file_list);
            fclose(manf);
            fclose(binf);
            return 1;
        }
        
        // Get file size
        file_list[i].length = 0;
        file_list[i].offset = current_offset;
        while (fgetc(packf) >= 0) file_list[i].length++;
        fclose(packf);
        
        // Output to binary
        fwrite(file_list + i, 4, 2, binf);
    
        if (testmode) {
            // Verify that the generated offsets and lengths of the new binary match exactly    
            if (offset != current_offset) {
                fprintf(stderr, "Warning offsets differ %s: Old %08X, New %08X\n", out_path, offset, current_offset);
                free(file_list);
                fclose(manf);
                fclose(binf);
                return 1;
            }
            
            if (file_list[i].length != length) {
                fprintf(stderr, "Warning file %s length of %d exceeds original file length of %d\n", out_path, file_list[i].length, length);
                free(file_list);
                fclose(manf);
                fclose(binf);
                return 1;
            }
        }
        
        // Compute offset of next file
        current_offset += file_list[i].length;
        
        if (flags & FLAG_PADDED) {
            int alignment = current_offset % DVD_SECTOR_SIZE;
            if (alignment) current_offset += DVD_SECTOR_SIZE - alignment;

            if (lengthmatch) {
                // Verify that the length of each file does not cross a new block boundry
                uint32_t original_offset = (((offset + length) / DVD_SECTOR_SIZE) + 1) * DVD_SECTOR_SIZE;
                if (current_offset != original_offset) {
                    fprintf(stderr, "File %s new size %d exceeded allocated space of %d\n", out_path, file_list[i].length, original_offset - offset); 
                    free(file_list);
                    fclose(manf);
                    fclose(binf);
                    return 1;
                }
            }
        }
        
        i++;
    }
    
    i = 0;
    fseek(manf, man_start, SEEK_SET);
    while (fscanf(manf, "%X,%X,%256s\n", &offset, &length, out_path) == 3) {
        // Pad until start of next file
        for (int j = ftell(binf); j < file_list[i].offset; j++) fputc(0x2A, binf);
        
        FILE * packf = fopen(out_path, "rb");
        if (!packf) {
            fprintf(stderr, "Failed to open file for packing: %s\n", out_path);
            free(file_list);
            fclose(manf);
            fclose(binf);
            return 1;
        }
        
        // Pack file
        for (int j = 0; j < file_list[i].length; j++) {
            int data = fgetc(packf);
            if (data < 0) {
                fprintf(stderr, "Unexpected end of binary in file %s, %d, %ld\n", out_path, file_list[i].length, ftell(packf));
                free(file_list);
                fclose(packf);
                fclose(manf);
                fclose(binf);
                return 1;
            }
            fputc(data, binf);
        }
        
        fclose(packf);
        i++;
    }
    
    // Pad to nearest block
    for (int j = ftell(binf); j < current_offset; j++) fputc(0x2A, binf);
    
    printf("Packed %d files in %ld bytes\n", i, ftell(binf));
    
    free(file_list);
    fclose(binf);
    fclose(manf);
    return 0;
}

int unpack(char * path) {
    FILE * binf = fopen(path, "rb");
    if (!binf) {
        fprintf(stderr, "No file at path %s\n", path);
        return 1;
    }
    
    bool is_motion = strstr(path, "MOTION.bin") != NULL;
    bool is_vtmodel = strstr(path, "VTMODEL.bin") != NULL;
    bool is_hitbox = strstr(path, "ATARI.bin") != NULL;
    bool is_lsq = strstr(path, "LSQ.bin") != NULL || strstr(path, "SEQ.bin") != NULL;

    // Validate magic
    char magic[4];
    fread(magic, 1, 4, binf);

    if (strncmp(magic, "BIN0", 4)) {
        fprintf(stderr, "Cannot unpack file, invalid magic number. Are you sure this is a SB binary file?\n");
        fclose(binf);
        return 1;
    }

    // Get number of entries
    uint32_t file_count;
    fread(&file_count, 4, 1, binf);

    printf("Unpacking %d files from %s\n", file_count, path);

    struct file_entry * file_list = malloc(file_count * sizeof(struct file_entry));

    uint32_t flags = 0;
    for (int i = 0; i < file_count; i++) {
        fread(file_list + i, 4, 2, binf);
        if (i) { // Check if the data is padded
            if (file_list[i].offset > file_list[i - 1].offset + file_list[i - 1].length) {
                flags |= FLAG_PADDED;
            }
        } else { // Check if the header is padded
            if (file_list[0].offset > file_count * sizeof(struct file_entry) + 8) {
                flags |= FLAG_PADDED;
            }
        }
//        printf("File data %04d: %08X %08X\n", i, file_list[i].offset, file_list[i].length);
    }
    
    if (flags & FLAG_PADDED) printf("Binary is padded\n");
    else printf("Binary is not padded\n");

    // Create output directory
    char * tail = strrchr(path, '.');
    *tail = '\0';

#ifdef __linux__
    if (mkdir(path, 0777) < 0 && errno != EEXIST) {
#else
    if (!CreateDirectory(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
#endif
        fprintf(stderr, "Failed to create output directory: %s\n", path);
        fclose(binf);
        return 1;
    }
    
    tail[0] = separator;
    tail[1] = '\0';
    
    // Open manifest file
    int pr = snprintf(out_path, MAX_OUT_PATH, "%smanifest.txt", path);
    if (pr < 0 || pr >= MAX_OUT_PATH) {
        fprintf(stderr, "Ran out of room when trying to allocate manifest path\n");
        fclose(binf);
        return 1;
    }
    
    FILE * manf = fopen(out_path, "w");
    if (!manf) {
        fprintf(stderr, "Failed to open manifest file: %s\n", out_path);
        fclose(binf);
        return 1;
    }
    
    fprintf(manf, "### DO NOT TOUCH ### Generated by Binarize, used for repacking this binary ### DO NOT TOUCH ###\n");
    
    // Print out the original file name and stats to the manifest
    tail[0] = '.';
    tail[1] = 'b';
    fprintf(manf, "%d,%08X,%s\n", file_count, flags, path);
    tail[0] = separator;
    tail[1] = '\0';

    for (int i = 0; i < file_count; i++) {
        fseek(binf, file_list[i].offset, SEEK_SET);
        fread(magic, 1, 4, binf);

        // Label known files as such
        char * ext = "part";
        if (!strncmp(magic, "XPR", 3)) ext = "xpr";
        else if (!strncmp(magic, "RIFF", 4)) ext = "wav";
        else if (!memcmp(magic, "P\0\0\0", 4) && file_list[i].length >= 96) {
            // Might be an SB Model, check for other magics
            fseek(binf, file_list[i].offset + 0x58, SEEK_SET);
            uint32_t sbmdl[2];
            fread(sbmdl, sizeof(uint32_t), 2, binf);
            if (sbmdl[0] == 0x08 && sbmdl[1] == 0x18) {
                uint32_t vertex_offset;
                fread(&vertex_offset, sizeof(uint32_t), 1, binf);
                fseek(binf, file_list[i].offset + 0x50 + vertex_offset + 24, SEEK_SET);

                uint32_t vertex_format;
                fread(&vertex_format, sizeof(uint32_t), 1, binf);
                if (vertex_format == FORMAT_XBO) ext = "xbo";
                else if (vertex_format == FORMAT_SHA) ext = "sha";
                else ext = "sbmodel";
            }
        } else if (is_motion) ext = "lmt";
        else if (is_vtmodel && i >= 764) ext = "lmt";
        else if (is_hitbox) ext = "ppd";
        else if (is_lsq) ext = "lsq";

        pr = snprintf(out_path, MAX_OUT_PATH, "%s%04d.%s", path, i, ext);
        if (pr < 0 || pr >= MAX_OUT_PATH) {
            fprintf(stderr, "Ran out of room when trying to allocate output path\n");
            fclose(manf);
            fclose(binf);
            return 1;
        }

        FILE * outf = fopen(out_path, "wb");
        if (!outf) {
            fprintf(stderr, "Failed to open output file: %s\n", out_path);
            fclose(manf);
            fclose(binf);
            return 1;
        }
        
        // Record file to manifest
        fprintf(manf, "%08X,%08X,%s\n", file_list[i].offset, file_list[i].length, out_path);

        // Copy data
        fseek(binf, file_list[i].offset, SEEK_SET);
        for (int j = 0; j < file_list[i].length; j++) {
            if (feof(binf)) {
                fprintf(stderr, "Unexpected end of binary\n");
                fclose(binf);
                fclose(manf);
                fclose(outf);
                return 1;
            }

            int data = fgetc(binf);
            fputc(data, outf);
        }

        fclose(outf);
    }

    free(file_list);
    fclose(manf);
    fclose(binf);
    return 0;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Binarize Tool - By QuantX\n");

    char progmode = 0;
    while (argc && **argv == '-') {
        (*argv)++;
        switch (**argv) {
        case 'h':
            printf("%s", helpmsg);
            return 0;
        case 'l':
            lengthmatch = 1;
            break;
        case 'f':
            nopadding = 1;
            break;
        case 't':
            testmode = 1;
            break;
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
        else if (progmode == 'u') fprintf(stderr, "Please specify a file to unpack\n");
        return 1;
    }

    if (progmode == 'p') return pack(*argv);
    else if (progmode == 'u') return unpack(*argv);

    fprintf(stderr, "Unknown progmode: %c\n", progmode);
    return 1;
}
