//------------------------------------------------------------------------------
//  sokol_spine_test.c
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#define SOKOL_SPINE_IMPL
#include "spine/spine.h"
#include "sokol_spine.h"
#include "utest.h"

#define T(b) EXPECT_TRUE(b)

static void init() {
    sg_setup(&(sg_desc){0});
    sspine_setup(&(sspine_desc){0});
}

static void shutdown() {
    sspine_shutdown();
    sg_shutdown();
}

// NOTE: this guarantees that the data is zero terminated because the loaded data
// might either be binary or text (the zero sentinel is NOT counted in the returned size)
static sspine_range load_data(const char* path) {
    assert(path);
    FILE* fp = fopen(path, "rb");
    assert(fp);
    fseek(fp, 0, SEEK_END);
    const size_t size = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    // room for terminating zero
    const size_t alloc_size = size + 1;
    uint8_t* ptr = (uint8_t*)malloc(alloc_size);
    memset(ptr, 0, alloc_size);
    fread(ptr, size, 1, fp);
    fclose(fp);
    return (sspine_range) { .ptr = ptr, .size = size };
}

static void free_data(sspine_range r) {
    free((void*)r.ptr);
}

static sspine_atlas create_atlas(void) {
    sspine_range atlas_data = load_data("spineboy.atlas");
    sspine_atlas atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = atlas_data
    });
    free_data(atlas_data);
    return atlas;
}

static sspine_skeleton create_skeleton_json(sspine_atlas atlas) {
    sspine_range skeleton_json_data = load_data("spineboy-pro.json");
    sspine_skeleton skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = atlas,
        .json_data = (const char*)skeleton_json_data.ptr
    });
    free_data(skeleton_json_data);
    return skeleton;
}

static sspine_skeleton create_skeleton_binary(sspine_atlas atlas) {
    sspine_range skeleton_binary_data = load_data("spineboy-pro.skel");
    sspine_skeleton skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = atlas,
        .binary_data = skeleton_binary_data
    });
    free_data(skeleton_binary_data);
    return skeleton;
}

static sspine_skeleton create_skeleton(void) {
    return create_skeleton_json(create_atlas());
}

static sspine_instance create_instance(void) {
    return sspine_make_instance(&(sspine_instance_desc){
        .skeleton = create_skeleton(),
    });
}

UTEST(sokol_spine, default_init_shutdown) {
    // FIXME!
    T(true);
}

UTEST(sokol_spine, atlas_pool_exhausted) {
    sg_setup(&(sg_desc){0});
    sspine_setup(&(sspine_desc){
        .atlas_pool_size = 4
    });
    for (int i = 0; i < 4; i++) {
        sspine_atlas atlas = sspine_make_atlas(&(sspine_atlas_desc){0});
        T(sspine_get_atlas_resource_state(atlas) == SSPINE_RESOURCESTATE_FAILED);
    }
    sspine_atlas atlas = sspine_make_atlas(&(sspine_atlas_desc){0});
    T(SSPINE_INVALID_ID == atlas.id);
    T(sspine_get_atlas_resource_state(atlas) == SSPINE_RESOURCESTATE_INVALID);
    shutdown();
}

UTEST(sokol_spine, make_destroy_atlas_ok) {
    init();
    sspine_atlas atlas = create_atlas();
    T(sspine_get_atlas_resource_state(atlas) == SSPINE_RESOURCESTATE_VALID);
    T(sspine_atlas_valid(atlas));
    sspine_destroy_atlas(atlas);
    T(sspine_get_atlas_resource_state(atlas) == SSPINE_RESOURCESTATE_INVALID);
    T(!sspine_atlas_valid(atlas))
    shutdown();
}

UTEST(sokol_spine, make_atlas_fail_no_data) {
    init();
    sspine_atlas atlas = sspine_make_atlas(&(sspine_atlas_desc){0});
    T(atlas.id != SSPINE_INVALID_ID);
    T(sspine_get_atlas_resource_state(atlas) == SSPINE_RESOURCESTATE_FAILED);
    T(!sspine_atlas_valid(atlas));
    shutdown();
}

