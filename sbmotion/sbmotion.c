#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#define CGLTF_VALIDATE_ENABLE_ASSERTS 1
#include "cgltf_write.h"

#ifdef __linux__
#define SEPARATOR '/'
#include <unistd.h>
#else
#define SEPARATOR '\\'
#include <io.h>
#define F_OK 0
#define access _access
#endif

const float fps = 20.0f;
const int animation_stride = 8 * sizeof(float);
const float scale_factor = 141.0f;

struct motion {
    uint32_t offset;
    uint32_t max_frame;
};

char * progname;

char path[256];

void euler2quat(float quat[4], float euler[3]) {
    // Conver from Euler to Quaternion
    double cx = cos(euler[0] * 0.5);
    double sx = sin(euler[0] * 0.5);
    double cy = cos(euler[1] * 0.5);
    double sy = sin(euler[1] * 0.5);
    double cz = cos(euler[2] * 0.5);
    double sz = sin(euler[2] * 0.5);
    
    quat[0] = sx * cy * cz - cx * sy * sz; // X
    quat[1] = cx * sy * cz + sx * cy * sz; // Y
    quat[2] = cx * cy * sz - sx * sy * cz; // Z
    quat[3] = cx * cy * cz + sx * sy * sz; // W
}

void realloc_cgltf(cgltf_data * data, int anim_count, int bone_count) {
    // We're going to need a ton of accessors for this: anim_count * bone_count * 3 (Time, Position, Rotation)
    cgltf_accessor * accessors = calloc(data->accessors_count + anim_count * bone_count * 3, sizeof(cgltf_accessor));
    cgltf_buffer_view * buffer_views = calloc(data->buffer_views_count + 1, sizeof(cgltf_buffer_view));

    memcpy(accessors, data->accessors, data->accessors_count * sizeof(cgltf_accessor));
    memcpy(buffer_views, data->buffer_views, data->buffer_views_count * sizeof(cgltf_buffer_view));
    
    for (long i = 0; i < data->accessors_count; i++) {
        long idx = data->accessors[i].buffer_view - data->buffer_views;
        accessors[i].buffer_view = buffer_views + idx;
    }
    
    for (long i = 0; i < data->meshes_count; i++) {
        for (long j = 0; j < data->meshes[i].primitives_count; j++) {
            long idx = data->meshes[i].primitives[j].indices - data->accessors;
            data->meshes[i].primitives[j].indices = accessors + idx;
//            printf("IDX: %ld\n", idx);
            
            data->meshes[i].primitives[j].type--; // Fix this stupid bug
            
            for (long k = 0; k < data->meshes[i].primitives[j].attributes_count; k++) {
                idx = data->meshes[i].primitives[j].attributes[k].data - data->accessors;
                data->meshes[i].primitives[j].attributes[k].index = idx;
                data->meshes[i].primitives[j].attributes[k].data = accessors + idx;
//                printf("IDX: %ld\n", idx);
            }
        }
    }

    free(data->accessors);    
    free(data->buffer_views);
    
    data->accessors = accessors;
    data->buffer_views = buffer_views;
}

