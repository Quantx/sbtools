#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#define CGLTF_VALIDATE_ENABLE_ASSERTS 1
#include "cgltf_write.h"

#define MODEL_HEADER_OFFSET 0x50
#define MODEL_HEADER_SIZE 16

#define FORMAT_XBO 0x152
#define FORMAT_SHA 0x12

const unsigned int verts_stride = 8 * sizeof(float) + 8 * sizeof(uint8_t);
const float scale_factor = 256.0f;

const cgltf_primitive_type prim_type = cgltf_primitive_type_triangle_strip;

struct buffer {
    uint32_t offset; // Offset in bytes
    uint32_t size;   // Size in bytes
};

struct primative {
    uint32_t offset;
    uint32_t size;
    uint32_t count;
    
    float min[3];
    float max[3];
    
    float pos[3];
    float dir[4];
};

char * progname;

/*
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
*/

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
    struct buffer index_section;
    fread(&index_section, sizeof(struct buffer), 1, sbmdl);
    
    // Read vertex header
    fseek(sbmdl, MODEL_HEADER_OFFSET, SEEK_SET);
    uint8_t node_count, mesh_attr;
    fread(&node_count, sizeof(uint8_t), 1, sbmdl);
    fread(&mesh_attr, sizeof(uint8_t), 1, sbmdl);
    
    uint8_t mesh_count = node_count - 1;
    
    fseek(sbmdl, 2, SEEK_CUR);
    
    // Check the magic numbers
    uint32_t magic;
    fread(&magic, sizeof(uint32_t), 1, sbmdl);
    if (magic != 0) printf("Magic 0x54 was %08X\n", magic);
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
    
    printf("Processing %d meshes, %d attributes\n", mesh_count, mesh_attr);

    // Read vertex properties
    struct buffer * verts_in = malloc(mesh_count * sizeof(struct buffer));
    fread(verts_in, sizeof(struct buffer), mesh_count, sbmdl);
    
    //printf("Mesh vertex offset last %08X %08X\n", meshes[mesh_count - 1].offset, meshes[mesh_count - 1].size);
    
    // Read nodes
    uint8_t * mesh_nodes = malloc(node_count * sizeof(uint8_t));
    fread(mesh_nodes, sizeof(uint8_t), node_count, sbmdl);
    
    char * ext = strrchr(path, '.') + 1;
    
    if (strlen(ext) < 3) {
        fprintf(stderr, "Failed to generate output extension, path to short\n");
        fclose(sbmdl);
        return 1;
    }
    
    // Replace file extension
    strcpy(ext, "glbin");
    
    char * glbin_path = strdup(path);
    
    FILE * outf = fopen(path, "w");
    if (!outf) {
        fprintf(stderr, "Failed to open output file: %s\n", path);
        fclose(sbmdl);
        return 1;
    };
    
    struct primative * verts_out = malloc(mesh_count * sizeof(struct primative));
    
    // Mesh data
    for (uint8_t mi = 0; mi < mesh_count; mi++) {
        fseek(sbmdl, MODEL_HEADER_OFFSET + verts_in[mi].offset, SEEK_SET);
        
        verts_out[mi].offset = mi ? verts_out[mi - 1].offset + verts_out[mi - 1].size : 0;

        float pos[3];
        fread(pos, sizeof(float), 3, sbmdl);
        float dir[3];
        fread(dir, sizeof(float), 3, sbmdl);
        
        printf("Mesh %d POS(%f, %f, %f), DIR(%f, %f, %f)\n", mi, pos[0], pos[1], pos[2], dir[0], dir[1], dir[2]);
        
        pos[0] /= scale_factor;
        pos[1] /= scale_factor;
        pos[2] /= scale_factor;
        
        verts_out[mi].pos[0] = pos[0];
        verts_out[mi].pos[1] = pos[1];
        verts_out[mi].pos[2] = pos[2];
        
        // Conver from Euler to Quaternion
        double cx = cos(dir[0] * 0.5);
        double sx = sin(dir[0] * 0.5);
        double cy = cos(dir[1] * 0.5);
        double sy = sin(dir[1] * 0.5);
        double cz = cos(dir[2] * 0.5);
        double sz = sin(dir[2] * 0.5);
        
        verts_out[mi].dir[0] = sx * cy * cz - cx * sy * sz; // X
        verts_out[mi].dir[1] = cx * sy * cz + sx * cy * sz; // Y
        verts_out[mi].dir[2] = cx * cy * sz - sx * sy * cz; // Z
        verts_out[mi].dir[3] = cx * cy * cz + sx * sy * sz; // W
        
        // Validate mesh format
        int32_t format;
        fread(&format, sizeof(uint32_t), 1, sbmdl);
        if (format != FORMAT_XBO) {// && format != FORMAT_SHA) {
            fprintf(stderr, "Mesh %d, unknown format %08X\n", mi, magic);
            return 1;
        }
        
        // Validate mesh size
        int32_t mesh_size;
        fread(&mesh_size, sizeof(uint32_t), 1, sbmdl);
        
        if (mesh_size != verts_in[mi].size - 48) printf("mesh_size mismatch, %d %d\n", mesh_size, verts_in[mi].size - 48);
        
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

        if (format == FORMAT_XBO && vert_size != 20) {
            printf("Format XBO does not have vert_size of 20, vert_size == %d\n", vert_size);
        } else if (format == FORMAT_SHA && vert_size != 12) {
            printf("Format SHA does not have vert_size of 12, vert_size == %d\n", vert_size);
        }
        
        int32_t verts = mesh_size;
        
        if (verts % vert_size) {
            fprintf(stderr, "%d verts, %d vert_size, remainder %d\n", verts, vert_size, verts % vert_size);
        }
        verts /= vert_size;

        verts_out[mi].count = verts;
        verts_out[mi].size = verts * verts_stride;
        
        printf("Reading %d vertices\n", verts);

        // Vertex data
        for (int vi = 0; vi < verts; vi++) {
            int16_t vpi[3];
            fread(vpi, sizeof(int16_t), 3, sbmdl);
            float vp[3] = { vpi[0], vpi[1], vpi[2] };
            
            vp[0] /= scale_factor;
            vp[1] /= scale_factor;
            vp[2] /= scale_factor;

            fwrite(vp, sizeof(float), 3, outf);

            if (!vi) {
                verts_out[mi].max[0] = verts_out[mi].min[0] = vp[0];
                verts_out[mi].max[1] = verts_out[mi].min[1] = vp[1];
                verts_out[mi].max[2] = verts_out[mi].min[2] = vp[2];
            } else {
                verts_out[mi].min[0] = fminf(verts_out[mi].min[0], vp[0]);
                verts_out[mi].min[1] = fminf(verts_out[mi].min[1], vp[1]);
                verts_out[mi].min[2] = fminf(verts_out[mi].min[2], vp[2]);
                
                verts_out[mi].max[0] = fmaxf(verts_out[mi].max[0], vp[0]);
                verts_out[mi].max[1] = fmaxf(verts_out[mi].max[1], vp[1]);
                verts_out[mi].max[2] = fmaxf(verts_out[mi].max[2], vp[2]);
            }

            // Read vertex normals
            int16_t vni[3];
            fread(vni, sizeof(int16_t), 3, sbmdl);
            float vn[3] = { vni[0], vni[1], vni[2] };
            
            // Prevent divide by zero error
            if (!vni[0] && !vni[1] && !vni[2]) vn[0] = 1.0f;
            
            // Normalize
            float vn_len = sqrt((vn[0] * vn[0]) + (vn[1] * vn[1]) + (vn[2] * vn[2]));
            
            vn[0] /= vn_len;
            vn[1] /= vn_len;
            vn[2] /= vn_len;
            
            fwrite(vn, sizeof(float), 3, outf);

            if (format == FORMAT_XBO) {
                // Read vertex texture coords
                int16_t vti[4];
                fread(vti, sizeof(int16_t), 4, sbmdl);
                float vt[4] = { vti[0], vti[1], vti[2], vti[3] };

                vt[0] /= 32768.0f;
                vt[1] /= 32768.0f;
                vt[2] /= 32768.0f;
                vt[3] /= 32768.0f;
                
                fwrite(vt + 2, sizeof(float), 2, outf);
            }
            
            uint8_t joints[4] = {mi};
            fwrite(joints, sizeof(uint8_t), 4, outf);
            
            uint8_t weights[4] = {0xFF};
            fwrite(weights, sizeof(uint8_t), 4, outf);

        }
    }
    
    fseek(sbmdl, index_section.offset, SEEK_SET);
    
    // Read Index Data Header    
    struct buffer * inds_in = malloc(mesh_count * sizeof(struct buffer));
    fread(inds_in, sizeof(struct buffer), mesh_count, sbmdl);
    
    struct primative * inds_out = malloc(mesh_count * sizeof(struct primative));
    
    //printf("Index offset last %08X %08X\n", indexes[mesh_count - 1].offset, indexes[mesh_count - 1].size);
    
    for (int mi = 0; mi < mesh_count; mi++) {
        fseek(sbmdl, index_section.offset + inds_in[mi].offset, SEEK_SET);
        int32_t inds = inds_in[mi].size;
        
        inds_out[mi].offset = mi ? inds_out[mi - 1].offset + inds_out[mi - 1].size : 0;

        if (inds % 2) {
            printf("inds %% 2 == %d, inds == %d\n", inds % 2, inds);
        }
        inds /= 2;
        
        printf("Reading %d indexes\n", inds);
        
        if (prim_type == cgltf_primitive_type_triangle_strip) {
            inds_out[mi].count = inds;
            for (int ci = 0; ci < inds; ci++) {
                uint16_t ind;
                fread(&ind,  sizeof(uint16_t), 1, sbmdl);
                fwrite(&ind, sizeof(uint16_t), 1, outf);
            }
        } else if (prim_type == cgltf_primitive_type_triangles) {
            inds_out[mi].count = 0;
            uint16_t face[3] = {0, 0, 0};
            for (int ci = 0; ci < inds; ci++) {
                face[2] = face[1];
                face[1] = face[0];
                fread(face, sizeof(uint16_t), 1, sbmdl);
                
                if (ci >= 2) {
                    if (inds == 3 || (face[0] != face[1] && face[1] != face[2] && face[2] != face[0])) {
                        fwrite(face, sizeof(uint16_t), 3, outf);
                        inds_out[mi].count += 3;
                    }
                }
            }
        } else {
            fprintf(stderr, "Unsupported primitive type\n");
            fclose(outf);
            fclose(sbmdl);
            return 1;
        }
        
        inds_out[mi].size = inds_out[mi].count * sizeof(uint16_t);
    }

    fclose(outf);
    fclose(sbmdl);


    ext[-1] = '\0';
    
    cgltf_options options = {0};
    cgltf_data data = {0};
    
    data.file_type = cgltf_file_type_gltf;
    
    data.asset.generator = strdup("sbmodel");
    data.asset.version = strdup("2.0");

    data.meshes_count = 1;
    cgltf_mesh * mesh = data.meshes = calloc(data.meshes_count, sizeof(cgltf_mesh));
    mesh->name = strdup(path);

    data.accessors_count = mesh_count * 6;
    data.accessors = calloc(data.accessors_count, sizeof(cgltf_accessor));
    
    data.buffers_count = 1;
    cgltf_buffer * buf = data.buffers = calloc(data.buffers_count, sizeof(cgltf_buffer));
    
    buf->name = strdup(path);
    buf->uri = glbin_path;

    data.buffer_views_count = 2;
    data.buffer_views = calloc(data.buffer_views_count, sizeof(cgltf_buffer_view));
    
    data.skins_count = 1;
    cgltf_skin * skin = data.skins = calloc(data.skins_count, sizeof(cgltf_skin));
    skin->joints_count = mesh_count;
    skin->joints = malloc(skin->joints_count * sizeof(cgltf_node *));
    
    data.nodes_count = mesh_count;
    data.nodes = calloc(data.nodes_count, sizeof(cgltf_node));
    
    for (int mi = 0; mi < mesh_count; mi++) {
        int ni = mi + 1;
        cgltf_node * node = data.nodes + mi;

        if (!mi) {
            node->name = strdup(path);
            node->mesh = mesh;
            node->skin = skin;
        }

        if (mesh_nodes[ni] == 0xFF) {
            fprintf(stderr, "Node %d is a second root node!\n", ni);
            return 1;
        }
        
        node->translation[0] = verts_out[mi].pos[0];
        node->translation[1] = verts_out[mi].pos[1];
        node->translation[2] = verts_out[mi].pos[2];
        node->has_translation = true;

        node->rotation[0] = verts_out[mi].dir[0];
        node->rotation[1] = verts_out[mi].dir[1];
        node->rotation[2] = verts_out[mi].dir[2];
        node->rotation[3] = verts_out[mi].dir[3];
        node->has_rotation = true;
            
        skin->joints[mi] = node;
        
        for (int nj = 0; nj < node_count; nj++) {
            if (mesh_nodes[nj] == ni) {
                if (nj == ni) {
                    fprintf(stderr, "Node %d is it's own child!\n", nj);
                    return 1;
                }
            
                node->children_count++;
            }
        }
        
        if (node->children_count) {
            node->children = malloc(node->children_count * sizeof(cgltf_node *));
            
            for (int cni = 0, nj = 0; nj < node_count; nj++) {
                if (mesh_nodes[nj] == ni) {
                    node->children[cni++] = data.nodes + (nj - 1);
                }
            }
        };
    }
    
    skin->skeleton = *skin->joints;
    
    cgltf_buffer_view * verts_view = data.buffer_views;
    cgltf_buffer_view * inds_view = data.buffer_views + 1;

    verts_view->name = strdup("Verticies");
    verts_view->buffer = buf;
    verts_view->type = cgltf_buffer_view_type_vertices;
    verts_view->stride = verts_stride;

    inds_view->name = strdup("Indicies");
    inds_view->buffer = buf;
    inds_view->type = cgltf_buffer_view_type_indices;

    for (int mi = 0; mi < mesh_count; mi++) {
        verts_view->size += verts_out[mi].size;
        inds_view->size += inds_out[mi].size;
    }

    inds_view->offset = verts_view->size;
    
    buf->size = verts_view->size + inds_view->size;
    
    mesh->primitives = calloc(mesh_count, sizeof(cgltf_primitive));
    mesh->primitives_count = mesh_count;
    
    for (int mi = 0; mi < mesh_count; mi++) {
        cgltf_primitive * prim = mesh->primitives + mi;
        
        char name[16];
        
        // There's a bug in this cgltf where we need to subtract 1
        prim->type = prim_type - 1;
        
        const int accessor_count = 6;
        const int accessor_pos = mi * accessor_count;

        prim->attributes_count = accessor_count - 1;
        prim->attributes = calloc(prim->attributes_count, sizeof(cgltf_attribute));
        
        for (int si = 0; si < prim->attributes_count; si++) {
            int sio = accessor_pos + si;
        
            data.accessors[sio].buffer_view = verts_view;

            data.accessors[sio].offset = verts_out[mi].offset;
            data.accessors[sio].stride = verts_view->stride;
            data.accessors[sio].count  = verts_out[mi].count;
        }
        
        cgltf_accessor * pos_acc = data.accessors + accessor_pos;
        
        printf("Mesh %d: Offset %ld, Size %d, Count %ld\n", mi, pos_acc->offset, verts_out[mi].size, pos_acc->count);
        
        snprintf(name, sizeof(name), "Position %d", mi);
        pos_acc->name = strdup(name);
        pos_acc->component_type = cgltf_component_type_r_32f;
        pos_acc->type = cgltf_type_vec3;
        
        pos_acc->min[0] = verts_out[mi].min[0];
        pos_acc->min[1] = verts_out[mi].min[1];
        pos_acc->min[2] = verts_out[mi].min[2];
        pos_acc->has_min = true;
        
        pos_acc->max[0] = verts_out[mi].max[0];
        pos_acc->max[1] = verts_out[mi].max[1];
        pos_acc->max[2] = verts_out[mi].max[2];
        pos_acc->has_max = true;
        
        cgltf_accessor * norm_acc = data.accessors + accessor_pos + 1;
        snprintf(name, sizeof(name), "Normal %d", mi);
        norm_acc->name = strdup(name);
        norm_acc->component_type = cgltf_component_type_r_32f;
        norm_acc->type = cgltf_type_vec3;
        norm_acc->offset += 3 * sizeof(float);
        
        cgltf_accessor * uv_acc = data.accessors + accessor_pos + 2;
        snprintf(name, sizeof(name), "Texture %d", mi);
        uv_acc->name = strdup(name);
        uv_acc->component_type = cgltf_component_type_r_32f;
        uv_acc->type = cgltf_type_vec2;
        uv_acc->offset += 6 * sizeof(float);
        
        cgltf_accessor * joint_acc = data.accessors + accessor_pos + 3;
        snprintf(name, sizeof(name), "Joint %d", mi);
        joint_acc->name = strdup(name);
        joint_acc->component_type = cgltf_component_type_r_8u;
        joint_acc->type = cgltf_type_vec4;
        joint_acc->offset += 8 * sizeof(float);
        
        cgltf_accessor * weight_acc = data.accessors + accessor_pos + 4;
        snprintf(name, sizeof(name), "Weight %d", mi);
        weight_acc->name = strdup(name);
        weight_acc->normalized = true;
        weight_acc->component_type = cgltf_component_type_r_8u;
        weight_acc->type = cgltf_type_vec4;
        weight_acc->offset += (8 * sizeof(float)) + (4 * sizeof(uint8_t));

        
        cgltf_accessor * ind_acc = data.accessors + accessor_pos + 5;
        snprintf(name, sizeof(name), "Indicies %d", mi);
        ind_acc->name = strdup(name);
        ind_acc->component_type = cgltf_component_type_r_16u;
        ind_acc->type = cgltf_type_scalar;
        
        ind_acc->offset = inds_out[mi].offset;
        ind_acc->stride = sizeof(uint16_t);
        ind_acc->count  = inds_out[mi].count;
        
        ind_acc->buffer_view = inds_view;
        
        prim->indices = ind_acc;
        
        cgltf_attribute * pos_atr = prim->attributes;
        pos_atr->name = strdup("POSITION");
        pos_atr->type = cgltf_attribute_type_position;
        pos_atr->index = accessor_pos;
        
        cgltf_attribute * norm_atr = prim->attributes + 1;
        norm_atr->name = strdup("NORMAL");
        norm_atr->type = cgltf_attribute_type_position;
        norm_atr->index = accessor_pos + 1;
        
        cgltf_attribute * uv_atr = prim->attributes + 2;
        uv_atr->name = strdup("TEXCOORD_0");
        uv_atr->type = cgltf_attribute_type_texcoord;
        uv_atr->index = accessor_pos + 2;
        
        cgltf_attribute * joint_atr = prim->attributes + 3;
        joint_atr->name = strdup("JOINTS_0");
        joint_atr->type = cgltf_attribute_type_joints;
        joint_atr->index = accessor_pos + 3;
        
        cgltf_attribute * weights_atr = prim->attributes + 4;
        weights_atr->name = strdup("WEIGHTS_0");
        weights_atr->type = cgltf_attribute_type_weights;
        weights_atr->index = accessor_pos + 4;
        
        for (int ai = 0; ai < prim->attributes_count; ai++) {
            prim->attributes[ai].data = data.accessors + prim->attributes[ai].index;
        }
    }
    
    cgltf_result result = cgltf_validate(&data);
    if (result != cgltf_result_success) {
	    fprintf(stderr, "Failed to validate glTF data\n");
        return 1;
    }
    
    ext[-1] = '.';
    strcpy(ext, "gltf");
    
    result = cgltf_write_file(&options, path, &data);
    if (result != cgltf_result_success) {
	    fprintf(stderr, "Failed to write glTF file\n");
        return 1;
    }

    return 0;
}