// an invalid atlas must return zero number of images
UTEST(sokol_spine, failed_atlas_no_images) {
    init();
    sspine_atlas atlas = sspine_make_atlas(&(sspine_atlas_desc){0});
    T(atlas.id != SSPINE_INVALID_ID);
    T(!sspine_atlas_valid(atlas));
    T(sspine_num_images(atlas) == 0);
    shutdown();

}

// NOTE: spine-c doesn't detect wrong/corrupt atlas file data, so we can't test for that

UTEST(sokol_spine, atlas_image_info) {
    init();
    sspine_atlas atlas = create_atlas();
    T(sspine_atlas_valid(atlas));
    T(sspine_num_images(atlas) == 1);
    const sspine_image_info img_info = sspine_get_image_info(atlas, 0);
    T(img_info.image.id != SG_INVALID_ID);
    T(sg_query_image_state(img_info.image) == SG_RESOURCESTATE_ALLOC);
    T(strcmp(img_info.filename, "spineboy.png") == 0);
    T(img_info.min_filter == SG_FILTER_LINEAR);
    T(img_info.mag_filter == SG_FILTER_LINEAR);
    T(img_info.wrap_u == SG_WRAP_MIRRORED_REPEAT);
    T(img_info.wrap_v == SG_WRAP_MIRRORED_REPEAT);
    T(img_info.width == 1024);
    T(img_info.height == 256);
    T(img_info.premul_alpha == false);
    shutdown();
}

UTEST(sokol_spine, atlas_with_overrides) {
    init();
    sspine_range atlas_data = load_data("spineboy.atlas");
    sspine_atlas atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = atlas_data,
        .override = {
            .min_filter = SG_FILTER_NEAREST_MIPMAP_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_REPEAT,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .premul_alpha_enabled = true,
        }
    });
    T(sspine_atlas_valid(atlas));
    T(sspine_num_images(atlas) == 1);
    const sspine_image_info img_info = sspine_get_image_info(atlas, 0);
    T(img_info.image.id != SG_INVALID_ID);
    T(sg_query_image_state(img_info.image) == SG_RESOURCESTATE_ALLOC);
    T(strcmp(img_info.filename, "spineboy.png") == 0);
    T(img_info.min_filter == SG_FILTER_NEAREST_MIPMAP_NEAREST);
    T(img_info.mag_filter == SG_FILTER_NEAREST);
    T(img_info.wrap_u == SG_WRAP_REPEAT);
    T(img_info.wrap_v == SG_WRAP_CLAMP_TO_EDGE);
    T(img_info.width == 1024);
    T(img_info.height == 256);
    T(img_info.premul_alpha == true);
    shutdown();
}

UTEST(sokol_spine, skeleton_pool_exhausted) {
    sg_setup(&(sg_desc){0});
    sspine_setup(&(sspine_desc){
        .skeleton_pool_size = 4
    });
    for (int i = 0; i < 4; i++) {
        sspine_skeleton skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){0});
        T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_FAILED);
    }
    sspine_skeleton skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){0});
    T(SSPINE_INVALID_ID == skeleton.id);
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_INVALID);
    shutdown();
}

UTEST(sokol_spine, make_destroy_skeleton_json_ok) {
    init();
    sspine_skeleton skeleton = create_skeleton_json(create_atlas());
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_VALID);
    T(sspine_skeleton_valid(skeleton));
    sspine_destroy_skeleton(skeleton);
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_INVALID);
    T(!sspine_skeleton_valid(skeleton));
    shutdown();
}

UTEST(sokol_spine, make_destroy_skeleton_binary_ok) {
    init();
    sspine_skeleton skeleton = create_skeleton_binary(create_atlas());
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_VALID);
    T(sspine_skeleton_valid(skeleton));
    sspine_destroy_skeleton(skeleton);
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_INVALID);
    T(!sspine_skeleton_valid(skeleton));
    shutdown();
}