int main(int argc, char ** argv) {
    progname = *argv++; argc--;

    printf("SB Motion Tool - By QuantX\n");

    if (argc < 2) {
        fprintf(stderr, "Please specify a LMT motion file and a glTF model file: %s <path/example.lmt> <path/example.gltf> --mirror\n", progname);
        return 1;
    }
    
    char * path_lmt = *argv++; argc--;
    char * path_gltf = *argv++; argc--;
    
    bool mirror = false;
    if (argc && !strcmp(*argv, "--mirror")) {
        *argv++; argc--;
        mirror = true;
    }
    
    FILE * lmt = fopen(path_lmt, "rb");
    if (!lmt) {
        fprintf(stderr, "Failed to open LMT file: %s\n", path_lmt);
        return 1;
    }
    
    uint8_t bone_count;
    fread(&bone_count, sizeof(uint8_t), 1, lmt);
    if (!bone_count) {
        fprintf(stderr, "Bone count was 0 in LMT file\n");
        fclose(lmt);
        return 0;
    }
    
    uint8_t motion_count;
    fread(&motion_count, sizeof(uint8_t), 1, lmt);
    if (!motion_count) {
        fprintf(stderr, "Motion count was 0 in LMT file\n");
        fclose(lmt);
        return 0;
    }
    
    int anim_count = motion_count;
    if (mirror) anim_count *= 2;
    
    printf("Processing %d motions, %d bones\n", motion_count, bone_count);
    
    uint16_t unknown;
    fread(&unknown, sizeof(uint16_t), 1, lmt);
    if (unknown) printf("Unknown was %04X\n", unknown);
    
    cgltf_options options = {0};
    cgltf_data * data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path_gltf, &data);
    if (result != cgltf_result_success) {
        fclose(lmt);
        fprintf(stderr, "Failed to open glTF file: %s\n", path_gltf);
        return 1;
    }
    
    if (!data->skins_count) {
        fprintf(stderr, "No skins defined in glTF file\n");
        fclose(lmt);
        cgltf_free(data);
        return 1;
    }
    
    if (data->animations_count) {
        fprintf(stderr, "Animation data already present in glTF file\n");
        fclose(lmt);
        cgltf_free(data);
        return 1;
    }
    
    cgltf_skin * skin = data->skins;
    if (bone_count != skin->joints_count) {
        fprintf(stderr, "Bone count %d does not match count %ld defined in glTF file\n", bone_count, skin->joints_count);
        fclose(lmt);
        cgltf_free(data);
        return 1;
    }
    
    if (!data->buffers_count) {
        fprintf(stderr, "No model glbin file defined inside glTF file\n");
        fclose(lmt);
        cgltf_free(data);
        return 1;
    }
    
    realloc_cgltf(data, anim_count, bone_count);

    cgltf_buffer * buf = data->buffers;
    cgltf_buffer_view * buf_view = data->buffer_views + data->buffer_views_count;
    buf_view->name = strdup("Keyframes");
    buf_view->buffer = buf;
    buf_view->offset = buf->size;
    buf_view->stride = animation_stride;
    buf_view->type = cgltf_buffer_view_type_vertices;
    
    data->buffer_views_count++;
    
    strncpy(path, path_gltf, sizeof(path));
    char * sep = strrchr(path, SEPARATOR);
    if (sep) strcpy(sep + 1, data->buffers->uri);
    else strncpy(path, data->buffers->uri, sizeof(path));
    
    if (access(path, F_OK)) {
        fprintf(stderr, "Could not locate %s\n", path);
        fclose(lmt);
        cgltf_free(data);
        return 1;
    }
    
    FILE * glbin = fopen(path, "ab");
    if (!glbin) {
        fprintf(stderr, "Failed to open %s\n", path);
        fclose(lmt);
        cgltf_free(data);
        return 1;
    }

	data->animations_count = anim_count;
    data->animations = calloc(data->animations_count, sizeof(cgltf_animation));
    
    struct motion * motions = malloc(motion_count * sizeof(struct motion));
    fread(motions, sizeof(struct motion), motion_count, lmt);
    
    uint32_t * bones = malloc(bone_count * sizeof(uint32_t));
    
    uint32_t output_offset = 0;
    
    int current_anim = 0;
    for (int mi = 0; mi < motion_count; mi++) {
        if (ftell(lmt) != motions[mi].offset) {
            fprintf(stderr, "Wrong motion offset %08lX expected %08X\n", ftell(lmt), motions[mi].offset);
            return 1;
        }

        for (int mc = 0; mc < 2; mc++) {
            fseek(lmt, motions[mi].offset, SEEK_SET);
            
            cgltf_animation * anim = data->animations + current_anim++;
            
            char name[32];
            snprintf(name, sizeof(name), "Anim_%d", mi);
            if (mc) strcat(name, "M");
            anim->name = strdup(name);

            // Read bone offsets
            fread(bones, sizeof(uint32_t), bone_count, lmt);
            // REMEMBER: If the offset (bones[...] == 0) then we ignore that bone
            printf("Motion %d%s, length %d frames, bone offsets:", mi, mc ? " (Mirror)" : "", motions[mi].max_frame);
            for (int bi = 0; bi < bone_count; bi++) {
                if (bones[bi]) {
                    anim->channels_count += 2;
                    anim->samplers_count += 2;
                
                    printf(" %08X", bones[bi] + motions[mi].offset);
                } else printf(" XXXXXXXX");
            }
            printf("\n");
            
            anim->channels = calloc(anim->channels_count, sizeof(cgltf_animation_channel));
            anim->samplers = calloc(anim->samplers_count, sizeof(cgltf_animation_sampler));
            
            unsigned int channel = 0;
            
            for (int bi = 0; bi < bone_count; bi++) {
                if (!bones[bi]) continue;
                
                uint16_t bone_id;
                fread(&bone_id, sizeof(uint16_t), 1, lmt);
                
                uint16_t frame_count;
                fread(&frame_count, sizeof(uint16_t), 1, lmt);
                
                cgltf_node * bone = skin->joints[bone_id];
                
                if (mc) {
                    // Find mirror bone
                    char * bname = strchr(bone->name, ':');
                    if (bname) bname++;
                    else bname = bone->name;
                    
                    char mname[16];
                    strncpy(mname, bname, sizeof(mname));
                    
                    char * mep = strchr(mname, '_');
                    if (mep) {
                        bool valid_mbone = false;
                        if (mep[1] == 'a') {
                            mep[1] = 'b';
                            valid_mbone = true;
                        } else if (mep[1] == 'b') {
                            mep[1] = 'a';
                            valid_mbone = true;
                        }
                        
                        if (valid_mbone) {
                            int mbi;
                            for (mbi = 0; mbi < skin->joints_count; mbi++) {
                                cgltf_node * mbone = skin->joints[mbi];
                                
                                char * mbone_name = strchr(mbone->name, ':');
                                if (mbone_name) mbone_name++;
                                else mbone_name = mbone->name;
                                
                                if (!strncmp(mbone_name, mname, sizeof(mname))) {
                                    mbone = mbone;
                                    break;
                                }
                            }
                            if (mbi == skin->joints_count) {
                                fprintf(stderr, "Failed to find mirror '%s' of bone: '%s'\n", mname, bone->name);
                                return 1; // This happens in 0028.lmt, 0176.gltf
                            }
                        }
                    }
                }
                
                printf("Reading %d frames for bone: %s\n", frame_count, bone->name);
                
                cgltf_accessor * accs = data->accessors + data->accessors_count;

                snprintf(name, sizeof(name), "Time_%d_%d", mi, bone_id);
                if (mc) strcat(name, "M");
                accs[0].name = strdup(name);
                accs[0].component_type = cgltf_component_type_r_32f;
                accs[0].type = cgltf_type_scalar;
                accs[0].buffer_view = buf_view;
                
                accs[0].offset = output_offset;
                accs[0].stride = animation_stride;
                
                accs[0].min[0] = 0.0f;
                accs[0].has_min = true;
                
                accs[0].max[0] = (float)(motions[mi + 1].max_frame) / fps;
                accs[0].has_max = true;
                
                snprintf(name, sizeof(name), "Translation_%d_%d", mi, bone_id);
                if (mc) strcat(name, "M");
                accs[1].name = strdup(name);
                accs[1].component_type = cgltf_component_type_r_32f;
                accs[1].type = cgltf_type_vec3;
                accs[1].buffer_view = buf_view;
                
                accs[1].offset = output_offset + sizeof(float);
                accs[1].stride = animation_stride;
                
                snprintf(name, sizeof(name), "Rotation_%d_%d", mi, bone_id);
                if (mc) strcat(name, "M");
                accs[2].name = strdup(name);
                accs[2].component_type = cgltf_component_type_r_32f;
                accs[2].type = cgltf_type_vec4;
                accs[2].buffer_view = buf_view;
                
                accs[2].offset = output_offset + (4 * sizeof(float));
                accs[2].stride = animation_stride;
                
                data->accessors_count += 3;


                cgltf_animation_sampler * samp = anim->samplers + channel;
                
                samp[0].input = accs;
                samp[0].output = accs + 1;
                samp[0].interpolation = cgltf_interpolation_type_linear;
                
                samp[1].input = accs;
                samp[1].output = accs + 2;
                samp[1].interpolation = cgltf_interpolation_type_linear;

                cgltf_animation_channel * chan = anim->channels + channel;
                
                chan[0].sampler = samp;
                chan[0].target_node = bone;
                chan[0].target_path = cgltf_animation_path_type_translation;
                
                chan[1].sampler = samp + 1;
                chan[1].target_node = bone;
                chan[1].target_path = cgltf_animation_path_type_rotation;
                
                channel += 2;
                
                uint32_t output_count = 0;
                for (int fi = 0; fi < frame_count; fi++) {
                    uint16_t frame_index;
                    fread(&frame_index, sizeof(uint16_t), 1, lmt);
                    
                    if (frame_index != fi) {
                        fprintf(stderr, "Frame index %d does not match actual index %d\n", frame_index, fi);
                        return 1;
                    }

                    uint16_t frame_time;
                    fread(&frame_time, sizeof(uint16_t), 1, lmt);
                    
                    float dir[3];
                    fread(dir, sizeof(float), 3, lmt);
                    
                    float pos[3];
                    fread(pos, sizeof(float), 3, lmt);
                    
                    pos[0] /= scale_factor;
                    pos[1] /= scale_factor;
                    pos[2] /= scale_factor;

                    float time = (float)(frame_time) / fps;
                    
                    if (mc) {
                        pos[0] = -pos[0];
                        
                        dir[1] = -dir[1];
                        dir[2] = -dir[2];
                    }
                    
    /*
                    printf("Bone %d, Frame %d, Time %f, Pos (%f %f %f), Dir (%f %f %f)\n",
                        bone_id, frame_time, time,
                        pos[0], pos[1], pos[2],
                        dir[0], dir[1], dir[2]);
    */
                    float quat[4];
                    euler2quat(quat, dir);
                    
                    fwrite(&time, sizeof(float), 1, glbin);
                    fwrite(pos,   sizeof(float), 3, glbin);
                    fwrite(quat,  sizeof(float), 4, glbin);
                    
                    output_offset += animation_stride;
                    output_count++;
                }
                
                for (int i = 0; i < 3; i++) {
                    accs[i].count = output_count;
                }
            }
            
            if (!mirror) break;
        }
    }
    
    buf_view->size = output_offset;
    buf->size += output_offset;
    
    fclose(lmt);
    fclose(glbin);
    
    // Remove trailing mirror IDs from specials
    for (int i = 0; i < skin->joints_count; i++) {
        char * sep = strchr(skin->joints[i]->name, ':');
        if (sep) *sep = '\0';
    }
    
    result = cgltf_validate(data);
    if (result != cgltf_result_success) {
	    fprintf(stderr, "Failed to validate glTF data\n");
        cgltf_free(data);
        return 1;
    }
    
    result = cgltf_write_file(&options, path_gltf, data);
    if (result != cgltf_result_success) {
	    fprintf(stderr, "Failed to write glTF file\n");
        cgltf_free(data);
        return 1;
    }

    cgltf_free(data);
    return 0;
}
