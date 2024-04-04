#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


#ifdef __linux__
#include <iconv.h>
#else
#include <stringapiset.h>
#endif

#define STRING_COUNT 1637
struct string_pair {
    uint32_t jp;
    uint32_t en;
} string_pointers[STRING_COUNT];

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

#define BUF16_LEN 2048
char buf16[BUF16_LEN];
#define BUF8_LEN 2048
char buf8[BUF8_LEN];

char * progname;
char * out_path;

void convert_string(FILE * outf) {    
#ifdef __linux__
    size_t buf16_len = BUF16_LEN;
    size_t buf8_len = BUF8_LEN;
    
    char * ptr8 = buf8;
    char * ptr16 = buf16;
    
    iconv_t conv = iconv_open("UTF-8", "UTF-16");
    iconv(conv, &ptr16, &buf16_len, &ptr8, &buf8_len);
    iconv_close(conv);
#else
    WideCharToMultiByte(CP_UTF8, 0, (uint16_t *)buf16, BUF16_LEN / 2, buf8, BUF8_LEN, NULL, NULL);
#endif

    for (int j = 0; buf8[j]; j++) {
        switch (buf8[j]) {
        case '\n': fprintf(outf, "\\n"); break;
        case '\v':
        case '\t':
        case '\a':
            fprintf(outf, "%%s");
            j++;
            break;
        default:
            fputc(buf8[j], outf);
        }
    }
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;
    
    if (!argc) {
        fprintf(stderr, "Usage: ./%s <strings.csv>\n", progname);
    }
    
    out_path = *argv++; argc--;

    FILE * datf = fopen(".data.seg", "rb");
    if (!datf) {
        fprintf(stderr, "Failed to open: .data.seg\n");
        return 1;
    }
    
    fseek(datf, 0x59D50, SEEK_SET);
    fread(string_pointers, sizeof(struct string_pair), STRING_COUNT, datf);
    
    fclose(datf);
    
    FILE * hdrf = fopen(".rdata.hdr", "rb");
    if (!hdrf) {
        fprintf(stderr, "Failed to open: .data.hdr\n");
        return 1;
    }
    
    struct section_header hdr_data;
    fread(&hdr_data, sizeof(struct section_header), 1, hdrf);
    
    fclose(hdrf);
    
    FILE * rdatf = fopen(".rdata.seg", "rb");
    if (!rdatf) {
        fprintf(stderr, "Failed to open: .rdata.seg\n");
        return 1;
    }
    
    FILE * outf = fopen(out_path, "w");
    if (!outf) {
        fprintf(stderr, "Failed to open: %s\n", out_path);
        return 1;
    }
    
    fprintf(outf, "keys,jp,en\n");
    
    for (int i = 0; i < STRING_COUNT; i++) {
        fprintf(outf, "%04d;", i);
        if (string_pointers[i].jp) {
            fseek(rdatf, string_pointers[i].jp - hdr_data.vaddr, SEEK_SET);
            fread(buf16, sizeof(char), BUF16_LEN, rdatf);
            
            convert_string(outf);
        }
        
        fprintf(outf, ";");
        
        if (string_pointers[i].en) {
            fseek(rdatf, string_pointers[i].en - hdr_data.vaddr, SEEK_SET);
            fread(buf16, sizeof(char), BUF16_LEN, rdatf);
            
            convert_string(outf);
        }
        
        fprintf(outf, "\n");
    }
    
    fclose(rdatf);
    fclose(outf);
}