UTEST(sokol_spine, make_skeleton_fail_no_data) {
    init();
    sspine_atlas atlas = create_atlas();
    sspine_skeleton skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = atlas
    });
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_FAILED);
    T(!sspine_skeleton_valid(skeleton));
    shutdown();
}

UTEST(sokol_spine, make_skeleton_fail_no_atlas) {
    init();
    sspine_range skeleton_json_data = load_data("spineboy-pro.json");
    sspine_skeleton skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .json_data = (const char*)skeleton_json_data.ptr
    });
    free_data(skeleton_json_data);
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_FAILED);
    T(!sspine_skeleton_valid(skeleton));
    shutdown();
}

UTEST(sokol_spine, make_skeleton_fail_with_failed_atlas) {
    init();
    sspine_atlas atlas = sspine_make_atlas(&(sspine_atlas_desc){0});
    T(sspine_get_atlas_resource_state(atlas) == SSPINE_RESOURCESTATE_FAILED);
    sspine_skeleton skeleton = create_skeleton_json(atlas);
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_FAILED);
    T(!sspine_skeleton_valid(skeleton));
    shutdown();
}

UTEST(sokol_spine, make_skeleton_json_fail_corrupt_data) {
    init();
    sspine_atlas atlas = create_atlas();
    const char* invalid_json_data = "This is not valid JSON!";
    sspine_skeleton skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = atlas,
        .json_data = (const char*)invalid_json_data,
    });
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_FAILED);
    sspine_destroy_skeleton(skeleton);
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_INVALID);
    shutdown();
}

// FIXME: this crashes the spine-c runtime
/*
UTEST(sokol_spine, make_skeleton_binary_fail_corrupt_data) {
    init();
    sspine_atlas atlas = create_atlas();
    uint8_t invalid_binary_data[] = { 0x23, 0x63, 0x11, 0xFF };
    sspine_skeleton skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = atlas,
        .binary_data = { .ptr = invalid_binary_data, .size = sizeof(invalid_binary_data) }
    });
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_FAILED);
    sspine_destroy_skeleton(skeleton);
    T(sspine_get_skeleton_resource_state(skeleton) == SSPINE_RESOURCESTATE_INVALID);
    shutdown();
}
*/

UTEST(sokol_spine, instance_pool_exhausted) {
    sg_setup(&(sg_desc){0});
    sspine_setup(&(sspine_desc){
        .instance_pool_size = 4
    });
    for (int i = 0; i < 4; i++) {
        sspine_instance instance = sspine_make_instance(&(sspine_instance_desc){0});
        T(sspine_get_instance_resource_state(instance) == SSPINE_RESOURCESTATE_FAILED);
    }
    sspine_instance instance = sspine_make_instance(&(sspine_instance_desc){0});
    T(SSPINE_INVALID_ID == instance.id);
    T(sspine_get_instance_resource_state(instance) == SSPINE_RESOURCESTATE_INVALID);
    shutdown();
}

UTEST(sokol_spine, make_destroy_instance_ok) {
    init();
    sspine_instance instance = sspine_make_instance(&(sspine_instance_desc){
        .skeleton = create_skeleton_json(create_atlas())
    });
    T(sspine_get_instance_resource_state(instance) == SSPINE_RESOURCESTATE_VALID);
    T(sspine_instance_valid(instance));
    sspine_destroy_instance(instance);
    T(sspine_get_instance_resource_state(instance) == SSPINE_RESOURCESTATE_INVALID);
    T(!sspine_instance_valid(instance));
    shutdown();
}

UTEST(sokol_spine, make_instance_fail_no_skeleton) {
    init();
    sspine_instance instance = sspine_make_instance(&(sspine_instance_desc){0});
    T(sspine_get_instance_resource_state(instance) == SSPINE_RESOURCESTATE_FAILED);
    sspine_destroy_instance(instance);
    T(sspine_get_instance_resource_state(instance) == SSPINE_RESOURCESTATE_INVALID);
    shutdown();
}

