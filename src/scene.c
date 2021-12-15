#include "private_api.h"

typedef struct scene_vs_uniforms_t {
    mat4 mat_vp;
    mat4 light_mat_vp;
} scene_vs_uniforms_t;

typedef struct scene_fs_uniforms_t {
    vec3 light_ambient;
    vec3 light_direction;
    vec3 light_color;
    vec3 eye_pos;
    float shadow_map_size;
} scene_fs_uniforms_t;

sg_pipeline init_scene_pipeline(void) {
    ecs_trace("sokol: initialize scene pipieline");

    /* create an instancing shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks = {
            [0] = {
                .size = sizeof(scene_vs_uniforms_t),
                .uniforms = {
                    [0] = { .name="u_mat_vp", .type=SG_UNIFORMTYPE_MAT4 },
                    [1] = { .name="u_light_vp", .type=SG_UNIFORMTYPE_MAT4 }
                },
            }
        },
        .fs = {
            .images = {
                [0] = {
                    .name = "shadow_map",
                    .image_type = SG_IMAGETYPE_2D
                }
            },
            .uniform_blocks = {
                [0] = {
                    .size = sizeof(scene_fs_uniforms_t),
                    .uniforms = {
                        [0] = { .name="u_light_ambient", .type=SG_UNIFORMTYPE_FLOAT3 },
                        [1] = { .name="u_light_direction", .type=SG_UNIFORMTYPE_FLOAT3 },
                        [2] = { .name="u_light_color", .type=SG_UNIFORMTYPE_FLOAT3 },
                        [3] = { .name="u_eye_pos", .type=SG_UNIFORMTYPE_FLOAT3 },
                        [4] = { .name="u_shadow_map_size", .type=SG_UNIFORMTYPE_FLOAT }
                    }
                }
            }
        },

        .vs.source =
            SOKOL_SHADER_HEADER
            "uniform mat4 u_mat_vp;\n"
            "uniform mat4 u_light_vp;\n"
            "layout(location=0) in vec4 v_position;\n"
            "layout(location=1) in vec3 v_normal;\n"
            "layout(location=2) in vec4 i_color;\n"
            "layout(location=3) in vec3 i_material;\n"
            "layout(location=4) in mat4 i_mat_m;\n"
            "out vec4 position;\n"
            "out vec4 light_position;\n"
            "out vec3 normal;\n"
            "out vec4 color;\n"
            "out vec3 material;\n"
            "flat out uint material_id;\n"
            "void main() {\n"
            "  gl_Position = u_mat_vp * i_mat_m * v_position;\n"
            "  light_position = u_light_vp * i_mat_m * v_position;\n"
            "  position = (i_mat_m * v_position);\n"
            "  normal = (i_mat_m * vec4(v_normal, 0.0)).xyz;\n"
            "  color = i_color;\n"
            "  material = i_material;\n"
            "}\n",

        .fs.source =
            SOKOL_SHADER_HEADER
            "uniform vec3 u_light_ambient;\n"
            "uniform vec3 u_light_direction;\n"
            "uniform vec3 u_light_color;\n"
            "uniform vec3 u_eye_pos;\n"
            "uniform float u_shadow_map_size;\n"
            "uniform sampler2D shadow_map;\n"
            "in vec4 position;\n"
            "in vec4 light_position;\n"
            "in vec3 normal;\n"
            "in vec4 color;\n"
            "in vec3 material;\n"
            "out vec4 frag_color;\n"

            "const int pcf_count = 2;\n"
            "const int pcf_samples = (2 * pcf_count + 1) * (2 * pcf_count + 1);\n"
            "const float texel_c = 1.0;\n"

            "float decodeDepth(vec4 rgba) {\n"
            "    return dot(rgba, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/160581375.0));\n"
            "}\n"

            "float sampleShadow(sampler2D shadowMap, vec2 uv, float compare) {\n"
            "    float depth = decodeDepth(texture(shadowMap, vec2(uv.x, uv.y)));\n"
            "    depth += 0.00001;\n"
            "    return step(compare, depth);\n"
            "}\n"

            "float sampleShadowPCF(sampler2D shadowMap, vec2 uv, float texel_size, float compare) {\n"
            "    float result = 0.0;\n"
            "    for (int x = -pcf_count; x <= pcf_count; x++) {\n"
            "        for (int y = -pcf_count; y <= pcf_count; y++) {\n"
            "            result += sampleShadow(shadowMap, uv + vec2(x, y) * texel_size * texel_c, compare);\n"
            "        }\n"
            "    }\n"
            "    return result / float(pcf_samples);\n"
            "}\n"

            "void main() {\n"
            "  float specular_power = material.x;\n"
            "  float shininess = max(material.y, 1.0);\n"
            "  float emissive = material.z;\n"
            "  vec4 ambient = vec4(u_light_ambient, 0);\n"
            "  vec3 l = normalize(u_light_direction);\n"
            "  vec3 n = normalize(normal);\n"
            "  float n_dot_l = dot(n, l);\n"

            "  if (n_dot_l >= 0.0) {"
            "    vec3 v = normalize(u_eye_pos - position.xyz);\n"
            "    vec3 r = reflect(-l, n);\n"

            "    vec3 light_pos = light_position.xyz / light_position.w;\n"
            "    vec2 sm_uv = (light_pos.xy + 1.0) * 0.5;\n"
            "    float depth = light_position.z;\n"
            "    float texel_size = 1.0 / u_shadow_map_size;\n"
            "    float s = sampleShadowPCF(shadow_map, sm_uv, texel_size, depth);\n"

            "    float r_dot_v = max(dot(r, v), 0.0);\n"
            "    float l_shiny = pow(r_dot_v * n_dot_l, shininess);\n"
            "    vec4 l_specular = vec4(specular_power * l_shiny * u_light_color, 0);\n"
            "    vec4 l_diffuse = vec4(u_light_color, 0) * n_dot_l;\n"
            "    float l_emissive = emissive + clamp(1.0 - emissive, 0.0, 1.0);\n"
            "    vec4 l_light = l_emissive * (ambient + s * l_diffuse);\n"

            "    frag_color = l_light * color + s * l_specular;\n"
            "  } else {\n"
            "    vec4 light = emissive + clamp(1.0 - emissive, 0.0, 1.0) * (ambient);\n"
            "    frag_color = light * color;\n"
            "  }\n"
            "}\n"
    });

    return sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .buffers = {
                [2] = { .stride = 16, .step_func=SG_VERTEXSTEP_PER_INSTANCE },
                [3] = { .stride = 12, .step_func=SG_VERTEXSTEP_PER_INSTANCE },
                [4] = { .stride = 64, .step_func=SG_VERTEXSTEP_PER_INSTANCE }
            },

            .attrs = {
                /* Static geometry */
                [0] = { .buffer_index=0, .offset=0,  .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .buffer_index=1, .offset=0,  .format=SG_VERTEXFORMAT_FLOAT3 },

                /* Color buffer (per instance) */
                [2] = { .buffer_index=2, .offset=0, .format=SG_VERTEXFORMAT_FLOAT4 },

                /* Material buffer (per instance) */
                [3] = { .buffer_index=3, .offset=0, .format=SG_VERTEXFORMAT_FLOAT3 },                

                /* Matrix (per instance) */
                [4] = { .buffer_index=4, .offset=0,  .format=SG_VERTEXFORMAT_FLOAT4 },
                [5] = { .buffer_index=4, .offset=16, .format=SG_VERTEXFORMAT_FLOAT4 },
                [6] = { .buffer_index=4, .offset=32, .format=SG_VERTEXFORMAT_FLOAT4 },
                [7] = { .buffer_index=4, .offset=48, .format=SG_VERTEXFORMAT_FLOAT4 }
            }
        },

        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = false
        },

        .colors = {{
            .pixel_format = SG_PIXELFORMAT_RGBA16F
        }},

        .cull_mode = SG_CULLMODE_BACK
    });
}

