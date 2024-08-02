#pragma once

#define DDS_HEADER_INIT {.size = 124, .flags = 0x1 | 0x2 | 0x4 | 0x1000, .format.size = 32, .format.flags = 0}

struct dds_pixelformat {
    uint32_t size;
    uint32_t flags;
    union {
        char codeStr[4];
        uint32_t codeInt;
    };
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
};

struct dds_header {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitch;
    uint32_t depth; // Depth of a 3D texture
    uint32_t levels; // Mipmap Count
    uint32_t reserved0[11];
    struct dds_pixelformat format;
    uint32_t caps[4];
    uint32_t reserved1;
};

#define DDS_DX10_DIMENSION_1D 2
#define DDS_DX10_DIMENSION_2D 3
#define DDS_DX10_DIMENSION_3D 4

struct dds_header_dx10 {
    uint32_t format;
    uint32_t dimensions;
    uint32_t misc1;
    uint32_t arraySize;
    uint32_t misc2;
};

struct dxt1_block {
    uint16_t color0;
    uint16_t color1;
    uint32_t codes;
};

struct dxt3_block {
    uint64_t alpha;
    struct dxt1_block dxt1;
};