UTEST(sokol_spine, make_instance_fail_with_failed_skeleton) {
    init();
    sspine_skeleton failed_skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){0});
    T(sspine_get_skeleton_resource_state(failed_skeleton) == SSPINE_RESOURCESTATE_FAILED);
    sspine_instance instance = sspine_make_instance(&(sspine_instance_desc){
        .skeleton = failed_skeleton
    });
    T(sspine_get_instance_resource_state(instance) == SSPINE_RESOURCESTATE_FAILED);
    shutdown();
}

UTEST(sokol_spine, make_instance_fail_with_destroyed_atlas) {
    init();
    sspine_atlas atlas = create_atlas();
    T(sspine_atlas_valid(atlas));
    sspine_skeleton skeleton = create_skeleton_json(atlas);
    T(sspine_skeleton_valid(skeleton));
    sspine_destroy_atlas(atlas);
    T(!sspine_atlas_valid(atlas));
    sspine_instance instance = sspine_make_instance(&(sspine_instance_desc){
        .skeleton = skeleton
    });
    T(sspine_get_instance_resource_state(instance) == SSPINE_RESOURCESTATE_FAILED);
    shutdown();
}

UTEST(sokol_spine, get_skeleton_atlas) {
    init();
    sspine_atlas atlas = create_atlas();
    sspine_skeleton skeleton = create_skeleton_json(atlas);
    T(sspine_get_skeleton_atlas(skeleton).id == atlas.id);
    sspine_destroy_skeleton(skeleton);
    T(sspine_get_skeleton_atlas(skeleton).id == SSPINE_INVALID_ID);
    shutdown();
}

UTEST(sokol_spine, get_instance_skeleton) {
    init();
    sspine_atlas atlas = create_atlas();
    sspine_skeleton skeleton = create_skeleton_json(atlas);
    sspine_instance instance = sspine_make_instance(&(sspine_instance_desc){
        .skeleton = skeleton
    });
    T(sspine_get_instance_skeleton(instance).id == skeleton.id);
    sspine_destroy_instance(instance);
    T(sspine_get_instance_skeleton(instance).id == SSPINE_INVALID_ID);
    shutdown();
}

UTEST(sokol_spine, set_get_position) {
    init();
    sspine_instance instance = create_instance();
    sspine_set_position(instance, (sspine_vec2){ .x=1.0f, .y=2.0f });
    const sspine_vec2 pos = sspine_get_position(instance);
    T(pos.x == 1.0f);
    T(pos.y == 2.0f);
    shutdown();
}

UTEST(sokol_spine, set_get_position_destroyed_instance) {
    init();
    sspine_instance instance = create_instance();
    sspine_set_position(instance, (sspine_vec2){ .x=1.0f, .y=2.0f });
    sspine_destroy_instance(instance);
    const sspine_vec2 pos = sspine_get_position(instance);
    T(pos.x == 0.0f);
    T(pos.y == 0.0f);
    shutdown();
}

UTEST(sokol_spine, set_get_scale) {
    init();
    sspine_instance instance = create_instance();
    sspine_set_scale(instance, (sspine_vec2){ .x=2.0f, .y=3.0f });
    const sspine_vec2 scale = sspine_get_scale(instance);
    T(scale.x == 2.0f);
    T(scale.y == 3.0f);
    shutdown();
}

UTEST(sokol_spine, set_get_scale_destroyed_instance) {
    init();
    sspine_instance instance = create_instance();
    sspine_set_scale(instance, (sspine_vec2){ .x=2.0f, .y=3.0f });
    sspine_destroy_instance(instance);
    const sspine_vec2 scale = sspine_get_scale(instance);
    T(scale.x == 0.0f);
    T(scale.y == 0.0f);
    shutdown();
}

UTEST(sokol_spine, set_get_color) {
    init();
    sspine_instance instance = create_instance();
    sspine_set_color(instance, (sspine_color) { .r=1.0f, .g=2.0f, .b=3.0f, .a=4.0f });
    const sspine_color color = sspine_get_color(instance);
    T(color.r == 1.0f);
    T(color.g == 2.0f);
    T(color.b == 3.0f);
    T(color.a == 4.0f);
    shutdown();
}