sokol_offscreen_pass_t sokol_init_scene_pass(
    ecs_rgb_t background_color,
    sg_image depth_target,
    int32_t w, 
    int32_t h) 
{
    sg_image color_target = sokol_target_rgba16f(w, h);

    return (sokol_offscreen_pass_t){
        .pass_action = sokol_clear_action(background_color, true, false),
        .pass = sg_make_pass(&(sg_pass_desc){
            .color_attachments[0].image = color_target,
            .depth_stencil_attachment.image = depth_target
        }),
        .pip = init_scene_pipeline(),
        .color_target = color_target,
        .depth_target = depth_target
    };   
}

static
void scene_draw_instances(
    SokolGeometry *geometry,
    sokol_instances_t *instances,
    sg_image shadow_map)
{
    if (!instances->instance_count) {
        return;
    }

    sg_bindings bind = {
        .vertex_buffers = {
            [0] = geometry->vertex_buffer,
            [1] = geometry->normal_buffer,
            [2] = instances->color_buffer,
            [3] = instances->material_buffer,
            [4] = instances->transform_buffer
        },
        .index_buffer = geometry->index_buffer,
        .fs_images[0] = shadow_map
    };

    sg_apply_bindings(&bind);
    sg_draw(0, geometry->index_count, instances->instance_count);
}

void sokol_run_scene_pass(
    sokol_offscreen_pass_t *pass,
    sokol_render_state_t *state)
{
    scene_vs_uniforms_t vs_u;
    glm_mat4_copy(state->uniforms.mat_vp, vs_u.mat_vp);
    glm_mat4_copy(state->uniforms.light_mat_vp, vs_u.light_mat_vp);
    
    scene_fs_uniforms_t fs_u;
    glm_vec3_copy(state->uniforms.light_ambient, fs_u.light_ambient);
    glm_vec3_copy(state->uniforms.light_direction, fs_u.light_direction);
    glm_vec3_copy(state->uniforms.light_color, fs_u.light_color);
    glm_vec3_copy(state->uniforms.eye_pos, fs_u.eye_pos);
    fs_u.shadow_map_size = state->uniforms.shadow_map_size;

    /* Render to offscreen texture so screen-space effects can be applied */
    sg_begin_pass(pass->pass, &pass->pass_action);
    sg_apply_pipeline(pass->pip);

    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &(sg_range){&vs_u, sizeof(scene_vs_uniforms_t)});
    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &(sg_range){&fs_u, sizeof(scene_fs_uniforms_t)});

    /* Loop geometry, render scene */
    ecs_iter_t qit = ecs_query_iter(state->world, state->q_scene);
    while (ecs_query_next(&qit)) {
        SokolGeometry *geometry = ecs_term(&qit, SokolGeometry, 1);

        int b;
        for (b = 0; b < qit.count; b ++) {
            scene_draw_instances(&geometry[b], &geometry[b].solid, state->shadow_map);
            scene_draw_instances(&geometry[b], &geometry[b].emissive, state->shadow_map);
        }
    }
    sg_end_pass();
}
