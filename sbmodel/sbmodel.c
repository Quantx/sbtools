#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define MODEL_HEADER_OFFSET 0x50
#define MODEL_HEADER_SIZE 16

#define FORMAT_XBO 0x152
#define FORMAT_SHA 0x12

struct buffer {
    uint32_t offset; // Offset in bytes
    uint32_t size;   // Size in bytes
};

char * progname;

void rotate(float pos[3], float dir[3]) {
    float a, b;
    
    // X-axis rotation
    a = pos[1]; b = pos[2];
    pos[1] = a * cos(dir[0]) - b * sin(dir[0]);
    pos[2] = a * sin(dir[0]) + b * cos(dir[0]);
    
    // Y-axis rotation
    a = pos[0]; b = pos[2];
    pos[0] = a *  cos(dir[1]) + b * sin(dir[1]);
    pos[2] = a * -sin(dir[1]) + b * cos(dir[1]);
    
    // Z-axis rotation
    a = pos[0]; b = pos[1];
    pos[0] = a * cos(dir[2]) - b * sin(dir[2]);
    pos[1] = a * sin(dir[2]) + b * cos(dir[2]);
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Model Tool - By QuantX\n");

    if (!argc) {
        fprintf(stderr, "Please specify a sb model file: %s <path/example.sbmodel>\n", progname);
        return 1;
    }
    
    char * path = *argv++; argc--;
    
    FILE * sbmdl = fopen(path, "rb");
    if (!sbmdl) {
        fprintf(stderr, "Failed to open sb model file: %s\n", path);
        return 1;
    }
    
    // Get position of index table
    fseek(sbmdl, 24, SEEK_SET);
    uint32_t index_offset, index_length;
    fread(&index_offset, sizeof(uint32_t), 1, sbmdl);
    fread(&index_length, sizeof(uint32_t), 1, sbmdl);
    
    // Read vertex header
    fseek(sbmdl, MODEL_HEADER_OFFSET, SEEK_SET);
    uint8_t attr_count[2];
    fread(attr_count, sizeof(uint8_t), 2, sbmdl);
    uint16_t mesh_count;
    fread(&mesh_count, sizeof(uint16_t), 1, sbmdl);
    
    mesh_count -= MODEL_HEADER_SIZE;
    if (mesh_count % 8) printf("mesh_count %% 4 == %d, mesh_count == %d\n", mesh_count % 8, mesh_count);
    mesh_count /= 8;
    
    attr_count[0]--;
    
    // Check the magic numbers
    uint32_t magic;
    fread(&magic, sizeof(uint32_t), 1, sbmdl);
    if (magic != 0) printf("Magic 0x53 was %08X\n", magic);
    fread(&magic, sizeof(uint32_t), 1, sbmdl);
    if (magic != 8) {
        fprintf(stderr, "Magic 0x58 was %X instead of 0x08, this is probably not an SB model\n", magic);
        fclose(sbmdl);
        return 1;
    }
    fread(&magic, sizeof(uint32_t), 1, sbmdl);
    if (magic != 0x18) {
        fprintf(stderr, "Magic 0x58 was %X instead of 0x18, this is probably not an SB model\n", magic);
        fclose(sbmdl);
        return 1;
    }
    
    printf("Processing %d meshes, %d attributes\n", mesh_count, attr_count[0]);

    // Read mesh properties
    struct buffer * meshes = malloc(mesh_count * sizeof(struct buffer));
    fread(meshes, sizeof(struct buffer), mesh_count, sbmdl);
    
    //printf("Mesh vertex offset last %08X %08X\n", meshes[mesh_count - 1].offset, meshes[mesh_count - 1].size);
    
    // Read meshe attributes
    fseek(sbmdl, 1, SEEK_CUR); // Skip first attr as it's always 0xFF
    uint8_t * model_attribs = malloc(attr_count[0] * sizeof(uint8_t));
    fread(model_attribs, sizeof(uint8_t), attr_count[0], sbmdl);
    
    char * ext = strrchr(path, '.') + 1;
    
    if (strlen(ext) < 3) {
        fprintf(stderr, "Failed to generate output extension, path to short\n");
        fclose(sbmdl);
        return 1;
    }
    
    *ext++ = 'o';
    *ext++ = 'b';
    *ext++ = 'j';
    *ext = '\0';
    
    FILE * outf = fopen(path, "w");
    if (!outf) {
        fprintf(stderr, "Failed to open output file: %s\n", path);
        fclose(sbmdl);
        return 1;
    };
    
    uint32_t * base_verts = malloc(mesh_count * sizeof(uint32_t));
    *base_verts = 0;
    uint32_t vert_count = 0;
    
    // Mesh data
    for (int mi = 0; mi < mesh_count; mi++) {
        fseek(sbmdl, MODEL_HEADER_OFFSET + meshes[mi].offset, SEEK_SET);

        float pos[3];
        fread(pos, sizeof(float), 3, sbmdl);
        float dir[3];
        fread(dir, sizeof(float), 3, sbmdl);
        
        printf("Mesh %d POS(%f, %f, %f), DIR(%f, %f, %f)\n", mi, pos[0], pos[1], pos[2], dir[0], dir[1], dir[2]);
    
        // Validate mesh format
        int32_t format;
        fread(&format, sizeof(uint32_t), 1, sbmdl);
        if (format != FORMAT_XBO && format != FORMAT_SHA) {
            fprintf(stderr, "Mesh %d, unknown format %08X\n", mi, magic);
            return 1;
        }
        
        // Validate mesh size
        int32_t mesh_size;
        fread(&mesh_size, sizeof(uint32_t), 1, sbmdl);
        
        if (mesh_size != meshes[mi].size - 48) printf("mesh_size mismatch, %d %d\n", mesh_size, meshes[mi].size - 48);
        
        // Valide mesh magic
        fread(&magic, sizeof(uint32_t), 1, sbmdl);
        if (magic != 6) printf("Mesh %d magic2 was %08X instead of 0x6\n", mi, magic);
        
        // Read mesh attributes
        int16_t attrs[4];
        fread(attrs, sizeof(int16_t), 4, sbmdl);
        printf("Mesh attributes: %d, %d, %d, %d\n", attrs[0], attrs[1], attrs[2], attrs[3]);
        
        // Read vertex length
        uint32_t vert_size;
        fread(&vert_size, sizeof(uint32_t), 1, sbmdl);
        
        // Vertex data
        int32_t verts = mesh_size;
        
        if (format == FORMAT_XBO && vert_size != 20) {
            printf("Format XBO does not have vert_size of 20, vert_size == %d\n", vert_size);
        } else if (format == FORMAT_SHA && vert_size != 12) {
            printf("Format SHA does not have vert_size of 12, vert_size == %d\n", vert_size);
        }
        
        if (verts % vert_size) {
            fprintf(stderr, "%d verts, %d vert_size, remainder %d\n", verts, vert_size, verts % vert_size);
        }
        verts /= vert_size;
        
        printf("Reading %d vertices\n", verts);
        
        if (mi < mesh_count - 1) base_verts[mi + 1] = base_verts[mi] + verts;
        vert_count += verts;
        
        for (int vi = 0; vi < verts; vi++) {
            int16_t vpi[3];
            fread(vpi, sizeof(int16_t), 3, sbmdl);
            
            float vp[3] = { vpi[0], vpi[1], vpi[2] };
            
            rotate(vp, dir);
            
            vp[0] += pos[0];
            vp[1] += pos[1];
            vp[2] += pos[2];
            
            vp[0] /= 256.0f;
            vp[1] /= 256.0f;
            vp[2] /= 256.0f;
            
            fprintf(outf, "v %f %f %f\n", vp[0], vp[1], vp[2]);

            // Read vertex normals
            int16_t vni[3];
            fread(vni, sizeof(int16_t), 3, sbmdl);
            
            float vn[3] = { vni[0], vni[1], vni[2] };
            rotate(vn, dir);
            
            vn[0] /= 32768.0f;
            vn[1] /= 32768.0f;
            vn[2] /= 32768.0f;
            
            fprintf(outf, "vn %f %f %f\n", vn[0], vn[1], vn[2]);

            if (format == FORMAT_XBO) {
                // Read vertex texture coords
                int16_t vti[4];
                fread(vti, sizeof(int16_t), 4, sbmdl);
                float vt[4] = { vti[0], vti[1], vti[2], vti[3] };

                vt[0] /= 32768.0f;
                vt[1] /= 32768.0f;
                vt[2] /= 32768.0f;
                vt[3] /= 32768.0f;
                
                fprintf(outf, "vt %f %f\n\n", vt[2], 1.0f - vt[3]);
            }
        }
    }   
    
    // Read Index Data Header    
    struct buffer * indexes = malloc(mesh_count * sizeof(struct buffer));

    fseek(sbmdl, index_offset, SEEK_SET);
    fread(indexes, sizeof(struct buffer), mesh_count, sbmdl);
    
    //printf("Index offset last %08X %08X\n", indexes[mesh_count - 1].offset, indexes[mesh_count - 1].size);
    
    for (int mi = 0; mi < mesh_count; mi++) {
        //if (mi != 1) continue;
        fseek(sbmdl, index_offset + indexes[mi].offset, SEEK_SET);
        
        int32_t inds = indexes[mi].size;
        
        if (inds % 2) {
            printf("inds %% 2 == %d, inds == %d\n", inds % 2, inds);
        }
        inds /= 2;
        
        printf("Reading %d indexes\n", inds);
        
        fprintf(outf, "g part%d\n", mi);
        
        uint16_t largest_index = 0;
        
        uint16_t face[3] = {0, 0, 0};
        for (int ci = 0; ci < inds; ci++) {
            face[2] = face[1];
            face[1] = face[0];
            fread(face, sizeof(uint16_t), 1, sbmdl);
            face[0] += base_verts[mi];
            
            if (face[0] > largest_index) largest_index = face[0];
            
            if (ci >= 2 && face[0] != face[1] && face[1] != face[2] && face[2] != face[1]) {
                int a = face[0] + 1;
                int b = face[1] + 1;
                int c = face[2] + 1;
            
                fprintf(outf, "f %d/%d/%d %d/%d/%d %d/%d/%d\n", a, a, a, b, b, b, c, c, c);
            }
        }
        
        if (largest_index >= vert_count) {
            printf("Largest index was out of bounds %d, verts: %d\n", largest_index, vert_count);
        }
    }

    fclose(outf);
    fclose(sbmdl);
    return 0;
}