UTEST(sokol_spine, set_get_color_destroyed_instance) {
    init();
    sspine_instance instance = create_instance();
    sspine_set_color(instance, (sspine_color) { .r=1.0f, .g=2.0f, .b=3.0f, .a=4.0f });
    sspine_destroy_instance(instance);
    const sspine_color color = sspine_get_color(instance);
    T(color.r == 0.0f);
    T(color.g == 0.0f);
    T(color.b == 0.0f);
    T(color.a == 0.0f);
    shutdown();
}

UTEST(sokol_spine, find_anim_index) {
    init();
    sspine_skeleton skeleton = create_skeleton();
    int a0 = sspine_find_anim_index(skeleton, "hoverboard");
    T(a0 == 2);
    int a1 = sspine_find_anim_index(skeleton, "bla");
    T(a1 == -1);
    shutdown();
}

UTEST(sokol_spine, find_anim_index_destroyed_instance) {
    init();
    sspine_skeleton skeleton = create_skeleton();
    sspine_destroy_skeleton(skeleton);
    int a0 = sspine_find_anim_index(skeleton, "hoverboard");
    T(a0 == -1);
    shutdown();
}

UTEST(sokol_spine, num_anims) {
    init();
    sspine_skeleton skeleton = create_skeleton();
    T(sspine_num_anims(skeleton) == 11);
    sspine_destroy_skeleton(skeleton);
    T(sspine_num_anims(skeleton) == 0);
    shutdown();
}

UTEST(sokol_spine, get_anim_info) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int anim_index = sspine_find_anim_index(skeleton, "hoverboard");
    const sspine_anim_info info = sspine_get_anim_info(instance, anim_index);
    T(info.index == 2);
    T(strcmp(info.name, "hoverboard") == 0);
    T(info.duration == 1.0f);
    shutdown();
}

UTEST(sokol_spine, get_anim_info_invalid_index) {
    init();
    sspine_instance instance = create_instance();
    const sspine_anim_info i0 = sspine_get_anim_info(instance, -1);
    T(i0.name == 0);
    const sspine_anim_info i1 = sspine_get_anim_info(instance, 1234);
    T(i1.name == 0);
    shutdown();
}

UTEST(sokol_spine, num_atlas_pages) {
    init();
    sspine_atlas atlas = create_atlas();
    T(sspine_num_atlas_pages(atlas) == 1);
    sspine_destroy_atlas(atlas);
    T(sspine_num_atlas_pages(atlas) == 0);
    shutdown();
}

UTEST(sokol_spine, get_atlas_page_info) {
    init();
    sspine_atlas atlas = create_atlas();
    const sspine_atlas_page_info info = sspine_get_atlas_page_info(atlas, 0);
    T(info.atlas.id == atlas.id);
    T(info.image.id != SG_INVALID_ID);
    T(sg_query_image_state(info.image) == SG_RESOURCESTATE_ALLOC);
    T(strcmp(info.name, "spineboy.png") == 0);
    T(info.min_filter == SG_FILTER_LINEAR);
    T(info.mag_filter == SG_FILTER_LINEAR);
    T(info.wrap_u == SG_WRAP_MIRRORED_REPEAT);
    T(info.wrap_v == SG_WRAP_MIRRORED_REPEAT);
    T(info.width == 1024);
    T(info.height == 256);
    T(info.premul_alpha == false);
    T(info.overrides.min_filter == _SG_FILTER_DEFAULT);
    T(info.overrides.mag_filter == _SG_FILTER_DEFAULT);
    T(info.overrides.wrap_u == _SG_WRAP_DEFAULT);
    T(info.overrides.wrap_v == _SG_WRAP_DEFAULT);
    T(!info.overrides.premul_alpha_enabled);
    T(!info.overrides.premul_alpha_disabled);
    shutdown();
}

