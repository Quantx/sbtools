#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "jWrite.h"
char json_buffer[1<<20]; // 1MB

#ifdef __linux__
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#define SEPARATOR '/'
#else
#include <windows.h>
#define SEPARATOR '\\'
#endif

char * progname;

struct xsb_cue {
    uint16_t flags;
    uint16_t sound;
    uint32_t name;
    uint32_t variations;
    uint16_t xfade;
    uint16_t unknown;
    uint32_t transitions;
} __attribute__((packed));

struct xsb_sound {
    union {
        struct {
            uint16_t track;
            uint16_t bank;
        };
        uint32_t offset;
    };
    uint16_t volume;
    uint16_t pitch;
    uint8_t track_count;
    int8_t layer;
    uint8_t category;
    uint8_t flags;
    uint16_t param3d;
    int8_t priority;
    uint8_t i3dl2_volume;
    uint16_t eq_gain;
    uint16_t eq_freq;
} __attribute__((packed));

struct xwb_region {
    uint32_t pos;
    uint32_t len;
};

#define XWB_TRACK_FLAGS(flags) (flags & 0xFu)
#define XWB_TRACK_DURATION(duration) (duration >> 4)

enum xwb_codec {
    XWB_CODEC_PCM,
    XWB_CODEC_ADPCM,
    XWB_CODEC_WMA,
    
    XWB_CODEC_UNKNOWN
};
const char * xwb_codec_names[] = {"  PCM", "ADPCM", "  WMA", "?????"};

#define XWB_TRACK_CODEC(format) ((format                    ) & ((1 <<  2) - 1))
#define XWB_TRACK_CHANS(format) ((format >> (2)             ) & ((1 <<  3) - 1))
#define XWB_TRACK_RATE(format)  ((format >> (2 + 3)         ) & ((1 << 18) - 1))
#define XWB_TRACK_ALIGN(format) ((format >> (2 + 3 + 18)    ) & ((1 <<  8) - 1))
#define XWB_TRACK_BITS(format)  ((format >> (2 + 3 + 18 + 8)) & ((1 <<  1) - 1))

struct xwb_track {
    uint32_t flags_duration; // flags:4 | duration:28
    uint32_t format;
    struct xwb_region play;
    struct xwb_region loop;
};

struct wav_header {
    uint16_t format;
    uint16_t channels;
    uint32_t samplesPerSec;
    uint32_t avgBytesPerSec;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    uint16_t extraSize;
} __attribute__((packed));

struct xadpcm_wav_header {
    struct wav_header wav;
    uint16_t nibblesPerBlock; // Always 64
} __attribute__((packed));

FILE * open_xwb(char * basepath, char * xwb_name) {
    char path[256];
    snprintf(path, sizeof(path), "%s%s.xwb", basepath, xwb_name);

    FILE * xwb = fopen(path, "rb");
    // Try again with a slightly different filename
    if (!xwb && xwb_name[0] >= 'A' && xwb_name[0] <= 'Z') {
        xwb_name[0] += 32;
        snprintf(path, sizeof(path), "%s%s.xwb", basepath, xwb_name);
        xwb = fopen(path, "rb");
    }
    
    if (!xwb) {
        fprintf(stderr, "Failed to open XWB: %s\n", path);
        return NULL;
    }
    
    char magic[4];
    fread(magic, sizeof(char), 4, xwb);
    
    if (strncmp(magic, "WBND", 4)) {
        fclose(xwb);
        fprintf(stderr, "Not an XWB wavebank: %s\n", path);
        return NULL;
    }
    
    uint32_t version;
    fread(&version, sizeof(uint32_t), 1, xwb);
    
    if (version != 3) {
        fclose(xwb);
        fprintf(stderr, "Unsupported XWB version %d: %s\n", version, path);
        return NULL;
    }
    
    return xwb;
}

int get_xwb_track_count(char * basepath, char * xwb_name) {
    FILE * xwb = open_xwb(basepath, xwb_name);
    if (!xwb) return -1;
    
    fseek(xwb, 0x2C, SEEK_SET);
    
    int32_t track_count;
    fread(&track_count, sizeof(uint32_t), 1, xwb);
    fclose(xwb);
    
    return track_count;
}