UTEST(sokol_spine, get_atlas_page_info_destroyed_atlas) {
    init();
    sspine_atlas atlas = create_atlas();
    sspine_destroy_atlas(atlas);
    const sspine_atlas_page_info info = sspine_get_atlas_page_info(atlas, 0);
    T(info.atlas.id == SSPINE_INVALID_ID);
    shutdown();
}

UTEST(sokol_spine, get_atlas_page_info_invalid_index) {
    init();
    sspine_atlas atlas = create_atlas();
    sspine_destroy_atlas(atlas);
    const sspine_atlas_page_info i0 = sspine_get_atlas_page_info(atlas, -1);
    T(i0.atlas.id == SSPINE_INVALID_ID);
    const sspine_atlas_page_info i1 = sspine_get_atlas_page_info(atlas, 1234);
    T(i1.atlas.id == SSPINE_INVALID_ID);
    shutdown();
}

UTEST(sokol_spine, atlas_get_atlas_page_info_with_overrides) {
    init();
    sspine_range atlas_data = load_data("spineboy.atlas");
    sspine_atlas atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = atlas_data,
        .override = {
            .min_filter = SG_FILTER_NEAREST_MIPMAP_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_REPEAT,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .premul_alpha_enabled = true,
        }
    });
    const sspine_atlas_page_info info = sspine_get_atlas_page_info(atlas, 0);
    T(info.atlas.id == atlas.id);
    T(info.image.id != SG_INVALID_ID);
    T(sg_query_image_state(info.image) == SG_RESOURCESTATE_ALLOC);
    T(strcmp(info.name, "spineboy.png") == 0);
    T(info.min_filter == SG_FILTER_LINEAR);
    T(info.mag_filter == SG_FILTER_LINEAR);
    T(info.wrap_u == SG_WRAP_MIRRORED_REPEAT);
    T(info.wrap_v == SG_WRAP_MIRRORED_REPEAT);
    T(info.width == 1024);
    T(info.height == 256);
    T(info.premul_alpha == true); // FIXME: hmm, this is actually inconsistent
    T(info.overrides.min_filter == SG_FILTER_NEAREST_MIPMAP_NEAREST);
    T(info.overrides.mag_filter == SG_FILTER_NEAREST);
    T(info.overrides.wrap_u == SG_WRAP_REPEAT);
    T(info.overrides.wrap_v == SG_WRAP_CLAMP_TO_EDGE);
    T(info.overrides.premul_alpha_enabled);
    T(!info.overrides.premul_alpha_disabled);
    shutdown();
}

UTEST(sokol_spine, find_bone_index) {
    init();
    sspine_skeleton skeleton = create_skeleton();
    int b0 = sspine_find_bone_index(skeleton, "crosshair");
    T(b0 == 2);
    int b1 = sspine_find_bone_index(skeleton, "blablub");
    T(b1 == -1);
    shutdown();
}

UTEST(sokol_spine, find_bone_index_destroyed_skeleton) {
    init();
    sspine_skeleton skeleton = create_skeleton();
    sspine_destroy_skeleton(skeleton);
    int b0 = sspine_find_bone_index(skeleton, "crosshair");
    T(b0 == -1);
    shutdown();
}

UTEST(sokol_spine, num_bones) {
    init();
    sspine_skeleton skeleton = create_skeleton();
    T(sspine_num_bones(skeleton) == 67);
    sspine_destroy_skeleton(skeleton);
    T(sspine_num_bones(skeleton) == 0);
    shutdown();
}

UTEST(sokol_spine, get_bone_info) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int bone_index = sspine_find_bone_index(skeleton, "root");
    const sspine_bone_info info = sspine_get_bone_info(instance, bone_index);
    T(info.index == 0);
    T(strcmp(info.name, "root") == 0);
    T(info.length == 0.0f);
    T(info.pose_tform.position.x == 0.0f);
    T(info.pose_tform.position.y == 0.0f);
    T(info.pose_tform.rotation == 0.05f);
    T(info.pose_tform.scale.x == 1.0f);
    T(info.pose_tform.scale.y == 1.0f);
    T(info.pose_tform.shear.x == 0.0f);
    T(info.pose_tform.shear.y == 0.0f);
    shutdown();
}

UTEST(sokol_spine, get_bone_info_destroyed_instance) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int bone_index = sspine_find_bone_index(skeleton, "root");
    sspine_destroy_instance(instance);
    const sspine_bone_info info = sspine_get_bone_info(instance, bone_index);
    T(info.name == 0);
    shutdown();
}

UTEST(sokol_spine, get_bone_info_invalid_index) {
    init();
    sspine_instance instance = create_instance();
    const sspine_bone_info i0 = sspine_get_bone_info(instance, -1);
    T(i0.name == 0);
    const sspine_bone_info i1 = sspine_get_bone_info(instance, 1234);
    T(i1.name == 0);
    shutdown();
}

UTEST(sokol_spine, set_get_bone_transform) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int bone_index = sspine_find_bone_index(skeleton, "root");
    sspine_set_bone_transform(instance, bone_index, &(sspine_bone_transform){
        .position = { 1.0f, 2.0f },
        .rotation = 3.0f,
        .scale = { 4.0f, 5.0f },
        .shear = { 6.0f, 7.0f }
    });
    const sspine_bone_transform tform = sspine_get_bone_transform(instance, bone_index);
    T(tform.position.x == 1.0f);
    T(tform.position.y == 2.0f);
    T(tform.rotation == 3.0f);
    T(tform.scale.x == 4.0f);
    T(tform.scale.y == 5.0f);
    T(tform.shear.x == 6.0f);
    T(tform.shear.y == 7.0f);
    shutdown();
}

UTEST(sokol_spine, set_get_bone_transform_destroyed_instance) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int bone_index = sspine_find_bone_index(skeleton, "root");
    sspine_destroy_instance(instance);
    sspine_set_bone_transform(instance, bone_index, &(sspine_bone_transform){
        .position = { 1.0f, 2.0f },
        .rotation = 3.0f,
        .scale = { 4.0f, 5.0f },
        .shear = { 6.0f, 7.0f }
    });
    const sspine_bone_transform tform = sspine_get_bone_transform(instance, bone_index);
    T(tform.position.x == 0.0f);
    T(tform.position.y == 0.0f);
    T(tform.rotation == 0.0f);
    T(tform.scale.x == 0.0f);
    T(tform.scale.y == 0.0f);
    T(tform.shear.x == 0.0f);
    T(tform.shear.y == 0.0f);
    shutdown();
}

UTEST(sokol_spine, set_get_bone_position) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int bone_index = sspine_find_bone_index(skeleton, "root");
    sspine_set_bone_position(instance, bone_index, (sspine_vec2){ 1.0f, 2.0f });
    const sspine_vec2 p0 = sspine_get_bone_position(instance, bone_index);
    T(p0.x == 1.0f);
    T(p0.y == 2.0f);
    sspine_destroy_instance(instance);
    const sspine_vec2 p1 = sspine_get_bone_position(instance, bone_index);
    T(p1.x == 0.0f);
    T(p1.y == 0.0f);
    shutdown();
}

UTEST(sokol_spine, set_get_bone_rotation) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int bone_index = sspine_find_bone_index(skeleton, "root");
    sspine_set_bone_rotation(instance, bone_index, 5.0f);
    T(sspine_get_bone_rotation(instance, bone_index) == 5.0f);
    sspine_destroy_instance(instance);
    T(sspine_get_bone_rotation(instance, bone_index) == 0.0f);
    shutdown();
}

UTEST(sokol_spine, set_get_bone_scale) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int bone_index = sspine_find_bone_index(skeleton, "root");
    sspine_set_bone_scale(instance, bone_index, (sspine_vec2){ 1.0f, 2.0f });
    const sspine_vec2 s0 = sspine_get_bone_scale(instance, bone_index);
    T(s0.x == 1.0f);
    T(s0.y == 2.0f);
    sspine_destroy_instance(instance);
    const sspine_vec2 s1 = sspine_get_bone_scale(instance, bone_index);
    T(s1.x == 0.0f);
    T(s1.y == 0.0f);
    shutdown();
}