int write_wav_header(FILE * fd, struct wav_header * header, uint32_t data_size) {
    uint32_t header_size = sizeof(struct wav_header);
    if (header->format == 1) header_size -= sizeof(uint16_t);
    else header_size += header->extraSize;
    
    uint32_t wav_size = 8 + // RIFF + file_size
                        12 + header_size + // (WAVEfmt + header_size) + header
                        8 + data_size; // (data + size) + data 

    fputs("RIFF", fd);
    fwrite(&wav_size, sizeof(uint32_t), 1, fd);
    
    fputs("WAVEfmt ", fd);
    fwrite(&header_size, sizeof(uint32_t), 1, fd);
    fwrite(header, header_size, 1, fd);
    
    fputs("data", fd);
    fwrite(&data_size, sizeof(uint32_t), 1, fd);
}

int decode_xwb(char * basepath, char * xwb_name, char ** track_names) {
    FILE * xwb = open_xwb(basepath, xwb_name);
    if (!xwb) return 1;

    struct xwb_region segments[4];
    fread(segments, sizeof(struct xwb_region), 4, xwb);
    
    fseek(xwb, segments[0].pos, SEEK_SET); // Read wavebank data
    
    uint32_t flags;
    fread(&flags, sizeof(uint32_t), 1, xwb);
    
    if (flags != 0 && flags != 1) {
        fclose(xwb);
        fprintf(stderr, "Unknown flag configuration %08X\n", flags);
        return 1;
    }
    
    uint32_t track_count;
    fread(&track_count, sizeof(uint32_t), 1, xwb);
    
    char bank_name[16];
    fread(bank_name, sizeof(char), 16, xwb);
    
    printf("Unpacking wavebank: Name \"%.16s\", Tracks %d\n", bank_name, track_count);
    
    uint32_t track_header_size;
    fread(&track_header_size, sizeof(uint32_t), 1, xwb);
    
    if (track_header_size != sizeof(struct xwb_track)) {
        fclose(xwb);
        fprintf(stderr, "Invalid track entry header size %d != %ld\n", track_header_size, sizeof(struct xwb_track));
        return 1;
    }
    
    uint32_t track_name_size;
    fread(&track_name_size, sizeof(uint32_t), 1, xwb);
    
    uint32_t alignment;
    fread(&alignment, sizeof(uint32_t), 1, xwb);
    
    fseek(xwb, segments[1].pos, SEEK_SET); // Read track headers

    struct xwb_track tracks[track_count];
    fread(tracks, sizeof(struct xwb_track), track_count, xwb);
    
    for (int i = 0; i < track_count; i++) {
        struct xwb_track * track = tracks + i;
        
        enum xwb_codec codec = XWB_TRACK_CODEC(track->format);
        uint32_t chans = XWB_TRACK_CHANS(track->format);
        uint32_t rate  = XWB_TRACK_RATE(track->format);
        uint32_t align = XWB_TRACK_ALIGN(track->format);
        uint32_t bits  = XWB_TRACK_BITS(track->format) ? 16 : 8;
        
        if (codec >= XWB_CODEC_UNKNOWN) codec = XWB_CODEC_UNKNOWN;
        const char * codec_name = xwb_codec_names[codec];
        
        char name_guess[64];
        snprintf(name_guess, sizeof(name_guess), "%s_track_%d", bank_name, i);
        
        char * name = track_names[i];
        if (!name) name = name_guess;
        
        printf("Track %03d | Codec %s, Chans %d, Rate %d, Align %d, Bits %2d | %s\n",
            i, codec_name, chans, rate, align, bits, name);
        
        if (codec == XWB_CODEC_UNKNOWN) continue;
        
        char * ext = codec == XWB_CODEC_WMA ? "wma" : "wav"; 
        
        char path[256];
        snprintf(path, sizeof(path), "%s%s%c", basepath, xwb_name, SEPARATOR);
        
#ifdef __linux__
        if (mkdir(path, 0777) < 0 && errno != EEXIST) {
#else
        if (!CreateDirectory(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
#endif
            fprintf(stderr, "Failed to create output directory: %s\n", path);
            fclose(xwb);
            return 1;
        }
        
        snprintf(path, sizeof(path), "%s%s%c%s.%s", basepath, xwb_name, SEPARATOR, name, ext);
        FILE * out = fopen(path, "wb");
        if (!out) {
            fprintf(stderr, "Failed to open output file: %s\n", path);
            continue;
        }
        
        // Generate header
        switch (codec) {
        case XWB_CODEC_PCM: {
            struct wav_header header = {0x0001, chans, rate};
            header.blockAlign = (bits / 8) * chans;
            header.avgBytesPerSec = header.blockAlign * rate;
            header.bitsPerSample = bits;
            
            write_wav_header(out, &header, track->play.len); 
        } break;
        case XWB_CODEC_ADPCM: {
            struct xadpcm_wav_header header = {{0x0069, chans, rate}, 64};
            header.wav.blockAlign = 36 * chans;
            header.wav.bitsPerSample = 4;
            header.wav.extraSize = 2;
            uint32_t dw = (((header.wav.blockAlign - (7 * chans)) * 8) / (4 * chans)) + 2;
            header.wav.avgBytesPerSec = (rate / dw) * header.wav.blockAlign;
            
            write_wav_header(out, &header.wav, track->play.len);
        } break;
        // WMA files are ready to go as the header is encoded with the data
        }
        
        fseek(xwb, segments[3].pos + track->play.pos, SEEK_SET);
        for (uint32_t i = 0; i < track->play.len; i++) {
            int byte = fgetc(xwb);
            if (byte == EOF) {
                fprintf(stderr, "Unexpected EOF\n");
                break;
            }
            fputc(byte, out);
        }
        
        fclose(out);
    }
    
    fclose(xwb);
    return 0;
}

struct sndque {
    char name[32];
    int id;
    int param;
};

int unpackDATA(char * path) {
    FILE * datf = fopen(path, "rb");
    if (!datf) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return 1;
    }

    jwOpen(json_buffer, sizeof(json_buffer), JW_ARRAY, JW_PRETTY);

    fseek(datf, 0x494B0, SEEK_SET);
    for (int i = 0; i < 1176; i++) {
        struct sndque que;
        fread(&que, sizeof(struct sndque), 1, datf);
        
        jwArr_object();
        
        jwObj_string("name", que.name);
        jwObj_int("id", que.id);
        jwObj_int("param", que.param);
        
        jwEnd();
    }

    int jw_err = jwClose();
    if (jw_err) {
        fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
        return 1;
    }
    
    char * sep = strrchr(path, SEPARATOR);
    if (sep) {
        sep[1] = '\0';
    } else {
        *path = '\0';
    }
    
    char outPath[256];
    snprintf(outPath, sizeof(outPath), "%scue_lookup.json", path);
    FILE * out = fopen(outPath, "w");
    if (!out) {
        fprintf(stderr, "Failed to open: %s\n", outPath);
        return 1;
    }
    
    fwrite(json_buffer, sizeof(char), strlen(json_buffer), out);
    fclose(out);

    return 0;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Sound Tool - By QuantX\n");

    if (!argc) {
        fprintf(stderr, "Please specify an XSB soundbank file: %s <path/example.xsb>\n", progname);
        return 1;
    }
    
    char * path = *argv++; argc--;
    
    char * ext = strrchr(path, '.');
    if (ext && !strcmp(ext + 1, "seg")) return unpackDATA(path);
    
    FILE * xsb = fopen(path, "rb");
    if (!xsb) {
        fprintf(stderr, "Failed to open XSB: %s\n", path);
        return 1;
    }
    
    char * sep = strrchr(path, SEPARATOR);
    if (sep) {
        sep[1] = '\0';
    } else {
        *path = '\0';
    }
    
    char magic[4];
    fread(magic, sizeof(char), 4, xsb);
    if (strncmp(magic, "SDBK", 4)) {
        fprintf(stderr, "Not an XSB file: %s\n", path);
        fclose(xsb);
    }
    
    uint16_t version;
    fread(&version, sizeof(uint16_t), 1, xsb);
    if (version != 11) {
        fprintf(stderr, "Unsupported XSB version: %d\n", version);
        fclose(xsb);
        return 1;
    }
    
    fseek(xsb, 0x8, SEEK_SET);
    uint32_t string_offset, entry_offset, string_end;
    fread(&string_offset, sizeof(uint32_t), 1, xsb);
    fread(&entry_offset, sizeof(uint32_t), 1, xsb);
    fread(&string_end, sizeof(uint32_t), 1, xsb);
    uint32_t string_length = string_end - string_offset;
    
    fseek(xsb, 0x1C, SEEK_SET);
    uint16_t sound_count, cue_count;
    fread(&sound_count, sizeof(uint16_t), 1, xsb);
    fread(&cue_count, sizeof(uint16_t), 1, xsb);
    
    fseek(xsb, 0x22, SEEK_SET);
    uint16_t xwb_count;
    fread(&xwb_count, sizeof(uint16_t), 1, xsb);
    
    printf("Reading XSB file: %d wavebanks, %d sounds, %d cues\n", xwb_count, sound_count, cue_count);
    
    fseek(xsb, 0x38, SEEK_SET);
    
    struct xsb_cue * cues = malloc(cue_count * sizeof(struct xsb_cue));
    fread(cues, sizeof(struct xsb_cue), cue_count, xsb);
    
    struct xsb_sound * sounds = malloc(sound_count * sizeof(struct xsb_sound));
    fread(sounds, sizeof(struct xsb_sound), sound_count, xsb);
    
    fseek(xsb, string_offset, SEEK_SET);
    char * string_table = malloc(string_length);
    fread(string_table, sizeof(char), string_length, xsb);
    
    char ** xwb_names = malloc(xwb_count * sizeof(char *));
    for (uint32_t i = 0; i < xwb_count; i++) {
        xwb_names[i] = string_table + (i * 16);
    }
    
    int xwb_track_counts[xwb_count];
    char ** xwb_track_names[xwb_count];
    for (uint32_t i = 0; i < xwb_count; i++) {
        xwb_track_counts[i] = get_xwb_track_count(path, xwb_names[i]);
        if (xwb_track_counts[i] < 0) return 1;
        xwb_track_names[i] = calloc(xwb_track_counts[i], sizeof(char *));
    }
    
    jwOpen(json_buffer, sizeof(json_buffer), JW_OBJECT, JW_PRETTY);
    
    for (uint32_t i = 0; i < cue_count; i++) {
        struct xsb_cue * cue = cues + i;
        
        char * name = string_table + (cue->name - string_offset);
        
        jwObj_object(name); // Start of Cue object
        
        if (cue->variations != -1) {
            printf("Cue %04d has variations\n", i);
            return 1;
        } 
        
        if (cue->sound >= sound_count) {
            printf("Invalid sound %04d, max sounds %04d for cue %04d name \"%.16s\"\n", cue->sound, sound_count, i, name);
            return 1;
        }
        
        struct xsb_sound * sound = sounds + cue->sound;
        
        jwObj_object("sound"); // Start of Sound object
        
        jwObj_double("volume", (double)(sound->volume) / 65535.0);
        jwObj_double("pitch", (double)(sound->pitch) / 65535.0);
        // jwObj_int("track_count", sound->track_count); // Always 1
        jwObj_int("layer", sound->layer);
        jwObj_int("category", sound->category);
        jwObj_int("flags", sound->flags);
        jwObj_int("param3d", sound->param3d);
        //jwObj_int("priority", sound->priority); // Always -1
        jwObj_int("i3dl2_volume", sound->i3dl2_volume);
        // jwObj_int("eq_gain", sound->eq_gain); // Always 0
        // jwObj_int("eq_freq", sound->eq_freq); // Always 0
        
        jwEnd(); // End of Sound object
        
        uint16_t track = sound->track;
        uint16_t bank = sound->bank;
        if (!(sound->flags & 0x8)) {
            printf("Cue %04d name \"%.16s\": sound %04d is complex, offset: %08X\n", i, name, cue->sound, sound->offset);
            fseek(xsb, sound->offset, SEEK_SET);
            
            uint8_t cmd_count;
            uint32_t cmd_offset;
            fread(&cmd_offset, sizeof(uint32_t), 1, xsb);
            cmd_count = cmd_offset & 0xFF;
            cmd_offset >>= 8;
            
            fseek(xsb, cmd_offset, SEEK_SET);
            
            printf("Command count: %d\n", cmd_count);
            
            for (int c = 0; c < cmd_count; c++) {
                long long cmd_pos = ftell(xsb);
                
                uint8_t cmd, args;
                fread(&cmd, sizeof(uint8_t), 1, xsb);
                fseek(xsb, 3, SEEK_CUR);
                fread(&args, sizeof(uint8_t), 1, xsb);
                
                if (cmd == 0xA) args += 3;
                
                printf("Command trace (%08llX): %02X args %d", cmd_pos, cmd, args);
                
                if (cmd == 0 || cmd == 1) {
                    fseek(xsb, 3, SEEK_CUR);
                    if (cmd == 0 && args != 0x4) {
                        printf(" | argument missmatch\n");
                        return 1;
                    }
                    if (cmd == 1 && args != 0x10) {
                        printf(" | argument missmatch\n");
                        return 1;
                    }
                    if (c != cmd_count - 1) {
                        printf(" | NOT LAST COMMAND\n");
                        return 1;
                    }
                
                    fread(&track, sizeof(uint16_t), 1, xsb);
                    fread(&bank, sizeof(uint16_t), 1, xsb);
                    printf(" | bank %d, track %d\n", bank, track);
                    break;
                }
                
                printf("\n");
                
                // Skip this command
                fseek(xsb, args, SEEK_CUR);
            }
        }
        
        if (bank >= xwb_count) {
            printf("Invalid bank %d for cue %04d name \"%.16s\" sound %04d\n", bank, i, name, cue->sound);
            return 1;
        }
        
        if (track >= xwb_track_counts[bank]) {
            printf("Invalid track id %d for bank %d with %d tracks for cue %04d name \"%.16s\" sound %04d\n",
                track, bank, xwb_track_counts[bank], i, name, cue->sound);
            return 1;
        }
        
        printf("Cue %04d name \"%.16s\": bank \"%s\", track %03d\n", i, name, xwb_names[bank], track);
        
        if (!xwb_track_names[bank][track] || strlen(name) < strlen(xwb_track_names[bank][track])) {
            xwb_track_names[bank][track] = name;
        }
        
        jwObj_string("bank", xwb_names[bank]);
        jwObj_string("file", xwb_track_names[bank][track]);
        
        jwEnd(); // End of Cue Object
    }
    
    int jw_err = jwClose();
    if (jw_err) {
        fprintf(stderr, "JSON writer error: %s\n", jwErrorToString(jw_err));
        return 1;
    }
    
    char outPath[256];
    snprintf(outPath, sizeof(outPath), "%scues.json", path);
    FILE * out = fopen(outPath, "w");
    if (!out) {
        fprintf(stderr, "Failed to open: %s\n", outPath);
        return 1;
    }
    
    fwrite(json_buffer, sizeof(char), strlen(json_buffer), out);
    fclose(out);
    
    for (uint32_t i = 0; i < xwb_count; i++) {
        if (decode_xwb(path, xwb_names[i], xwb_track_names[i])) return 1; 
    }
    
    for (uint32_t i = 0; i < xwb_count; i++) free(xwb_track_names[i]);
    free(cues);
    free(sounds);
    free(string_table);
    free(xwb_names);
    fclose(xsb);
    
    return 0;
}