UTEST(sokol_spine, set_get_bone_shear) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int bone_index = sspine_find_bone_index(skeleton, "root");
    sspine_set_bone_shear(instance, bone_index, (sspine_vec2){ 1.0f, 2.0f });
    const sspine_vec2 s0 = sspine_get_bone_shear(instance, bone_index);
    T(s0.x == 1.0f);
    T(s0.y == 2.0f);
    sspine_destroy_instance(instance);
    const sspine_vec2 s1 = sspine_get_bone_shear(instance, bone_index);
    T(s1.x == 0.0f);
    T(s1.y == 0.0f);
    shutdown();
}

UTEST(sokol_spine, find_slot_index) {
    init();
    sspine_skeleton skeleton = create_skeleton();
    int s0 = sspine_find_slot_index(skeleton, "portal-streaks1");
    T(s0 == 3);
    int s1 = sspine_find_slot_index(skeleton, "blablub");
    T(s1 == -1);
    shutdown();
}

UTEST(sokol_spine, find_slot_index_destroyed_skeleton) {
    init();
    sspine_skeleton skeleton = create_skeleton();
    sspine_destroy_skeleton(skeleton);
    int s0 = sspine_find_slot_index(skeleton, "portal-streaks1");
    T(s0 == -1);
    shutdown();
}

UTEST(sokol_spine, num_slots) {
    init();
    sspine_skeleton skeleton = create_skeleton();
    T(sspine_num_slots(skeleton) == 52);
    sspine_destroy_skeleton(skeleton);
    T(sspine_num_slots(skeleton) == -1);
    shutdown();
}

UTEST(sokol_spine, get_slot_info) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int slot_index = sspine_find_slot_index(skeleton, "portal-streaks1");
    const sspine_slot_info info = sspine_get_slot_info(instance, slot_index);
    T(info.index == 3);
    T(strcmp(info.name, "portal-streaks1") == 0);
    T(info.attachment_name == 0);
    T(info.bone_index == 62);
    T(info.color.r == 1.0f);
    T(info.color.g == 1.0f);
    T(info.color.b == 1.0f);
    T(info.color.a == 1.0f);
    shutdown();
}

UTEST(sokol_spine, get_slot_info_destroyed_instance) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int slot_index = sspine_find_slot_index(skeleton, "portal-streaks1");
    sspine_destroy_instance(instance);
    const sspine_slot_info info = sspine_get_slot_info(instance, slot_index);
    T(info.name == 0);
    shutdown();
}

UTEST(sokol_spine, get_slot_info_invalid_index) {
    init();
    sspine_instance instance = create_instance();
    sspine_destroy_instance(instance);
    const sspine_slot_info i0 = sspine_get_slot_info(instance, -1);
    T(i0.name == 0);
    const sspine_slot_info i1 = sspine_get_slot_info(instance, 1234);
    T(i1.name == 0);
    shutdown();
}

UTEST(sokol_spine, set_get_slot_color) {
    init();
    sspine_instance instance = create_instance();
    sspine_skeleton skeleton = sspine_get_instance_skeleton(instance);
    int slot_index = sspine_find_slot_index(skeleton, "portal-streaks1");
    sspine_set_slot_color(instance, slot_index, (sspine_color){ 1.0f, 2.0f, 3.0f, 4.0f });
    const sspine_color color = sspine_get_slot_color(instance, slot_index);
    T(color.r == 1.0f);
    T(color.g == 2.0f);
    T(color.b == 3.0f);
    T(color.a == 4.0f);
    const sspine_slot_info info = sspine_get_slot_info(instance, slot_index);
    T(info.color.r == 1.0f);
    T(info.color.g == 2.0f);
    T(info.color.b == 3.0f);
    T(info.color.a == 4.0f);
    shutdown();
}
