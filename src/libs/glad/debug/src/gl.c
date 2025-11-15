/**
 * SPDX-License-Identifier: (WTFPL OR CC0-1.0) AND Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glad/gl.h>

#ifndef GLAD_IMPL_UTIL_C_
#define GLAD_IMPL_UTIL_C_

#ifdef _MSC_VER
#define GLAD_IMPL_UTIL_SSCANF sscanf_s
#else
#define GLAD_IMPL_UTIL_SSCANF sscanf
#endif

#endif /* GLAD_IMPL_UTIL_C_ */

#ifdef __cplusplus
extern "C" {
#endif



int GLAD_GL_VERSION_1_0 = 0;
int GLAD_GL_VERSION_1_1 = 0;
int GLAD_GL_VERSION_1_2 = 0;
int GLAD_GL_VERSION_1_3 = 0;
int GLAD_GL_VERSION_1_4 = 0;
int GLAD_GL_VERSION_1_5 = 0;
int GLAD_GL_VERSION_2_0 = 0;
int GLAD_GL_VERSION_2_1 = 0;
int GLAD_GL_VERSION_3_0 = 0;
int GLAD_GL_VERSION_3_1 = 0;
int GLAD_GL_VERSION_3_2 = 0;
int GLAD_GL_VERSION_3_3 = 0;
int GLAD_GL_APPLE_flush_buffer_range = 0;
int GLAD_GL_APPLE_vertex_array_object = 0;
int GLAD_GL_ARB_blend_func_extended = 0;
int GLAD_GL_ARB_color_buffer_float = 0;
int GLAD_GL_ARB_copy_buffer = 0;
int GLAD_GL_ARB_draw_buffers = 0;
int GLAD_GL_ARB_draw_elements_base_vertex = 0;
int GLAD_GL_ARB_draw_instanced = 0;
int GLAD_GL_ARB_framebuffer_object = 0;
int GLAD_GL_ARB_geometry_shader4 = 0;
int GLAD_GL_ARB_imaging = 0;
int GLAD_GL_ARB_instanced_arrays = 0;
int GLAD_GL_ARB_map_buffer_range = 0;
int GLAD_GL_ARB_multisample = 0;
int GLAD_GL_ARB_multitexture = 0;
int GLAD_GL_ARB_occlusion_query = 0;
int GLAD_GL_ARB_point_parameters = 0;
int GLAD_GL_ARB_provoking_vertex = 0;
int GLAD_GL_ARB_sampler_objects = 0;
int GLAD_GL_ARB_shader_objects = 0;
int GLAD_GL_ARB_sync = 0;
int GLAD_GL_ARB_texture_buffer_object = 0;
int GLAD_GL_ARB_texture_compression = 0;
int GLAD_GL_ARB_texture_multisample = 0;
int GLAD_GL_ARB_timer_query = 0;
int GLAD_GL_ARB_uniform_buffer_object = 0;
int GLAD_GL_ARB_vertex_array_object = 0;
int GLAD_GL_ARB_vertex_buffer_object = 0;
int GLAD_GL_ARB_vertex_program = 0;
int GLAD_GL_ARB_vertex_shader = 0;
int GLAD_GL_ARB_vertex_type_2_10_10_10_rev = 0;
int GLAD_GL_ATI_draw_buffers = 0;
int GLAD_GL_ATI_separate_stencil = 0;
int GLAD_GL_EXT_blend_color = 0;
int GLAD_GL_EXT_blend_equation_separate = 0;
int GLAD_GL_EXT_blend_func_separate = 0;
int GLAD_GL_EXT_blend_minmax = 0;
int GLAD_GL_EXT_copy_texture = 0;
int GLAD_GL_EXT_direct_state_access = 0;
int GLAD_GL_EXT_draw_buffers2 = 0;
int GLAD_GL_EXT_draw_instanced = 0;
int GLAD_GL_EXT_draw_range_elements = 0;
int GLAD_GL_EXT_framebuffer_blit = 0;
int GLAD_GL_EXT_framebuffer_multisample = 0;
int GLAD_GL_EXT_framebuffer_object = 0;
int GLAD_GL_EXT_gpu_shader4 = 0;
int GLAD_GL_EXT_multi_draw_arrays = 0;
int GLAD_GL_EXT_point_parameters = 0;
int GLAD_GL_EXT_provoking_vertex = 0;
int GLAD_GL_EXT_subtexture = 0;
int GLAD_GL_EXT_texture3D = 0;
int GLAD_GL_EXT_texture_array = 0;
int GLAD_GL_EXT_texture_buffer_object = 0;
int GLAD_GL_EXT_texture_integer = 0;
int GLAD_GL_EXT_texture_object = 0;
int GLAD_GL_EXT_timer_query = 0;
int GLAD_GL_EXT_transform_feedback = 0;
int GLAD_GL_EXT_vertex_array = 0;
int GLAD_GL_INGR_blend_func_separate = 0;
int GLAD_GL_NVX_conditional_render = 0;
int GLAD_GL_NV_conditional_render = 0;
int GLAD_GL_NV_explicit_multisample = 0;
int GLAD_GL_NV_geometry_program4 = 0;
int GLAD_GL_NV_point_sprite = 0;
int GLAD_GL_NV_transform_feedback = 0;
int GLAD_GL_NV_vertex_program = 0;
int GLAD_GL_NV_vertex_program4 = 0;
int GLAD_GL_SGIS_point_parameters = 0;


static void _pre_call_gl_callback_default(const char *name, GLADapiproc apiproc, int len_args, ...) {
    GLAD_UNUSED(len_args);

    if (apiproc == NULL) {
        fprintf(stderr, "GLAD: ERROR %s is NULL!\n", name);
        return;
    }
    if (glad_glGetError == NULL) {
        fprintf(stderr, "GLAD: ERROR glGetError is NULL!\n");
        return;
    }

    (void) glad_glGetError();
}
static void _post_call_gl_callback_default(void *ret, const char *name, GLADapiproc apiproc, int len_args, ...) {
    GLenum error_code;

    GLAD_UNUSED(ret);
    GLAD_UNUSED(apiproc);
    GLAD_UNUSED(len_args);

    error_code = glad_glGetError();

    if (error_code != GL_NO_ERROR) {
        fprintf(stderr, "GLAD: ERROR %d in %s!\n", error_code, name);
    }
}

static GLADprecallback _pre_call_gl_callback = _pre_call_gl_callback_default;
void gladSetGLPreCallback(GLADprecallback cb) {
    _pre_call_gl_callback = cb;
}
static GLADpostcallback _post_call_gl_callback = _post_call_gl_callback_default;
void gladSetGLPostCallback(GLADpostcallback cb) {
    _post_call_gl_callback = cb;
}

PFNGLACTIVETEXTUREPROC glad_glActiveTexture = NULL;
static void GLAD_API_PTR glad_debug_impl_glActiveTexture(GLenum texture) {
    _pre_call_gl_callback("glActiveTexture", (GLADapiproc) glad_glActiveTexture, 1, texture);
    glad_glActiveTexture(texture);
    _post_call_gl_callback(NULL, "glActiveTexture", (GLADapiproc) glad_glActiveTexture, 1, texture);
    
}
PFNGLACTIVETEXTUREPROC glad_debug_glActiveTexture = glad_debug_impl_glActiveTexture;
PFNGLACTIVETEXTUREARBPROC glad_glActiveTextureARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glActiveTextureARB(GLenum texture) {
    _pre_call_gl_callback("glActiveTextureARB", (GLADapiproc) glad_glActiveTextureARB, 1, texture);
    glad_glActiveTextureARB(texture);
    _post_call_gl_callback(NULL, "glActiveTextureARB", (GLADapiproc) glad_glActiveTextureARB, 1, texture);
    
}
PFNGLACTIVETEXTUREARBPROC glad_debug_glActiveTextureARB = glad_debug_impl_glActiveTextureARB;
PFNGLACTIVEVARYINGNVPROC glad_glActiveVaryingNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glActiveVaryingNV(GLuint program, const GLchar * name) {
    _pre_call_gl_callback("glActiveVaryingNV", (GLADapiproc) glad_glActiveVaryingNV, 2, program, name);
    glad_glActiveVaryingNV(program, name);
    _post_call_gl_callback(NULL, "glActiveVaryingNV", (GLADapiproc) glad_glActiveVaryingNV, 2, program, name);
    
}
PFNGLACTIVEVARYINGNVPROC glad_debug_glActiveVaryingNV = glad_debug_impl_glActiveVaryingNV;
PFNGLAREPROGRAMSRESIDENTNVPROC glad_glAreProgramsResidentNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glAreProgramsResidentNV(GLsizei n, const GLuint * programs, GLboolean * residences) {
    GLboolean ret;
    _pre_call_gl_callback("glAreProgramsResidentNV", (GLADapiproc) glad_glAreProgramsResidentNV, 3, n, programs, residences);
    ret = glad_glAreProgramsResidentNV(n, programs, residences);
    _post_call_gl_callback((void*) &ret, "glAreProgramsResidentNV", (GLADapiproc) glad_glAreProgramsResidentNV, 3, n, programs, residences);
    return ret;
}
PFNGLAREPROGRAMSRESIDENTNVPROC glad_debug_glAreProgramsResidentNV = glad_debug_impl_glAreProgramsResidentNV;
PFNGLARETEXTURESRESIDENTEXTPROC glad_glAreTexturesResidentEXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glAreTexturesResidentEXT(GLsizei n, const GLuint * textures, GLboolean * residences) {
    GLboolean ret;
    _pre_call_gl_callback("glAreTexturesResidentEXT", (GLADapiproc) glad_glAreTexturesResidentEXT, 3, n, textures, residences);
    ret = glad_glAreTexturesResidentEXT(n, textures, residences);
    _post_call_gl_callback((void*) &ret, "glAreTexturesResidentEXT", (GLADapiproc) glad_glAreTexturesResidentEXT, 3, n, textures, residences);
    return ret;
}
PFNGLARETEXTURESRESIDENTEXTPROC glad_debug_glAreTexturesResidentEXT = glad_debug_impl_glAreTexturesResidentEXT;
PFNGLARRAYELEMENTEXTPROC glad_glArrayElementEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glArrayElementEXT(GLint i) {
    _pre_call_gl_callback("glArrayElementEXT", (GLADapiproc) glad_glArrayElementEXT, 1, i);
    glad_glArrayElementEXT(i);
    _post_call_gl_callback(NULL, "glArrayElementEXT", (GLADapiproc) glad_glArrayElementEXT, 1, i);
    
}
PFNGLARRAYELEMENTEXTPROC glad_debug_glArrayElementEXT = glad_debug_impl_glArrayElementEXT;
PFNGLATTACHOBJECTARBPROC glad_glAttachObjectARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glAttachObjectARB(GLhandleARB containerObj, GLhandleARB obj) {
    _pre_call_gl_callback("glAttachObjectARB", (GLADapiproc) glad_glAttachObjectARB, 2, containerObj, obj);
    glad_glAttachObjectARB(containerObj, obj);
    _post_call_gl_callback(NULL, "glAttachObjectARB", (GLADapiproc) glad_glAttachObjectARB, 2, containerObj, obj);
    
}
PFNGLATTACHOBJECTARBPROC glad_debug_glAttachObjectARB = glad_debug_impl_glAttachObjectARB;
PFNGLATTACHSHADERPROC glad_glAttachShader = NULL;
static void GLAD_API_PTR glad_debug_impl_glAttachShader(GLuint program, GLuint shader) {
    _pre_call_gl_callback("glAttachShader", (GLADapiproc) glad_glAttachShader, 2, program, shader);
    glad_glAttachShader(program, shader);
    _post_call_gl_callback(NULL, "glAttachShader", (GLADapiproc) glad_glAttachShader, 2, program, shader);
    
}
PFNGLATTACHSHADERPROC glad_debug_glAttachShader = glad_debug_impl_glAttachShader;
PFNGLBEGINCONDITIONALRENDERPROC glad_glBeginConditionalRender = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginConditionalRender(GLuint id, GLenum mode) {
    _pre_call_gl_callback("glBeginConditionalRender", (GLADapiproc) glad_glBeginConditionalRender, 2, id, mode);
    glad_glBeginConditionalRender(id, mode);
    _post_call_gl_callback(NULL, "glBeginConditionalRender", (GLADapiproc) glad_glBeginConditionalRender, 2, id, mode);
    
}
PFNGLBEGINCONDITIONALRENDERPROC glad_debug_glBeginConditionalRender = glad_debug_impl_glBeginConditionalRender;
PFNGLBEGINCONDITIONALRENDERNVPROC glad_glBeginConditionalRenderNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginConditionalRenderNV(GLuint id, GLenum mode) {
    _pre_call_gl_callback("glBeginConditionalRenderNV", (GLADapiproc) glad_glBeginConditionalRenderNV, 2, id, mode);
    glad_glBeginConditionalRenderNV(id, mode);
    _post_call_gl_callback(NULL, "glBeginConditionalRenderNV", (GLADapiproc) glad_glBeginConditionalRenderNV, 2, id, mode);
    
}
PFNGLBEGINCONDITIONALRENDERNVPROC glad_debug_glBeginConditionalRenderNV = glad_debug_impl_glBeginConditionalRenderNV;
PFNGLBEGINCONDITIONALRENDERNVXPROC glad_glBeginConditionalRenderNVX = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginConditionalRenderNVX(GLuint id) {
    _pre_call_gl_callback("glBeginConditionalRenderNVX", (GLADapiproc) glad_glBeginConditionalRenderNVX, 1, id);
    glad_glBeginConditionalRenderNVX(id);
    _post_call_gl_callback(NULL, "glBeginConditionalRenderNVX", (GLADapiproc) glad_glBeginConditionalRenderNVX, 1, id);
    
}
PFNGLBEGINCONDITIONALRENDERNVXPROC glad_debug_glBeginConditionalRenderNVX = glad_debug_impl_glBeginConditionalRenderNVX;
PFNGLBEGINQUERYPROC glad_glBeginQuery = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginQuery(GLenum target, GLuint id) {
    _pre_call_gl_callback("glBeginQuery", (GLADapiproc) glad_glBeginQuery, 2, target, id);
    glad_glBeginQuery(target, id);
    _post_call_gl_callback(NULL, "glBeginQuery", (GLADapiproc) glad_glBeginQuery, 2, target, id);
    
}
PFNGLBEGINQUERYPROC glad_debug_glBeginQuery = glad_debug_impl_glBeginQuery;
PFNGLBEGINQUERYARBPROC glad_glBeginQueryARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginQueryARB(GLenum target, GLuint id) {
    _pre_call_gl_callback("glBeginQueryARB", (GLADapiproc) glad_glBeginQueryARB, 2, target, id);
    glad_glBeginQueryARB(target, id);
    _post_call_gl_callback(NULL, "glBeginQueryARB", (GLADapiproc) glad_glBeginQueryARB, 2, target, id);
    
}
PFNGLBEGINQUERYARBPROC glad_debug_glBeginQueryARB = glad_debug_impl_glBeginQueryARB;
PFNGLBEGINTRANSFORMFEEDBACKPROC glad_glBeginTransformFeedback = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginTransformFeedback(GLenum primitiveMode) {
    _pre_call_gl_callback("glBeginTransformFeedback", (GLADapiproc) glad_glBeginTransformFeedback, 1, primitiveMode);
    glad_glBeginTransformFeedback(primitiveMode);
    _post_call_gl_callback(NULL, "glBeginTransformFeedback", (GLADapiproc) glad_glBeginTransformFeedback, 1, primitiveMode);
    
}
PFNGLBEGINTRANSFORMFEEDBACKPROC glad_debug_glBeginTransformFeedback = glad_debug_impl_glBeginTransformFeedback;
PFNGLBEGINTRANSFORMFEEDBACKEXTPROC glad_glBeginTransformFeedbackEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginTransformFeedbackEXT(GLenum primitiveMode) {
    _pre_call_gl_callback("glBeginTransformFeedbackEXT", (GLADapiproc) glad_glBeginTransformFeedbackEXT, 1, primitiveMode);
    glad_glBeginTransformFeedbackEXT(primitiveMode);
    _post_call_gl_callback(NULL, "glBeginTransformFeedbackEXT", (GLADapiproc) glad_glBeginTransformFeedbackEXT, 1, primitiveMode);
    
}
PFNGLBEGINTRANSFORMFEEDBACKEXTPROC glad_debug_glBeginTransformFeedbackEXT = glad_debug_impl_glBeginTransformFeedbackEXT;
PFNGLBEGINTRANSFORMFEEDBACKNVPROC glad_glBeginTransformFeedbackNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginTransformFeedbackNV(GLenum primitiveMode) {
    _pre_call_gl_callback("glBeginTransformFeedbackNV", (GLADapiproc) glad_glBeginTransformFeedbackNV, 1, primitiveMode);
    glad_glBeginTransformFeedbackNV(primitiveMode);
    _post_call_gl_callback(NULL, "glBeginTransformFeedbackNV", (GLADapiproc) glad_glBeginTransformFeedbackNV, 1, primitiveMode);
    
}
PFNGLBEGINTRANSFORMFEEDBACKNVPROC glad_debug_glBeginTransformFeedbackNV = glad_debug_impl_glBeginTransformFeedbackNV;
PFNGLBINDATTRIBLOCATIONPROC glad_glBindAttribLocation = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindAttribLocation(GLuint program, GLuint index, const GLchar * name) {
    _pre_call_gl_callback("glBindAttribLocation", (GLADapiproc) glad_glBindAttribLocation, 3, program, index, name);
    glad_glBindAttribLocation(program, index, name);
    _post_call_gl_callback(NULL, "glBindAttribLocation", (GLADapiproc) glad_glBindAttribLocation, 3, program, index, name);
    
}
PFNGLBINDATTRIBLOCATIONPROC glad_debug_glBindAttribLocation = glad_debug_impl_glBindAttribLocation;
PFNGLBINDATTRIBLOCATIONARBPROC glad_glBindAttribLocationARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindAttribLocationARB(GLhandleARB programObj, GLuint index, const GLcharARB * name) {
    _pre_call_gl_callback("glBindAttribLocationARB", (GLADapiproc) glad_glBindAttribLocationARB, 3, programObj, index, name);
    glad_glBindAttribLocationARB(programObj, index, name);
    _post_call_gl_callback(NULL, "glBindAttribLocationARB", (GLADapiproc) glad_glBindAttribLocationARB, 3, programObj, index, name);
    
}
PFNGLBINDATTRIBLOCATIONARBPROC glad_debug_glBindAttribLocationARB = glad_debug_impl_glBindAttribLocationARB;
PFNGLBINDBUFFERPROC glad_glBindBuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBuffer(GLenum target, GLuint buffer) {
    _pre_call_gl_callback("glBindBuffer", (GLADapiproc) glad_glBindBuffer, 2, target, buffer);
    glad_glBindBuffer(target, buffer);
    _post_call_gl_callback(NULL, "glBindBuffer", (GLADapiproc) glad_glBindBuffer, 2, target, buffer);
    
}
PFNGLBINDBUFFERPROC glad_debug_glBindBuffer = glad_debug_impl_glBindBuffer;
PFNGLBINDBUFFERARBPROC glad_glBindBufferARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBufferARB(GLenum target, GLuint buffer) {
    _pre_call_gl_callback("glBindBufferARB", (GLADapiproc) glad_glBindBufferARB, 2, target, buffer);
    glad_glBindBufferARB(target, buffer);
    _post_call_gl_callback(NULL, "glBindBufferARB", (GLADapiproc) glad_glBindBufferARB, 2, target, buffer);
    
}
PFNGLBINDBUFFERARBPROC glad_debug_glBindBufferARB = glad_debug_impl_glBindBufferARB;
PFNGLBINDBUFFERBASEPROC glad_glBindBufferBase = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    _pre_call_gl_callback("glBindBufferBase", (GLADapiproc) glad_glBindBufferBase, 3, target, index, buffer);
    glad_glBindBufferBase(target, index, buffer);
    _post_call_gl_callback(NULL, "glBindBufferBase", (GLADapiproc) glad_glBindBufferBase, 3, target, index, buffer);
    
}
PFNGLBINDBUFFERBASEPROC glad_debug_glBindBufferBase = glad_debug_impl_glBindBufferBase;
PFNGLBINDBUFFERBASEEXTPROC glad_glBindBufferBaseEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBufferBaseEXT(GLenum target, GLuint index, GLuint buffer) {
    _pre_call_gl_callback("glBindBufferBaseEXT", (GLADapiproc) glad_glBindBufferBaseEXT, 3, target, index, buffer);
    glad_glBindBufferBaseEXT(target, index, buffer);
    _post_call_gl_callback(NULL, "glBindBufferBaseEXT", (GLADapiproc) glad_glBindBufferBaseEXT, 3, target, index, buffer);
    
}
PFNGLBINDBUFFERBASEEXTPROC glad_debug_glBindBufferBaseEXT = glad_debug_impl_glBindBufferBaseEXT;
PFNGLBINDBUFFERBASENVPROC glad_glBindBufferBaseNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBufferBaseNV(GLenum target, GLuint index, GLuint buffer) {
    _pre_call_gl_callback("glBindBufferBaseNV", (GLADapiproc) glad_glBindBufferBaseNV, 3, target, index, buffer);
    glad_glBindBufferBaseNV(target, index, buffer);
    _post_call_gl_callback(NULL, "glBindBufferBaseNV", (GLADapiproc) glad_glBindBufferBaseNV, 3, target, index, buffer);
    
}
PFNGLBINDBUFFERBASENVPROC glad_debug_glBindBufferBaseNV = glad_debug_impl_glBindBufferBaseNV;
PFNGLBINDBUFFEROFFSETEXTPROC glad_glBindBufferOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBufferOffsetEXT(GLenum target, GLuint index, GLuint buffer, GLintptr offset) {
    _pre_call_gl_callback("glBindBufferOffsetEXT", (GLADapiproc) glad_glBindBufferOffsetEXT, 4, target, index, buffer, offset);
    glad_glBindBufferOffsetEXT(target, index, buffer, offset);
    _post_call_gl_callback(NULL, "glBindBufferOffsetEXT", (GLADapiproc) glad_glBindBufferOffsetEXT, 4, target, index, buffer, offset);
    
}
PFNGLBINDBUFFEROFFSETEXTPROC glad_debug_glBindBufferOffsetEXT = glad_debug_impl_glBindBufferOffsetEXT;
PFNGLBINDBUFFEROFFSETNVPROC glad_glBindBufferOffsetNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBufferOffsetNV(GLenum target, GLuint index, GLuint buffer, GLintptr offset) {
    _pre_call_gl_callback("glBindBufferOffsetNV", (GLADapiproc) glad_glBindBufferOffsetNV, 4, target, index, buffer, offset);
    glad_glBindBufferOffsetNV(target, index, buffer, offset);
    _post_call_gl_callback(NULL, "glBindBufferOffsetNV", (GLADapiproc) glad_glBindBufferOffsetNV, 4, target, index, buffer, offset);
    
}
PFNGLBINDBUFFEROFFSETNVPROC glad_debug_glBindBufferOffsetNV = glad_debug_impl_glBindBufferOffsetNV;
PFNGLBINDBUFFERRANGEPROC glad_glBindBufferRange = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    _pre_call_gl_callback("glBindBufferRange", (GLADapiproc) glad_glBindBufferRange, 5, target, index, buffer, offset, size);
    glad_glBindBufferRange(target, index, buffer, offset, size);
    _post_call_gl_callback(NULL, "glBindBufferRange", (GLADapiproc) glad_glBindBufferRange, 5, target, index, buffer, offset, size);
    
}
PFNGLBINDBUFFERRANGEPROC glad_debug_glBindBufferRange = glad_debug_impl_glBindBufferRange;
PFNGLBINDBUFFERRANGEEXTPROC glad_glBindBufferRangeEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBufferRangeEXT(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    _pre_call_gl_callback("glBindBufferRangeEXT", (GLADapiproc) glad_glBindBufferRangeEXT, 5, target, index, buffer, offset, size);
    glad_glBindBufferRangeEXT(target, index, buffer, offset, size);
    _post_call_gl_callback(NULL, "glBindBufferRangeEXT", (GLADapiproc) glad_glBindBufferRangeEXT, 5, target, index, buffer, offset, size);
    
}
PFNGLBINDBUFFERRANGEEXTPROC glad_debug_glBindBufferRangeEXT = glad_debug_impl_glBindBufferRangeEXT;
PFNGLBINDBUFFERRANGENVPROC glad_glBindBufferRangeNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBufferRangeNV(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    _pre_call_gl_callback("glBindBufferRangeNV", (GLADapiproc) glad_glBindBufferRangeNV, 5, target, index, buffer, offset, size);
    glad_glBindBufferRangeNV(target, index, buffer, offset, size);
    _post_call_gl_callback(NULL, "glBindBufferRangeNV", (GLADapiproc) glad_glBindBufferRangeNV, 5, target, index, buffer, offset, size);
    
}
PFNGLBINDBUFFERRANGENVPROC glad_debug_glBindBufferRangeNV = glad_debug_impl_glBindBufferRangeNV;
PFNGLBINDFRAGDATALOCATIONPROC glad_glBindFragDataLocation = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindFragDataLocation(GLuint program, GLuint color, const GLchar * name) {
    _pre_call_gl_callback("glBindFragDataLocation", (GLADapiproc) glad_glBindFragDataLocation, 3, program, color, name);
    glad_glBindFragDataLocation(program, color, name);
    _post_call_gl_callback(NULL, "glBindFragDataLocation", (GLADapiproc) glad_glBindFragDataLocation, 3, program, color, name);
    
}
PFNGLBINDFRAGDATALOCATIONPROC glad_debug_glBindFragDataLocation = glad_debug_impl_glBindFragDataLocation;
PFNGLBINDFRAGDATALOCATIONEXTPROC glad_glBindFragDataLocationEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindFragDataLocationEXT(GLuint program, GLuint color, const GLchar * name) {
    _pre_call_gl_callback("glBindFragDataLocationEXT", (GLADapiproc) glad_glBindFragDataLocationEXT, 3, program, color, name);
    glad_glBindFragDataLocationEXT(program, color, name);
    _post_call_gl_callback(NULL, "glBindFragDataLocationEXT", (GLADapiproc) glad_glBindFragDataLocationEXT, 3, program, color, name);
    
}
PFNGLBINDFRAGDATALOCATIONEXTPROC glad_debug_glBindFragDataLocationEXT = glad_debug_impl_glBindFragDataLocationEXT;
PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glad_glBindFragDataLocationIndexed = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindFragDataLocationIndexed(GLuint program, GLuint colorNumber, GLuint index, const GLchar * name) {
    _pre_call_gl_callback("glBindFragDataLocationIndexed", (GLADapiproc) glad_glBindFragDataLocationIndexed, 4, program, colorNumber, index, name);
    glad_glBindFragDataLocationIndexed(program, colorNumber, index, name);
    _post_call_gl_callback(NULL, "glBindFragDataLocationIndexed", (GLADapiproc) glad_glBindFragDataLocationIndexed, 4, program, colorNumber, index, name);
    
}
PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glad_debug_glBindFragDataLocationIndexed = glad_debug_impl_glBindFragDataLocationIndexed;
PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    _pre_call_gl_callback("glBindFramebuffer", (GLADapiproc) glad_glBindFramebuffer, 2, target, framebuffer);
    glad_glBindFramebuffer(target, framebuffer);
    _post_call_gl_callback(NULL, "glBindFramebuffer", (GLADapiproc) glad_glBindFramebuffer, 2, target, framebuffer);
    
}
PFNGLBINDFRAMEBUFFERPROC glad_debug_glBindFramebuffer = glad_debug_impl_glBindFramebuffer;
PFNGLBINDFRAMEBUFFEREXTPROC glad_glBindFramebufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindFramebufferEXT(GLenum target, GLuint framebuffer) {
    _pre_call_gl_callback("glBindFramebufferEXT", (GLADapiproc) glad_glBindFramebufferEXT, 2, target, framebuffer);
    glad_glBindFramebufferEXT(target, framebuffer);
    _post_call_gl_callback(NULL, "glBindFramebufferEXT", (GLADapiproc) glad_glBindFramebufferEXT, 2, target, framebuffer);
    
}
PFNGLBINDFRAMEBUFFEREXTPROC glad_debug_glBindFramebufferEXT = glad_debug_impl_glBindFramebufferEXT;
PFNGLBINDMULTITEXTUREEXTPROC glad_glBindMultiTextureEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture) {
    _pre_call_gl_callback("glBindMultiTextureEXT", (GLADapiproc) glad_glBindMultiTextureEXT, 3, texunit, target, texture);
    glad_glBindMultiTextureEXT(texunit, target, texture);
    _post_call_gl_callback(NULL, "glBindMultiTextureEXT", (GLADapiproc) glad_glBindMultiTextureEXT, 3, texunit, target, texture);
    
}
PFNGLBINDMULTITEXTUREEXTPROC glad_debug_glBindMultiTextureEXT = glad_debug_impl_glBindMultiTextureEXT;
PFNGLBINDPROGRAMARBPROC glad_glBindProgramARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindProgramARB(GLenum target, GLuint program) {
    _pre_call_gl_callback("glBindProgramARB", (GLADapiproc) glad_glBindProgramARB, 2, target, program);
    glad_glBindProgramARB(target, program);
    _post_call_gl_callback(NULL, "glBindProgramARB", (GLADapiproc) glad_glBindProgramARB, 2, target, program);
    
}
PFNGLBINDPROGRAMARBPROC glad_debug_glBindProgramARB = glad_debug_impl_glBindProgramARB;
PFNGLBINDPROGRAMNVPROC glad_glBindProgramNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindProgramNV(GLenum target, GLuint id) {
    _pre_call_gl_callback("glBindProgramNV", (GLADapiproc) glad_glBindProgramNV, 2, target, id);
    glad_glBindProgramNV(target, id);
    _post_call_gl_callback(NULL, "glBindProgramNV", (GLADapiproc) glad_glBindProgramNV, 2, target, id);
    
}
PFNGLBINDPROGRAMNVPROC glad_debug_glBindProgramNV = glad_debug_impl_glBindProgramNV;
PFNGLBINDRENDERBUFFERPROC glad_glBindRenderbuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    _pre_call_gl_callback("glBindRenderbuffer", (GLADapiproc) glad_glBindRenderbuffer, 2, target, renderbuffer);
    glad_glBindRenderbuffer(target, renderbuffer);
    _post_call_gl_callback(NULL, "glBindRenderbuffer", (GLADapiproc) glad_glBindRenderbuffer, 2, target, renderbuffer);
    
}
PFNGLBINDRENDERBUFFERPROC glad_debug_glBindRenderbuffer = glad_debug_impl_glBindRenderbuffer;
PFNGLBINDRENDERBUFFEREXTPROC glad_glBindRenderbufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindRenderbufferEXT(GLenum target, GLuint renderbuffer) {
    _pre_call_gl_callback("glBindRenderbufferEXT", (GLADapiproc) glad_glBindRenderbufferEXT, 2, target, renderbuffer);
    glad_glBindRenderbufferEXT(target, renderbuffer);
    _post_call_gl_callback(NULL, "glBindRenderbufferEXT", (GLADapiproc) glad_glBindRenderbufferEXT, 2, target, renderbuffer);
    
}
PFNGLBINDRENDERBUFFEREXTPROC glad_debug_glBindRenderbufferEXT = glad_debug_impl_glBindRenderbufferEXT;
PFNGLBINDSAMPLERPROC glad_glBindSampler = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindSampler(GLuint unit, GLuint sampler) {
    _pre_call_gl_callback("glBindSampler", (GLADapiproc) glad_glBindSampler, 2, unit, sampler);
    glad_glBindSampler(unit, sampler);
    _post_call_gl_callback(NULL, "glBindSampler", (GLADapiproc) glad_glBindSampler, 2, unit, sampler);
    
}
PFNGLBINDSAMPLERPROC glad_debug_glBindSampler = glad_debug_impl_glBindSampler;
PFNGLBINDTEXTUREPROC glad_glBindTexture = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindTexture(GLenum target, GLuint texture) {
    _pre_call_gl_callback("glBindTexture", (GLADapiproc) glad_glBindTexture, 2, target, texture);
    glad_glBindTexture(target, texture);
    _post_call_gl_callback(NULL, "glBindTexture", (GLADapiproc) glad_glBindTexture, 2, target, texture);
    
}
PFNGLBINDTEXTUREPROC glad_debug_glBindTexture = glad_debug_impl_glBindTexture;
PFNGLBINDTEXTUREEXTPROC glad_glBindTextureEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindTextureEXT(GLenum target, GLuint texture) {
    _pre_call_gl_callback("glBindTextureEXT", (GLADapiproc) glad_glBindTextureEXT, 2, target, texture);
    glad_glBindTextureEXT(target, texture);
    _post_call_gl_callback(NULL, "glBindTextureEXT", (GLADapiproc) glad_glBindTextureEXT, 2, target, texture);
    
}
PFNGLBINDTEXTUREEXTPROC glad_debug_glBindTextureEXT = glad_debug_impl_glBindTextureEXT;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindVertexArray(GLuint array) {
    _pre_call_gl_callback("glBindVertexArray", (GLADapiproc) glad_glBindVertexArray, 1, array);
    glad_glBindVertexArray(array);
    _post_call_gl_callback(NULL, "glBindVertexArray", (GLADapiproc) glad_glBindVertexArray, 1, array);
    
}
PFNGLBINDVERTEXARRAYPROC glad_debug_glBindVertexArray = glad_debug_impl_glBindVertexArray;
PFNGLBINDVERTEXARRAYAPPLEPROC glad_glBindVertexArrayAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindVertexArrayAPPLE(GLuint array) {
    _pre_call_gl_callback("glBindVertexArrayAPPLE", (GLADapiproc) glad_glBindVertexArrayAPPLE, 1, array);
    glad_glBindVertexArrayAPPLE(array);
    _post_call_gl_callback(NULL, "glBindVertexArrayAPPLE", (GLADapiproc) glad_glBindVertexArrayAPPLE, 1, array);
    
}
PFNGLBINDVERTEXARRAYAPPLEPROC glad_debug_glBindVertexArrayAPPLE = glad_debug_impl_glBindVertexArrayAPPLE;
PFNGLBLENDCOLORPROC glad_glBlendColor = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    _pre_call_gl_callback("glBlendColor", (GLADapiproc) glad_glBlendColor, 4, red, green, blue, alpha);
    glad_glBlendColor(red, green, blue, alpha);
    _post_call_gl_callback(NULL, "glBlendColor", (GLADapiproc) glad_glBlendColor, 4, red, green, blue, alpha);
    
}
PFNGLBLENDCOLORPROC glad_debug_glBlendColor = glad_debug_impl_glBlendColor;
PFNGLBLENDCOLOREXTPROC glad_glBlendColorEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendColorEXT(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    _pre_call_gl_callback("glBlendColorEXT", (GLADapiproc) glad_glBlendColorEXT, 4, red, green, blue, alpha);
    glad_glBlendColorEXT(red, green, blue, alpha);
    _post_call_gl_callback(NULL, "glBlendColorEXT", (GLADapiproc) glad_glBlendColorEXT, 4, red, green, blue, alpha);
    
}
PFNGLBLENDCOLOREXTPROC glad_debug_glBlendColorEXT = glad_debug_impl_glBlendColorEXT;
PFNGLBLENDEQUATIONPROC glad_glBlendEquation = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquation(GLenum mode) {
    _pre_call_gl_callback("glBlendEquation", (GLADapiproc) glad_glBlendEquation, 1, mode);
    glad_glBlendEquation(mode);
    _post_call_gl_callback(NULL, "glBlendEquation", (GLADapiproc) glad_glBlendEquation, 1, mode);
    
}
PFNGLBLENDEQUATIONPROC glad_debug_glBlendEquation = glad_debug_impl_glBlendEquation;
PFNGLBLENDEQUATIONEXTPROC glad_glBlendEquationEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationEXT(GLenum mode) {
    _pre_call_gl_callback("glBlendEquationEXT", (GLADapiproc) glad_glBlendEquationEXT, 1, mode);
    glad_glBlendEquationEXT(mode);
    _post_call_gl_callback(NULL, "glBlendEquationEXT", (GLADapiproc) glad_glBlendEquationEXT, 1, mode);
    
}
PFNGLBLENDEQUATIONEXTPROC glad_debug_glBlendEquationEXT = glad_debug_impl_glBlendEquationEXT;
PFNGLBLENDEQUATIONSEPARATEPROC glad_glBlendEquationSeparate = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    _pre_call_gl_callback("glBlendEquationSeparate", (GLADapiproc) glad_glBlendEquationSeparate, 2, modeRGB, modeAlpha);
    glad_glBlendEquationSeparate(modeRGB, modeAlpha);
    _post_call_gl_callback(NULL, "glBlendEquationSeparate", (GLADapiproc) glad_glBlendEquationSeparate, 2, modeRGB, modeAlpha);
    
}
PFNGLBLENDEQUATIONSEPARATEPROC glad_debug_glBlendEquationSeparate = glad_debug_impl_glBlendEquationSeparate;
PFNGLBLENDEQUATIONSEPARATEEXTPROC glad_glBlendEquationSeparateEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationSeparateEXT(GLenum modeRGB, GLenum modeAlpha) {
    _pre_call_gl_callback("glBlendEquationSeparateEXT", (GLADapiproc) glad_glBlendEquationSeparateEXT, 2, modeRGB, modeAlpha);
    glad_glBlendEquationSeparateEXT(modeRGB, modeAlpha);
    _post_call_gl_callback(NULL, "glBlendEquationSeparateEXT", (GLADapiproc) glad_glBlendEquationSeparateEXT, 2, modeRGB, modeAlpha);
    
}
PFNGLBLENDEQUATIONSEPARATEEXTPROC glad_debug_glBlendEquationSeparateEXT = glad_debug_impl_glBlendEquationSeparateEXT;
PFNGLBLENDFUNCPROC glad_glBlendFunc = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFunc(GLenum sfactor, GLenum dfactor) {
    _pre_call_gl_callback("glBlendFunc", (GLADapiproc) glad_glBlendFunc, 2, sfactor, dfactor);
    glad_glBlendFunc(sfactor, dfactor);
    _post_call_gl_callback(NULL, "glBlendFunc", (GLADapiproc) glad_glBlendFunc, 2, sfactor, dfactor);
    
}
PFNGLBLENDFUNCPROC glad_debug_glBlendFunc = glad_debug_impl_glBlendFunc;
PFNGLBLENDFUNCSEPARATEPROC glad_glBlendFuncSeparate = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    _pre_call_gl_callback("glBlendFuncSeparate", (GLADapiproc) glad_glBlendFuncSeparate, 4, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    glad_glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    _post_call_gl_callback(NULL, "glBlendFuncSeparate", (GLADapiproc) glad_glBlendFuncSeparate, 4, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    
}
PFNGLBLENDFUNCSEPARATEPROC glad_debug_glBlendFuncSeparate = glad_debug_impl_glBlendFuncSeparate;
PFNGLBLENDFUNCSEPARATEEXTPROC glad_glBlendFuncSeparateEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFuncSeparateEXT(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    _pre_call_gl_callback("glBlendFuncSeparateEXT", (GLADapiproc) glad_glBlendFuncSeparateEXT, 4, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    glad_glBlendFuncSeparateEXT(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    _post_call_gl_callback(NULL, "glBlendFuncSeparateEXT", (GLADapiproc) glad_glBlendFuncSeparateEXT, 4, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    
}
PFNGLBLENDFUNCSEPARATEEXTPROC glad_debug_glBlendFuncSeparateEXT = glad_debug_impl_glBlendFuncSeparateEXT;
PFNGLBLENDFUNCSEPARATEINGRPROC glad_glBlendFuncSeparateINGR = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFuncSeparateINGR(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    _pre_call_gl_callback("glBlendFuncSeparateINGR", (GLADapiproc) glad_glBlendFuncSeparateINGR, 4, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    glad_glBlendFuncSeparateINGR(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    _post_call_gl_callback(NULL, "glBlendFuncSeparateINGR", (GLADapiproc) glad_glBlendFuncSeparateINGR, 4, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    
}
PFNGLBLENDFUNCSEPARATEINGRPROC glad_debug_glBlendFuncSeparateINGR = glad_debug_impl_glBlendFuncSeparateINGR;
PFNGLBLITFRAMEBUFFERPROC glad_glBlitFramebuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    _pre_call_gl_callback("glBlitFramebuffer", (GLADapiproc) glad_glBlitFramebuffer, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    glad_glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    _post_call_gl_callback(NULL, "glBlitFramebuffer", (GLADapiproc) glad_glBlitFramebuffer, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    
}
PFNGLBLITFRAMEBUFFERPROC glad_debug_glBlitFramebuffer = glad_debug_impl_glBlitFramebuffer;
PFNGLBLITFRAMEBUFFEREXTPROC glad_glBlitFramebufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlitFramebufferEXT(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    _pre_call_gl_callback("glBlitFramebufferEXT", (GLADapiproc) glad_glBlitFramebufferEXT, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    glad_glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    _post_call_gl_callback(NULL, "glBlitFramebufferEXT", (GLADapiproc) glad_glBlitFramebufferEXT, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    
}
PFNGLBLITFRAMEBUFFEREXTPROC glad_debug_glBlitFramebufferEXT = glad_debug_impl_glBlitFramebufferEXT;
PFNGLBUFFERDATAPROC glad_glBufferData = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferData(GLenum target, GLsizeiptr size, const void * data, GLenum usage) {
    _pre_call_gl_callback("glBufferData", (GLADapiproc) glad_glBufferData, 4, target, size, data, usage);
    glad_glBufferData(target, size, data, usage);
    _post_call_gl_callback(NULL, "glBufferData", (GLADapiproc) glad_glBufferData, 4, target, size, data, usage);
    
}
PFNGLBUFFERDATAPROC glad_debug_glBufferData = glad_debug_impl_glBufferData;
PFNGLBUFFERDATAARBPROC glad_glBufferDataARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferDataARB(GLenum target, GLsizeiptrARB size, const void * data, GLenum usage) {
    _pre_call_gl_callback("glBufferDataARB", (GLADapiproc) glad_glBufferDataARB, 4, target, size, data, usage);
    glad_glBufferDataARB(target, size, data, usage);
    _post_call_gl_callback(NULL, "glBufferDataARB", (GLADapiproc) glad_glBufferDataARB, 4, target, size, data, usage);
    
}
PFNGLBUFFERDATAARBPROC glad_debug_glBufferDataARB = glad_debug_impl_glBufferDataARB;
PFNGLBUFFERPARAMETERIAPPLEPROC glad_glBufferParameteriAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferParameteriAPPLE(GLenum target, GLenum pname, GLint param) {
    _pre_call_gl_callback("glBufferParameteriAPPLE", (GLADapiproc) glad_glBufferParameteriAPPLE, 3, target, pname, param);
    glad_glBufferParameteriAPPLE(target, pname, param);
    _post_call_gl_callback(NULL, "glBufferParameteriAPPLE", (GLADapiproc) glad_glBufferParameteriAPPLE, 3, target, pname, param);
    
}
PFNGLBUFFERPARAMETERIAPPLEPROC glad_debug_glBufferParameteriAPPLE = glad_debug_impl_glBufferParameteriAPPLE;
PFNGLBUFFERSUBDATAPROC glad_glBufferSubData = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void * data) {
    _pre_call_gl_callback("glBufferSubData", (GLADapiproc) glad_glBufferSubData, 4, target, offset, size, data);
    glad_glBufferSubData(target, offset, size, data);
    _post_call_gl_callback(NULL, "glBufferSubData", (GLADapiproc) glad_glBufferSubData, 4, target, offset, size, data);
    
}
PFNGLBUFFERSUBDATAPROC glad_debug_glBufferSubData = glad_debug_impl_glBufferSubData;
PFNGLBUFFERSUBDATAARBPROC glad_glBufferSubDataARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const void * data) {
    _pre_call_gl_callback("glBufferSubDataARB", (GLADapiproc) glad_glBufferSubDataARB, 4, target, offset, size, data);
    glad_glBufferSubDataARB(target, offset, size, data);
    _post_call_gl_callback(NULL, "glBufferSubDataARB", (GLADapiproc) glad_glBufferSubDataARB, 4, target, offset, size, data);
    
}
PFNGLBUFFERSUBDATAARBPROC glad_debug_glBufferSubDataARB = glad_debug_impl_glBufferSubDataARB;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glCheckFramebufferStatus(GLenum target) {
    GLenum ret;
    _pre_call_gl_callback("glCheckFramebufferStatus", (GLADapiproc) glad_glCheckFramebufferStatus, 1, target);
    ret = glad_glCheckFramebufferStatus(target);
    _post_call_gl_callback((void*) &ret, "glCheckFramebufferStatus", (GLADapiproc) glad_glCheckFramebufferStatus, 1, target);
    return ret;
}
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_debug_glCheckFramebufferStatus = glad_debug_impl_glCheckFramebufferStatus;
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glad_glCheckFramebufferStatusEXT = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glCheckFramebufferStatusEXT(GLenum target) {
    GLenum ret;
    _pre_call_gl_callback("glCheckFramebufferStatusEXT", (GLADapiproc) glad_glCheckFramebufferStatusEXT, 1, target);
    ret = glad_glCheckFramebufferStatusEXT(target);
    _post_call_gl_callback((void*) &ret, "glCheckFramebufferStatusEXT", (GLADapiproc) glad_glCheckFramebufferStatusEXT, 1, target);
    return ret;
}
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glad_debug_glCheckFramebufferStatusEXT = glad_debug_impl_glCheckFramebufferStatusEXT;
PFNGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC glad_glCheckNamedFramebufferStatusEXT = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glCheckNamedFramebufferStatusEXT(GLuint framebuffer, GLenum target) {
    GLenum ret;
    _pre_call_gl_callback("glCheckNamedFramebufferStatusEXT", (GLADapiproc) glad_glCheckNamedFramebufferStatusEXT, 2, framebuffer, target);
    ret = glad_glCheckNamedFramebufferStatusEXT(framebuffer, target);
    _post_call_gl_callback((void*) &ret, "glCheckNamedFramebufferStatusEXT", (GLADapiproc) glad_glCheckNamedFramebufferStatusEXT, 2, framebuffer, target);
    return ret;
}
PFNGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC glad_debug_glCheckNamedFramebufferStatusEXT = glad_debug_impl_glCheckNamedFramebufferStatusEXT;
PFNGLCLAMPCOLORPROC glad_glClampColor = NULL;
static void GLAD_API_PTR glad_debug_impl_glClampColor(GLenum target, GLenum clamp) {
    _pre_call_gl_callback("glClampColor", (GLADapiproc) glad_glClampColor, 2, target, clamp);
    glad_glClampColor(target, clamp);
    _post_call_gl_callback(NULL, "glClampColor", (GLADapiproc) glad_glClampColor, 2, target, clamp);
    
}
PFNGLCLAMPCOLORPROC glad_debug_glClampColor = glad_debug_impl_glClampColor;
PFNGLCLAMPCOLORARBPROC glad_glClampColorARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glClampColorARB(GLenum target, GLenum clamp) {
    _pre_call_gl_callback("glClampColorARB", (GLADapiproc) glad_glClampColorARB, 2, target, clamp);
    glad_glClampColorARB(target, clamp);
    _post_call_gl_callback(NULL, "glClampColorARB", (GLADapiproc) glad_glClampColorARB, 2, target, clamp);
    
}
PFNGLCLAMPCOLORARBPROC glad_debug_glClampColorARB = glad_debug_impl_glClampColorARB;
PFNGLCLEARPROC glad_glClear = NULL;
static void GLAD_API_PTR glad_debug_impl_glClear(GLbitfield mask) {
    _pre_call_gl_callback("glClear", (GLADapiproc) glad_glClear, 1, mask);
    glad_glClear(mask);
    _post_call_gl_callback(NULL, "glClear", (GLADapiproc) glad_glClear, 1, mask);
    
}
PFNGLCLEARPROC glad_debug_glClear = glad_debug_impl_glClear;
PFNGLCLEARBUFFERFIPROC glad_glClearBufferfi = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    _pre_call_gl_callback("glClearBufferfi", (GLADapiproc) glad_glClearBufferfi, 4, buffer, drawbuffer, depth, stencil);
    glad_glClearBufferfi(buffer, drawbuffer, depth, stencil);
    _post_call_gl_callback(NULL, "glClearBufferfi", (GLADapiproc) glad_glClearBufferfi, 4, buffer, drawbuffer, depth, stencil);
    
}
PFNGLCLEARBUFFERFIPROC glad_debug_glClearBufferfi = glad_debug_impl_glClearBufferfi;
PFNGLCLEARBUFFERFVPROC glad_glClearBufferfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat * value) {
    _pre_call_gl_callback("glClearBufferfv", (GLADapiproc) glad_glClearBufferfv, 3, buffer, drawbuffer, value);
    glad_glClearBufferfv(buffer, drawbuffer, value);
    _post_call_gl_callback(NULL, "glClearBufferfv", (GLADapiproc) glad_glClearBufferfv, 3, buffer, drawbuffer, value);
    
}
PFNGLCLEARBUFFERFVPROC glad_debug_glClearBufferfv = glad_debug_impl_glClearBufferfv;
PFNGLCLEARBUFFERIVPROC glad_glClearBufferiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint * value) {
    _pre_call_gl_callback("glClearBufferiv", (GLADapiproc) glad_glClearBufferiv, 3, buffer, drawbuffer, value);
    glad_glClearBufferiv(buffer, drawbuffer, value);
    _post_call_gl_callback(NULL, "glClearBufferiv", (GLADapiproc) glad_glClearBufferiv, 3, buffer, drawbuffer, value);
    
}
PFNGLCLEARBUFFERIVPROC glad_debug_glClearBufferiv = glad_debug_impl_glClearBufferiv;
PFNGLCLEARBUFFERUIVPROC glad_glClearBufferuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint * value) {
    _pre_call_gl_callback("glClearBufferuiv", (GLADapiproc) glad_glClearBufferuiv, 3, buffer, drawbuffer, value);
    glad_glClearBufferuiv(buffer, drawbuffer, value);
    _post_call_gl_callback(NULL, "glClearBufferuiv", (GLADapiproc) glad_glClearBufferuiv, 3, buffer, drawbuffer, value);
    
}
PFNGLCLEARBUFFERUIVPROC glad_debug_glClearBufferuiv = glad_debug_impl_glClearBufferuiv;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    _pre_call_gl_callback("glClearColor", (GLADapiproc) glad_glClearColor, 4, red, green, blue, alpha);
    glad_glClearColor(red, green, blue, alpha);
    _post_call_gl_callback(NULL, "glClearColor", (GLADapiproc) glad_glClearColor, 4, red, green, blue, alpha);
    
}
PFNGLCLEARCOLORPROC glad_debug_glClearColor = glad_debug_impl_glClearColor;
PFNGLCLEARCOLORIIEXTPROC glad_glClearColorIiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearColorIiEXT(GLint red, GLint green, GLint blue, GLint alpha) {
    _pre_call_gl_callback("glClearColorIiEXT", (GLADapiproc) glad_glClearColorIiEXT, 4, red, green, blue, alpha);
    glad_glClearColorIiEXT(red, green, blue, alpha);
    _post_call_gl_callback(NULL, "glClearColorIiEXT", (GLADapiproc) glad_glClearColorIiEXT, 4, red, green, blue, alpha);
    
}
PFNGLCLEARCOLORIIEXTPROC glad_debug_glClearColorIiEXT = glad_debug_impl_glClearColorIiEXT;
PFNGLCLEARCOLORIUIEXTPROC glad_glClearColorIuiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearColorIuiEXT(GLuint red, GLuint green, GLuint blue, GLuint alpha) {
    _pre_call_gl_callback("glClearColorIuiEXT", (GLADapiproc) glad_glClearColorIuiEXT, 4, red, green, blue, alpha);
    glad_glClearColorIuiEXT(red, green, blue, alpha);
    _post_call_gl_callback(NULL, "glClearColorIuiEXT", (GLADapiproc) glad_glClearColorIuiEXT, 4, red, green, blue, alpha);
    
}
PFNGLCLEARCOLORIUIEXTPROC glad_debug_glClearColorIuiEXT = glad_debug_impl_glClearColorIuiEXT;
PFNGLCLEARDEPTHPROC glad_glClearDepth = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearDepth(GLdouble depth) {
    _pre_call_gl_callback("glClearDepth", (GLADapiproc) glad_glClearDepth, 1, depth);
    glad_glClearDepth(depth);
    _post_call_gl_callback(NULL, "glClearDepth", (GLADapiproc) glad_glClearDepth, 1, depth);
    
}
PFNGLCLEARDEPTHPROC glad_debug_glClearDepth = glad_debug_impl_glClearDepth;
PFNGLCLEARNAMEDBUFFERDATAEXTPROC glad_glClearNamedBufferDataEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearNamedBufferDataEXT(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void * data) {
    _pre_call_gl_callback("glClearNamedBufferDataEXT", (GLADapiproc) glad_glClearNamedBufferDataEXT, 5, buffer, internalformat, format, type, data);
    glad_glClearNamedBufferDataEXT(buffer, internalformat, format, type, data);
    _post_call_gl_callback(NULL, "glClearNamedBufferDataEXT", (GLADapiproc) glad_glClearNamedBufferDataEXT, 5, buffer, internalformat, format, type, data);
    
}
PFNGLCLEARNAMEDBUFFERDATAEXTPROC glad_debug_glClearNamedBufferDataEXT = glad_debug_impl_glClearNamedBufferDataEXT;
PFNGLCLEARNAMEDBUFFERSUBDATAEXTPROC glad_glClearNamedBufferSubDataEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearNamedBufferSubDataEXT(GLuint buffer, GLenum internalformat, GLsizeiptr offset, GLsizeiptr size, GLenum format, GLenum type, const void * data) {
    _pre_call_gl_callback("glClearNamedBufferSubDataEXT", (GLADapiproc) glad_glClearNamedBufferSubDataEXT, 7, buffer, internalformat, offset, size, format, type, data);
    glad_glClearNamedBufferSubDataEXT(buffer, internalformat, offset, size, format, type, data);
    _post_call_gl_callback(NULL, "glClearNamedBufferSubDataEXT", (GLADapiproc) glad_glClearNamedBufferSubDataEXT, 7, buffer, internalformat, offset, size, format, type, data);
    
}
PFNGLCLEARNAMEDBUFFERSUBDATAEXTPROC glad_debug_glClearNamedBufferSubDataEXT = glad_debug_impl_glClearNamedBufferSubDataEXT;
PFNGLCLEARSTENCILPROC glad_glClearStencil = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearStencil(GLint s) {
    _pre_call_gl_callback("glClearStencil", (GLADapiproc) glad_glClearStencil, 1, s);
    glad_glClearStencil(s);
    _post_call_gl_callback(NULL, "glClearStencil", (GLADapiproc) glad_glClearStencil, 1, s);
    
}
PFNGLCLEARSTENCILPROC glad_debug_glClearStencil = glad_debug_impl_glClearStencil;
PFNGLCLIENTACTIVETEXTUREARBPROC glad_glClientActiveTextureARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glClientActiveTextureARB(GLenum texture) {
    _pre_call_gl_callback("glClientActiveTextureARB", (GLADapiproc) glad_glClientActiveTextureARB, 1, texture);
    glad_glClientActiveTextureARB(texture);
    _post_call_gl_callback(NULL, "glClientActiveTextureARB", (GLADapiproc) glad_glClientActiveTextureARB, 1, texture);
    
}
PFNGLCLIENTACTIVETEXTUREARBPROC glad_debug_glClientActiveTextureARB = glad_debug_impl_glClientActiveTextureARB;
PFNGLCLIENTATTRIBDEFAULTEXTPROC glad_glClientAttribDefaultEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glClientAttribDefaultEXT(GLbitfield mask) {
    _pre_call_gl_callback("glClientAttribDefaultEXT", (GLADapiproc) glad_glClientAttribDefaultEXT, 1, mask);
    glad_glClientAttribDefaultEXT(mask);
    _post_call_gl_callback(NULL, "glClientAttribDefaultEXT", (GLADapiproc) glad_glClientAttribDefaultEXT, 1, mask);
    
}
PFNGLCLIENTATTRIBDEFAULTEXTPROC glad_debug_glClientAttribDefaultEXT = glad_debug_impl_glClientAttribDefaultEXT;
PFNGLCLIENTWAITSYNCPROC glad_glClientWaitSync = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    GLenum ret;
    _pre_call_gl_callback("glClientWaitSync", (GLADapiproc) glad_glClientWaitSync, 3, sync, flags, timeout);
    ret = glad_glClientWaitSync(sync, flags, timeout);
    _post_call_gl_callback((void*) &ret, "glClientWaitSync", (GLADapiproc) glad_glClientWaitSync, 3, sync, flags, timeout);
    return ret;
}
PFNGLCLIENTWAITSYNCPROC glad_debug_glClientWaitSync = glad_debug_impl_glClientWaitSync;
PFNGLCOLORMASKPROC glad_glColorMask = NULL;
static void GLAD_API_PTR glad_debug_impl_glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    _pre_call_gl_callback("glColorMask", (GLADapiproc) glad_glColorMask, 4, red, green, blue, alpha);
    glad_glColorMask(red, green, blue, alpha);
    _post_call_gl_callback(NULL, "glColorMask", (GLADapiproc) glad_glColorMask, 4, red, green, blue, alpha);
    
}
PFNGLCOLORMASKPROC glad_debug_glColorMask = glad_debug_impl_glColorMask;
PFNGLCOLORMASKINDEXEDEXTPROC glad_glColorMaskIndexedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glColorMaskIndexedEXT(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    _pre_call_gl_callback("glColorMaskIndexedEXT", (GLADapiproc) glad_glColorMaskIndexedEXT, 5, index, r, g, b, a);
    glad_glColorMaskIndexedEXT(index, r, g, b, a);
    _post_call_gl_callback(NULL, "glColorMaskIndexedEXT", (GLADapiproc) glad_glColorMaskIndexedEXT, 5, index, r, g, b, a);
    
}
PFNGLCOLORMASKINDEXEDEXTPROC glad_debug_glColorMaskIndexedEXT = glad_debug_impl_glColorMaskIndexedEXT;
PFNGLCOLORMASKIPROC glad_glColorMaski = NULL;
static void GLAD_API_PTR glad_debug_impl_glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    _pre_call_gl_callback("glColorMaski", (GLADapiproc) glad_glColorMaski, 5, index, r, g, b, a);
    glad_glColorMaski(index, r, g, b, a);
    _post_call_gl_callback(NULL, "glColorMaski", (GLADapiproc) glad_glColorMaski, 5, index, r, g, b, a);
    
}
PFNGLCOLORMASKIPROC glad_debug_glColorMaski = glad_debug_impl_glColorMaski;
PFNGLCOLORPOINTEREXTPROC glad_glColorPointerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glColorPointerEXT(GLint size, GLenum type, GLsizei stride, GLsizei count, const void * pointer) {
    _pre_call_gl_callback("glColorPointerEXT", (GLADapiproc) glad_glColorPointerEXT, 5, size, type, stride, count, pointer);
    glad_glColorPointerEXT(size, type, stride, count, pointer);
    _post_call_gl_callback(NULL, "glColorPointerEXT", (GLADapiproc) glad_glColorPointerEXT, 5, size, type, stride, count, pointer);
    
}
PFNGLCOLORPOINTEREXTPROC glad_debug_glColorPointerEXT = glad_debug_impl_glColorPointerEXT;
PFNGLCOMPILESHADERPROC glad_glCompileShader = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompileShader(GLuint shader) {
    _pre_call_gl_callback("glCompileShader", (GLADapiproc) glad_glCompileShader, 1, shader);
    glad_glCompileShader(shader);
    _post_call_gl_callback(NULL, "glCompileShader", (GLADapiproc) glad_glCompileShader, 1, shader);
    
}
PFNGLCOMPILESHADERPROC glad_debug_glCompileShader = glad_debug_impl_glCompileShader;
PFNGLCOMPILESHADERARBPROC glad_glCompileShaderARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompileShaderARB(GLhandleARB shaderObj) {
    _pre_call_gl_callback("glCompileShaderARB", (GLADapiproc) glad_glCompileShaderARB, 1, shaderObj);
    glad_glCompileShaderARB(shaderObj);
    _post_call_gl_callback(NULL, "glCompileShaderARB", (GLADapiproc) glad_glCompileShaderARB, 1, shaderObj);
    
}
PFNGLCOMPILESHADERARBPROC glad_debug_glCompileShaderARB = glad_debug_impl_glCompileShaderARB;
PFNGLCOMPRESSEDMULTITEXIMAGE1DEXTPROC glad_glCompressedMultiTexImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedMultiTexImage1DEXT", (GLADapiproc) glad_glCompressedMultiTexImage1DEXT, 8, texunit, target, level, internalformat, width, border, imageSize, bits);
    glad_glCompressedMultiTexImage1DEXT(texunit, target, level, internalformat, width, border, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedMultiTexImage1DEXT", (GLADapiproc) glad_glCompressedMultiTexImage1DEXT, 8, texunit, target, level, internalformat, width, border, imageSize, bits);
    
}
PFNGLCOMPRESSEDMULTITEXIMAGE1DEXTPROC glad_debug_glCompressedMultiTexImage1DEXT = glad_debug_impl_glCompressedMultiTexImage1DEXT;
PFNGLCOMPRESSEDMULTITEXIMAGE2DEXTPROC glad_glCompressedMultiTexImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedMultiTexImage2DEXT", (GLADapiproc) glad_glCompressedMultiTexImage2DEXT, 9, texunit, target, level, internalformat, width, height, border, imageSize, bits);
    glad_glCompressedMultiTexImage2DEXT(texunit, target, level, internalformat, width, height, border, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedMultiTexImage2DEXT", (GLADapiproc) glad_glCompressedMultiTexImage2DEXT, 9, texunit, target, level, internalformat, width, height, border, imageSize, bits);
    
}
PFNGLCOMPRESSEDMULTITEXIMAGE2DEXTPROC glad_debug_glCompressedMultiTexImage2DEXT = glad_debug_impl_glCompressedMultiTexImage2DEXT;
PFNGLCOMPRESSEDMULTITEXIMAGE3DEXTPROC glad_glCompressedMultiTexImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedMultiTexImage3DEXT", (GLADapiproc) glad_glCompressedMultiTexImage3DEXT, 10, texunit, target, level, internalformat, width, height, depth, border, imageSize, bits);
    glad_glCompressedMultiTexImage3DEXT(texunit, target, level, internalformat, width, height, depth, border, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedMultiTexImage3DEXT", (GLADapiproc) glad_glCompressedMultiTexImage3DEXT, 10, texunit, target, level, internalformat, width, height, depth, border, imageSize, bits);
    
}
PFNGLCOMPRESSEDMULTITEXIMAGE3DEXTPROC glad_debug_glCompressedMultiTexImage3DEXT = glad_debug_impl_glCompressedMultiTexImage3DEXT;
PFNGLCOMPRESSEDMULTITEXSUBIMAGE1DEXTPROC glad_glCompressedMultiTexSubImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedMultiTexSubImage1DEXT", (GLADapiproc) glad_glCompressedMultiTexSubImage1DEXT, 8, texunit, target, level, xoffset, width, format, imageSize, bits);
    glad_glCompressedMultiTexSubImage1DEXT(texunit, target, level, xoffset, width, format, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedMultiTexSubImage1DEXT", (GLADapiproc) glad_glCompressedMultiTexSubImage1DEXT, 8, texunit, target, level, xoffset, width, format, imageSize, bits);
    
}
PFNGLCOMPRESSEDMULTITEXSUBIMAGE1DEXTPROC glad_debug_glCompressedMultiTexSubImage1DEXT = glad_debug_impl_glCompressedMultiTexSubImage1DEXT;
PFNGLCOMPRESSEDMULTITEXSUBIMAGE2DEXTPROC glad_glCompressedMultiTexSubImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedMultiTexSubImage2DEXT", (GLADapiproc) glad_glCompressedMultiTexSubImage2DEXT, 10, texunit, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
    glad_glCompressedMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedMultiTexSubImage2DEXT", (GLADapiproc) glad_glCompressedMultiTexSubImage2DEXT, 10, texunit, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
    
}
PFNGLCOMPRESSEDMULTITEXSUBIMAGE2DEXTPROC glad_debug_glCompressedMultiTexSubImage2DEXT = glad_debug_impl_glCompressedMultiTexSubImage2DEXT;
PFNGLCOMPRESSEDMULTITEXSUBIMAGE3DEXTPROC glad_glCompressedMultiTexSubImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedMultiTexSubImage3DEXT", (GLADapiproc) glad_glCompressedMultiTexSubImage3DEXT, 12, texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
    glad_glCompressedMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedMultiTexSubImage3DEXT", (GLADapiproc) glad_glCompressedMultiTexSubImage3DEXT, 12, texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
    
}
PFNGLCOMPRESSEDMULTITEXSUBIMAGE3DEXTPROC glad_debug_glCompressedMultiTexSubImage3DEXT = glad_debug_impl_glCompressedMultiTexSubImage3DEXT;
PFNGLCOMPRESSEDTEXIMAGE1DPROC glad_glCompressedTexImage1D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexImage1D", (GLADapiproc) glad_glCompressedTexImage1D, 7, target, level, internalformat, width, border, imageSize, data);
    glad_glCompressedTexImage1D(target, level, internalformat, width, border, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexImage1D", (GLADapiproc) glad_glCompressedTexImage1D, 7, target, level, internalformat, width, border, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXIMAGE1DPROC glad_debug_glCompressedTexImage1D = glad_debug_impl_glCompressedTexImage1D;
PFNGLCOMPRESSEDTEXIMAGE1DARBPROC glad_glCompressedTexImage1DARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexImage1DARB(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexImage1DARB", (GLADapiproc) glad_glCompressedTexImage1DARB, 7, target, level, internalformat, width, border, imageSize, data);
    glad_glCompressedTexImage1DARB(target, level, internalformat, width, border, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexImage1DARB", (GLADapiproc) glad_glCompressedTexImage1DARB, 7, target, level, internalformat, width, border, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXIMAGE1DARBPROC glad_debug_glCompressedTexImage1DARB = glad_debug_impl_glCompressedTexImage1DARB;
PFNGLCOMPRESSEDTEXIMAGE2DPROC glad_glCompressedTexImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexImage2D", (GLADapiproc) glad_glCompressedTexImage2D, 8, target, level, internalformat, width, height, border, imageSize, data);
    glad_glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexImage2D", (GLADapiproc) glad_glCompressedTexImage2D, 8, target, level, internalformat, width, height, border, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXIMAGE2DPROC glad_debug_glCompressedTexImage2D = glad_debug_impl_glCompressedTexImage2D;
PFNGLCOMPRESSEDTEXIMAGE2DARBPROC glad_glCompressedTexImage2DARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexImage2DARB(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexImage2DARB", (GLADapiproc) glad_glCompressedTexImage2DARB, 8, target, level, internalformat, width, height, border, imageSize, data);
    glad_glCompressedTexImage2DARB(target, level, internalformat, width, height, border, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexImage2DARB", (GLADapiproc) glad_glCompressedTexImage2DARB, 8, target, level, internalformat, width, height, border, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXIMAGE2DARBPROC glad_debug_glCompressedTexImage2DARB = glad_debug_impl_glCompressedTexImage2DARB;
PFNGLCOMPRESSEDTEXIMAGE3DPROC glad_glCompressedTexImage3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexImage3D", (GLADapiproc) glad_glCompressedTexImage3D, 9, target, level, internalformat, width, height, depth, border, imageSize, data);
    glad_glCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexImage3D", (GLADapiproc) glad_glCompressedTexImage3D, 9, target, level, internalformat, width, height, depth, border, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXIMAGE3DPROC glad_debug_glCompressedTexImage3D = glad_debug_impl_glCompressedTexImage3D;
PFNGLCOMPRESSEDTEXIMAGE3DARBPROC glad_glCompressedTexImage3DARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexImage3DARB(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexImage3DARB", (GLADapiproc) glad_glCompressedTexImage3DARB, 9, target, level, internalformat, width, height, depth, border, imageSize, data);
    glad_glCompressedTexImage3DARB(target, level, internalformat, width, height, depth, border, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexImage3DARB", (GLADapiproc) glad_glCompressedTexImage3DARB, 9, target, level, internalformat, width, height, depth, border, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXIMAGE3DARBPROC glad_debug_glCompressedTexImage3DARB = glad_debug_impl_glCompressedTexImage3DARB;
PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glad_glCompressedTexSubImage1D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexSubImage1D", (GLADapiproc) glad_glCompressedTexSubImage1D, 7, target, level, xoffset, width, format, imageSize, data);
    glad_glCompressedTexSubImage1D(target, level, xoffset, width, format, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexSubImage1D", (GLADapiproc) glad_glCompressedTexSubImage1D, 7, target, level, xoffset, width, format, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glad_debug_glCompressedTexSubImage1D = glad_debug_impl_glCompressedTexSubImage1D;
PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC glad_glCompressedTexSubImage1DARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexSubImage1DARB(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexSubImage1DARB", (GLADapiproc) glad_glCompressedTexSubImage1DARB, 7, target, level, xoffset, width, format, imageSize, data);
    glad_glCompressedTexSubImage1DARB(target, level, xoffset, width, format, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexSubImage1DARB", (GLADapiproc) glad_glCompressedTexSubImage1DARB, 7, target, level, xoffset, width, format, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC glad_debug_glCompressedTexSubImage1DARB = glad_debug_impl_glCompressedTexSubImage1DARB;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glad_glCompressedTexSubImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexSubImage2D", (GLADapiproc) glad_glCompressedTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, imageSize, data);
    glad_glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexSubImage2D", (GLADapiproc) glad_glCompressedTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glad_debug_glCompressedTexSubImage2D = glad_debug_impl_glCompressedTexSubImage2D;
PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC glad_glCompressedTexSubImage2DARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexSubImage2DARB(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexSubImage2DARB", (GLADapiproc) glad_glCompressedTexSubImage2DARB, 9, target, level, xoffset, yoffset, width, height, format, imageSize, data);
    glad_glCompressedTexSubImage2DARB(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexSubImage2DARB", (GLADapiproc) glad_glCompressedTexSubImage2DARB, 9, target, level, xoffset, yoffset, width, height, format, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC glad_debug_glCompressedTexSubImage2DARB = glad_debug_impl_glCompressedTexSubImage2DARB;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glad_glCompressedTexSubImage3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexSubImage3D", (GLADapiproc) glad_glCompressedTexSubImage3D, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    glad_glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexSubImage3D", (GLADapiproc) glad_glCompressedTexSubImage3D, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glad_debug_glCompressedTexSubImage3D = glad_debug_impl_glCompressedTexSubImage3D;
PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC glad_glCompressedTexSubImage3DARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexSubImage3DARB(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void * data) {
    _pre_call_gl_callback("glCompressedTexSubImage3DARB", (GLADapiproc) glad_glCompressedTexSubImage3DARB, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    glad_glCompressedTexSubImage3DARB(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    _post_call_gl_callback(NULL, "glCompressedTexSubImage3DARB", (GLADapiproc) glad_glCompressedTexSubImage3DARB, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC glad_debug_glCompressedTexSubImage3DARB = glad_debug_impl_glCompressedTexSubImage3DARB;
PFNGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC glad_glCompressedTextureImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedTextureImage1DEXT", (GLADapiproc) glad_glCompressedTextureImage1DEXT, 8, texture, target, level, internalformat, width, border, imageSize, bits);
    glad_glCompressedTextureImage1DEXT(texture, target, level, internalformat, width, border, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedTextureImage1DEXT", (GLADapiproc) glad_glCompressedTextureImage1DEXT, 8, texture, target, level, internalformat, width, border, imageSize, bits);
    
}
PFNGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC glad_debug_glCompressedTextureImage1DEXT = glad_debug_impl_glCompressedTextureImage1DEXT;
PFNGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC glad_glCompressedTextureImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedTextureImage2DEXT", (GLADapiproc) glad_glCompressedTextureImage2DEXT, 9, texture, target, level, internalformat, width, height, border, imageSize, bits);
    glad_glCompressedTextureImage2DEXT(texture, target, level, internalformat, width, height, border, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedTextureImage2DEXT", (GLADapiproc) glad_glCompressedTextureImage2DEXT, 9, texture, target, level, internalformat, width, height, border, imageSize, bits);
    
}
PFNGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC glad_debug_glCompressedTextureImage2DEXT = glad_debug_impl_glCompressedTextureImage2DEXT;
PFNGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC glad_glCompressedTextureImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedTextureImage3DEXT", (GLADapiproc) glad_glCompressedTextureImage3DEXT, 10, texture, target, level, internalformat, width, height, depth, border, imageSize, bits);
    glad_glCompressedTextureImage3DEXT(texture, target, level, internalformat, width, height, depth, border, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedTextureImage3DEXT", (GLADapiproc) glad_glCompressedTextureImage3DEXT, 10, texture, target, level, internalformat, width, height, depth, border, imageSize, bits);
    
}
PFNGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC glad_debug_glCompressedTextureImage3DEXT = glad_debug_impl_glCompressedTextureImage3DEXT;
PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC glad_glCompressedTextureSubImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedTextureSubImage1DEXT", (GLADapiproc) glad_glCompressedTextureSubImage1DEXT, 8, texture, target, level, xoffset, width, format, imageSize, bits);
    glad_glCompressedTextureSubImage1DEXT(texture, target, level, xoffset, width, format, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedTextureSubImage1DEXT", (GLADapiproc) glad_glCompressedTextureSubImage1DEXT, 8, texture, target, level, xoffset, width, format, imageSize, bits);
    
}
PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC glad_debug_glCompressedTextureSubImage1DEXT = glad_debug_impl_glCompressedTextureSubImage1DEXT;
PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC glad_glCompressedTextureSubImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedTextureSubImage2DEXT", (GLADapiproc) glad_glCompressedTextureSubImage2DEXT, 10, texture, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
    glad_glCompressedTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedTextureSubImage2DEXT", (GLADapiproc) glad_glCompressedTextureSubImage2DEXT, 10, texture, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
    
}
PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC glad_debug_glCompressedTextureSubImage2DEXT = glad_debug_impl_glCompressedTextureSubImage2DEXT;
PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC glad_glCompressedTextureSubImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void * bits) {
    _pre_call_gl_callback("glCompressedTextureSubImage3DEXT", (GLADapiproc) glad_glCompressedTextureSubImage3DEXT, 12, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
    glad_glCompressedTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
    _post_call_gl_callback(NULL, "glCompressedTextureSubImage3DEXT", (GLADapiproc) glad_glCompressedTextureSubImage3DEXT, 12, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
    
}
PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC glad_debug_glCompressedTextureSubImage3DEXT = glad_debug_impl_glCompressedTextureSubImage3DEXT;
PFNGLCOPYBUFFERSUBDATAPROC glad_glCopyBufferSubData = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    _pre_call_gl_callback("glCopyBufferSubData", (GLADapiproc) glad_glCopyBufferSubData, 5, readTarget, writeTarget, readOffset, writeOffset, size);
    glad_glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
    _post_call_gl_callback(NULL, "glCopyBufferSubData", (GLADapiproc) glad_glCopyBufferSubData, 5, readTarget, writeTarget, readOffset, writeOffset, size);
    
}
PFNGLCOPYBUFFERSUBDATAPROC glad_debug_glCopyBufferSubData = glad_debug_impl_glCopyBufferSubData;
PFNGLCOPYMULTITEXIMAGE1DEXTPROC glad_glCopyMultiTexImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    _pre_call_gl_callback("glCopyMultiTexImage1DEXT", (GLADapiproc) glad_glCopyMultiTexImage1DEXT, 8, texunit, target, level, internalformat, x, y, width, border);
    glad_glCopyMultiTexImage1DEXT(texunit, target, level, internalformat, x, y, width, border);
    _post_call_gl_callback(NULL, "glCopyMultiTexImage1DEXT", (GLADapiproc) glad_glCopyMultiTexImage1DEXT, 8, texunit, target, level, internalformat, x, y, width, border);
    
}
PFNGLCOPYMULTITEXIMAGE1DEXTPROC glad_debug_glCopyMultiTexImage1DEXT = glad_debug_impl_glCopyMultiTexImage1DEXT;
PFNGLCOPYMULTITEXIMAGE2DEXTPROC glad_glCopyMultiTexImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    _pre_call_gl_callback("glCopyMultiTexImage2DEXT", (GLADapiproc) glad_glCopyMultiTexImage2DEXT, 9, texunit, target, level, internalformat, x, y, width, height, border);
    glad_glCopyMultiTexImage2DEXT(texunit, target, level, internalformat, x, y, width, height, border);
    _post_call_gl_callback(NULL, "glCopyMultiTexImage2DEXT", (GLADapiproc) glad_glCopyMultiTexImage2DEXT, 9, texunit, target, level, internalformat, x, y, width, height, border);
    
}
PFNGLCOPYMULTITEXIMAGE2DEXTPROC glad_debug_glCopyMultiTexImage2DEXT = glad_debug_impl_glCopyMultiTexImage2DEXT;
PFNGLCOPYMULTITEXSUBIMAGE1DEXTPROC glad_glCopyMultiTexSubImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    _pre_call_gl_callback("glCopyMultiTexSubImage1DEXT", (GLADapiproc) glad_glCopyMultiTexSubImage1DEXT, 7, texunit, target, level, xoffset, x, y, width);
    glad_glCopyMultiTexSubImage1DEXT(texunit, target, level, xoffset, x, y, width);
    _post_call_gl_callback(NULL, "glCopyMultiTexSubImage1DEXT", (GLADapiproc) glad_glCopyMultiTexSubImage1DEXT, 7, texunit, target, level, xoffset, x, y, width);
    
}
PFNGLCOPYMULTITEXSUBIMAGE1DEXTPROC glad_debug_glCopyMultiTexSubImage1DEXT = glad_debug_impl_glCopyMultiTexSubImage1DEXT;
PFNGLCOPYMULTITEXSUBIMAGE2DEXTPROC glad_glCopyMultiTexSubImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glCopyMultiTexSubImage2DEXT", (GLADapiproc) glad_glCopyMultiTexSubImage2DEXT, 9, texunit, target, level, xoffset, yoffset, x, y, width, height);
    glad_glCopyMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, x, y, width, height);
    _post_call_gl_callback(NULL, "glCopyMultiTexSubImage2DEXT", (GLADapiproc) glad_glCopyMultiTexSubImage2DEXT, 9, texunit, target, level, xoffset, yoffset, x, y, width, height);
    
}
PFNGLCOPYMULTITEXSUBIMAGE2DEXTPROC glad_debug_glCopyMultiTexSubImage2DEXT = glad_debug_impl_glCopyMultiTexSubImage2DEXT;
PFNGLCOPYMULTITEXSUBIMAGE3DEXTPROC glad_glCopyMultiTexSubImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glCopyMultiTexSubImage3DEXT", (GLADapiproc) glad_glCopyMultiTexSubImage3DEXT, 10, texunit, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    glad_glCopyMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    _post_call_gl_callback(NULL, "glCopyMultiTexSubImage3DEXT", (GLADapiproc) glad_glCopyMultiTexSubImage3DEXT, 10, texunit, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    
}
PFNGLCOPYMULTITEXSUBIMAGE3DEXTPROC glad_debug_glCopyMultiTexSubImage3DEXT = glad_debug_impl_glCopyMultiTexSubImage3DEXT;
PFNGLCOPYTEXIMAGE1DPROC glad_glCopyTexImage1D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    _pre_call_gl_callback("glCopyTexImage1D", (GLADapiproc) glad_glCopyTexImage1D, 7, target, level, internalformat, x, y, width, border);
    glad_glCopyTexImage1D(target, level, internalformat, x, y, width, border);
    _post_call_gl_callback(NULL, "glCopyTexImage1D", (GLADapiproc) glad_glCopyTexImage1D, 7, target, level, internalformat, x, y, width, border);
    
}
PFNGLCOPYTEXIMAGE1DPROC glad_debug_glCopyTexImage1D = glad_debug_impl_glCopyTexImage1D;
PFNGLCOPYTEXIMAGE1DEXTPROC glad_glCopyTexImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexImage1DEXT(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    _pre_call_gl_callback("glCopyTexImage1DEXT", (GLADapiproc) glad_glCopyTexImage1DEXT, 7, target, level, internalformat, x, y, width, border);
    glad_glCopyTexImage1DEXT(target, level, internalformat, x, y, width, border);
    _post_call_gl_callback(NULL, "glCopyTexImage1DEXT", (GLADapiproc) glad_glCopyTexImage1DEXT, 7, target, level, internalformat, x, y, width, border);
    
}
PFNGLCOPYTEXIMAGE1DEXTPROC glad_debug_glCopyTexImage1DEXT = glad_debug_impl_glCopyTexImage1DEXT;
PFNGLCOPYTEXIMAGE2DPROC glad_glCopyTexImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    _pre_call_gl_callback("glCopyTexImage2D", (GLADapiproc) glad_glCopyTexImage2D, 8, target, level, internalformat, x, y, width, height, border);
    glad_glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    _post_call_gl_callback(NULL, "glCopyTexImage2D", (GLADapiproc) glad_glCopyTexImage2D, 8, target, level, internalformat, x, y, width, height, border);
    
}
PFNGLCOPYTEXIMAGE2DPROC glad_debug_glCopyTexImage2D = glad_debug_impl_glCopyTexImage2D;
PFNGLCOPYTEXIMAGE2DEXTPROC glad_glCopyTexImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexImage2DEXT(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    _pre_call_gl_callback("glCopyTexImage2DEXT", (GLADapiproc) glad_glCopyTexImage2DEXT, 8, target, level, internalformat, x, y, width, height, border);
    glad_glCopyTexImage2DEXT(target, level, internalformat, x, y, width, height, border);
    _post_call_gl_callback(NULL, "glCopyTexImage2DEXT", (GLADapiproc) glad_glCopyTexImage2DEXT, 8, target, level, internalformat, x, y, width, height, border);
    
}
PFNGLCOPYTEXIMAGE2DEXTPROC glad_debug_glCopyTexImage2DEXT = glad_debug_impl_glCopyTexImage2DEXT;
PFNGLCOPYTEXSUBIMAGE1DPROC glad_glCopyTexSubImage1D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    _pre_call_gl_callback("glCopyTexSubImage1D", (GLADapiproc) glad_glCopyTexSubImage1D, 6, target, level, xoffset, x, y, width);
    glad_glCopyTexSubImage1D(target, level, xoffset, x, y, width);
    _post_call_gl_callback(NULL, "glCopyTexSubImage1D", (GLADapiproc) glad_glCopyTexSubImage1D, 6, target, level, xoffset, x, y, width);
    
}
PFNGLCOPYTEXSUBIMAGE1DPROC glad_debug_glCopyTexSubImage1D = glad_debug_impl_glCopyTexSubImage1D;
PFNGLCOPYTEXSUBIMAGE1DEXTPROC glad_glCopyTexSubImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexSubImage1DEXT(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    _pre_call_gl_callback("glCopyTexSubImage1DEXT", (GLADapiproc) glad_glCopyTexSubImage1DEXT, 6, target, level, xoffset, x, y, width);
    glad_glCopyTexSubImage1DEXT(target, level, xoffset, x, y, width);
    _post_call_gl_callback(NULL, "glCopyTexSubImage1DEXT", (GLADapiproc) glad_glCopyTexSubImage1DEXT, 6, target, level, xoffset, x, y, width);
    
}
PFNGLCOPYTEXSUBIMAGE1DEXTPROC glad_debug_glCopyTexSubImage1DEXT = glad_debug_impl_glCopyTexSubImage1DEXT;
PFNGLCOPYTEXSUBIMAGE2DPROC glad_glCopyTexSubImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glCopyTexSubImage2D", (GLADapiproc) glad_glCopyTexSubImage2D, 8, target, level, xoffset, yoffset, x, y, width, height);
    glad_glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    _post_call_gl_callback(NULL, "glCopyTexSubImage2D", (GLADapiproc) glad_glCopyTexSubImage2D, 8, target, level, xoffset, yoffset, x, y, width, height);
    
}
PFNGLCOPYTEXSUBIMAGE2DPROC glad_debug_glCopyTexSubImage2D = glad_debug_impl_glCopyTexSubImage2D;
PFNGLCOPYTEXSUBIMAGE2DEXTPROC glad_glCopyTexSubImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexSubImage2DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glCopyTexSubImage2DEXT", (GLADapiproc) glad_glCopyTexSubImage2DEXT, 8, target, level, xoffset, yoffset, x, y, width, height);
    glad_glCopyTexSubImage2DEXT(target, level, xoffset, yoffset, x, y, width, height);
    _post_call_gl_callback(NULL, "glCopyTexSubImage2DEXT", (GLADapiproc) glad_glCopyTexSubImage2DEXT, 8, target, level, xoffset, yoffset, x, y, width, height);
    
}
PFNGLCOPYTEXSUBIMAGE2DEXTPROC glad_debug_glCopyTexSubImage2DEXT = glad_debug_impl_glCopyTexSubImage2DEXT;
PFNGLCOPYTEXSUBIMAGE3DPROC glad_glCopyTexSubImage3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glCopyTexSubImage3D", (GLADapiproc) glad_glCopyTexSubImage3D, 9, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    glad_glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
    _post_call_gl_callback(NULL, "glCopyTexSubImage3D", (GLADapiproc) glad_glCopyTexSubImage3D, 9, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    
}
PFNGLCOPYTEXSUBIMAGE3DPROC glad_debug_glCopyTexSubImage3D = glad_debug_impl_glCopyTexSubImage3D;
PFNGLCOPYTEXSUBIMAGE3DEXTPROC glad_glCopyTexSubImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexSubImage3DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glCopyTexSubImage3DEXT", (GLADapiproc) glad_glCopyTexSubImage3DEXT, 9, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    glad_glCopyTexSubImage3DEXT(target, level, xoffset, yoffset, zoffset, x, y, width, height);
    _post_call_gl_callback(NULL, "glCopyTexSubImage3DEXT", (GLADapiproc) glad_glCopyTexSubImage3DEXT, 9, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    
}
PFNGLCOPYTEXSUBIMAGE3DEXTPROC glad_debug_glCopyTexSubImage3DEXT = glad_debug_impl_glCopyTexSubImage3DEXT;
PFNGLCOPYTEXTUREIMAGE1DEXTPROC glad_glCopyTextureImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    _pre_call_gl_callback("glCopyTextureImage1DEXT", (GLADapiproc) glad_glCopyTextureImage1DEXT, 8, texture, target, level, internalformat, x, y, width, border);
    glad_glCopyTextureImage1DEXT(texture, target, level, internalformat, x, y, width, border);
    _post_call_gl_callback(NULL, "glCopyTextureImage1DEXT", (GLADapiproc) glad_glCopyTextureImage1DEXT, 8, texture, target, level, internalformat, x, y, width, border);
    
}
PFNGLCOPYTEXTUREIMAGE1DEXTPROC glad_debug_glCopyTextureImage1DEXT = glad_debug_impl_glCopyTextureImage1DEXT;
PFNGLCOPYTEXTUREIMAGE2DEXTPROC glad_glCopyTextureImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    _pre_call_gl_callback("glCopyTextureImage2DEXT", (GLADapiproc) glad_glCopyTextureImage2DEXT, 9, texture, target, level, internalformat, x, y, width, height, border);
    glad_glCopyTextureImage2DEXT(texture, target, level, internalformat, x, y, width, height, border);
    _post_call_gl_callback(NULL, "glCopyTextureImage2DEXT", (GLADapiproc) glad_glCopyTextureImage2DEXT, 9, texture, target, level, internalformat, x, y, width, height, border);
    
}
PFNGLCOPYTEXTUREIMAGE2DEXTPROC glad_debug_glCopyTextureImage2DEXT = glad_debug_impl_glCopyTextureImage2DEXT;
PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC glad_glCopyTextureSubImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    _pre_call_gl_callback("glCopyTextureSubImage1DEXT", (GLADapiproc) glad_glCopyTextureSubImage1DEXT, 7, texture, target, level, xoffset, x, y, width);
    glad_glCopyTextureSubImage1DEXT(texture, target, level, xoffset, x, y, width);
    _post_call_gl_callback(NULL, "glCopyTextureSubImage1DEXT", (GLADapiproc) glad_glCopyTextureSubImage1DEXT, 7, texture, target, level, xoffset, x, y, width);
    
}
PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC glad_debug_glCopyTextureSubImage1DEXT = glad_debug_impl_glCopyTextureSubImage1DEXT;
PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC glad_glCopyTextureSubImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glCopyTextureSubImage2DEXT", (GLADapiproc) glad_glCopyTextureSubImage2DEXT, 9, texture, target, level, xoffset, yoffset, x, y, width, height);
    glad_glCopyTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, x, y, width, height);
    _post_call_gl_callback(NULL, "glCopyTextureSubImage2DEXT", (GLADapiproc) glad_glCopyTextureSubImage2DEXT, 9, texture, target, level, xoffset, yoffset, x, y, width, height);
    
}
PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC glad_debug_glCopyTextureSubImage2DEXT = glad_debug_impl_glCopyTextureSubImage2DEXT;
PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC glad_glCopyTextureSubImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glCopyTextureSubImage3DEXT", (GLADapiproc) glad_glCopyTextureSubImage3DEXT, 10, texture, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    glad_glCopyTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    _post_call_gl_callback(NULL, "glCopyTextureSubImage3DEXT", (GLADapiproc) glad_glCopyTextureSubImage3DEXT, 10, texture, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    
}
PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC glad_debug_glCopyTextureSubImage3DEXT = glad_debug_impl_glCopyTextureSubImage3DEXT;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glCreateProgram(void) {
    GLuint ret;
    _pre_call_gl_callback("glCreateProgram", (GLADapiproc) glad_glCreateProgram, 0);
    ret = glad_glCreateProgram();
    _post_call_gl_callback((void*) &ret, "glCreateProgram", (GLADapiproc) glad_glCreateProgram, 0);
    return ret;
}
PFNGLCREATEPROGRAMPROC glad_debug_glCreateProgram = glad_debug_impl_glCreateProgram;
PFNGLCREATEPROGRAMOBJECTARBPROC glad_glCreateProgramObjectARB = NULL;
static GLhandleARB GLAD_API_PTR glad_debug_impl_glCreateProgramObjectARB(void) {
    GLhandleARB ret;
    _pre_call_gl_callback("glCreateProgramObjectARB", (GLADapiproc) glad_glCreateProgramObjectARB, 0);
    ret = glad_glCreateProgramObjectARB();
    _post_call_gl_callback((void*) &ret, "glCreateProgramObjectARB", (GLADapiproc) glad_glCreateProgramObjectARB, 0);
    return ret;
}
PFNGLCREATEPROGRAMOBJECTARBPROC glad_debug_glCreateProgramObjectARB = glad_debug_impl_glCreateProgramObjectARB;
PFNGLCREATESHADERPROC glad_glCreateShader = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glCreateShader(GLenum type) {
    GLuint ret;
    _pre_call_gl_callback("glCreateShader", (GLADapiproc) glad_glCreateShader, 1, type);
    ret = glad_glCreateShader(type);
    _post_call_gl_callback((void*) &ret, "glCreateShader", (GLADapiproc) glad_glCreateShader, 1, type);
    return ret;
}
PFNGLCREATESHADERPROC glad_debug_glCreateShader = glad_debug_impl_glCreateShader;
PFNGLCREATESHADEROBJECTARBPROC glad_glCreateShaderObjectARB = NULL;
static GLhandleARB GLAD_API_PTR glad_debug_impl_glCreateShaderObjectARB(GLenum shaderType) {
    GLhandleARB ret;
    _pre_call_gl_callback("glCreateShaderObjectARB", (GLADapiproc) glad_glCreateShaderObjectARB, 1, shaderType);
    ret = glad_glCreateShaderObjectARB(shaderType);
    _post_call_gl_callback((void*) &ret, "glCreateShaderObjectARB", (GLADapiproc) glad_glCreateShaderObjectARB, 1, shaderType);
    return ret;
}
PFNGLCREATESHADEROBJECTARBPROC glad_debug_glCreateShaderObjectARB = glad_debug_impl_glCreateShaderObjectARB;
PFNGLCULLFACEPROC glad_glCullFace = NULL;
static void GLAD_API_PTR glad_debug_impl_glCullFace(GLenum mode) {
    _pre_call_gl_callback("glCullFace", (GLADapiproc) glad_glCullFace, 1, mode);
    glad_glCullFace(mode);
    _post_call_gl_callback(NULL, "glCullFace", (GLADapiproc) glad_glCullFace, 1, mode);
    
}
PFNGLCULLFACEPROC glad_debug_glCullFace = glad_debug_impl_glCullFace;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteBuffers(GLsizei n, const GLuint * buffers) {
    _pre_call_gl_callback("glDeleteBuffers", (GLADapiproc) glad_glDeleteBuffers, 2, n, buffers);
    glad_glDeleteBuffers(n, buffers);
    _post_call_gl_callback(NULL, "glDeleteBuffers", (GLADapiproc) glad_glDeleteBuffers, 2, n, buffers);
    
}
PFNGLDELETEBUFFERSPROC glad_debug_glDeleteBuffers = glad_debug_impl_glDeleteBuffers;
PFNGLDELETEBUFFERSARBPROC glad_glDeleteBuffersARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteBuffersARB(GLsizei n, const GLuint * buffers) {
    _pre_call_gl_callback("glDeleteBuffersARB", (GLADapiproc) glad_glDeleteBuffersARB, 2, n, buffers);
    glad_glDeleteBuffersARB(n, buffers);
    _post_call_gl_callback(NULL, "glDeleteBuffersARB", (GLADapiproc) glad_glDeleteBuffersARB, 2, n, buffers);
    
}
PFNGLDELETEBUFFERSARBPROC glad_debug_glDeleteBuffersARB = glad_debug_impl_glDeleteBuffersARB;
PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteFramebuffers(GLsizei n, const GLuint * framebuffers) {
    _pre_call_gl_callback("glDeleteFramebuffers", (GLADapiproc) glad_glDeleteFramebuffers, 2, n, framebuffers);
    glad_glDeleteFramebuffers(n, framebuffers);
    _post_call_gl_callback(NULL, "glDeleteFramebuffers", (GLADapiproc) glad_glDeleteFramebuffers, 2, n, framebuffers);
    
}
PFNGLDELETEFRAMEBUFFERSPROC glad_debug_glDeleteFramebuffers = glad_debug_impl_glDeleteFramebuffers;
PFNGLDELETEFRAMEBUFFERSEXTPROC glad_glDeleteFramebuffersEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteFramebuffersEXT(GLsizei n, const GLuint * framebuffers) {
    _pre_call_gl_callback("glDeleteFramebuffersEXT", (GLADapiproc) glad_glDeleteFramebuffersEXT, 2, n, framebuffers);
    glad_glDeleteFramebuffersEXT(n, framebuffers);
    _post_call_gl_callback(NULL, "glDeleteFramebuffersEXT", (GLADapiproc) glad_glDeleteFramebuffersEXT, 2, n, framebuffers);
    
}
PFNGLDELETEFRAMEBUFFERSEXTPROC glad_debug_glDeleteFramebuffersEXT = glad_debug_impl_glDeleteFramebuffersEXT;
PFNGLDELETEOBJECTARBPROC glad_glDeleteObjectARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteObjectARB(GLhandleARB obj) {
    _pre_call_gl_callback("glDeleteObjectARB", (GLADapiproc) glad_glDeleteObjectARB, 1, obj);
    glad_glDeleteObjectARB(obj);
    _post_call_gl_callback(NULL, "glDeleteObjectARB", (GLADapiproc) glad_glDeleteObjectARB, 1, obj);
    
}
PFNGLDELETEOBJECTARBPROC glad_debug_glDeleteObjectARB = glad_debug_impl_glDeleteObjectARB;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteProgram(GLuint program) {
    _pre_call_gl_callback("glDeleteProgram", (GLADapiproc) glad_glDeleteProgram, 1, program);
    glad_glDeleteProgram(program);
    _post_call_gl_callback(NULL, "glDeleteProgram", (GLADapiproc) glad_glDeleteProgram, 1, program);
    
}
PFNGLDELETEPROGRAMPROC glad_debug_glDeleteProgram = glad_debug_impl_glDeleteProgram;
PFNGLDELETEPROGRAMSARBPROC glad_glDeleteProgramsARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteProgramsARB(GLsizei n, const GLuint * programs) {
    _pre_call_gl_callback("glDeleteProgramsARB", (GLADapiproc) glad_glDeleteProgramsARB, 2, n, programs);
    glad_glDeleteProgramsARB(n, programs);
    _post_call_gl_callback(NULL, "glDeleteProgramsARB", (GLADapiproc) glad_glDeleteProgramsARB, 2, n, programs);
    
}
PFNGLDELETEPROGRAMSARBPROC glad_debug_glDeleteProgramsARB = glad_debug_impl_glDeleteProgramsARB;
PFNGLDELETEPROGRAMSNVPROC glad_glDeleteProgramsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteProgramsNV(GLsizei n, const GLuint * programs) {
    _pre_call_gl_callback("glDeleteProgramsNV", (GLADapiproc) glad_glDeleteProgramsNV, 2, n, programs);
    glad_glDeleteProgramsNV(n, programs);
    _post_call_gl_callback(NULL, "glDeleteProgramsNV", (GLADapiproc) glad_glDeleteProgramsNV, 2, n, programs);
    
}
PFNGLDELETEPROGRAMSNVPROC glad_debug_glDeleteProgramsNV = glad_debug_impl_glDeleteProgramsNV;
PFNGLDELETEQUERIESPROC glad_glDeleteQueries = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteQueries(GLsizei n, const GLuint * ids) {
    _pre_call_gl_callback("glDeleteQueries", (GLADapiproc) glad_glDeleteQueries, 2, n, ids);
    glad_glDeleteQueries(n, ids);
    _post_call_gl_callback(NULL, "glDeleteQueries", (GLADapiproc) glad_glDeleteQueries, 2, n, ids);
    
}
PFNGLDELETEQUERIESPROC glad_debug_glDeleteQueries = glad_debug_impl_glDeleteQueries;
PFNGLDELETEQUERIESARBPROC glad_glDeleteQueriesARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteQueriesARB(GLsizei n, const GLuint * ids) {
    _pre_call_gl_callback("glDeleteQueriesARB", (GLADapiproc) glad_glDeleteQueriesARB, 2, n, ids);
    glad_glDeleteQueriesARB(n, ids);
    _post_call_gl_callback(NULL, "glDeleteQueriesARB", (GLADapiproc) glad_glDeleteQueriesARB, 2, n, ids);
    
}
PFNGLDELETEQUERIESARBPROC glad_debug_glDeleteQueriesARB = glad_debug_impl_glDeleteQueriesARB;
PFNGLDELETERENDERBUFFERSPROC glad_glDeleteRenderbuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteRenderbuffers(GLsizei n, const GLuint * renderbuffers) {
    _pre_call_gl_callback("glDeleteRenderbuffers", (GLADapiproc) glad_glDeleteRenderbuffers, 2, n, renderbuffers);
    glad_glDeleteRenderbuffers(n, renderbuffers);
    _post_call_gl_callback(NULL, "glDeleteRenderbuffers", (GLADapiproc) glad_glDeleteRenderbuffers, 2, n, renderbuffers);
    
}
PFNGLDELETERENDERBUFFERSPROC glad_debug_glDeleteRenderbuffers = glad_debug_impl_glDeleteRenderbuffers;
PFNGLDELETERENDERBUFFERSEXTPROC glad_glDeleteRenderbuffersEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteRenderbuffersEXT(GLsizei n, const GLuint * renderbuffers) {
    _pre_call_gl_callback("glDeleteRenderbuffersEXT", (GLADapiproc) glad_glDeleteRenderbuffersEXT, 2, n, renderbuffers);
    glad_glDeleteRenderbuffersEXT(n, renderbuffers);
    _post_call_gl_callback(NULL, "glDeleteRenderbuffersEXT", (GLADapiproc) glad_glDeleteRenderbuffersEXT, 2, n, renderbuffers);
    
}
PFNGLDELETERENDERBUFFERSEXTPROC glad_debug_glDeleteRenderbuffersEXT = glad_debug_impl_glDeleteRenderbuffersEXT;
PFNGLDELETESAMPLERSPROC glad_glDeleteSamplers = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteSamplers(GLsizei count, const GLuint * samplers) {
    _pre_call_gl_callback("glDeleteSamplers", (GLADapiproc) glad_glDeleteSamplers, 2, count, samplers);
    glad_glDeleteSamplers(count, samplers);
    _post_call_gl_callback(NULL, "glDeleteSamplers", (GLADapiproc) glad_glDeleteSamplers, 2, count, samplers);
    
}
PFNGLDELETESAMPLERSPROC glad_debug_glDeleteSamplers = glad_debug_impl_glDeleteSamplers;
PFNGLDELETESHADERPROC glad_glDeleteShader = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteShader(GLuint shader) {
    _pre_call_gl_callback("glDeleteShader", (GLADapiproc) glad_glDeleteShader, 1, shader);
    glad_glDeleteShader(shader);
    _post_call_gl_callback(NULL, "glDeleteShader", (GLADapiproc) glad_glDeleteShader, 1, shader);
    
}
PFNGLDELETESHADERPROC glad_debug_glDeleteShader = glad_debug_impl_glDeleteShader;
PFNGLDELETESYNCPROC glad_glDeleteSync = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteSync(GLsync sync) {
    _pre_call_gl_callback("glDeleteSync", (GLADapiproc) glad_glDeleteSync, 1, sync);
    glad_glDeleteSync(sync);
    _post_call_gl_callback(NULL, "glDeleteSync", (GLADapiproc) glad_glDeleteSync, 1, sync);
    
}
PFNGLDELETESYNCPROC glad_debug_glDeleteSync = glad_debug_impl_glDeleteSync;
PFNGLDELETETEXTURESPROC glad_glDeleteTextures = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteTextures(GLsizei n, const GLuint * textures) {
    _pre_call_gl_callback("glDeleteTextures", (GLADapiproc) glad_glDeleteTextures, 2, n, textures);
    glad_glDeleteTextures(n, textures);
    _post_call_gl_callback(NULL, "glDeleteTextures", (GLADapiproc) glad_glDeleteTextures, 2, n, textures);
    
}
PFNGLDELETETEXTURESPROC glad_debug_glDeleteTextures = glad_debug_impl_glDeleteTextures;
PFNGLDELETETEXTURESEXTPROC glad_glDeleteTexturesEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteTexturesEXT(GLsizei n, const GLuint * textures) {
    _pre_call_gl_callback("glDeleteTexturesEXT", (GLADapiproc) glad_glDeleteTexturesEXT, 2, n, textures);
    glad_glDeleteTexturesEXT(n, textures);
    _post_call_gl_callback(NULL, "glDeleteTexturesEXT", (GLADapiproc) glad_glDeleteTexturesEXT, 2, n, textures);
    
}
PFNGLDELETETEXTURESEXTPROC glad_debug_glDeleteTexturesEXT = glad_debug_impl_glDeleteTexturesEXT;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteVertexArrays(GLsizei n, const GLuint * arrays) {
    _pre_call_gl_callback("glDeleteVertexArrays", (GLADapiproc) glad_glDeleteVertexArrays, 2, n, arrays);
    glad_glDeleteVertexArrays(n, arrays);
    _post_call_gl_callback(NULL, "glDeleteVertexArrays", (GLADapiproc) glad_glDeleteVertexArrays, 2, n, arrays);
    
}
PFNGLDELETEVERTEXARRAYSPROC glad_debug_glDeleteVertexArrays = glad_debug_impl_glDeleteVertexArrays;
PFNGLDELETEVERTEXARRAYSAPPLEPROC glad_glDeleteVertexArraysAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteVertexArraysAPPLE(GLsizei n, const GLuint * arrays) {
    _pre_call_gl_callback("glDeleteVertexArraysAPPLE", (GLADapiproc) glad_glDeleteVertexArraysAPPLE, 2, n, arrays);
    glad_glDeleteVertexArraysAPPLE(n, arrays);
    _post_call_gl_callback(NULL, "glDeleteVertexArraysAPPLE", (GLADapiproc) glad_glDeleteVertexArraysAPPLE, 2, n, arrays);
    
}
PFNGLDELETEVERTEXARRAYSAPPLEPROC glad_debug_glDeleteVertexArraysAPPLE = glad_debug_impl_glDeleteVertexArraysAPPLE;
PFNGLDEPTHFUNCPROC glad_glDepthFunc = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthFunc(GLenum func) {
    _pre_call_gl_callback("glDepthFunc", (GLADapiproc) glad_glDepthFunc, 1, func);
    glad_glDepthFunc(func);
    _post_call_gl_callback(NULL, "glDepthFunc", (GLADapiproc) glad_glDepthFunc, 1, func);
    
}
PFNGLDEPTHFUNCPROC glad_debug_glDepthFunc = glad_debug_impl_glDepthFunc;
PFNGLDEPTHMASKPROC glad_glDepthMask = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthMask(GLboolean flag) {
    _pre_call_gl_callback("glDepthMask", (GLADapiproc) glad_glDepthMask, 1, flag);
    glad_glDepthMask(flag);
    _post_call_gl_callback(NULL, "glDepthMask", (GLADapiproc) glad_glDepthMask, 1, flag);
    
}
PFNGLDEPTHMASKPROC glad_debug_glDepthMask = glad_debug_impl_glDepthMask;
PFNGLDEPTHRANGEPROC glad_glDepthRange = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthRange(GLdouble n, GLdouble f) {
    _pre_call_gl_callback("glDepthRange", (GLADapiproc) glad_glDepthRange, 2, n, f);
    glad_glDepthRange(n, f);
    _post_call_gl_callback(NULL, "glDepthRange", (GLADapiproc) glad_glDepthRange, 2, n, f);
    
}
PFNGLDEPTHRANGEPROC glad_debug_glDepthRange = glad_debug_impl_glDepthRange;
PFNGLDETACHOBJECTARBPROC glad_glDetachObjectARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glDetachObjectARB(GLhandleARB containerObj, GLhandleARB attachedObj) {
    _pre_call_gl_callback("glDetachObjectARB", (GLADapiproc) glad_glDetachObjectARB, 2, containerObj, attachedObj);
    glad_glDetachObjectARB(containerObj, attachedObj);
    _post_call_gl_callback(NULL, "glDetachObjectARB", (GLADapiproc) glad_glDetachObjectARB, 2, containerObj, attachedObj);
    
}
PFNGLDETACHOBJECTARBPROC glad_debug_glDetachObjectARB = glad_debug_impl_glDetachObjectARB;
PFNGLDETACHSHADERPROC glad_glDetachShader = NULL;
static void GLAD_API_PTR glad_debug_impl_glDetachShader(GLuint program, GLuint shader) {
    _pre_call_gl_callback("glDetachShader", (GLADapiproc) glad_glDetachShader, 2, program, shader);
    glad_glDetachShader(program, shader);
    _post_call_gl_callback(NULL, "glDetachShader", (GLADapiproc) glad_glDetachShader, 2, program, shader);
    
}
PFNGLDETACHSHADERPROC glad_debug_glDetachShader = glad_debug_impl_glDetachShader;
PFNGLDISABLEPROC glad_glDisable = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisable(GLenum cap) {
    _pre_call_gl_callback("glDisable", (GLADapiproc) glad_glDisable, 1, cap);
    glad_glDisable(cap);
    _post_call_gl_callback(NULL, "glDisable", (GLADapiproc) glad_glDisable, 1, cap);
    
}
PFNGLDISABLEPROC glad_debug_glDisable = glad_debug_impl_glDisable;
PFNGLDISABLECLIENTSTATEINDEXEDEXTPROC glad_glDisableClientStateIndexedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableClientStateIndexedEXT(GLenum array, GLuint index) {
    _pre_call_gl_callback("glDisableClientStateIndexedEXT", (GLADapiproc) glad_glDisableClientStateIndexedEXT, 2, array, index);
    glad_glDisableClientStateIndexedEXT(array, index);
    _post_call_gl_callback(NULL, "glDisableClientStateIndexedEXT", (GLADapiproc) glad_glDisableClientStateIndexedEXT, 2, array, index);
    
}
PFNGLDISABLECLIENTSTATEINDEXEDEXTPROC glad_debug_glDisableClientStateIndexedEXT = glad_debug_impl_glDisableClientStateIndexedEXT;
PFNGLDISABLECLIENTSTATEIEXTPROC glad_glDisableClientStateiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableClientStateiEXT(GLenum array, GLuint index) {
    _pre_call_gl_callback("glDisableClientStateiEXT", (GLADapiproc) glad_glDisableClientStateiEXT, 2, array, index);
    glad_glDisableClientStateiEXT(array, index);
    _post_call_gl_callback(NULL, "glDisableClientStateiEXT", (GLADapiproc) glad_glDisableClientStateiEXT, 2, array, index);
    
}
PFNGLDISABLECLIENTSTATEIEXTPROC glad_debug_glDisableClientStateiEXT = glad_debug_impl_glDisableClientStateiEXT;
PFNGLDISABLEINDEXEDEXTPROC glad_glDisableIndexedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableIndexedEXT(GLenum target, GLuint index) {
    _pre_call_gl_callback("glDisableIndexedEXT", (GLADapiproc) glad_glDisableIndexedEXT, 2, target, index);
    glad_glDisableIndexedEXT(target, index);
    _post_call_gl_callback(NULL, "glDisableIndexedEXT", (GLADapiproc) glad_glDisableIndexedEXT, 2, target, index);
    
}
PFNGLDISABLEINDEXEDEXTPROC glad_debug_glDisableIndexedEXT = glad_debug_impl_glDisableIndexedEXT;
PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC glad_glDisableVertexArrayAttribEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index) {
    _pre_call_gl_callback("glDisableVertexArrayAttribEXT", (GLADapiproc) glad_glDisableVertexArrayAttribEXT, 2, vaobj, index);
    glad_glDisableVertexArrayAttribEXT(vaobj, index);
    _post_call_gl_callback(NULL, "glDisableVertexArrayAttribEXT", (GLADapiproc) glad_glDisableVertexArrayAttribEXT, 2, vaobj, index);
    
}
PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC glad_debug_glDisableVertexArrayAttribEXT = glad_debug_impl_glDisableVertexArrayAttribEXT;
PFNGLDISABLEVERTEXARRAYEXTPROC glad_glDisableVertexArrayEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableVertexArrayEXT(GLuint vaobj, GLenum array) {
    _pre_call_gl_callback("glDisableVertexArrayEXT", (GLADapiproc) glad_glDisableVertexArrayEXT, 2, vaobj, array);
    glad_glDisableVertexArrayEXT(vaobj, array);
    _post_call_gl_callback(NULL, "glDisableVertexArrayEXT", (GLADapiproc) glad_glDisableVertexArrayEXT, 2, vaobj, array);
    
}
PFNGLDISABLEVERTEXARRAYEXTPROC glad_debug_glDisableVertexArrayEXT = glad_debug_impl_glDisableVertexArrayEXT;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableVertexAttribArray(GLuint index) {
    _pre_call_gl_callback("glDisableVertexAttribArray", (GLADapiproc) glad_glDisableVertexAttribArray, 1, index);
    glad_glDisableVertexAttribArray(index);
    _post_call_gl_callback(NULL, "glDisableVertexAttribArray", (GLADapiproc) glad_glDisableVertexAttribArray, 1, index);
    
}
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_debug_glDisableVertexAttribArray = glad_debug_impl_glDisableVertexAttribArray;
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glad_glDisableVertexAttribArrayARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableVertexAttribArrayARB(GLuint index) {
    _pre_call_gl_callback("glDisableVertexAttribArrayARB", (GLADapiproc) glad_glDisableVertexAttribArrayARB, 1, index);
    glad_glDisableVertexAttribArrayARB(index);
    _post_call_gl_callback(NULL, "glDisableVertexAttribArrayARB", (GLADapiproc) glad_glDisableVertexAttribArrayARB, 1, index);
    
}
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glad_debug_glDisableVertexAttribArrayARB = glad_debug_impl_glDisableVertexAttribArrayARB;
PFNGLDISABLEIPROC glad_glDisablei = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisablei(GLenum target, GLuint index) {
    _pre_call_gl_callback("glDisablei", (GLADapiproc) glad_glDisablei, 2, target, index);
    glad_glDisablei(target, index);
    _post_call_gl_callback(NULL, "glDisablei", (GLADapiproc) glad_glDisablei, 2, target, index);
    
}
PFNGLDISABLEIPROC glad_debug_glDisablei = glad_debug_impl_glDisablei;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    _pre_call_gl_callback("glDrawArrays", (GLADapiproc) glad_glDrawArrays, 3, mode, first, count);
    glad_glDrawArrays(mode, first, count);
    _post_call_gl_callback(NULL, "glDrawArrays", (GLADapiproc) glad_glDrawArrays, 3, mode, first, count);
    
}
PFNGLDRAWARRAYSPROC glad_debug_glDrawArrays = glad_debug_impl_glDrawArrays;
PFNGLDRAWARRAYSEXTPROC glad_glDrawArraysEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArraysEXT(GLenum mode, GLint first, GLsizei count) {
    _pre_call_gl_callback("glDrawArraysEXT", (GLADapiproc) glad_glDrawArraysEXT, 3, mode, first, count);
    glad_glDrawArraysEXT(mode, first, count);
    _post_call_gl_callback(NULL, "glDrawArraysEXT", (GLADapiproc) glad_glDrawArraysEXT, 3, mode, first, count);
    
}
PFNGLDRAWARRAYSEXTPROC glad_debug_glDrawArraysEXT = glad_debug_impl_glDrawArraysEXT;
PFNGLDRAWARRAYSINSTANCEDPROC glad_glDrawArraysInstanced = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) {
    _pre_call_gl_callback("glDrawArraysInstanced", (GLADapiproc) glad_glDrawArraysInstanced, 4, mode, first, count, instancecount);
    glad_glDrawArraysInstanced(mode, first, count, instancecount);
    _post_call_gl_callback(NULL, "glDrawArraysInstanced", (GLADapiproc) glad_glDrawArraysInstanced, 4, mode, first, count, instancecount);
    
}
PFNGLDRAWARRAYSINSTANCEDPROC glad_debug_glDrawArraysInstanced = glad_debug_impl_glDrawArraysInstanced;
PFNGLDRAWARRAYSINSTANCEDARBPROC glad_glDrawArraysInstancedARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArraysInstancedARB(GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
    _pre_call_gl_callback("glDrawArraysInstancedARB", (GLADapiproc) glad_glDrawArraysInstancedARB, 4, mode, first, count, primcount);
    glad_glDrawArraysInstancedARB(mode, first, count, primcount);
    _post_call_gl_callback(NULL, "glDrawArraysInstancedARB", (GLADapiproc) glad_glDrawArraysInstancedARB, 4, mode, first, count, primcount);
    
}
PFNGLDRAWARRAYSINSTANCEDARBPROC glad_debug_glDrawArraysInstancedARB = glad_debug_impl_glDrawArraysInstancedARB;
PFNGLDRAWARRAYSINSTANCEDEXTPROC glad_glDrawArraysInstancedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArraysInstancedEXT(GLenum mode, GLint start, GLsizei count, GLsizei primcount) {
    _pre_call_gl_callback("glDrawArraysInstancedEXT", (GLADapiproc) glad_glDrawArraysInstancedEXT, 4, mode, start, count, primcount);
    glad_glDrawArraysInstancedEXT(mode, start, count, primcount);
    _post_call_gl_callback(NULL, "glDrawArraysInstancedEXT", (GLADapiproc) glad_glDrawArraysInstancedEXT, 4, mode, start, count, primcount);
    
}
PFNGLDRAWARRAYSINSTANCEDEXTPROC glad_debug_glDrawArraysInstancedEXT = glad_debug_impl_glDrawArraysInstancedEXT;
PFNGLDRAWBUFFERPROC glad_glDrawBuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawBuffer(GLenum buf) {
    _pre_call_gl_callback("glDrawBuffer", (GLADapiproc) glad_glDrawBuffer, 1, buf);
    glad_glDrawBuffer(buf);
    _post_call_gl_callback(NULL, "glDrawBuffer", (GLADapiproc) glad_glDrawBuffer, 1, buf);
    
}
PFNGLDRAWBUFFERPROC glad_debug_glDrawBuffer = glad_debug_impl_glDrawBuffer;
PFNGLDRAWBUFFERSPROC glad_glDrawBuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawBuffers(GLsizei n, const GLenum * bufs) {
    _pre_call_gl_callback("glDrawBuffers", (GLADapiproc) glad_glDrawBuffers, 2, n, bufs);
    glad_glDrawBuffers(n, bufs);
    _post_call_gl_callback(NULL, "glDrawBuffers", (GLADapiproc) glad_glDrawBuffers, 2, n, bufs);
    
}
PFNGLDRAWBUFFERSPROC glad_debug_glDrawBuffers = glad_debug_impl_glDrawBuffers;
PFNGLDRAWBUFFERSARBPROC glad_glDrawBuffersARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawBuffersARB(GLsizei n, const GLenum * bufs) {
    _pre_call_gl_callback("glDrawBuffersARB", (GLADapiproc) glad_glDrawBuffersARB, 2, n, bufs);
    glad_glDrawBuffersARB(n, bufs);
    _post_call_gl_callback(NULL, "glDrawBuffersARB", (GLADapiproc) glad_glDrawBuffersARB, 2, n, bufs);
    
}
PFNGLDRAWBUFFERSARBPROC glad_debug_glDrawBuffersARB = glad_debug_impl_glDrawBuffersARB;
PFNGLDRAWBUFFERSATIPROC glad_glDrawBuffersATI = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawBuffersATI(GLsizei n, const GLenum * bufs) {
    _pre_call_gl_callback("glDrawBuffersATI", (GLADapiproc) glad_glDrawBuffersATI, 2, n, bufs);
    glad_glDrawBuffersATI(n, bufs);
    _post_call_gl_callback(NULL, "glDrawBuffersATI", (GLADapiproc) glad_glDrawBuffersATI, 2, n, bufs);
    
}
PFNGLDRAWBUFFERSATIPROC glad_debug_glDrawBuffersATI = glad_debug_impl_glDrawBuffersATI;
PFNGLDRAWELEMENTSPROC glad_glDrawElements = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void * indices) {
    _pre_call_gl_callback("glDrawElements", (GLADapiproc) glad_glDrawElements, 4, mode, count, type, indices);
    glad_glDrawElements(mode, count, type, indices);
    _post_call_gl_callback(NULL, "glDrawElements", (GLADapiproc) glad_glDrawElements, 4, mode, count, type, indices);
    
}
PFNGLDRAWELEMENTSPROC glad_debug_glDrawElements = glad_debug_impl_glDrawElements;
PFNGLDRAWELEMENTSBASEVERTEXPROC glad_glDrawElementsBaseVertex = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void * indices, GLint basevertex) {
    _pre_call_gl_callback("glDrawElementsBaseVertex", (GLADapiproc) glad_glDrawElementsBaseVertex, 5, mode, count, type, indices, basevertex);
    glad_glDrawElementsBaseVertex(mode, count, type, indices, basevertex);
    _post_call_gl_callback(NULL, "glDrawElementsBaseVertex", (GLADapiproc) glad_glDrawElementsBaseVertex, 5, mode, count, type, indices, basevertex);
    
}
PFNGLDRAWELEMENTSBASEVERTEXPROC glad_debug_glDrawElementsBaseVertex = glad_debug_impl_glDrawElementsBaseVertex;
PFNGLDRAWELEMENTSINSTANCEDPROC glad_glDrawElementsInstanced = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount) {
    _pre_call_gl_callback("glDrawElementsInstanced", (GLADapiproc) glad_glDrawElementsInstanced, 5, mode, count, type, indices, instancecount);
    glad_glDrawElementsInstanced(mode, count, type, indices, instancecount);
    _post_call_gl_callback(NULL, "glDrawElementsInstanced", (GLADapiproc) glad_glDrawElementsInstanced, 5, mode, count, type, indices, instancecount);
    
}
PFNGLDRAWELEMENTSINSTANCEDPROC glad_debug_glDrawElementsInstanced = glad_debug_impl_glDrawElementsInstanced;
PFNGLDRAWELEMENTSINSTANCEDARBPROC glad_glDrawElementsInstancedARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstancedARB(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei primcount) {
    _pre_call_gl_callback("glDrawElementsInstancedARB", (GLADapiproc) glad_glDrawElementsInstancedARB, 5, mode, count, type, indices, primcount);
    glad_glDrawElementsInstancedARB(mode, count, type, indices, primcount);
    _post_call_gl_callback(NULL, "glDrawElementsInstancedARB", (GLADapiproc) glad_glDrawElementsInstancedARB, 5, mode, count, type, indices, primcount);
    
}
PFNGLDRAWELEMENTSINSTANCEDARBPROC glad_debug_glDrawElementsInstancedARB = glad_debug_impl_glDrawElementsInstancedARB;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glad_glDrawElementsInstancedBaseVertex = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount, GLint basevertex) {
    _pre_call_gl_callback("glDrawElementsInstancedBaseVertex", (GLADapiproc) glad_glDrawElementsInstancedBaseVertex, 6, mode, count, type, indices, instancecount, basevertex);
    glad_glDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);
    _post_call_gl_callback(NULL, "glDrawElementsInstancedBaseVertex", (GLADapiproc) glad_glDrawElementsInstancedBaseVertex, 6, mode, count, type, indices, instancecount, basevertex);
    
}
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glad_debug_glDrawElementsInstancedBaseVertex = glad_debug_impl_glDrawElementsInstancedBaseVertex;
PFNGLDRAWELEMENTSINSTANCEDEXTPROC glad_glDrawElementsInstancedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstancedEXT(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei primcount) {
    _pre_call_gl_callback("glDrawElementsInstancedEXT", (GLADapiproc) glad_glDrawElementsInstancedEXT, 5, mode, count, type, indices, primcount);
    glad_glDrawElementsInstancedEXT(mode, count, type, indices, primcount);
    _post_call_gl_callback(NULL, "glDrawElementsInstancedEXT", (GLADapiproc) glad_glDrawElementsInstancedEXT, 5, mode, count, type, indices, primcount);
    
}
PFNGLDRAWELEMENTSINSTANCEDEXTPROC glad_debug_glDrawElementsInstancedEXT = glad_debug_impl_glDrawElementsInstancedEXT;
PFNGLDRAWRANGEELEMENTSPROC glad_glDrawRangeElements = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void * indices) {
    _pre_call_gl_callback("glDrawRangeElements", (GLADapiproc) glad_glDrawRangeElements, 6, mode, start, end, count, type, indices);
    glad_glDrawRangeElements(mode, start, end, count, type, indices);
    _post_call_gl_callback(NULL, "glDrawRangeElements", (GLADapiproc) glad_glDrawRangeElements, 6, mode, start, end, count, type, indices);
    
}
PFNGLDRAWRANGEELEMENTSPROC glad_debug_glDrawRangeElements = glad_debug_impl_glDrawRangeElements;
PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glad_glDrawRangeElementsBaseVertex = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void * indices, GLint basevertex) {
    _pre_call_gl_callback("glDrawRangeElementsBaseVertex", (GLADapiproc) glad_glDrawRangeElementsBaseVertex, 7, mode, start, end, count, type, indices, basevertex);
    glad_glDrawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);
    _post_call_gl_callback(NULL, "glDrawRangeElementsBaseVertex", (GLADapiproc) glad_glDrawRangeElementsBaseVertex, 7, mode, start, end, count, type, indices, basevertex);
    
}
PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glad_debug_glDrawRangeElementsBaseVertex = glad_debug_impl_glDrawRangeElementsBaseVertex;
PFNGLDRAWRANGEELEMENTSEXTPROC glad_glDrawRangeElementsEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawRangeElementsEXT(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void * indices) {
    _pre_call_gl_callback("glDrawRangeElementsEXT", (GLADapiproc) glad_glDrawRangeElementsEXT, 6, mode, start, end, count, type, indices);
    glad_glDrawRangeElementsEXT(mode, start, end, count, type, indices);
    _post_call_gl_callback(NULL, "glDrawRangeElementsEXT", (GLADapiproc) glad_glDrawRangeElementsEXT, 6, mode, start, end, count, type, indices);
    
}
PFNGLDRAWRANGEELEMENTSEXTPROC glad_debug_glDrawRangeElementsEXT = glad_debug_impl_glDrawRangeElementsEXT;
PFNGLEDGEFLAGPOINTEREXTPROC glad_glEdgeFlagPointerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glEdgeFlagPointerEXT(GLsizei stride, GLsizei count, const GLboolean * pointer) {
    _pre_call_gl_callback("glEdgeFlagPointerEXT", (GLADapiproc) glad_glEdgeFlagPointerEXT, 3, stride, count, pointer);
    glad_glEdgeFlagPointerEXT(stride, count, pointer);
    _post_call_gl_callback(NULL, "glEdgeFlagPointerEXT", (GLADapiproc) glad_glEdgeFlagPointerEXT, 3, stride, count, pointer);
    
}
PFNGLEDGEFLAGPOINTEREXTPROC glad_debug_glEdgeFlagPointerEXT = glad_debug_impl_glEdgeFlagPointerEXT;
PFNGLENABLEPROC glad_glEnable = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnable(GLenum cap) {
    _pre_call_gl_callback("glEnable", (GLADapiproc) glad_glEnable, 1, cap);
    glad_glEnable(cap);
    _post_call_gl_callback(NULL, "glEnable", (GLADapiproc) glad_glEnable, 1, cap);
    
}
PFNGLENABLEPROC glad_debug_glEnable = glad_debug_impl_glEnable;
PFNGLENABLECLIENTSTATEINDEXEDEXTPROC glad_glEnableClientStateIndexedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableClientStateIndexedEXT(GLenum array, GLuint index) {
    _pre_call_gl_callback("glEnableClientStateIndexedEXT", (GLADapiproc) glad_glEnableClientStateIndexedEXT, 2, array, index);
    glad_glEnableClientStateIndexedEXT(array, index);
    _post_call_gl_callback(NULL, "glEnableClientStateIndexedEXT", (GLADapiproc) glad_glEnableClientStateIndexedEXT, 2, array, index);
    
}
PFNGLENABLECLIENTSTATEINDEXEDEXTPROC glad_debug_glEnableClientStateIndexedEXT = glad_debug_impl_glEnableClientStateIndexedEXT;
PFNGLENABLECLIENTSTATEIEXTPROC glad_glEnableClientStateiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableClientStateiEXT(GLenum array, GLuint index) {
    _pre_call_gl_callback("glEnableClientStateiEXT", (GLADapiproc) glad_glEnableClientStateiEXT, 2, array, index);
    glad_glEnableClientStateiEXT(array, index);
    _post_call_gl_callback(NULL, "glEnableClientStateiEXT", (GLADapiproc) glad_glEnableClientStateiEXT, 2, array, index);
    
}
PFNGLENABLECLIENTSTATEIEXTPROC glad_debug_glEnableClientStateiEXT = glad_debug_impl_glEnableClientStateiEXT;
PFNGLENABLEINDEXEDEXTPROC glad_glEnableIndexedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableIndexedEXT(GLenum target, GLuint index) {
    _pre_call_gl_callback("glEnableIndexedEXT", (GLADapiproc) glad_glEnableIndexedEXT, 2, target, index);
    glad_glEnableIndexedEXT(target, index);
    _post_call_gl_callback(NULL, "glEnableIndexedEXT", (GLADapiproc) glad_glEnableIndexedEXT, 2, target, index);
    
}
PFNGLENABLEINDEXEDEXTPROC glad_debug_glEnableIndexedEXT = glad_debug_impl_glEnableIndexedEXT;
PFNGLENABLEVERTEXARRAYATTRIBEXTPROC glad_glEnableVertexArrayAttribEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index) {
    _pre_call_gl_callback("glEnableVertexArrayAttribEXT", (GLADapiproc) glad_glEnableVertexArrayAttribEXT, 2, vaobj, index);
    glad_glEnableVertexArrayAttribEXT(vaobj, index);
    _post_call_gl_callback(NULL, "glEnableVertexArrayAttribEXT", (GLADapiproc) glad_glEnableVertexArrayAttribEXT, 2, vaobj, index);
    
}
PFNGLENABLEVERTEXARRAYATTRIBEXTPROC glad_debug_glEnableVertexArrayAttribEXT = glad_debug_impl_glEnableVertexArrayAttribEXT;
PFNGLENABLEVERTEXARRAYEXTPROC glad_glEnableVertexArrayEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableVertexArrayEXT(GLuint vaobj, GLenum array) {
    _pre_call_gl_callback("glEnableVertexArrayEXT", (GLADapiproc) glad_glEnableVertexArrayEXT, 2, vaobj, array);
    glad_glEnableVertexArrayEXT(vaobj, array);
    _post_call_gl_callback(NULL, "glEnableVertexArrayEXT", (GLADapiproc) glad_glEnableVertexArrayEXT, 2, vaobj, array);
    
}
PFNGLENABLEVERTEXARRAYEXTPROC glad_debug_glEnableVertexArrayEXT = glad_debug_impl_glEnableVertexArrayEXT;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableVertexAttribArray(GLuint index) {
    _pre_call_gl_callback("glEnableVertexAttribArray", (GLADapiproc) glad_glEnableVertexAttribArray, 1, index);
    glad_glEnableVertexAttribArray(index);
    _post_call_gl_callback(NULL, "glEnableVertexAttribArray", (GLADapiproc) glad_glEnableVertexAttribArray, 1, index);
    
}
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_debug_glEnableVertexAttribArray = glad_debug_impl_glEnableVertexAttribArray;
PFNGLENABLEVERTEXATTRIBARRAYARBPROC glad_glEnableVertexAttribArrayARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableVertexAttribArrayARB(GLuint index) {
    _pre_call_gl_callback("glEnableVertexAttribArrayARB", (GLADapiproc) glad_glEnableVertexAttribArrayARB, 1, index);
    glad_glEnableVertexAttribArrayARB(index);
    _post_call_gl_callback(NULL, "glEnableVertexAttribArrayARB", (GLADapiproc) glad_glEnableVertexAttribArrayARB, 1, index);
    
}
PFNGLENABLEVERTEXATTRIBARRAYARBPROC glad_debug_glEnableVertexAttribArrayARB = glad_debug_impl_glEnableVertexAttribArrayARB;
PFNGLENABLEIPROC glad_glEnablei = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnablei(GLenum target, GLuint index) {
    _pre_call_gl_callback("glEnablei", (GLADapiproc) glad_glEnablei, 2, target, index);
    glad_glEnablei(target, index);
    _post_call_gl_callback(NULL, "glEnablei", (GLADapiproc) glad_glEnablei, 2, target, index);
    
}
PFNGLENABLEIPROC glad_debug_glEnablei = glad_debug_impl_glEnablei;
PFNGLENDCONDITIONALRENDERPROC glad_glEndConditionalRender = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndConditionalRender(void) {
    _pre_call_gl_callback("glEndConditionalRender", (GLADapiproc) glad_glEndConditionalRender, 0);
    glad_glEndConditionalRender();
    _post_call_gl_callback(NULL, "glEndConditionalRender", (GLADapiproc) glad_glEndConditionalRender, 0);
    
}
PFNGLENDCONDITIONALRENDERPROC glad_debug_glEndConditionalRender = glad_debug_impl_glEndConditionalRender;
PFNGLENDCONDITIONALRENDERNVPROC glad_glEndConditionalRenderNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndConditionalRenderNV(void) {
    _pre_call_gl_callback("glEndConditionalRenderNV", (GLADapiproc) glad_glEndConditionalRenderNV, 0);
    glad_glEndConditionalRenderNV();
    _post_call_gl_callback(NULL, "glEndConditionalRenderNV", (GLADapiproc) glad_glEndConditionalRenderNV, 0);
    
}
PFNGLENDCONDITIONALRENDERNVPROC glad_debug_glEndConditionalRenderNV = glad_debug_impl_glEndConditionalRenderNV;
PFNGLENDCONDITIONALRENDERNVXPROC glad_glEndConditionalRenderNVX = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndConditionalRenderNVX(void) {
    _pre_call_gl_callback("glEndConditionalRenderNVX", (GLADapiproc) glad_glEndConditionalRenderNVX, 0);
    glad_glEndConditionalRenderNVX();
    _post_call_gl_callback(NULL, "glEndConditionalRenderNVX", (GLADapiproc) glad_glEndConditionalRenderNVX, 0);
    
}
PFNGLENDCONDITIONALRENDERNVXPROC glad_debug_glEndConditionalRenderNVX = glad_debug_impl_glEndConditionalRenderNVX;
PFNGLENDQUERYPROC glad_glEndQuery = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndQuery(GLenum target) {
    _pre_call_gl_callback("glEndQuery", (GLADapiproc) glad_glEndQuery, 1, target);
    glad_glEndQuery(target);
    _post_call_gl_callback(NULL, "glEndQuery", (GLADapiproc) glad_glEndQuery, 1, target);
    
}
PFNGLENDQUERYPROC glad_debug_glEndQuery = glad_debug_impl_glEndQuery;
PFNGLENDQUERYARBPROC glad_glEndQueryARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndQueryARB(GLenum target) {
    _pre_call_gl_callback("glEndQueryARB", (GLADapiproc) glad_glEndQueryARB, 1, target);
    glad_glEndQueryARB(target);
    _post_call_gl_callback(NULL, "glEndQueryARB", (GLADapiproc) glad_glEndQueryARB, 1, target);
    
}
PFNGLENDQUERYARBPROC glad_debug_glEndQueryARB = glad_debug_impl_glEndQueryARB;
PFNGLENDTRANSFORMFEEDBACKPROC glad_glEndTransformFeedback = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndTransformFeedback(void) {
    _pre_call_gl_callback("glEndTransformFeedback", (GLADapiproc) glad_glEndTransformFeedback, 0);
    glad_glEndTransformFeedback();
    _post_call_gl_callback(NULL, "glEndTransformFeedback", (GLADapiproc) glad_glEndTransformFeedback, 0);
    
}
PFNGLENDTRANSFORMFEEDBACKPROC glad_debug_glEndTransformFeedback = glad_debug_impl_glEndTransformFeedback;
PFNGLENDTRANSFORMFEEDBACKEXTPROC glad_glEndTransformFeedbackEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndTransformFeedbackEXT(void) {
    _pre_call_gl_callback("glEndTransformFeedbackEXT", (GLADapiproc) glad_glEndTransformFeedbackEXT, 0);
    glad_glEndTransformFeedbackEXT();
    _post_call_gl_callback(NULL, "glEndTransformFeedbackEXT", (GLADapiproc) glad_glEndTransformFeedbackEXT, 0);
    
}
PFNGLENDTRANSFORMFEEDBACKEXTPROC glad_debug_glEndTransformFeedbackEXT = glad_debug_impl_glEndTransformFeedbackEXT;
PFNGLENDTRANSFORMFEEDBACKNVPROC glad_glEndTransformFeedbackNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndTransformFeedbackNV(void) {
    _pre_call_gl_callback("glEndTransformFeedbackNV", (GLADapiproc) glad_glEndTransformFeedbackNV, 0);
    glad_glEndTransformFeedbackNV();
    _post_call_gl_callback(NULL, "glEndTransformFeedbackNV", (GLADapiproc) glad_glEndTransformFeedbackNV, 0);
    
}
PFNGLENDTRANSFORMFEEDBACKNVPROC glad_debug_glEndTransformFeedbackNV = glad_debug_impl_glEndTransformFeedbackNV;
PFNGLEXECUTEPROGRAMNVPROC glad_glExecuteProgramNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glExecuteProgramNV(GLenum target, GLuint id, const GLfloat * params) {
    _pre_call_gl_callback("glExecuteProgramNV", (GLADapiproc) glad_glExecuteProgramNV, 3, target, id, params);
    glad_glExecuteProgramNV(target, id, params);
    _post_call_gl_callback(NULL, "glExecuteProgramNV", (GLADapiproc) glad_glExecuteProgramNV, 3, target, id, params);
    
}
PFNGLEXECUTEPROGRAMNVPROC glad_debug_glExecuteProgramNV = glad_debug_impl_glExecuteProgramNV;
PFNGLFENCESYNCPROC glad_glFenceSync = NULL;
static GLsync GLAD_API_PTR glad_debug_impl_glFenceSync(GLenum condition, GLbitfield flags) {
    GLsync ret;
    _pre_call_gl_callback("glFenceSync", (GLADapiproc) glad_glFenceSync, 2, condition, flags);
    ret = glad_glFenceSync(condition, flags);
    _post_call_gl_callback((void*) &ret, "glFenceSync", (GLADapiproc) glad_glFenceSync, 2, condition, flags);
    return ret;
}
PFNGLFENCESYNCPROC glad_debug_glFenceSync = glad_debug_impl_glFenceSync;
PFNGLFINISHPROC glad_glFinish = NULL;
static void GLAD_API_PTR glad_debug_impl_glFinish(void) {
    _pre_call_gl_callback("glFinish", (GLADapiproc) glad_glFinish, 0);
    glad_glFinish();
    _post_call_gl_callback(NULL, "glFinish", (GLADapiproc) glad_glFinish, 0);
    
}
PFNGLFINISHPROC glad_debug_glFinish = glad_debug_impl_glFinish;
PFNGLFLUSHPROC glad_glFlush = NULL;
static void GLAD_API_PTR glad_debug_impl_glFlush(void) {
    _pre_call_gl_callback("glFlush", (GLADapiproc) glad_glFlush, 0);
    glad_glFlush();
    _post_call_gl_callback(NULL, "glFlush", (GLADapiproc) glad_glFlush, 0);
    
}
PFNGLFLUSHPROC glad_debug_glFlush = glad_debug_impl_glFlush;
PFNGLFLUSHMAPPEDBUFFERRANGEPROC glad_glFlushMappedBufferRange = NULL;
static void GLAD_API_PTR glad_debug_impl_glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    _pre_call_gl_callback("glFlushMappedBufferRange", (GLADapiproc) glad_glFlushMappedBufferRange, 3, target, offset, length);
    glad_glFlushMappedBufferRange(target, offset, length);
    _post_call_gl_callback(NULL, "glFlushMappedBufferRange", (GLADapiproc) glad_glFlushMappedBufferRange, 3, target, offset, length);
    
}
PFNGLFLUSHMAPPEDBUFFERRANGEPROC glad_debug_glFlushMappedBufferRange = glad_debug_impl_glFlushMappedBufferRange;
PFNGLFLUSHMAPPEDBUFFERRANGEAPPLEPROC glad_glFlushMappedBufferRangeAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glFlushMappedBufferRangeAPPLE(GLenum target, GLintptr offset, GLsizeiptr size) {
    _pre_call_gl_callback("glFlushMappedBufferRangeAPPLE", (GLADapiproc) glad_glFlushMappedBufferRangeAPPLE, 3, target, offset, size);
    glad_glFlushMappedBufferRangeAPPLE(target, offset, size);
    _post_call_gl_callback(NULL, "glFlushMappedBufferRangeAPPLE", (GLADapiproc) glad_glFlushMappedBufferRangeAPPLE, 3, target, offset, size);
    
}
PFNGLFLUSHMAPPEDBUFFERRANGEAPPLEPROC glad_debug_glFlushMappedBufferRangeAPPLE = glad_debug_impl_glFlushMappedBufferRangeAPPLE;
PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEEXTPROC glad_glFlushMappedNamedBufferRangeEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFlushMappedNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length) {
    _pre_call_gl_callback("glFlushMappedNamedBufferRangeEXT", (GLADapiproc) glad_glFlushMappedNamedBufferRangeEXT, 3, buffer, offset, length);
    glad_glFlushMappedNamedBufferRangeEXT(buffer, offset, length);
    _post_call_gl_callback(NULL, "glFlushMappedNamedBufferRangeEXT", (GLADapiproc) glad_glFlushMappedNamedBufferRangeEXT, 3, buffer, offset, length);
    
}
PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEEXTPROC glad_debug_glFlushMappedNamedBufferRangeEXT = glad_debug_impl_glFlushMappedNamedBufferRangeEXT;
PFNGLFRAMEBUFFERDRAWBUFFEREXTPROC glad_glFramebufferDrawBufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferDrawBufferEXT(GLuint framebuffer, GLenum mode) {
    _pre_call_gl_callback("glFramebufferDrawBufferEXT", (GLADapiproc) glad_glFramebufferDrawBufferEXT, 2, framebuffer, mode);
    glad_glFramebufferDrawBufferEXT(framebuffer, mode);
    _post_call_gl_callback(NULL, "glFramebufferDrawBufferEXT", (GLADapiproc) glad_glFramebufferDrawBufferEXT, 2, framebuffer, mode);
    
}
PFNGLFRAMEBUFFERDRAWBUFFEREXTPROC glad_debug_glFramebufferDrawBufferEXT = glad_debug_impl_glFramebufferDrawBufferEXT;
PFNGLFRAMEBUFFERDRAWBUFFERSEXTPROC glad_glFramebufferDrawBuffersEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum * bufs) {
    _pre_call_gl_callback("glFramebufferDrawBuffersEXT", (GLADapiproc) glad_glFramebufferDrawBuffersEXT, 3, framebuffer, n, bufs);
    glad_glFramebufferDrawBuffersEXT(framebuffer, n, bufs);
    _post_call_gl_callback(NULL, "glFramebufferDrawBuffersEXT", (GLADapiproc) glad_glFramebufferDrawBuffersEXT, 3, framebuffer, n, bufs);
    
}
PFNGLFRAMEBUFFERDRAWBUFFERSEXTPROC glad_debug_glFramebufferDrawBuffersEXT = glad_debug_impl_glFramebufferDrawBuffersEXT;
PFNGLFRAMEBUFFERREADBUFFEREXTPROC glad_glFramebufferReadBufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferReadBufferEXT(GLuint framebuffer, GLenum mode) {
    _pre_call_gl_callback("glFramebufferReadBufferEXT", (GLADapiproc) glad_glFramebufferReadBufferEXT, 2, framebuffer, mode);
    glad_glFramebufferReadBufferEXT(framebuffer, mode);
    _post_call_gl_callback(NULL, "glFramebufferReadBufferEXT", (GLADapiproc) glad_glFramebufferReadBufferEXT, 2, framebuffer, mode);
    
}
PFNGLFRAMEBUFFERREADBUFFEREXTPROC glad_debug_glFramebufferReadBufferEXT = glad_debug_impl_glFramebufferReadBufferEXT;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    _pre_call_gl_callback("glFramebufferRenderbuffer", (GLADapiproc) glad_glFramebufferRenderbuffer, 4, target, attachment, renderbuffertarget, renderbuffer);
    glad_glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
    _post_call_gl_callback(NULL, "glFramebufferRenderbuffer", (GLADapiproc) glad_glFramebufferRenderbuffer, 4, target, attachment, renderbuffertarget, renderbuffer);
    
}
PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_debug_glFramebufferRenderbuffer = glad_debug_impl_glFramebufferRenderbuffer;
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glad_glFramebufferRenderbufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    _pre_call_gl_callback("glFramebufferRenderbufferEXT", (GLADapiproc) glad_glFramebufferRenderbufferEXT, 4, target, attachment, renderbuffertarget, renderbuffer);
    glad_glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, renderbuffer);
    _post_call_gl_callback(NULL, "glFramebufferRenderbufferEXT", (GLADapiproc) glad_glFramebufferRenderbufferEXT, 4, target, attachment, renderbuffertarget, renderbuffer);
    
}
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glad_debug_glFramebufferRenderbufferEXT = glad_debug_impl_glFramebufferRenderbufferEXT;
PFNGLFRAMEBUFFERTEXTUREPROC glad_glFramebufferTexture = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    _pre_call_gl_callback("glFramebufferTexture", (GLADapiproc) glad_glFramebufferTexture, 4, target, attachment, texture, level);
    glad_glFramebufferTexture(target, attachment, texture, level);
    _post_call_gl_callback(NULL, "glFramebufferTexture", (GLADapiproc) glad_glFramebufferTexture, 4, target, attachment, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTUREPROC glad_debug_glFramebufferTexture = glad_debug_impl_glFramebufferTexture;
PFNGLFRAMEBUFFERTEXTURE1DPROC glad_glFramebufferTexture1D = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    _pre_call_gl_callback("glFramebufferTexture1D", (GLADapiproc) glad_glFramebufferTexture1D, 5, target, attachment, textarget, texture, level);
    glad_glFramebufferTexture1D(target, attachment, textarget, texture, level);
    _post_call_gl_callback(NULL, "glFramebufferTexture1D", (GLADapiproc) glad_glFramebufferTexture1D, 5, target, attachment, textarget, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTURE1DPROC glad_debug_glFramebufferTexture1D = glad_debug_impl_glFramebufferTexture1D;
PFNGLFRAMEBUFFERTEXTURE1DEXTPROC glad_glFramebufferTexture1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    _pre_call_gl_callback("glFramebufferTexture1DEXT", (GLADapiproc) glad_glFramebufferTexture1DEXT, 5, target, attachment, textarget, texture, level);
    glad_glFramebufferTexture1DEXT(target, attachment, textarget, texture, level);
    _post_call_gl_callback(NULL, "glFramebufferTexture1DEXT", (GLADapiproc) glad_glFramebufferTexture1DEXT, 5, target, attachment, textarget, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTURE1DEXTPROC glad_debug_glFramebufferTexture1DEXT = glad_debug_impl_glFramebufferTexture1DEXT;
PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    _pre_call_gl_callback("glFramebufferTexture2D", (GLADapiproc) glad_glFramebufferTexture2D, 5, target, attachment, textarget, texture, level);
    glad_glFramebufferTexture2D(target, attachment, textarget, texture, level);
    _post_call_gl_callback(NULL, "glFramebufferTexture2D", (GLADapiproc) glad_glFramebufferTexture2D, 5, target, attachment, textarget, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTURE2DPROC glad_debug_glFramebufferTexture2D = glad_debug_impl_glFramebufferTexture2D;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glad_glFramebufferTexture2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    _pre_call_gl_callback("glFramebufferTexture2DEXT", (GLADapiproc) glad_glFramebufferTexture2DEXT, 5, target, attachment, textarget, texture, level);
    glad_glFramebufferTexture2DEXT(target, attachment, textarget, texture, level);
    _post_call_gl_callback(NULL, "glFramebufferTexture2DEXT", (GLADapiproc) glad_glFramebufferTexture2DEXT, 5, target, attachment, textarget, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glad_debug_glFramebufferTexture2DEXT = glad_debug_impl_glFramebufferTexture2DEXT;
PFNGLFRAMEBUFFERTEXTURE3DPROC glad_glFramebufferTexture3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) {
    _pre_call_gl_callback("glFramebufferTexture3D", (GLADapiproc) glad_glFramebufferTexture3D, 6, target, attachment, textarget, texture, level, zoffset);
    glad_glFramebufferTexture3D(target, attachment, textarget, texture, level, zoffset);
    _post_call_gl_callback(NULL, "glFramebufferTexture3D", (GLADapiproc) glad_glFramebufferTexture3D, 6, target, attachment, textarget, texture, level, zoffset);
    
}
PFNGLFRAMEBUFFERTEXTURE3DPROC glad_debug_glFramebufferTexture3D = glad_debug_impl_glFramebufferTexture3D;
PFNGLFRAMEBUFFERTEXTURE3DEXTPROC glad_glFramebufferTexture3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) {
    _pre_call_gl_callback("glFramebufferTexture3DEXT", (GLADapiproc) glad_glFramebufferTexture3DEXT, 6, target, attachment, textarget, texture, level, zoffset);
    glad_glFramebufferTexture3DEXT(target, attachment, textarget, texture, level, zoffset);
    _post_call_gl_callback(NULL, "glFramebufferTexture3DEXT", (GLADapiproc) glad_glFramebufferTexture3DEXT, 6, target, attachment, textarget, texture, level, zoffset);
    
}
PFNGLFRAMEBUFFERTEXTURE3DEXTPROC glad_debug_glFramebufferTexture3DEXT = glad_debug_impl_glFramebufferTexture3DEXT;
PFNGLFRAMEBUFFERTEXTUREARBPROC glad_glFramebufferTextureARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureARB(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    _pre_call_gl_callback("glFramebufferTextureARB", (GLADapiproc) glad_glFramebufferTextureARB, 4, target, attachment, texture, level);
    glad_glFramebufferTextureARB(target, attachment, texture, level);
    _post_call_gl_callback(NULL, "glFramebufferTextureARB", (GLADapiproc) glad_glFramebufferTextureARB, 4, target, attachment, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTUREARBPROC glad_debug_glFramebufferTextureARB = glad_debug_impl_glFramebufferTextureARB;
PFNGLFRAMEBUFFERTEXTUREEXTPROC glad_glFramebufferTextureEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureEXT(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    _pre_call_gl_callback("glFramebufferTextureEXT", (GLADapiproc) glad_glFramebufferTextureEXT, 4, target, attachment, texture, level);
    glad_glFramebufferTextureEXT(target, attachment, texture, level);
    _post_call_gl_callback(NULL, "glFramebufferTextureEXT", (GLADapiproc) glad_glFramebufferTextureEXT, 4, target, attachment, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTUREEXTPROC glad_debug_glFramebufferTextureEXT = glad_debug_impl_glFramebufferTextureEXT;
PFNGLFRAMEBUFFERTEXTUREFACEARBPROC glad_glFramebufferTextureFaceARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureFaceARB(GLenum target, GLenum attachment, GLuint texture, GLint level, GLenum face) {
    _pre_call_gl_callback("glFramebufferTextureFaceARB", (GLADapiproc) glad_glFramebufferTextureFaceARB, 5, target, attachment, texture, level, face);
    glad_glFramebufferTextureFaceARB(target, attachment, texture, level, face);
    _post_call_gl_callback(NULL, "glFramebufferTextureFaceARB", (GLADapiproc) glad_glFramebufferTextureFaceARB, 5, target, attachment, texture, level, face);
    
}
PFNGLFRAMEBUFFERTEXTUREFACEARBPROC glad_debug_glFramebufferTextureFaceARB = glad_debug_impl_glFramebufferTextureFaceARB;
PFNGLFRAMEBUFFERTEXTUREFACEEXTPROC glad_glFramebufferTextureFaceEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureFaceEXT(GLenum target, GLenum attachment, GLuint texture, GLint level, GLenum face) {
    _pre_call_gl_callback("glFramebufferTextureFaceEXT", (GLADapiproc) glad_glFramebufferTextureFaceEXT, 5, target, attachment, texture, level, face);
    glad_glFramebufferTextureFaceEXT(target, attachment, texture, level, face);
    _post_call_gl_callback(NULL, "glFramebufferTextureFaceEXT", (GLADapiproc) glad_glFramebufferTextureFaceEXT, 5, target, attachment, texture, level, face);
    
}
PFNGLFRAMEBUFFERTEXTUREFACEEXTPROC glad_debug_glFramebufferTextureFaceEXT = glad_debug_impl_glFramebufferTextureFaceEXT;
PFNGLFRAMEBUFFERTEXTURELAYERPROC glad_glFramebufferTextureLayer = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    _pre_call_gl_callback("glFramebufferTextureLayer", (GLADapiproc) glad_glFramebufferTextureLayer, 5, target, attachment, texture, level, layer);
    glad_glFramebufferTextureLayer(target, attachment, texture, level, layer);
    _post_call_gl_callback(NULL, "glFramebufferTextureLayer", (GLADapiproc) glad_glFramebufferTextureLayer, 5, target, attachment, texture, level, layer);
    
}
PFNGLFRAMEBUFFERTEXTURELAYERPROC glad_debug_glFramebufferTextureLayer = glad_debug_impl_glFramebufferTextureLayer;
PFNGLFRAMEBUFFERTEXTURELAYERARBPROC glad_glFramebufferTextureLayerARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureLayerARB(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    _pre_call_gl_callback("glFramebufferTextureLayerARB", (GLADapiproc) glad_glFramebufferTextureLayerARB, 5, target, attachment, texture, level, layer);
    glad_glFramebufferTextureLayerARB(target, attachment, texture, level, layer);
    _post_call_gl_callback(NULL, "glFramebufferTextureLayerARB", (GLADapiproc) glad_glFramebufferTextureLayerARB, 5, target, attachment, texture, level, layer);
    
}
PFNGLFRAMEBUFFERTEXTURELAYERARBPROC glad_debug_glFramebufferTextureLayerARB = glad_debug_impl_glFramebufferTextureLayerARB;
PFNGLFRAMEBUFFERTEXTURELAYEREXTPROC glad_glFramebufferTextureLayerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureLayerEXT(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    _pre_call_gl_callback("glFramebufferTextureLayerEXT", (GLADapiproc) glad_glFramebufferTextureLayerEXT, 5, target, attachment, texture, level, layer);
    glad_glFramebufferTextureLayerEXT(target, attachment, texture, level, layer);
    _post_call_gl_callback(NULL, "glFramebufferTextureLayerEXT", (GLADapiproc) glad_glFramebufferTextureLayerEXT, 5, target, attachment, texture, level, layer);
    
}
PFNGLFRAMEBUFFERTEXTURELAYEREXTPROC glad_debug_glFramebufferTextureLayerEXT = glad_debug_impl_glFramebufferTextureLayerEXT;
PFNGLFRONTFACEPROC glad_glFrontFace = NULL;
static void GLAD_API_PTR glad_debug_impl_glFrontFace(GLenum mode) {
    _pre_call_gl_callback("glFrontFace", (GLADapiproc) glad_glFrontFace, 1, mode);
    glad_glFrontFace(mode);
    _post_call_gl_callback(NULL, "glFrontFace", (GLADapiproc) glad_glFrontFace, 1, mode);
    
}
PFNGLFRONTFACEPROC glad_debug_glFrontFace = glad_debug_impl_glFrontFace;
PFNGLGENBUFFERSPROC glad_glGenBuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenBuffers(GLsizei n, GLuint * buffers) {
    _pre_call_gl_callback("glGenBuffers", (GLADapiproc) glad_glGenBuffers, 2, n, buffers);
    glad_glGenBuffers(n, buffers);
    _post_call_gl_callback(NULL, "glGenBuffers", (GLADapiproc) glad_glGenBuffers, 2, n, buffers);
    
}
PFNGLGENBUFFERSPROC glad_debug_glGenBuffers = glad_debug_impl_glGenBuffers;
PFNGLGENBUFFERSARBPROC glad_glGenBuffersARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenBuffersARB(GLsizei n, GLuint * buffers) {
    _pre_call_gl_callback("glGenBuffersARB", (GLADapiproc) glad_glGenBuffersARB, 2, n, buffers);
    glad_glGenBuffersARB(n, buffers);
    _post_call_gl_callback(NULL, "glGenBuffersARB", (GLADapiproc) glad_glGenBuffersARB, 2, n, buffers);
    
}
PFNGLGENBUFFERSARBPROC glad_debug_glGenBuffersARB = glad_debug_impl_glGenBuffersARB;
PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenFramebuffers(GLsizei n, GLuint * framebuffers) {
    _pre_call_gl_callback("glGenFramebuffers", (GLADapiproc) glad_glGenFramebuffers, 2, n, framebuffers);
    glad_glGenFramebuffers(n, framebuffers);
    _post_call_gl_callback(NULL, "glGenFramebuffers", (GLADapiproc) glad_glGenFramebuffers, 2, n, framebuffers);
    
}
PFNGLGENFRAMEBUFFERSPROC glad_debug_glGenFramebuffers = glad_debug_impl_glGenFramebuffers;
PFNGLGENFRAMEBUFFERSEXTPROC glad_glGenFramebuffersEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenFramebuffersEXT(GLsizei n, GLuint * framebuffers) {
    _pre_call_gl_callback("glGenFramebuffersEXT", (GLADapiproc) glad_glGenFramebuffersEXT, 2, n, framebuffers);
    glad_glGenFramebuffersEXT(n, framebuffers);
    _post_call_gl_callback(NULL, "glGenFramebuffersEXT", (GLADapiproc) glad_glGenFramebuffersEXT, 2, n, framebuffers);
    
}
PFNGLGENFRAMEBUFFERSEXTPROC glad_debug_glGenFramebuffersEXT = glad_debug_impl_glGenFramebuffersEXT;
PFNGLGENPROGRAMSARBPROC glad_glGenProgramsARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenProgramsARB(GLsizei n, GLuint * programs) {
    _pre_call_gl_callback("glGenProgramsARB", (GLADapiproc) glad_glGenProgramsARB, 2, n, programs);
    glad_glGenProgramsARB(n, programs);
    _post_call_gl_callback(NULL, "glGenProgramsARB", (GLADapiproc) glad_glGenProgramsARB, 2, n, programs);
    
}
PFNGLGENPROGRAMSARBPROC glad_debug_glGenProgramsARB = glad_debug_impl_glGenProgramsARB;
PFNGLGENPROGRAMSNVPROC glad_glGenProgramsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenProgramsNV(GLsizei n, GLuint * programs) {
    _pre_call_gl_callback("glGenProgramsNV", (GLADapiproc) glad_glGenProgramsNV, 2, n, programs);
    glad_glGenProgramsNV(n, programs);
    _post_call_gl_callback(NULL, "glGenProgramsNV", (GLADapiproc) glad_glGenProgramsNV, 2, n, programs);
    
}
PFNGLGENPROGRAMSNVPROC glad_debug_glGenProgramsNV = glad_debug_impl_glGenProgramsNV;
PFNGLGENQUERIESPROC glad_glGenQueries = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenQueries(GLsizei n, GLuint * ids) {
    _pre_call_gl_callback("glGenQueries", (GLADapiproc) glad_glGenQueries, 2, n, ids);
    glad_glGenQueries(n, ids);
    _post_call_gl_callback(NULL, "glGenQueries", (GLADapiproc) glad_glGenQueries, 2, n, ids);
    
}
PFNGLGENQUERIESPROC glad_debug_glGenQueries = glad_debug_impl_glGenQueries;
PFNGLGENQUERIESARBPROC glad_glGenQueriesARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenQueriesARB(GLsizei n, GLuint * ids) {
    _pre_call_gl_callback("glGenQueriesARB", (GLADapiproc) glad_glGenQueriesARB, 2, n, ids);
    glad_glGenQueriesARB(n, ids);
    _post_call_gl_callback(NULL, "glGenQueriesARB", (GLADapiproc) glad_glGenQueriesARB, 2, n, ids);
    
}
PFNGLGENQUERIESARBPROC glad_debug_glGenQueriesARB = glad_debug_impl_glGenQueriesARB;
PFNGLGENRENDERBUFFERSPROC glad_glGenRenderbuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenRenderbuffers(GLsizei n, GLuint * renderbuffers) {
    _pre_call_gl_callback("glGenRenderbuffers", (GLADapiproc) glad_glGenRenderbuffers, 2, n, renderbuffers);
    glad_glGenRenderbuffers(n, renderbuffers);
    _post_call_gl_callback(NULL, "glGenRenderbuffers", (GLADapiproc) glad_glGenRenderbuffers, 2, n, renderbuffers);
    
}
PFNGLGENRENDERBUFFERSPROC glad_debug_glGenRenderbuffers = glad_debug_impl_glGenRenderbuffers;
PFNGLGENRENDERBUFFERSEXTPROC glad_glGenRenderbuffersEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenRenderbuffersEXT(GLsizei n, GLuint * renderbuffers) {
    _pre_call_gl_callback("glGenRenderbuffersEXT", (GLADapiproc) glad_glGenRenderbuffersEXT, 2, n, renderbuffers);
    glad_glGenRenderbuffersEXT(n, renderbuffers);
    _post_call_gl_callback(NULL, "glGenRenderbuffersEXT", (GLADapiproc) glad_glGenRenderbuffersEXT, 2, n, renderbuffers);
    
}
PFNGLGENRENDERBUFFERSEXTPROC glad_debug_glGenRenderbuffersEXT = glad_debug_impl_glGenRenderbuffersEXT;
PFNGLGENSAMPLERSPROC glad_glGenSamplers = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenSamplers(GLsizei count, GLuint * samplers) {
    _pre_call_gl_callback("glGenSamplers", (GLADapiproc) glad_glGenSamplers, 2, count, samplers);
    glad_glGenSamplers(count, samplers);
    _post_call_gl_callback(NULL, "glGenSamplers", (GLADapiproc) glad_glGenSamplers, 2, count, samplers);
    
}
PFNGLGENSAMPLERSPROC glad_debug_glGenSamplers = glad_debug_impl_glGenSamplers;
PFNGLGENTEXTURESPROC glad_glGenTextures = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenTextures(GLsizei n, GLuint * textures) {
    _pre_call_gl_callback("glGenTextures", (GLADapiproc) glad_glGenTextures, 2, n, textures);
    glad_glGenTextures(n, textures);
    _post_call_gl_callback(NULL, "glGenTextures", (GLADapiproc) glad_glGenTextures, 2, n, textures);
    
}
PFNGLGENTEXTURESPROC glad_debug_glGenTextures = glad_debug_impl_glGenTextures;
PFNGLGENTEXTURESEXTPROC glad_glGenTexturesEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenTexturesEXT(GLsizei n, GLuint * textures) {
    _pre_call_gl_callback("glGenTexturesEXT", (GLADapiproc) glad_glGenTexturesEXT, 2, n, textures);
    glad_glGenTexturesEXT(n, textures);
    _post_call_gl_callback(NULL, "glGenTexturesEXT", (GLADapiproc) glad_glGenTexturesEXT, 2, n, textures);
    
}
PFNGLGENTEXTURESEXTPROC glad_debug_glGenTexturesEXT = glad_debug_impl_glGenTexturesEXT;
PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenVertexArrays(GLsizei n, GLuint * arrays) {
    _pre_call_gl_callback("glGenVertexArrays", (GLADapiproc) glad_glGenVertexArrays, 2, n, arrays);
    glad_glGenVertexArrays(n, arrays);
    _post_call_gl_callback(NULL, "glGenVertexArrays", (GLADapiproc) glad_glGenVertexArrays, 2, n, arrays);
    
}
PFNGLGENVERTEXARRAYSPROC glad_debug_glGenVertexArrays = glad_debug_impl_glGenVertexArrays;
PFNGLGENVERTEXARRAYSAPPLEPROC glad_glGenVertexArraysAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenVertexArraysAPPLE(GLsizei n, GLuint * arrays) {
    _pre_call_gl_callback("glGenVertexArraysAPPLE", (GLADapiproc) glad_glGenVertexArraysAPPLE, 2, n, arrays);
    glad_glGenVertexArraysAPPLE(n, arrays);
    _post_call_gl_callback(NULL, "glGenVertexArraysAPPLE", (GLADapiproc) glad_glGenVertexArraysAPPLE, 2, n, arrays);
    
}
PFNGLGENVERTEXARRAYSAPPLEPROC glad_debug_glGenVertexArraysAPPLE = glad_debug_impl_glGenVertexArraysAPPLE;
PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenerateMipmap(GLenum target) {
    _pre_call_gl_callback("glGenerateMipmap", (GLADapiproc) glad_glGenerateMipmap, 1, target);
    glad_glGenerateMipmap(target);
    _post_call_gl_callback(NULL, "glGenerateMipmap", (GLADapiproc) glad_glGenerateMipmap, 1, target);
    
}
PFNGLGENERATEMIPMAPPROC glad_debug_glGenerateMipmap = glad_debug_impl_glGenerateMipmap;
PFNGLGENERATEMIPMAPEXTPROC glad_glGenerateMipmapEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenerateMipmapEXT(GLenum target) {
    _pre_call_gl_callback("glGenerateMipmapEXT", (GLADapiproc) glad_glGenerateMipmapEXT, 1, target);
    glad_glGenerateMipmapEXT(target);
    _post_call_gl_callback(NULL, "glGenerateMipmapEXT", (GLADapiproc) glad_glGenerateMipmapEXT, 1, target);
    
}
PFNGLGENERATEMIPMAPEXTPROC glad_debug_glGenerateMipmapEXT = glad_debug_impl_glGenerateMipmapEXT;
PFNGLGENERATEMULTITEXMIPMAPEXTPROC glad_glGenerateMultiTexMipmapEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenerateMultiTexMipmapEXT(GLenum texunit, GLenum target) {
    _pre_call_gl_callback("glGenerateMultiTexMipmapEXT", (GLADapiproc) glad_glGenerateMultiTexMipmapEXT, 2, texunit, target);
    glad_glGenerateMultiTexMipmapEXT(texunit, target);
    _post_call_gl_callback(NULL, "glGenerateMultiTexMipmapEXT", (GLADapiproc) glad_glGenerateMultiTexMipmapEXT, 2, texunit, target);
    
}
PFNGLGENERATEMULTITEXMIPMAPEXTPROC glad_debug_glGenerateMultiTexMipmapEXT = glad_debug_impl_glGenerateMultiTexMipmapEXT;
PFNGLGENERATETEXTUREMIPMAPEXTPROC glad_glGenerateTextureMipmapEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenerateTextureMipmapEXT(GLuint texture, GLenum target) {
    _pre_call_gl_callback("glGenerateTextureMipmapEXT", (GLADapiproc) glad_glGenerateTextureMipmapEXT, 2, texture, target);
    glad_glGenerateTextureMipmapEXT(texture, target);
    _post_call_gl_callback(NULL, "glGenerateTextureMipmapEXT", (GLADapiproc) glad_glGenerateTextureMipmapEXT, 2, texture, target);
    
}
PFNGLGENERATETEXTUREMIPMAPEXTPROC glad_debug_glGenerateTextureMipmapEXT = glad_debug_impl_glGenerateTextureMipmapEXT;
PFNGLGETACTIVEATTRIBPROC glad_glGetActiveAttrib = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name) {
    _pre_call_gl_callback("glGetActiveAttrib", (GLADapiproc) glad_glGetActiveAttrib, 7, program, index, bufSize, length, size, type, name);
    glad_glGetActiveAttrib(program, index, bufSize, length, size, type, name);
    _post_call_gl_callback(NULL, "glGetActiveAttrib", (GLADapiproc) glad_glGetActiveAttrib, 7, program, index, bufSize, length, size, type, name);
    
}
PFNGLGETACTIVEATTRIBPROC glad_debug_glGetActiveAttrib = glad_debug_impl_glGetActiveAttrib;
PFNGLGETACTIVEATTRIBARBPROC glad_glGetActiveAttribARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveAttribARB(GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei * length, GLint * size, GLenum * type, GLcharARB * name) {
    _pre_call_gl_callback("glGetActiveAttribARB", (GLADapiproc) glad_glGetActiveAttribARB, 7, programObj, index, maxLength, length, size, type, name);
    glad_glGetActiveAttribARB(programObj, index, maxLength, length, size, type, name);
    _post_call_gl_callback(NULL, "glGetActiveAttribARB", (GLADapiproc) glad_glGetActiveAttribARB, 7, programObj, index, maxLength, length, size, type, name);
    
}
PFNGLGETACTIVEATTRIBARBPROC glad_debug_glGetActiveAttribARB = glad_debug_impl_glGetActiveAttribARB;
PFNGLGETACTIVEUNIFORMPROC glad_glGetActiveUniform = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name) {
    _pre_call_gl_callback("glGetActiveUniform", (GLADapiproc) glad_glGetActiveUniform, 7, program, index, bufSize, length, size, type, name);
    glad_glGetActiveUniform(program, index, bufSize, length, size, type, name);
    _post_call_gl_callback(NULL, "glGetActiveUniform", (GLADapiproc) glad_glGetActiveUniform, 7, program, index, bufSize, length, size, type, name);
    
}
PFNGLGETACTIVEUNIFORMPROC glad_debug_glGetActiveUniform = glad_debug_impl_glGetActiveUniform;
PFNGLGETACTIVEUNIFORMARBPROC glad_glGetActiveUniformARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveUniformARB(GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei * length, GLint * size, GLenum * type, GLcharARB * name) {
    _pre_call_gl_callback("glGetActiveUniformARB", (GLADapiproc) glad_glGetActiveUniformARB, 7, programObj, index, maxLength, length, size, type, name);
    glad_glGetActiveUniformARB(programObj, index, maxLength, length, size, type, name);
    _post_call_gl_callback(NULL, "glGetActiveUniformARB", (GLADapiproc) glad_glGetActiveUniformARB, 7, programObj, index, maxLength, length, size, type, name);
    
}
PFNGLGETACTIVEUNIFORMARBPROC glad_debug_glGetActiveUniformARB = glad_debug_impl_glGetActiveUniformARB;
PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glad_glGetActiveUniformBlockName = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei * length, GLchar * uniformBlockName) {
    _pre_call_gl_callback("glGetActiveUniformBlockName", (GLADapiproc) glad_glGetActiveUniformBlockName, 5, program, uniformBlockIndex, bufSize, length, uniformBlockName);
    glad_glGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
    _post_call_gl_callback(NULL, "glGetActiveUniformBlockName", (GLADapiproc) glad_glGetActiveUniformBlockName, 5, program, uniformBlockIndex, bufSize, length, uniformBlockName);
    
}
PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glad_debug_glGetActiveUniformBlockName = glad_debug_impl_glGetActiveUniformBlockName;
PFNGLGETACTIVEUNIFORMBLOCKIVPROC glad_glGetActiveUniformBlockiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetActiveUniformBlockiv", (GLADapiproc) glad_glGetActiveUniformBlockiv, 4, program, uniformBlockIndex, pname, params);
    glad_glGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
    _post_call_gl_callback(NULL, "glGetActiveUniformBlockiv", (GLADapiproc) glad_glGetActiveUniformBlockiv, 4, program, uniformBlockIndex, pname, params);
    
}
PFNGLGETACTIVEUNIFORMBLOCKIVPROC glad_debug_glGetActiveUniformBlockiv = glad_debug_impl_glGetActiveUniformBlockiv;
PFNGLGETACTIVEUNIFORMNAMEPROC glad_glGetActiveUniformName = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei * length, GLchar * uniformName) {
    _pre_call_gl_callback("glGetActiveUniformName", (GLADapiproc) glad_glGetActiveUniformName, 5, program, uniformIndex, bufSize, length, uniformName);
    glad_glGetActiveUniformName(program, uniformIndex, bufSize, length, uniformName);
    _post_call_gl_callback(NULL, "glGetActiveUniformName", (GLADapiproc) glad_glGetActiveUniformName, 5, program, uniformIndex, bufSize, length, uniformName);
    
}
PFNGLGETACTIVEUNIFORMNAMEPROC glad_debug_glGetActiveUniformName = glad_debug_impl_glGetActiveUniformName;
PFNGLGETACTIVEUNIFORMSIVPROC glad_glGetActiveUniformsiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint * uniformIndices, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetActiveUniformsiv", (GLADapiproc) glad_glGetActiveUniformsiv, 5, program, uniformCount, uniformIndices, pname, params);
    glad_glGetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
    _post_call_gl_callback(NULL, "glGetActiveUniformsiv", (GLADapiproc) glad_glGetActiveUniformsiv, 5, program, uniformCount, uniformIndices, pname, params);
    
}
PFNGLGETACTIVEUNIFORMSIVPROC glad_debug_glGetActiveUniformsiv = glad_debug_impl_glGetActiveUniformsiv;
PFNGLGETACTIVEVARYINGNVPROC glad_glGetActiveVaryingNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveVaryingNV(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLsizei * size, GLenum * type, GLchar * name) {
    _pre_call_gl_callback("glGetActiveVaryingNV", (GLADapiproc) glad_glGetActiveVaryingNV, 7, program, index, bufSize, length, size, type, name);
    glad_glGetActiveVaryingNV(program, index, bufSize, length, size, type, name);
    _post_call_gl_callback(NULL, "glGetActiveVaryingNV", (GLADapiproc) glad_glGetActiveVaryingNV, 7, program, index, bufSize, length, size, type, name);
    
}
PFNGLGETACTIVEVARYINGNVPROC glad_debug_glGetActiveVaryingNV = glad_debug_impl_glGetActiveVaryingNV;
PFNGLGETATTACHEDOBJECTSARBPROC glad_glGetAttachedObjectsARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetAttachedObjectsARB(GLhandleARB containerObj, GLsizei maxCount, GLsizei * count, GLhandleARB * obj) {
    _pre_call_gl_callback("glGetAttachedObjectsARB", (GLADapiproc) glad_glGetAttachedObjectsARB, 4, containerObj, maxCount, count, obj);
    glad_glGetAttachedObjectsARB(containerObj, maxCount, count, obj);
    _post_call_gl_callback(NULL, "glGetAttachedObjectsARB", (GLADapiproc) glad_glGetAttachedObjectsARB, 4, containerObj, maxCount, count, obj);
    
}
PFNGLGETATTACHEDOBJECTSARBPROC glad_debug_glGetAttachedObjectsARB = glad_debug_impl_glGetAttachedObjectsARB;
PFNGLGETATTACHEDSHADERSPROC glad_glGetAttachedShaders = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei * count, GLuint * shaders) {
    _pre_call_gl_callback("glGetAttachedShaders", (GLADapiproc) glad_glGetAttachedShaders, 4, program, maxCount, count, shaders);
    glad_glGetAttachedShaders(program, maxCount, count, shaders);
    _post_call_gl_callback(NULL, "glGetAttachedShaders", (GLADapiproc) glad_glGetAttachedShaders, 4, program, maxCount, count, shaders);
    
}
PFNGLGETATTACHEDSHADERSPROC glad_debug_glGetAttachedShaders = glad_debug_impl_glGetAttachedShaders;
PFNGLGETATTRIBLOCATIONPROC glad_glGetAttribLocation = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetAttribLocation(GLuint program, const GLchar * name) {
    GLint ret;
    _pre_call_gl_callback("glGetAttribLocation", (GLADapiproc) glad_glGetAttribLocation, 2, program, name);
    ret = glad_glGetAttribLocation(program, name);
    _post_call_gl_callback((void*) &ret, "glGetAttribLocation", (GLADapiproc) glad_glGetAttribLocation, 2, program, name);
    return ret;
}
PFNGLGETATTRIBLOCATIONPROC glad_debug_glGetAttribLocation = glad_debug_impl_glGetAttribLocation;
PFNGLGETATTRIBLOCATIONARBPROC glad_glGetAttribLocationARB = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetAttribLocationARB(GLhandleARB programObj, const GLcharARB * name) {
    GLint ret;
    _pre_call_gl_callback("glGetAttribLocationARB", (GLADapiproc) glad_glGetAttribLocationARB, 2, programObj, name);
    ret = glad_glGetAttribLocationARB(programObj, name);
    _post_call_gl_callback((void*) &ret, "glGetAttribLocationARB", (GLADapiproc) glad_glGetAttribLocationARB, 2, programObj, name);
    return ret;
}
PFNGLGETATTRIBLOCATIONARBPROC glad_debug_glGetAttribLocationARB = glad_debug_impl_glGetAttribLocationARB;
PFNGLGETBOOLEANINDEXEDVEXTPROC glad_glGetBooleanIndexedvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBooleanIndexedvEXT(GLenum target, GLuint index, GLboolean * data) {
    _pre_call_gl_callback("glGetBooleanIndexedvEXT", (GLADapiproc) glad_glGetBooleanIndexedvEXT, 3, target, index, data);
    glad_glGetBooleanIndexedvEXT(target, index, data);
    _post_call_gl_callback(NULL, "glGetBooleanIndexedvEXT", (GLADapiproc) glad_glGetBooleanIndexedvEXT, 3, target, index, data);
    
}
PFNGLGETBOOLEANINDEXEDVEXTPROC glad_debug_glGetBooleanIndexedvEXT = glad_debug_impl_glGetBooleanIndexedvEXT;
PFNGLGETBOOLEANI_VPROC glad_glGetBooleani_v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBooleani_v(GLenum target, GLuint index, GLboolean * data) {
    _pre_call_gl_callback("glGetBooleani_v", (GLADapiproc) glad_glGetBooleani_v, 3, target, index, data);
    glad_glGetBooleani_v(target, index, data);
    _post_call_gl_callback(NULL, "glGetBooleani_v", (GLADapiproc) glad_glGetBooleani_v, 3, target, index, data);
    
}
PFNGLGETBOOLEANI_VPROC glad_debug_glGetBooleani_v = glad_debug_impl_glGetBooleani_v;
PFNGLGETBOOLEANVPROC glad_glGetBooleanv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBooleanv(GLenum pname, GLboolean * data) {
    _pre_call_gl_callback("glGetBooleanv", (GLADapiproc) glad_glGetBooleanv, 2, pname, data);
    glad_glGetBooleanv(pname, data);
    _post_call_gl_callback(NULL, "glGetBooleanv", (GLADapiproc) glad_glGetBooleanv, 2, pname, data);
    
}
PFNGLGETBOOLEANVPROC glad_debug_glGetBooleanv = glad_debug_impl_glGetBooleanv;
PFNGLGETBUFFERPARAMETERI64VPROC glad_glGetBufferParameteri64v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 * params) {
    _pre_call_gl_callback("glGetBufferParameteri64v", (GLADapiproc) glad_glGetBufferParameteri64v, 3, target, pname, params);
    glad_glGetBufferParameteri64v(target, pname, params);
    _post_call_gl_callback(NULL, "glGetBufferParameteri64v", (GLADapiproc) glad_glGetBufferParameteri64v, 3, target, pname, params);
    
}
PFNGLGETBUFFERPARAMETERI64VPROC glad_debug_glGetBufferParameteri64v = glad_debug_impl_glGetBufferParameteri64v;
PFNGLGETBUFFERPARAMETERIVPROC glad_glGetBufferParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetBufferParameteriv", (GLADapiproc) glad_glGetBufferParameteriv, 3, target, pname, params);
    glad_glGetBufferParameteriv(target, pname, params);
    _post_call_gl_callback(NULL, "glGetBufferParameteriv", (GLADapiproc) glad_glGetBufferParameteriv, 3, target, pname, params);
    
}
PFNGLGETBUFFERPARAMETERIVPROC glad_debug_glGetBufferParameteriv = glad_debug_impl_glGetBufferParameteriv;
PFNGLGETBUFFERPARAMETERIVARBPROC glad_glGetBufferParameterivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferParameterivARB(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetBufferParameterivARB", (GLADapiproc) glad_glGetBufferParameterivARB, 3, target, pname, params);
    glad_glGetBufferParameterivARB(target, pname, params);
    _post_call_gl_callback(NULL, "glGetBufferParameterivARB", (GLADapiproc) glad_glGetBufferParameterivARB, 3, target, pname, params);
    
}
PFNGLGETBUFFERPARAMETERIVARBPROC glad_debug_glGetBufferParameterivARB = glad_debug_impl_glGetBufferParameterivARB;
PFNGLGETBUFFERPOINTERVPROC glad_glGetBufferPointerv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferPointerv(GLenum target, GLenum pname, void ** params) {
    _pre_call_gl_callback("glGetBufferPointerv", (GLADapiproc) glad_glGetBufferPointerv, 3, target, pname, params);
    glad_glGetBufferPointerv(target, pname, params);
    _post_call_gl_callback(NULL, "glGetBufferPointerv", (GLADapiproc) glad_glGetBufferPointerv, 3, target, pname, params);
    
}
PFNGLGETBUFFERPOINTERVPROC glad_debug_glGetBufferPointerv = glad_debug_impl_glGetBufferPointerv;
PFNGLGETBUFFERPOINTERVARBPROC glad_glGetBufferPointervARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferPointervARB(GLenum target, GLenum pname, void ** params) {
    _pre_call_gl_callback("glGetBufferPointervARB", (GLADapiproc) glad_glGetBufferPointervARB, 3, target, pname, params);
    glad_glGetBufferPointervARB(target, pname, params);
    _post_call_gl_callback(NULL, "glGetBufferPointervARB", (GLADapiproc) glad_glGetBufferPointervARB, 3, target, pname, params);
    
}
PFNGLGETBUFFERPOINTERVARBPROC glad_debug_glGetBufferPointervARB = glad_debug_impl_glGetBufferPointervARB;
PFNGLGETBUFFERSUBDATAPROC glad_glGetBufferSubData = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void * data) {
    _pre_call_gl_callback("glGetBufferSubData", (GLADapiproc) glad_glGetBufferSubData, 4, target, offset, size, data);
    glad_glGetBufferSubData(target, offset, size, data);
    _post_call_gl_callback(NULL, "glGetBufferSubData", (GLADapiproc) glad_glGetBufferSubData, 4, target, offset, size, data);
    
}
PFNGLGETBUFFERSUBDATAPROC glad_debug_glGetBufferSubData = glad_debug_impl_glGetBufferSubData;
PFNGLGETBUFFERSUBDATAARBPROC glad_glGetBufferSubDataARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, void * data) {
    _pre_call_gl_callback("glGetBufferSubDataARB", (GLADapiproc) glad_glGetBufferSubDataARB, 4, target, offset, size, data);
    glad_glGetBufferSubDataARB(target, offset, size, data);
    _post_call_gl_callback(NULL, "glGetBufferSubDataARB", (GLADapiproc) glad_glGetBufferSubDataARB, 4, target, offset, size, data);
    
}
PFNGLGETBUFFERSUBDATAARBPROC glad_debug_glGetBufferSubDataARB = glad_debug_impl_glGetBufferSubDataARB;
PFNGLGETCOMPRESSEDMULTITEXIMAGEEXTPROC glad_glGetCompressedMultiTexImageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetCompressedMultiTexImageEXT(GLenum texunit, GLenum target, GLint lod, void * img) {
    _pre_call_gl_callback("glGetCompressedMultiTexImageEXT", (GLADapiproc) glad_glGetCompressedMultiTexImageEXT, 4, texunit, target, lod, img);
    glad_glGetCompressedMultiTexImageEXT(texunit, target, lod, img);
    _post_call_gl_callback(NULL, "glGetCompressedMultiTexImageEXT", (GLADapiproc) glad_glGetCompressedMultiTexImageEXT, 4, texunit, target, lod, img);
    
}
PFNGLGETCOMPRESSEDMULTITEXIMAGEEXTPROC glad_debug_glGetCompressedMultiTexImageEXT = glad_debug_impl_glGetCompressedMultiTexImageEXT;
PFNGLGETCOMPRESSEDTEXIMAGEPROC glad_glGetCompressedTexImage = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetCompressedTexImage(GLenum target, GLint level, void * img) {
    _pre_call_gl_callback("glGetCompressedTexImage", (GLADapiproc) glad_glGetCompressedTexImage, 3, target, level, img);
    glad_glGetCompressedTexImage(target, level, img);
    _post_call_gl_callback(NULL, "glGetCompressedTexImage", (GLADapiproc) glad_glGetCompressedTexImage, 3, target, level, img);
    
}
PFNGLGETCOMPRESSEDTEXIMAGEPROC glad_debug_glGetCompressedTexImage = glad_debug_impl_glGetCompressedTexImage;
PFNGLGETCOMPRESSEDTEXIMAGEARBPROC glad_glGetCompressedTexImageARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetCompressedTexImageARB(GLenum target, GLint level, void * img) {
    _pre_call_gl_callback("glGetCompressedTexImageARB", (GLADapiproc) glad_glGetCompressedTexImageARB, 3, target, level, img);
    glad_glGetCompressedTexImageARB(target, level, img);
    _post_call_gl_callback(NULL, "glGetCompressedTexImageARB", (GLADapiproc) glad_glGetCompressedTexImageARB, 3, target, level, img);
    
}
PFNGLGETCOMPRESSEDTEXIMAGEARBPROC glad_debug_glGetCompressedTexImageARB = glad_debug_impl_glGetCompressedTexImageARB;
PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC glad_glGetCompressedTextureImageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint lod, void * img) {
    _pre_call_gl_callback("glGetCompressedTextureImageEXT", (GLADapiproc) glad_glGetCompressedTextureImageEXT, 4, texture, target, lod, img);
    glad_glGetCompressedTextureImageEXT(texture, target, lod, img);
    _post_call_gl_callback(NULL, "glGetCompressedTextureImageEXT", (GLADapiproc) glad_glGetCompressedTextureImageEXT, 4, texture, target, lod, img);
    
}
PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC glad_debug_glGetCompressedTextureImageEXT = glad_debug_impl_glGetCompressedTextureImageEXT;
PFNGLGETDOUBLEINDEXEDVEXTPROC glad_glGetDoubleIndexedvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetDoubleIndexedvEXT(GLenum target, GLuint index, GLdouble * data) {
    _pre_call_gl_callback("glGetDoubleIndexedvEXT", (GLADapiproc) glad_glGetDoubleIndexedvEXT, 3, target, index, data);
    glad_glGetDoubleIndexedvEXT(target, index, data);
    _post_call_gl_callback(NULL, "glGetDoubleIndexedvEXT", (GLADapiproc) glad_glGetDoubleIndexedvEXT, 3, target, index, data);
    
}
PFNGLGETDOUBLEINDEXEDVEXTPROC glad_debug_glGetDoubleIndexedvEXT = glad_debug_impl_glGetDoubleIndexedvEXT;
PFNGLGETDOUBLEI_VEXTPROC glad_glGetDoublei_vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetDoublei_vEXT(GLenum pname, GLuint index, GLdouble * params) {
    _pre_call_gl_callback("glGetDoublei_vEXT", (GLADapiproc) glad_glGetDoublei_vEXT, 3, pname, index, params);
    glad_glGetDoublei_vEXT(pname, index, params);
    _post_call_gl_callback(NULL, "glGetDoublei_vEXT", (GLADapiproc) glad_glGetDoublei_vEXT, 3, pname, index, params);
    
}
PFNGLGETDOUBLEI_VEXTPROC glad_debug_glGetDoublei_vEXT = glad_debug_impl_glGetDoublei_vEXT;
PFNGLGETDOUBLEVPROC glad_glGetDoublev = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetDoublev(GLenum pname, GLdouble * data) {
    _pre_call_gl_callback("glGetDoublev", (GLADapiproc) glad_glGetDoublev, 2, pname, data);
    glad_glGetDoublev(pname, data);
    _post_call_gl_callback(NULL, "glGetDoublev", (GLADapiproc) glad_glGetDoublev, 2, pname, data);
    
}
PFNGLGETDOUBLEVPROC glad_debug_glGetDoublev = glad_debug_impl_glGetDoublev;
PFNGLGETERRORPROC glad_glGetError = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glGetError(void) {
    GLenum ret;
    _pre_call_gl_callback("glGetError", (GLADapiproc) glad_glGetError, 0);
    ret = glad_glGetError();
    _post_call_gl_callback((void*) &ret, "glGetError", (GLADapiproc) glad_glGetError, 0);
    return ret;
}
PFNGLGETERRORPROC glad_debug_glGetError = glad_debug_impl_glGetError;
PFNGLGETFLOATINDEXEDVEXTPROC glad_glGetFloatIndexedvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFloatIndexedvEXT(GLenum target, GLuint index, GLfloat * data) {
    _pre_call_gl_callback("glGetFloatIndexedvEXT", (GLADapiproc) glad_glGetFloatIndexedvEXT, 3, target, index, data);
    glad_glGetFloatIndexedvEXT(target, index, data);
    _post_call_gl_callback(NULL, "glGetFloatIndexedvEXT", (GLADapiproc) glad_glGetFloatIndexedvEXT, 3, target, index, data);
    
}
PFNGLGETFLOATINDEXEDVEXTPROC glad_debug_glGetFloatIndexedvEXT = glad_debug_impl_glGetFloatIndexedvEXT;
PFNGLGETFLOATI_VEXTPROC glad_glGetFloati_vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFloati_vEXT(GLenum pname, GLuint index, GLfloat * params) {
    _pre_call_gl_callback("glGetFloati_vEXT", (GLADapiproc) glad_glGetFloati_vEXT, 3, pname, index, params);
    glad_glGetFloati_vEXT(pname, index, params);
    _post_call_gl_callback(NULL, "glGetFloati_vEXT", (GLADapiproc) glad_glGetFloati_vEXT, 3, pname, index, params);
    
}
PFNGLGETFLOATI_VEXTPROC glad_debug_glGetFloati_vEXT = glad_debug_impl_glGetFloati_vEXT;
PFNGLGETFLOATVPROC glad_glGetFloatv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFloatv(GLenum pname, GLfloat * data) {
    _pre_call_gl_callback("glGetFloatv", (GLADapiproc) glad_glGetFloatv, 2, pname, data);
    glad_glGetFloatv(pname, data);
    _post_call_gl_callback(NULL, "glGetFloatv", (GLADapiproc) glad_glGetFloatv, 2, pname, data);
    
}
PFNGLGETFLOATVPROC glad_debug_glGetFloatv = glad_debug_impl_glGetFloatv;
PFNGLGETFRAGDATAINDEXPROC glad_glGetFragDataIndex = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetFragDataIndex(GLuint program, const GLchar * name) {
    GLint ret;
    _pre_call_gl_callback("glGetFragDataIndex", (GLADapiproc) glad_glGetFragDataIndex, 2, program, name);
    ret = glad_glGetFragDataIndex(program, name);
    _post_call_gl_callback((void*) &ret, "glGetFragDataIndex", (GLADapiproc) glad_glGetFragDataIndex, 2, program, name);
    return ret;
}
PFNGLGETFRAGDATAINDEXPROC glad_debug_glGetFragDataIndex = glad_debug_impl_glGetFragDataIndex;
PFNGLGETFRAGDATALOCATIONPROC glad_glGetFragDataLocation = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetFragDataLocation(GLuint program, const GLchar * name) {
    GLint ret;
    _pre_call_gl_callback("glGetFragDataLocation", (GLADapiproc) glad_glGetFragDataLocation, 2, program, name);
    ret = glad_glGetFragDataLocation(program, name);
    _post_call_gl_callback((void*) &ret, "glGetFragDataLocation", (GLADapiproc) glad_glGetFragDataLocation, 2, program, name);
    return ret;
}
PFNGLGETFRAGDATALOCATIONPROC glad_debug_glGetFragDataLocation = glad_debug_impl_glGetFragDataLocation;
PFNGLGETFRAGDATALOCATIONEXTPROC glad_glGetFragDataLocationEXT = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetFragDataLocationEXT(GLuint program, const GLchar * name) {
    GLint ret;
    _pre_call_gl_callback("glGetFragDataLocationEXT", (GLADapiproc) glad_glGetFragDataLocationEXT, 2, program, name);
    ret = glad_glGetFragDataLocationEXT(program, name);
    _post_call_gl_callback((void*) &ret, "glGetFragDataLocationEXT", (GLADapiproc) glad_glGetFragDataLocationEXT, 2, program, name);
    return ret;
}
PFNGLGETFRAGDATALOCATIONEXTPROC glad_debug_glGetFragDataLocationEXT = glad_debug_impl_glGetFragDataLocationEXT;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_glGetFramebufferAttachmentParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetFramebufferAttachmentParameteriv", (GLADapiproc) glad_glGetFramebufferAttachmentParameteriv, 4, target, attachment, pname, params);
    glad_glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
    _post_call_gl_callback(NULL, "glGetFramebufferAttachmentParameteriv", (GLADapiproc) glad_glGetFramebufferAttachmentParameteriv, 4, target, attachment, pname, params);
    
}
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_debug_glGetFramebufferAttachmentParameteriv = glad_debug_impl_glGetFramebufferAttachmentParameteriv;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC glad_glGetFramebufferAttachmentParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetFramebufferAttachmentParameterivEXT", (GLADapiproc) glad_glGetFramebufferAttachmentParameterivEXT, 4, target, attachment, pname, params);
    glad_glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, params);
    _post_call_gl_callback(NULL, "glGetFramebufferAttachmentParameterivEXT", (GLADapiproc) glad_glGetFramebufferAttachmentParameterivEXT, 4, target, attachment, pname, params);
    
}
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC glad_debug_glGetFramebufferAttachmentParameterivEXT = glad_debug_impl_glGetFramebufferAttachmentParameterivEXT;
PFNGLGETFRAMEBUFFERPARAMETERIVEXTPROC glad_glGetFramebufferParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetFramebufferParameterivEXT", (GLADapiproc) glad_glGetFramebufferParameterivEXT, 3, framebuffer, pname, params);
    glad_glGetFramebufferParameterivEXT(framebuffer, pname, params);
    _post_call_gl_callback(NULL, "glGetFramebufferParameterivEXT", (GLADapiproc) glad_glGetFramebufferParameterivEXT, 3, framebuffer, pname, params);
    
}
PFNGLGETFRAMEBUFFERPARAMETERIVEXTPROC glad_debug_glGetFramebufferParameterivEXT = glad_debug_impl_glGetFramebufferParameterivEXT;
PFNGLGETHANDLEARBPROC glad_glGetHandleARB = NULL;
static GLhandleARB GLAD_API_PTR glad_debug_impl_glGetHandleARB(GLenum pname) {
    GLhandleARB ret;
    _pre_call_gl_callback("glGetHandleARB", (GLADapiproc) glad_glGetHandleARB, 1, pname);
    ret = glad_glGetHandleARB(pname);
    _post_call_gl_callback((void*) &ret, "glGetHandleARB", (GLADapiproc) glad_glGetHandleARB, 1, pname);
    return ret;
}
PFNGLGETHANDLEARBPROC glad_debug_glGetHandleARB = glad_debug_impl_glGetHandleARB;
PFNGLGETINFOLOGARBPROC glad_glGetInfoLogARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetInfoLogARB(GLhandleARB obj, GLsizei maxLength, GLsizei * length, GLcharARB * infoLog) {
    _pre_call_gl_callback("glGetInfoLogARB", (GLADapiproc) glad_glGetInfoLogARB, 4, obj, maxLength, length, infoLog);
    glad_glGetInfoLogARB(obj, maxLength, length, infoLog);
    _post_call_gl_callback(NULL, "glGetInfoLogARB", (GLADapiproc) glad_glGetInfoLogARB, 4, obj, maxLength, length, infoLog);
    
}
PFNGLGETINFOLOGARBPROC glad_debug_glGetInfoLogARB = glad_debug_impl_glGetInfoLogARB;
PFNGLGETINTEGER64I_VPROC glad_glGetInteger64i_v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetInteger64i_v(GLenum target, GLuint index, GLint64 * data) {
    _pre_call_gl_callback("glGetInteger64i_v", (GLADapiproc) glad_glGetInteger64i_v, 3, target, index, data);
    glad_glGetInteger64i_v(target, index, data);
    _post_call_gl_callback(NULL, "glGetInteger64i_v", (GLADapiproc) glad_glGetInteger64i_v, 3, target, index, data);
    
}
PFNGLGETINTEGER64I_VPROC glad_debug_glGetInteger64i_v = glad_debug_impl_glGetInteger64i_v;
PFNGLGETINTEGER64VPROC glad_glGetInteger64v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetInteger64v(GLenum pname, GLint64 * data) {
    _pre_call_gl_callback("glGetInteger64v", (GLADapiproc) glad_glGetInteger64v, 2, pname, data);
    glad_glGetInteger64v(pname, data);
    _post_call_gl_callback(NULL, "glGetInteger64v", (GLADapiproc) glad_glGetInteger64v, 2, pname, data);
    
}
PFNGLGETINTEGER64VPROC glad_debug_glGetInteger64v = glad_debug_impl_glGetInteger64v;
PFNGLGETINTEGERINDEXEDVEXTPROC glad_glGetIntegerIndexedvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetIntegerIndexedvEXT(GLenum target, GLuint index, GLint * data) {
    _pre_call_gl_callback("glGetIntegerIndexedvEXT", (GLADapiproc) glad_glGetIntegerIndexedvEXT, 3, target, index, data);
    glad_glGetIntegerIndexedvEXT(target, index, data);
    _post_call_gl_callback(NULL, "glGetIntegerIndexedvEXT", (GLADapiproc) glad_glGetIntegerIndexedvEXT, 3, target, index, data);
    
}
PFNGLGETINTEGERINDEXEDVEXTPROC glad_debug_glGetIntegerIndexedvEXT = glad_debug_impl_glGetIntegerIndexedvEXT;
PFNGLGETINTEGERI_VPROC glad_glGetIntegeri_v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetIntegeri_v(GLenum target, GLuint index, GLint * data) {
    _pre_call_gl_callback("glGetIntegeri_v", (GLADapiproc) glad_glGetIntegeri_v, 3, target, index, data);
    glad_glGetIntegeri_v(target, index, data);
    _post_call_gl_callback(NULL, "glGetIntegeri_v", (GLADapiproc) glad_glGetIntegeri_v, 3, target, index, data);
    
}
PFNGLGETINTEGERI_VPROC glad_debug_glGetIntegeri_v = glad_debug_impl_glGetIntegeri_v;
PFNGLGETINTEGERVPROC glad_glGetIntegerv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetIntegerv(GLenum pname, GLint * data) {
    _pre_call_gl_callback("glGetIntegerv", (GLADapiproc) glad_glGetIntegerv, 2, pname, data);
    glad_glGetIntegerv(pname, data);
    _post_call_gl_callback(NULL, "glGetIntegerv", (GLADapiproc) glad_glGetIntegerv, 2, pname, data);
    
}
PFNGLGETINTEGERVPROC glad_debug_glGetIntegerv = glad_debug_impl_glGetIntegerv;
PFNGLGETMULTITEXENVFVEXTPROC glad_glGetMultiTexEnvfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexEnvfvEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetMultiTexEnvfvEXT", (GLADapiproc) glad_glGetMultiTexEnvfvEXT, 4, texunit, target, pname, params);
    glad_glGetMultiTexEnvfvEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glGetMultiTexEnvfvEXT", (GLADapiproc) glad_glGetMultiTexEnvfvEXT, 4, texunit, target, pname, params);
    
}
PFNGLGETMULTITEXENVFVEXTPROC glad_debug_glGetMultiTexEnvfvEXT = glad_debug_impl_glGetMultiTexEnvfvEXT;
PFNGLGETMULTITEXENVIVEXTPROC glad_glGetMultiTexEnvivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexEnvivEXT(GLenum texunit, GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetMultiTexEnvivEXT", (GLADapiproc) glad_glGetMultiTexEnvivEXT, 4, texunit, target, pname, params);
    glad_glGetMultiTexEnvivEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glGetMultiTexEnvivEXT", (GLADapiproc) glad_glGetMultiTexEnvivEXT, 4, texunit, target, pname, params);
    
}
PFNGLGETMULTITEXENVIVEXTPROC glad_debug_glGetMultiTexEnvivEXT = glad_debug_impl_glGetMultiTexEnvivEXT;
PFNGLGETMULTITEXGENDVEXTPROC glad_glGetMultiTexGendvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexGendvEXT(GLenum texunit, GLenum coord, GLenum pname, GLdouble * params) {
    _pre_call_gl_callback("glGetMultiTexGendvEXT", (GLADapiproc) glad_glGetMultiTexGendvEXT, 4, texunit, coord, pname, params);
    glad_glGetMultiTexGendvEXT(texunit, coord, pname, params);
    _post_call_gl_callback(NULL, "glGetMultiTexGendvEXT", (GLADapiproc) glad_glGetMultiTexGendvEXT, 4, texunit, coord, pname, params);
    
}
PFNGLGETMULTITEXGENDVEXTPROC glad_debug_glGetMultiTexGendvEXT = glad_debug_impl_glGetMultiTexGendvEXT;
PFNGLGETMULTITEXGENFVEXTPROC glad_glGetMultiTexGenfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexGenfvEXT(GLenum texunit, GLenum coord, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetMultiTexGenfvEXT", (GLADapiproc) glad_glGetMultiTexGenfvEXT, 4, texunit, coord, pname, params);
    glad_glGetMultiTexGenfvEXT(texunit, coord, pname, params);
    _post_call_gl_callback(NULL, "glGetMultiTexGenfvEXT", (GLADapiproc) glad_glGetMultiTexGenfvEXT, 4, texunit, coord, pname, params);
    
}
PFNGLGETMULTITEXGENFVEXTPROC glad_debug_glGetMultiTexGenfvEXT = glad_debug_impl_glGetMultiTexGenfvEXT;
PFNGLGETMULTITEXGENIVEXTPROC glad_glGetMultiTexGenivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexGenivEXT(GLenum texunit, GLenum coord, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetMultiTexGenivEXT", (GLADapiproc) glad_glGetMultiTexGenivEXT, 4, texunit, coord, pname, params);
    glad_glGetMultiTexGenivEXT(texunit, coord, pname, params);
    _post_call_gl_callback(NULL, "glGetMultiTexGenivEXT", (GLADapiproc) glad_glGetMultiTexGenivEXT, 4, texunit, coord, pname, params);
    
}
PFNGLGETMULTITEXGENIVEXTPROC glad_debug_glGetMultiTexGenivEXT = glad_debug_impl_glGetMultiTexGenivEXT;
PFNGLGETMULTITEXIMAGEEXTPROC glad_glGetMultiTexImageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexImageEXT(GLenum texunit, GLenum target, GLint level, GLenum format, GLenum type, void * pixels) {
    _pre_call_gl_callback("glGetMultiTexImageEXT", (GLADapiproc) glad_glGetMultiTexImageEXT, 6, texunit, target, level, format, type, pixels);
    glad_glGetMultiTexImageEXT(texunit, target, level, format, type, pixels);
    _post_call_gl_callback(NULL, "glGetMultiTexImageEXT", (GLADapiproc) glad_glGetMultiTexImageEXT, 6, texunit, target, level, format, type, pixels);
    
}
PFNGLGETMULTITEXIMAGEEXTPROC glad_debug_glGetMultiTexImageEXT = glad_debug_impl_glGetMultiTexImageEXT;
PFNGLGETMULTITEXLEVELPARAMETERFVEXTPROC glad_glGetMultiTexLevelParameterfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexLevelParameterfvEXT(GLenum texunit, GLenum target, GLint level, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetMultiTexLevelParameterfvEXT", (GLADapiproc) glad_glGetMultiTexLevelParameterfvEXT, 5, texunit, target, level, pname, params);
    glad_glGetMultiTexLevelParameterfvEXT(texunit, target, level, pname, params);
    _post_call_gl_callback(NULL, "glGetMultiTexLevelParameterfvEXT", (GLADapiproc) glad_glGetMultiTexLevelParameterfvEXT, 5, texunit, target, level, pname, params);
    
}
PFNGLGETMULTITEXLEVELPARAMETERFVEXTPROC glad_debug_glGetMultiTexLevelParameterfvEXT = glad_debug_impl_glGetMultiTexLevelParameterfvEXT;
PFNGLGETMULTITEXLEVELPARAMETERIVEXTPROC glad_glGetMultiTexLevelParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexLevelParameterivEXT(GLenum texunit, GLenum target, GLint level, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetMultiTexLevelParameterivEXT", (GLADapiproc) glad_glGetMultiTexLevelParameterivEXT, 5, texunit, target, level, pname, params);
    glad_glGetMultiTexLevelParameterivEXT(texunit, target, level, pname, params);
    _post_call_gl_callback(NULL, "glGetMultiTexLevelParameterivEXT", (GLADapiproc) glad_glGetMultiTexLevelParameterivEXT, 5, texunit, target, level, pname, params);
    
}
PFNGLGETMULTITEXLEVELPARAMETERIVEXTPROC glad_debug_glGetMultiTexLevelParameterivEXT = glad_debug_impl_glGetMultiTexLevelParameterivEXT;
PFNGLGETMULTITEXPARAMETERIIVEXTPROC glad_glGetMultiTexParameterIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetMultiTexParameterIivEXT", (GLADapiproc) glad_glGetMultiTexParameterIivEXT, 4, texunit, target, pname, params);
    glad_glGetMultiTexParameterIivEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glGetMultiTexParameterIivEXT", (GLADapiproc) glad_glGetMultiTexParameterIivEXT, 4, texunit, target, pname, params);
    
}
PFNGLGETMULTITEXPARAMETERIIVEXTPROC glad_debug_glGetMultiTexParameterIivEXT = glad_debug_impl_glGetMultiTexParameterIivEXT;
PFNGLGETMULTITEXPARAMETERIUIVEXTPROC glad_glGetMultiTexParameterIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname, GLuint * params) {
    _pre_call_gl_callback("glGetMultiTexParameterIuivEXT", (GLADapiproc) glad_glGetMultiTexParameterIuivEXT, 4, texunit, target, pname, params);
    glad_glGetMultiTexParameterIuivEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glGetMultiTexParameterIuivEXT", (GLADapiproc) glad_glGetMultiTexParameterIuivEXT, 4, texunit, target, pname, params);
    
}
PFNGLGETMULTITEXPARAMETERIUIVEXTPROC glad_debug_glGetMultiTexParameterIuivEXT = glad_debug_impl_glGetMultiTexParameterIuivEXT;
PFNGLGETMULTITEXPARAMETERFVEXTPROC glad_glGetMultiTexParameterfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetMultiTexParameterfvEXT", (GLADapiproc) glad_glGetMultiTexParameterfvEXT, 4, texunit, target, pname, params);
    glad_glGetMultiTexParameterfvEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glGetMultiTexParameterfvEXT", (GLADapiproc) glad_glGetMultiTexParameterfvEXT, 4, texunit, target, pname, params);
    
}
PFNGLGETMULTITEXPARAMETERFVEXTPROC glad_debug_glGetMultiTexParameterfvEXT = glad_debug_impl_glGetMultiTexParameterfvEXT;
PFNGLGETMULTITEXPARAMETERIVEXTPROC glad_glGetMultiTexParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetMultiTexParameterivEXT", (GLADapiproc) glad_glGetMultiTexParameterivEXT, 4, texunit, target, pname, params);
    glad_glGetMultiTexParameterivEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glGetMultiTexParameterivEXT", (GLADapiproc) glad_glGetMultiTexParameterivEXT, 4, texunit, target, pname, params);
    
}
PFNGLGETMULTITEXPARAMETERIVEXTPROC glad_debug_glGetMultiTexParameterivEXT = glad_debug_impl_glGetMultiTexParameterivEXT;
PFNGLGETMULTISAMPLEFVPROC glad_glGetMultisamplefv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultisamplefv(GLenum pname, GLuint index, GLfloat * val) {
    _pre_call_gl_callback("glGetMultisamplefv", (GLADapiproc) glad_glGetMultisamplefv, 3, pname, index, val);
    glad_glGetMultisamplefv(pname, index, val);
    _post_call_gl_callback(NULL, "glGetMultisamplefv", (GLADapiproc) glad_glGetMultisamplefv, 3, pname, index, val);
    
}
PFNGLGETMULTISAMPLEFVPROC glad_debug_glGetMultisamplefv = glad_debug_impl_glGetMultisamplefv;
PFNGLGETMULTISAMPLEFVNVPROC glad_glGetMultisamplefvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultisamplefvNV(GLenum pname, GLuint index, GLfloat * val) {
    _pre_call_gl_callback("glGetMultisamplefvNV", (GLADapiproc) glad_glGetMultisamplefvNV, 3, pname, index, val);
    glad_glGetMultisamplefvNV(pname, index, val);
    _post_call_gl_callback(NULL, "glGetMultisamplefvNV", (GLADapiproc) glad_glGetMultisamplefvNV, 3, pname, index, val);
    
}
PFNGLGETMULTISAMPLEFVNVPROC glad_debug_glGetMultisamplefvNV = glad_debug_impl_glGetMultisamplefvNV;
PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC glad_glGetNamedBufferParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedBufferParameterivEXT(GLuint buffer, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetNamedBufferParameterivEXT", (GLADapiproc) glad_glGetNamedBufferParameterivEXT, 3, buffer, pname, params);
    glad_glGetNamedBufferParameterivEXT(buffer, pname, params);
    _post_call_gl_callback(NULL, "glGetNamedBufferParameterivEXT", (GLADapiproc) glad_glGetNamedBufferParameterivEXT, 3, buffer, pname, params);
    
}
PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC glad_debug_glGetNamedBufferParameterivEXT = glad_debug_impl_glGetNamedBufferParameterivEXT;
PFNGLGETNAMEDBUFFERPOINTERVEXTPROC glad_glGetNamedBufferPointervEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedBufferPointervEXT(GLuint buffer, GLenum pname, void ** params) {
    _pre_call_gl_callback("glGetNamedBufferPointervEXT", (GLADapiproc) glad_glGetNamedBufferPointervEXT, 3, buffer, pname, params);
    glad_glGetNamedBufferPointervEXT(buffer, pname, params);
    _post_call_gl_callback(NULL, "glGetNamedBufferPointervEXT", (GLADapiproc) glad_glGetNamedBufferPointervEXT, 3, buffer, pname, params);
    
}
PFNGLGETNAMEDBUFFERPOINTERVEXTPROC glad_debug_glGetNamedBufferPointervEXT = glad_debug_impl_glGetNamedBufferPointervEXT;
PFNGLGETNAMEDBUFFERSUBDATAEXTPROC glad_glGetNamedBufferSubDataEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, void * data) {
    _pre_call_gl_callback("glGetNamedBufferSubDataEXT", (GLADapiproc) glad_glGetNamedBufferSubDataEXT, 4, buffer, offset, size, data);
    glad_glGetNamedBufferSubDataEXT(buffer, offset, size, data);
    _post_call_gl_callback(NULL, "glGetNamedBufferSubDataEXT", (GLADapiproc) glad_glGetNamedBufferSubDataEXT, 4, buffer, offset, size, data);
    
}
PFNGLGETNAMEDBUFFERSUBDATAEXTPROC glad_debug_glGetNamedBufferSubDataEXT = glad_debug_impl_glGetNamedBufferSubDataEXT;
PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC glad_glGetNamedFramebufferAttachmentParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedFramebufferAttachmentParameterivEXT(GLuint framebuffer, GLenum attachment, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetNamedFramebufferAttachmentParameterivEXT", (GLADapiproc) glad_glGetNamedFramebufferAttachmentParameterivEXT, 4, framebuffer, attachment, pname, params);
    glad_glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, attachment, pname, params);
    _post_call_gl_callback(NULL, "glGetNamedFramebufferAttachmentParameterivEXT", (GLADapiproc) glad_glGetNamedFramebufferAttachmentParameterivEXT, 4, framebuffer, attachment, pname, params);
    
}
PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC glad_debug_glGetNamedFramebufferAttachmentParameterivEXT = glad_debug_impl_glGetNamedFramebufferAttachmentParameterivEXT;
PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVEXTPROC glad_glGetNamedFramebufferParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetNamedFramebufferParameterivEXT", (GLADapiproc) glad_glGetNamedFramebufferParameterivEXT, 3, framebuffer, pname, params);
    glad_glGetNamedFramebufferParameterivEXT(framebuffer, pname, params);
    _post_call_gl_callback(NULL, "glGetNamedFramebufferParameterivEXT", (GLADapiproc) glad_glGetNamedFramebufferParameterivEXT, 3, framebuffer, pname, params);
    
}
PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVEXTPROC glad_debug_glGetNamedFramebufferParameterivEXT = glad_debug_impl_glGetNamedFramebufferParameterivEXT;
PFNGLGETNAMEDPROGRAMLOCALPARAMETERIIVEXTPROC glad_glGetNamedProgramLocalParameterIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedProgramLocalParameterIivEXT(GLuint program, GLenum target, GLuint index, GLint * params) {
    _pre_call_gl_callback("glGetNamedProgramLocalParameterIivEXT", (GLADapiproc) glad_glGetNamedProgramLocalParameterIivEXT, 4, program, target, index, params);
    glad_glGetNamedProgramLocalParameterIivEXT(program, target, index, params);
    _post_call_gl_callback(NULL, "glGetNamedProgramLocalParameterIivEXT", (GLADapiproc) glad_glGetNamedProgramLocalParameterIivEXT, 4, program, target, index, params);
    
}
PFNGLGETNAMEDPROGRAMLOCALPARAMETERIIVEXTPROC glad_debug_glGetNamedProgramLocalParameterIivEXT = glad_debug_impl_glGetNamedProgramLocalParameterIivEXT;
PFNGLGETNAMEDPROGRAMLOCALPARAMETERIUIVEXTPROC glad_glGetNamedProgramLocalParameterIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedProgramLocalParameterIuivEXT(GLuint program, GLenum target, GLuint index, GLuint * params) {
    _pre_call_gl_callback("glGetNamedProgramLocalParameterIuivEXT", (GLADapiproc) glad_glGetNamedProgramLocalParameterIuivEXT, 4, program, target, index, params);
    glad_glGetNamedProgramLocalParameterIuivEXT(program, target, index, params);
    _post_call_gl_callback(NULL, "glGetNamedProgramLocalParameterIuivEXT", (GLADapiproc) glad_glGetNamedProgramLocalParameterIuivEXT, 4, program, target, index, params);
    
}
PFNGLGETNAMEDPROGRAMLOCALPARAMETERIUIVEXTPROC glad_debug_glGetNamedProgramLocalParameterIuivEXT = glad_debug_impl_glGetNamedProgramLocalParameterIuivEXT;
PFNGLGETNAMEDPROGRAMLOCALPARAMETERDVEXTPROC glad_glGetNamedProgramLocalParameterdvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedProgramLocalParameterdvEXT(GLuint program, GLenum target, GLuint index, GLdouble * params) {
    _pre_call_gl_callback("glGetNamedProgramLocalParameterdvEXT", (GLADapiproc) glad_glGetNamedProgramLocalParameterdvEXT, 4, program, target, index, params);
    glad_glGetNamedProgramLocalParameterdvEXT(program, target, index, params);
    _post_call_gl_callback(NULL, "glGetNamedProgramLocalParameterdvEXT", (GLADapiproc) glad_glGetNamedProgramLocalParameterdvEXT, 4, program, target, index, params);
    
}
PFNGLGETNAMEDPROGRAMLOCALPARAMETERDVEXTPROC glad_debug_glGetNamedProgramLocalParameterdvEXT = glad_debug_impl_glGetNamedProgramLocalParameterdvEXT;
PFNGLGETNAMEDPROGRAMLOCALPARAMETERFVEXTPROC glad_glGetNamedProgramLocalParameterfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedProgramLocalParameterfvEXT(GLuint program, GLenum target, GLuint index, GLfloat * params) {
    _pre_call_gl_callback("glGetNamedProgramLocalParameterfvEXT", (GLADapiproc) glad_glGetNamedProgramLocalParameterfvEXT, 4, program, target, index, params);
    glad_glGetNamedProgramLocalParameterfvEXT(program, target, index, params);
    _post_call_gl_callback(NULL, "glGetNamedProgramLocalParameterfvEXT", (GLADapiproc) glad_glGetNamedProgramLocalParameterfvEXT, 4, program, target, index, params);
    
}
PFNGLGETNAMEDPROGRAMLOCALPARAMETERFVEXTPROC glad_debug_glGetNamedProgramLocalParameterfvEXT = glad_debug_impl_glGetNamedProgramLocalParameterfvEXT;
PFNGLGETNAMEDPROGRAMSTRINGEXTPROC glad_glGetNamedProgramStringEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedProgramStringEXT(GLuint program, GLenum target, GLenum pname, void * string) {
    _pre_call_gl_callback("glGetNamedProgramStringEXT", (GLADapiproc) glad_glGetNamedProgramStringEXT, 4, program, target, pname, string);
    glad_glGetNamedProgramStringEXT(program, target, pname, string);
    _post_call_gl_callback(NULL, "glGetNamedProgramStringEXT", (GLADapiproc) glad_glGetNamedProgramStringEXT, 4, program, target, pname, string);
    
}
PFNGLGETNAMEDPROGRAMSTRINGEXTPROC glad_debug_glGetNamedProgramStringEXT = glad_debug_impl_glGetNamedProgramStringEXT;
PFNGLGETNAMEDPROGRAMIVEXTPROC glad_glGetNamedProgramivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedProgramivEXT(GLuint program, GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetNamedProgramivEXT", (GLADapiproc) glad_glGetNamedProgramivEXT, 4, program, target, pname, params);
    glad_glGetNamedProgramivEXT(program, target, pname, params);
    _post_call_gl_callback(NULL, "glGetNamedProgramivEXT", (GLADapiproc) glad_glGetNamedProgramivEXT, 4, program, target, pname, params);
    
}
PFNGLGETNAMEDPROGRAMIVEXTPROC glad_debug_glGetNamedProgramivEXT = glad_debug_impl_glGetNamedProgramivEXT;
PFNGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC glad_glGetNamedRenderbufferParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNamedRenderbufferParameterivEXT(GLuint renderbuffer, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetNamedRenderbufferParameterivEXT", (GLADapiproc) glad_glGetNamedRenderbufferParameterivEXT, 3, renderbuffer, pname, params);
    glad_glGetNamedRenderbufferParameterivEXT(renderbuffer, pname, params);
    _post_call_gl_callback(NULL, "glGetNamedRenderbufferParameterivEXT", (GLADapiproc) glad_glGetNamedRenderbufferParameterivEXT, 3, renderbuffer, pname, params);
    
}
PFNGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC glad_debug_glGetNamedRenderbufferParameterivEXT = glad_debug_impl_glGetNamedRenderbufferParameterivEXT;
PFNGLGETOBJECTPARAMETERFVARBPROC glad_glGetObjectParameterfvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetObjectParameterfvARB(GLhandleARB obj, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetObjectParameterfvARB", (GLADapiproc) glad_glGetObjectParameterfvARB, 3, obj, pname, params);
    glad_glGetObjectParameterfvARB(obj, pname, params);
    _post_call_gl_callback(NULL, "glGetObjectParameterfvARB", (GLADapiproc) glad_glGetObjectParameterfvARB, 3, obj, pname, params);
    
}
PFNGLGETOBJECTPARAMETERFVARBPROC glad_debug_glGetObjectParameterfvARB = glad_debug_impl_glGetObjectParameterfvARB;
PFNGLGETOBJECTPARAMETERIVARBPROC glad_glGetObjectParameterivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetObjectParameterivARB(GLhandleARB obj, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetObjectParameterivARB", (GLADapiproc) glad_glGetObjectParameterivARB, 3, obj, pname, params);
    glad_glGetObjectParameterivARB(obj, pname, params);
    _post_call_gl_callback(NULL, "glGetObjectParameterivARB", (GLADapiproc) glad_glGetObjectParameterivARB, 3, obj, pname, params);
    
}
PFNGLGETOBJECTPARAMETERIVARBPROC glad_debug_glGetObjectParameterivARB = glad_debug_impl_glGetObjectParameterivARB;
PFNGLGETPOINTERINDEXEDVEXTPROC glad_glGetPointerIndexedvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPointerIndexedvEXT(GLenum target, GLuint index, void ** data) {
    _pre_call_gl_callback("glGetPointerIndexedvEXT", (GLADapiproc) glad_glGetPointerIndexedvEXT, 3, target, index, data);
    glad_glGetPointerIndexedvEXT(target, index, data);
    _post_call_gl_callback(NULL, "glGetPointerIndexedvEXT", (GLADapiproc) glad_glGetPointerIndexedvEXT, 3, target, index, data);
    
}
PFNGLGETPOINTERINDEXEDVEXTPROC glad_debug_glGetPointerIndexedvEXT = glad_debug_impl_glGetPointerIndexedvEXT;
PFNGLGETPOINTERI_VEXTPROC glad_glGetPointeri_vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPointeri_vEXT(GLenum pname, GLuint index, void ** params) {
    _pre_call_gl_callback("glGetPointeri_vEXT", (GLADapiproc) glad_glGetPointeri_vEXT, 3, pname, index, params);
    glad_glGetPointeri_vEXT(pname, index, params);
    _post_call_gl_callback(NULL, "glGetPointeri_vEXT", (GLADapiproc) glad_glGetPointeri_vEXT, 3, pname, index, params);
    
}
PFNGLGETPOINTERI_VEXTPROC glad_debug_glGetPointeri_vEXT = glad_debug_impl_glGetPointeri_vEXT;
PFNGLGETPOINTERVEXTPROC glad_glGetPointervEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPointervEXT(GLenum pname, void ** params) {
    _pre_call_gl_callback("glGetPointervEXT", (GLADapiproc) glad_glGetPointervEXT, 2, pname, params);
    glad_glGetPointervEXT(pname, params);
    _post_call_gl_callback(NULL, "glGetPointervEXT", (GLADapiproc) glad_glGetPointervEXT, 2, pname, params);
    
}
PFNGLGETPOINTERVEXTPROC glad_debug_glGetPointervEXT = glad_debug_impl_glGetPointervEXT;
PFNGLGETPROGRAMENVPARAMETERDVARBPROC glad_glGetProgramEnvParameterdvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramEnvParameterdvARB(GLenum target, GLuint index, GLdouble * params) {
    _pre_call_gl_callback("glGetProgramEnvParameterdvARB", (GLADapiproc) glad_glGetProgramEnvParameterdvARB, 3, target, index, params);
    glad_glGetProgramEnvParameterdvARB(target, index, params);
    _post_call_gl_callback(NULL, "glGetProgramEnvParameterdvARB", (GLADapiproc) glad_glGetProgramEnvParameterdvARB, 3, target, index, params);
    
}
PFNGLGETPROGRAMENVPARAMETERDVARBPROC glad_debug_glGetProgramEnvParameterdvARB = glad_debug_impl_glGetProgramEnvParameterdvARB;
PFNGLGETPROGRAMENVPARAMETERFVARBPROC glad_glGetProgramEnvParameterfvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramEnvParameterfvARB(GLenum target, GLuint index, GLfloat * params) {
    _pre_call_gl_callback("glGetProgramEnvParameterfvARB", (GLADapiproc) glad_glGetProgramEnvParameterfvARB, 3, target, index, params);
    glad_glGetProgramEnvParameterfvARB(target, index, params);
    _post_call_gl_callback(NULL, "glGetProgramEnvParameterfvARB", (GLADapiproc) glad_glGetProgramEnvParameterfvARB, 3, target, index, params);
    
}
PFNGLGETPROGRAMENVPARAMETERFVARBPROC glad_debug_glGetProgramEnvParameterfvARB = glad_debug_impl_glGetProgramEnvParameterfvARB;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog) {
    _pre_call_gl_callback("glGetProgramInfoLog", (GLADapiproc) glad_glGetProgramInfoLog, 4, program, bufSize, length, infoLog);
    glad_glGetProgramInfoLog(program, bufSize, length, infoLog);
    _post_call_gl_callback(NULL, "glGetProgramInfoLog", (GLADapiproc) glad_glGetProgramInfoLog, 4, program, bufSize, length, infoLog);
    
}
PFNGLGETPROGRAMINFOLOGPROC glad_debug_glGetProgramInfoLog = glad_debug_impl_glGetProgramInfoLog;
PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC glad_glGetProgramLocalParameterdvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramLocalParameterdvARB(GLenum target, GLuint index, GLdouble * params) {
    _pre_call_gl_callback("glGetProgramLocalParameterdvARB", (GLADapiproc) glad_glGetProgramLocalParameterdvARB, 3, target, index, params);
    glad_glGetProgramLocalParameterdvARB(target, index, params);
    _post_call_gl_callback(NULL, "glGetProgramLocalParameterdvARB", (GLADapiproc) glad_glGetProgramLocalParameterdvARB, 3, target, index, params);
    
}
PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC glad_debug_glGetProgramLocalParameterdvARB = glad_debug_impl_glGetProgramLocalParameterdvARB;
PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC glad_glGetProgramLocalParameterfvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramLocalParameterfvARB(GLenum target, GLuint index, GLfloat * params) {
    _pre_call_gl_callback("glGetProgramLocalParameterfvARB", (GLADapiproc) glad_glGetProgramLocalParameterfvARB, 3, target, index, params);
    glad_glGetProgramLocalParameterfvARB(target, index, params);
    _post_call_gl_callback(NULL, "glGetProgramLocalParameterfvARB", (GLADapiproc) glad_glGetProgramLocalParameterfvARB, 3, target, index, params);
    
}
PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC glad_debug_glGetProgramLocalParameterfvARB = glad_debug_impl_glGetProgramLocalParameterfvARB;
PFNGLGETPROGRAMPARAMETERDVNVPROC glad_glGetProgramParameterdvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramParameterdvNV(GLenum target, GLuint index, GLenum pname, GLdouble * params) {
    _pre_call_gl_callback("glGetProgramParameterdvNV", (GLADapiproc) glad_glGetProgramParameterdvNV, 4, target, index, pname, params);
    glad_glGetProgramParameterdvNV(target, index, pname, params);
    _post_call_gl_callback(NULL, "glGetProgramParameterdvNV", (GLADapiproc) glad_glGetProgramParameterdvNV, 4, target, index, pname, params);
    
}
PFNGLGETPROGRAMPARAMETERDVNVPROC glad_debug_glGetProgramParameterdvNV = glad_debug_impl_glGetProgramParameterdvNV;
PFNGLGETPROGRAMPARAMETERFVNVPROC glad_glGetProgramParameterfvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramParameterfvNV(GLenum target, GLuint index, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetProgramParameterfvNV", (GLADapiproc) glad_glGetProgramParameterfvNV, 4, target, index, pname, params);
    glad_glGetProgramParameterfvNV(target, index, pname, params);
    _post_call_gl_callback(NULL, "glGetProgramParameterfvNV", (GLADapiproc) glad_glGetProgramParameterfvNV, 4, target, index, pname, params);
    
}
PFNGLGETPROGRAMPARAMETERFVNVPROC glad_debug_glGetProgramParameterfvNV = glad_debug_impl_glGetProgramParameterfvNV;
PFNGLGETPROGRAMSTRINGARBPROC glad_glGetProgramStringARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramStringARB(GLenum target, GLenum pname, void * string) {
    _pre_call_gl_callback("glGetProgramStringARB", (GLADapiproc) glad_glGetProgramStringARB, 3, target, pname, string);
    glad_glGetProgramStringARB(target, pname, string);
    _post_call_gl_callback(NULL, "glGetProgramStringARB", (GLADapiproc) glad_glGetProgramStringARB, 3, target, pname, string);
    
}
PFNGLGETPROGRAMSTRINGARBPROC glad_debug_glGetProgramStringARB = glad_debug_impl_glGetProgramStringARB;
PFNGLGETPROGRAMSTRINGNVPROC glad_glGetProgramStringNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramStringNV(GLuint id, GLenum pname, GLubyte * program) {
    _pre_call_gl_callback("glGetProgramStringNV", (GLADapiproc) glad_glGetProgramStringNV, 3, id, pname, program);
    glad_glGetProgramStringNV(id, pname, program);
    _post_call_gl_callback(NULL, "glGetProgramStringNV", (GLADapiproc) glad_glGetProgramStringNV, 3, id, pname, program);
    
}
PFNGLGETPROGRAMSTRINGNVPROC glad_debug_glGetProgramStringNV = glad_debug_impl_glGetProgramStringNV;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramiv(GLuint program, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetProgramiv", (GLADapiproc) glad_glGetProgramiv, 3, program, pname, params);
    glad_glGetProgramiv(program, pname, params);
    _post_call_gl_callback(NULL, "glGetProgramiv", (GLADapiproc) glad_glGetProgramiv, 3, program, pname, params);
    
}
PFNGLGETPROGRAMIVPROC glad_debug_glGetProgramiv = glad_debug_impl_glGetProgramiv;
PFNGLGETPROGRAMIVARBPROC glad_glGetProgramivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramivARB(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetProgramivARB", (GLADapiproc) glad_glGetProgramivARB, 3, target, pname, params);
    glad_glGetProgramivARB(target, pname, params);
    _post_call_gl_callback(NULL, "glGetProgramivARB", (GLADapiproc) glad_glGetProgramivARB, 3, target, pname, params);
    
}
PFNGLGETPROGRAMIVARBPROC glad_debug_glGetProgramivARB = glad_debug_impl_glGetProgramivARB;
PFNGLGETPROGRAMIVNVPROC glad_glGetProgramivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramivNV(GLuint id, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetProgramivNV", (GLADapiproc) glad_glGetProgramivNV, 3, id, pname, params);
    glad_glGetProgramivNV(id, pname, params);
    _post_call_gl_callback(NULL, "glGetProgramivNV", (GLADapiproc) glad_glGetProgramivNV, 3, id, pname, params);
    
}
PFNGLGETPROGRAMIVNVPROC glad_debug_glGetProgramivNV = glad_debug_impl_glGetProgramivNV;
PFNGLGETQUERYOBJECTI64VPROC glad_glGetQueryObjecti64v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 * params) {
    _pre_call_gl_callback("glGetQueryObjecti64v", (GLADapiproc) glad_glGetQueryObjecti64v, 3, id, pname, params);
    glad_glGetQueryObjecti64v(id, pname, params);
    _post_call_gl_callback(NULL, "glGetQueryObjecti64v", (GLADapiproc) glad_glGetQueryObjecti64v, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTI64VPROC glad_debug_glGetQueryObjecti64v = glad_debug_impl_glGetQueryObjecti64v;
PFNGLGETQUERYOBJECTI64VEXTPROC glad_glGetQueryObjecti64vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjecti64vEXT(GLuint id, GLenum pname, GLint64 * params) {
    _pre_call_gl_callback("glGetQueryObjecti64vEXT", (GLADapiproc) glad_glGetQueryObjecti64vEXT, 3, id, pname, params);
    glad_glGetQueryObjecti64vEXT(id, pname, params);
    _post_call_gl_callback(NULL, "glGetQueryObjecti64vEXT", (GLADapiproc) glad_glGetQueryObjecti64vEXT, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTI64VEXTPROC glad_debug_glGetQueryObjecti64vEXT = glad_debug_impl_glGetQueryObjecti64vEXT;
PFNGLGETQUERYOBJECTIVPROC glad_glGetQueryObjectiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjectiv(GLuint id, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetQueryObjectiv", (GLADapiproc) glad_glGetQueryObjectiv, 3, id, pname, params);
    glad_glGetQueryObjectiv(id, pname, params);
    _post_call_gl_callback(NULL, "glGetQueryObjectiv", (GLADapiproc) glad_glGetQueryObjectiv, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTIVPROC glad_debug_glGetQueryObjectiv = glad_debug_impl_glGetQueryObjectiv;
PFNGLGETQUERYOBJECTIVARBPROC glad_glGetQueryObjectivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjectivARB(GLuint id, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetQueryObjectivARB", (GLADapiproc) glad_glGetQueryObjectivARB, 3, id, pname, params);
    glad_glGetQueryObjectivARB(id, pname, params);
    _post_call_gl_callback(NULL, "glGetQueryObjectivARB", (GLADapiproc) glad_glGetQueryObjectivARB, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTIVARBPROC glad_debug_glGetQueryObjectivARB = glad_debug_impl_glGetQueryObjectivARB;
PFNGLGETQUERYOBJECTUI64VPROC glad_glGetQueryObjectui64v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 * params) {
    _pre_call_gl_callback("glGetQueryObjectui64v", (GLADapiproc) glad_glGetQueryObjectui64v, 3, id, pname, params);
    glad_glGetQueryObjectui64v(id, pname, params);
    _post_call_gl_callback(NULL, "glGetQueryObjectui64v", (GLADapiproc) glad_glGetQueryObjectui64v, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTUI64VPROC glad_debug_glGetQueryObjectui64v = glad_debug_impl_glGetQueryObjectui64v;
PFNGLGETQUERYOBJECTUI64VEXTPROC glad_glGetQueryObjectui64vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjectui64vEXT(GLuint id, GLenum pname, GLuint64 * params) {
    _pre_call_gl_callback("glGetQueryObjectui64vEXT", (GLADapiproc) glad_glGetQueryObjectui64vEXT, 3, id, pname, params);
    glad_glGetQueryObjectui64vEXT(id, pname, params);
    _post_call_gl_callback(NULL, "glGetQueryObjectui64vEXT", (GLADapiproc) glad_glGetQueryObjectui64vEXT, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTUI64VEXTPROC glad_debug_glGetQueryObjectui64vEXT = glad_debug_impl_glGetQueryObjectui64vEXT;
PFNGLGETQUERYOBJECTUIVPROC glad_glGetQueryObjectuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint * params) {
    _pre_call_gl_callback("glGetQueryObjectuiv", (GLADapiproc) glad_glGetQueryObjectuiv, 3, id, pname, params);
    glad_glGetQueryObjectuiv(id, pname, params);
    _post_call_gl_callback(NULL, "glGetQueryObjectuiv", (GLADapiproc) glad_glGetQueryObjectuiv, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTUIVPROC glad_debug_glGetQueryObjectuiv = glad_debug_impl_glGetQueryObjectuiv;
PFNGLGETQUERYOBJECTUIVARBPROC glad_glGetQueryObjectuivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjectuivARB(GLuint id, GLenum pname, GLuint * params) {
    _pre_call_gl_callback("glGetQueryObjectuivARB", (GLADapiproc) glad_glGetQueryObjectuivARB, 3, id, pname, params);
    glad_glGetQueryObjectuivARB(id, pname, params);
    _post_call_gl_callback(NULL, "glGetQueryObjectuivARB", (GLADapiproc) glad_glGetQueryObjectuivARB, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTUIVARBPROC glad_debug_glGetQueryObjectuivARB = glad_debug_impl_glGetQueryObjectuivARB;
PFNGLGETQUERYIVPROC glad_glGetQueryiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryiv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetQueryiv", (GLADapiproc) glad_glGetQueryiv, 3, target, pname, params);
    glad_glGetQueryiv(target, pname, params);
    _post_call_gl_callback(NULL, "glGetQueryiv", (GLADapiproc) glad_glGetQueryiv, 3, target, pname, params);
    
}
PFNGLGETQUERYIVPROC glad_debug_glGetQueryiv = glad_debug_impl_glGetQueryiv;
PFNGLGETQUERYIVARBPROC glad_glGetQueryivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryivARB(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetQueryivARB", (GLADapiproc) glad_glGetQueryivARB, 3, target, pname, params);
    glad_glGetQueryivARB(target, pname, params);
    _post_call_gl_callback(NULL, "glGetQueryivARB", (GLADapiproc) glad_glGetQueryivARB, 3, target, pname, params);
    
}
PFNGLGETQUERYIVARBPROC glad_debug_glGetQueryivARB = glad_debug_impl_glGetQueryivARB;
PFNGLGETRENDERBUFFERPARAMETERIVPROC glad_glGetRenderbufferParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetRenderbufferParameteriv", (GLADapiproc) glad_glGetRenderbufferParameteriv, 3, target, pname, params);
    glad_glGetRenderbufferParameteriv(target, pname, params);
    _post_call_gl_callback(NULL, "glGetRenderbufferParameteriv", (GLADapiproc) glad_glGetRenderbufferParameteriv, 3, target, pname, params);
    
}
PFNGLGETRENDERBUFFERPARAMETERIVPROC glad_debug_glGetRenderbufferParameteriv = glad_debug_impl_glGetRenderbufferParameteriv;
PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC glad_glGetRenderbufferParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetRenderbufferParameterivEXT(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetRenderbufferParameterivEXT", (GLADapiproc) glad_glGetRenderbufferParameterivEXT, 3, target, pname, params);
    glad_glGetRenderbufferParameterivEXT(target, pname, params);
    _post_call_gl_callback(NULL, "glGetRenderbufferParameterivEXT", (GLADapiproc) glad_glGetRenderbufferParameterivEXT, 3, target, pname, params);
    
}
PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC glad_debug_glGetRenderbufferParameterivEXT = glad_debug_impl_glGetRenderbufferParameterivEXT;
PFNGLGETSAMPLERPARAMETERIIVPROC glad_glGetSamplerParameterIiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetSamplerParameterIiv", (GLADapiproc) glad_glGetSamplerParameterIiv, 3, sampler, pname, params);
    glad_glGetSamplerParameterIiv(sampler, pname, params);
    _post_call_gl_callback(NULL, "glGetSamplerParameterIiv", (GLADapiproc) glad_glGetSamplerParameterIiv, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERIIVPROC glad_debug_glGetSamplerParameterIiv = glad_debug_impl_glGetSamplerParameterIiv;
PFNGLGETSAMPLERPARAMETERIUIVPROC glad_glGetSamplerParameterIuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint * params) {
    _pre_call_gl_callback("glGetSamplerParameterIuiv", (GLADapiproc) glad_glGetSamplerParameterIuiv, 3, sampler, pname, params);
    glad_glGetSamplerParameterIuiv(sampler, pname, params);
    _post_call_gl_callback(NULL, "glGetSamplerParameterIuiv", (GLADapiproc) glad_glGetSamplerParameterIuiv, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERIUIVPROC glad_debug_glGetSamplerParameterIuiv = glad_debug_impl_glGetSamplerParameterIuiv;
PFNGLGETSAMPLERPARAMETERFVPROC glad_glGetSamplerParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetSamplerParameterfv", (GLADapiproc) glad_glGetSamplerParameterfv, 3, sampler, pname, params);
    glad_glGetSamplerParameterfv(sampler, pname, params);
    _post_call_gl_callback(NULL, "glGetSamplerParameterfv", (GLADapiproc) glad_glGetSamplerParameterfv, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERFVPROC glad_debug_glGetSamplerParameterfv = glad_debug_impl_glGetSamplerParameterfv;
PFNGLGETSAMPLERPARAMETERIVPROC glad_glGetSamplerParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetSamplerParameteriv", (GLADapiproc) glad_glGetSamplerParameteriv, 3, sampler, pname, params);
    glad_glGetSamplerParameteriv(sampler, pname, params);
    _post_call_gl_callback(NULL, "glGetSamplerParameteriv", (GLADapiproc) glad_glGetSamplerParameteriv, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERIVPROC glad_debug_glGetSamplerParameteriv = glad_debug_impl_glGetSamplerParameteriv;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog) {
    _pre_call_gl_callback("glGetShaderInfoLog", (GLADapiproc) glad_glGetShaderInfoLog, 4, shader, bufSize, length, infoLog);
    glad_glGetShaderInfoLog(shader, bufSize, length, infoLog);
    _post_call_gl_callback(NULL, "glGetShaderInfoLog", (GLADapiproc) glad_glGetShaderInfoLog, 4, shader, bufSize, length, infoLog);
    
}
PFNGLGETSHADERINFOLOGPROC glad_debug_glGetShaderInfoLog = glad_debug_impl_glGetShaderInfoLog;
PFNGLGETSHADERSOURCEPROC glad_glGetShaderSource = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * source) {
    _pre_call_gl_callback("glGetShaderSource", (GLADapiproc) glad_glGetShaderSource, 4, shader, bufSize, length, source);
    glad_glGetShaderSource(shader, bufSize, length, source);
    _post_call_gl_callback(NULL, "glGetShaderSource", (GLADapiproc) glad_glGetShaderSource, 4, shader, bufSize, length, source);
    
}
PFNGLGETSHADERSOURCEPROC glad_debug_glGetShaderSource = glad_debug_impl_glGetShaderSource;
PFNGLGETSHADERSOURCEARBPROC glad_glGetShaderSourceARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetShaderSourceARB(GLhandleARB obj, GLsizei maxLength, GLsizei * length, GLcharARB * source) {
    _pre_call_gl_callback("glGetShaderSourceARB", (GLADapiproc) glad_glGetShaderSourceARB, 4, obj, maxLength, length, source);
    glad_glGetShaderSourceARB(obj, maxLength, length, source);
    _post_call_gl_callback(NULL, "glGetShaderSourceARB", (GLADapiproc) glad_glGetShaderSourceARB, 4, obj, maxLength, length, source);
    
}
PFNGLGETSHADERSOURCEARBPROC glad_debug_glGetShaderSourceARB = glad_debug_impl_glGetShaderSourceARB;
PFNGLGETSHADERIVPROC glad_glGetShaderiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetShaderiv(GLuint shader, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetShaderiv", (GLADapiproc) glad_glGetShaderiv, 3, shader, pname, params);
    glad_glGetShaderiv(shader, pname, params);
    _post_call_gl_callback(NULL, "glGetShaderiv", (GLADapiproc) glad_glGetShaderiv, 3, shader, pname, params);
    
}
PFNGLGETSHADERIVPROC glad_debug_glGetShaderiv = glad_debug_impl_glGetShaderiv;
PFNGLGETSTRINGPROC glad_glGetString = NULL;
static const GLubyte * GLAD_API_PTR glad_debug_impl_glGetString(GLenum name) {
    const GLubyte * ret;
    _pre_call_gl_callback("glGetString", (GLADapiproc) glad_glGetString, 1, name);
    ret = glad_glGetString(name);
    _post_call_gl_callback((void*) &ret, "glGetString", (GLADapiproc) glad_glGetString, 1, name);
    return ret;
}
PFNGLGETSTRINGPROC glad_debug_glGetString = glad_debug_impl_glGetString;
PFNGLGETSTRINGIPROC glad_glGetStringi = NULL;
static const GLubyte * GLAD_API_PTR glad_debug_impl_glGetStringi(GLenum name, GLuint index) {
    const GLubyte * ret;
    _pre_call_gl_callback("glGetStringi", (GLADapiproc) glad_glGetStringi, 2, name, index);
    ret = glad_glGetStringi(name, index);
    _post_call_gl_callback((void*) &ret, "glGetStringi", (GLADapiproc) glad_glGetStringi, 2, name, index);
    return ret;
}
PFNGLGETSTRINGIPROC glad_debug_glGetStringi = glad_debug_impl_glGetStringi;
PFNGLGETSYNCIVPROC glad_glGetSynciv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSynciv(GLsync sync, GLenum pname, GLsizei count, GLsizei * length, GLint * values) {
    _pre_call_gl_callback("glGetSynciv", (GLADapiproc) glad_glGetSynciv, 5, sync, pname, count, length, values);
    glad_glGetSynciv(sync, pname, count, length, values);
    _post_call_gl_callback(NULL, "glGetSynciv", (GLADapiproc) glad_glGetSynciv, 5, sync, pname, count, length, values);
    
}
PFNGLGETSYNCIVPROC glad_debug_glGetSynciv = glad_debug_impl_glGetSynciv;
PFNGLGETTEXIMAGEPROC glad_glGetTexImage = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void * pixels) {
    _pre_call_gl_callback("glGetTexImage", (GLADapiproc) glad_glGetTexImage, 5, target, level, format, type, pixels);
    glad_glGetTexImage(target, level, format, type, pixels);
    _post_call_gl_callback(NULL, "glGetTexImage", (GLADapiproc) glad_glGetTexImage, 5, target, level, format, type, pixels);
    
}
PFNGLGETTEXIMAGEPROC glad_debug_glGetTexImage = glad_debug_impl_glGetTexImage;
PFNGLGETTEXLEVELPARAMETERFVPROC glad_glGetTexLevelParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetTexLevelParameterfv", (GLADapiproc) glad_glGetTexLevelParameterfv, 4, target, level, pname, params);
    glad_glGetTexLevelParameterfv(target, level, pname, params);
    _post_call_gl_callback(NULL, "glGetTexLevelParameterfv", (GLADapiproc) glad_glGetTexLevelParameterfv, 4, target, level, pname, params);
    
}
PFNGLGETTEXLEVELPARAMETERFVPROC glad_debug_glGetTexLevelParameterfv = glad_debug_impl_glGetTexLevelParameterfv;
PFNGLGETTEXLEVELPARAMETERIVPROC glad_glGetTexLevelParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetTexLevelParameteriv", (GLADapiproc) glad_glGetTexLevelParameteriv, 4, target, level, pname, params);
    glad_glGetTexLevelParameteriv(target, level, pname, params);
    _post_call_gl_callback(NULL, "glGetTexLevelParameteriv", (GLADapiproc) glad_glGetTexLevelParameteriv, 4, target, level, pname, params);
    
}
PFNGLGETTEXLEVELPARAMETERIVPROC glad_debug_glGetTexLevelParameteriv = glad_debug_impl_glGetTexLevelParameteriv;
PFNGLGETTEXPARAMETERIIVPROC glad_glGetTexParameterIiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterIiv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetTexParameterIiv", (GLADapiproc) glad_glGetTexParameterIiv, 3, target, pname, params);
    glad_glGetTexParameterIiv(target, pname, params);
    _post_call_gl_callback(NULL, "glGetTexParameterIiv", (GLADapiproc) glad_glGetTexParameterIiv, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIIVPROC glad_debug_glGetTexParameterIiv = glad_debug_impl_glGetTexParameterIiv;
PFNGLGETTEXPARAMETERIIVEXTPROC glad_glGetTexParameterIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterIivEXT(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetTexParameterIivEXT", (GLADapiproc) glad_glGetTexParameterIivEXT, 3, target, pname, params);
    glad_glGetTexParameterIivEXT(target, pname, params);
    _post_call_gl_callback(NULL, "glGetTexParameterIivEXT", (GLADapiproc) glad_glGetTexParameterIivEXT, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIIVEXTPROC glad_debug_glGetTexParameterIivEXT = glad_debug_impl_glGetTexParameterIivEXT;
PFNGLGETTEXPARAMETERIUIVPROC glad_glGetTexParameterIuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint * params) {
    _pre_call_gl_callback("glGetTexParameterIuiv", (GLADapiproc) glad_glGetTexParameterIuiv, 3, target, pname, params);
    glad_glGetTexParameterIuiv(target, pname, params);
    _post_call_gl_callback(NULL, "glGetTexParameterIuiv", (GLADapiproc) glad_glGetTexParameterIuiv, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIUIVPROC glad_debug_glGetTexParameterIuiv = glad_debug_impl_glGetTexParameterIuiv;
PFNGLGETTEXPARAMETERIUIVEXTPROC glad_glGetTexParameterIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterIuivEXT(GLenum target, GLenum pname, GLuint * params) {
    _pre_call_gl_callback("glGetTexParameterIuivEXT", (GLADapiproc) glad_glGetTexParameterIuivEXT, 3, target, pname, params);
    glad_glGetTexParameterIuivEXT(target, pname, params);
    _post_call_gl_callback(NULL, "glGetTexParameterIuivEXT", (GLADapiproc) glad_glGetTexParameterIuivEXT, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIUIVEXTPROC glad_debug_glGetTexParameterIuivEXT = glad_debug_impl_glGetTexParameterIuivEXT;
PFNGLGETTEXPARAMETERFVPROC glad_glGetTexParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetTexParameterfv", (GLADapiproc) glad_glGetTexParameterfv, 3, target, pname, params);
    glad_glGetTexParameterfv(target, pname, params);
    _post_call_gl_callback(NULL, "glGetTexParameterfv", (GLADapiproc) glad_glGetTexParameterfv, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERFVPROC glad_debug_glGetTexParameterfv = glad_debug_impl_glGetTexParameterfv;
PFNGLGETTEXPARAMETERIVPROC glad_glGetTexParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameteriv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetTexParameteriv", (GLADapiproc) glad_glGetTexParameteriv, 3, target, pname, params);
    glad_glGetTexParameteriv(target, pname, params);
    _post_call_gl_callback(NULL, "glGetTexParameteriv", (GLADapiproc) glad_glGetTexParameteriv, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIVPROC glad_debug_glGetTexParameteriv = glad_debug_impl_glGetTexParameteriv;
PFNGLGETTEXTUREIMAGEEXTPROC glad_glGetTextureImageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, void * pixels) {
    _pre_call_gl_callback("glGetTextureImageEXT", (GLADapiproc) glad_glGetTextureImageEXT, 6, texture, target, level, format, type, pixels);
    glad_glGetTextureImageEXT(texture, target, level, format, type, pixels);
    _post_call_gl_callback(NULL, "glGetTextureImageEXT", (GLADapiproc) glad_glGetTextureImageEXT, 6, texture, target, level, format, type, pixels);
    
}
PFNGLGETTEXTUREIMAGEEXTPROC glad_debug_glGetTextureImageEXT = glad_debug_impl_glGetTextureImageEXT;
PFNGLGETTEXTURELEVELPARAMETERFVEXTPROC glad_glGetTextureLevelParameterfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTextureLevelParameterfvEXT(GLuint texture, GLenum target, GLint level, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetTextureLevelParameterfvEXT", (GLADapiproc) glad_glGetTextureLevelParameterfvEXT, 5, texture, target, level, pname, params);
    glad_glGetTextureLevelParameterfvEXT(texture, target, level, pname, params);
    _post_call_gl_callback(NULL, "glGetTextureLevelParameterfvEXT", (GLADapiproc) glad_glGetTextureLevelParameterfvEXT, 5, texture, target, level, pname, params);
    
}
PFNGLGETTEXTURELEVELPARAMETERFVEXTPROC glad_debug_glGetTextureLevelParameterfvEXT = glad_debug_impl_glGetTextureLevelParameterfvEXT;
PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC glad_glGetTextureLevelParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetTextureLevelParameterivEXT", (GLADapiproc) glad_glGetTextureLevelParameterivEXT, 5, texture, target, level, pname, params);
    glad_glGetTextureLevelParameterivEXT(texture, target, level, pname, params);
    _post_call_gl_callback(NULL, "glGetTextureLevelParameterivEXT", (GLADapiproc) glad_glGetTextureLevelParameterivEXT, 5, texture, target, level, pname, params);
    
}
PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC glad_debug_glGetTextureLevelParameterivEXT = glad_debug_impl_glGetTextureLevelParameterivEXT;
PFNGLGETTEXTUREPARAMETERIIVEXTPROC glad_glGetTextureParameterIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetTextureParameterIivEXT", (GLADapiproc) glad_glGetTextureParameterIivEXT, 4, texture, target, pname, params);
    glad_glGetTextureParameterIivEXT(texture, target, pname, params);
    _post_call_gl_callback(NULL, "glGetTextureParameterIivEXT", (GLADapiproc) glad_glGetTextureParameterIivEXT, 4, texture, target, pname, params);
    
}
PFNGLGETTEXTUREPARAMETERIIVEXTPROC glad_debug_glGetTextureParameterIivEXT = glad_debug_impl_glGetTextureParameterIivEXT;
PFNGLGETTEXTUREPARAMETERIUIVEXTPROC glad_glGetTextureParameterIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname, GLuint * params) {
    _pre_call_gl_callback("glGetTextureParameterIuivEXT", (GLADapiproc) glad_glGetTextureParameterIuivEXT, 4, texture, target, pname, params);
    glad_glGetTextureParameterIuivEXT(texture, target, pname, params);
    _post_call_gl_callback(NULL, "glGetTextureParameterIuivEXT", (GLADapiproc) glad_glGetTextureParameterIuivEXT, 4, texture, target, pname, params);
    
}
PFNGLGETTEXTUREPARAMETERIUIVEXTPROC glad_debug_glGetTextureParameterIuivEXT = glad_debug_impl_glGetTextureParameterIuivEXT;
PFNGLGETTEXTUREPARAMETERFVEXTPROC glad_glGetTextureParameterfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetTextureParameterfvEXT", (GLADapiproc) glad_glGetTextureParameterfvEXT, 4, texture, target, pname, params);
    glad_glGetTextureParameterfvEXT(texture, target, pname, params);
    _post_call_gl_callback(NULL, "glGetTextureParameterfvEXT", (GLADapiproc) glad_glGetTextureParameterfvEXT, 4, texture, target, pname, params);
    
}
PFNGLGETTEXTUREPARAMETERFVEXTPROC glad_debug_glGetTextureParameterfvEXT = glad_debug_impl_glGetTextureParameterfvEXT;
PFNGLGETTEXTUREPARAMETERIVEXTPROC glad_glGetTextureParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetTextureParameterivEXT", (GLADapiproc) glad_glGetTextureParameterivEXT, 4, texture, target, pname, params);
    glad_glGetTextureParameterivEXT(texture, target, pname, params);
    _post_call_gl_callback(NULL, "glGetTextureParameterivEXT", (GLADapiproc) glad_glGetTextureParameterivEXT, 4, texture, target, pname, params);
    
}
PFNGLGETTEXTUREPARAMETERIVEXTPROC glad_debug_glGetTextureParameterivEXT = glad_debug_impl_glGetTextureParameterivEXT;
PFNGLGETTRACKMATRIXIVNVPROC glad_glGetTrackMatrixivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTrackMatrixivNV(GLenum target, GLuint address, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetTrackMatrixivNV", (GLADapiproc) glad_glGetTrackMatrixivNV, 4, target, address, pname, params);
    glad_glGetTrackMatrixivNV(target, address, pname, params);
    _post_call_gl_callback(NULL, "glGetTrackMatrixivNV", (GLADapiproc) glad_glGetTrackMatrixivNV, 4, target, address, pname, params);
    
}
PFNGLGETTRACKMATRIXIVNVPROC glad_debug_glGetTrackMatrixivNV = glad_debug_impl_glGetTrackMatrixivNV;
PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glad_glGetTransformFeedbackVarying = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLsizei * size, GLenum * type, GLchar * name) {
    _pre_call_gl_callback("glGetTransformFeedbackVarying", (GLADapiproc) glad_glGetTransformFeedbackVarying, 7, program, index, bufSize, length, size, type, name);
    glad_glGetTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
    _post_call_gl_callback(NULL, "glGetTransformFeedbackVarying", (GLADapiproc) glad_glGetTransformFeedbackVarying, 7, program, index, bufSize, length, size, type, name);
    
}
PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glad_debug_glGetTransformFeedbackVarying = glad_debug_impl_glGetTransformFeedbackVarying;
PFNGLGETTRANSFORMFEEDBACKVARYINGEXTPROC glad_glGetTransformFeedbackVaryingEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTransformFeedbackVaryingEXT(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLsizei * size, GLenum * type, GLchar * name) {
    _pre_call_gl_callback("glGetTransformFeedbackVaryingEXT", (GLADapiproc) glad_glGetTransformFeedbackVaryingEXT, 7, program, index, bufSize, length, size, type, name);
    glad_glGetTransformFeedbackVaryingEXT(program, index, bufSize, length, size, type, name);
    _post_call_gl_callback(NULL, "glGetTransformFeedbackVaryingEXT", (GLADapiproc) glad_glGetTransformFeedbackVaryingEXT, 7, program, index, bufSize, length, size, type, name);
    
}
PFNGLGETTRANSFORMFEEDBACKVARYINGEXTPROC glad_debug_glGetTransformFeedbackVaryingEXT = glad_debug_impl_glGetTransformFeedbackVaryingEXT;
PFNGLGETTRANSFORMFEEDBACKVARYINGNVPROC glad_glGetTransformFeedbackVaryingNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTransformFeedbackVaryingNV(GLuint program, GLuint index, GLint * location) {
    _pre_call_gl_callback("glGetTransformFeedbackVaryingNV", (GLADapiproc) glad_glGetTransformFeedbackVaryingNV, 3, program, index, location);
    glad_glGetTransformFeedbackVaryingNV(program, index, location);
    _post_call_gl_callback(NULL, "glGetTransformFeedbackVaryingNV", (GLADapiproc) glad_glGetTransformFeedbackVaryingNV, 3, program, index, location);
    
}
PFNGLGETTRANSFORMFEEDBACKVARYINGNVPROC glad_debug_glGetTransformFeedbackVaryingNV = glad_debug_impl_glGetTransformFeedbackVaryingNV;
PFNGLGETUNIFORMBLOCKINDEXPROC glad_glGetUniformBlockIndex = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glGetUniformBlockIndex(GLuint program, const GLchar * uniformBlockName) {
    GLuint ret;
    _pre_call_gl_callback("glGetUniformBlockIndex", (GLADapiproc) glad_glGetUniformBlockIndex, 2, program, uniformBlockName);
    ret = glad_glGetUniformBlockIndex(program, uniformBlockName);
    _post_call_gl_callback((void*) &ret, "glGetUniformBlockIndex", (GLADapiproc) glad_glGetUniformBlockIndex, 2, program, uniformBlockName);
    return ret;
}
PFNGLGETUNIFORMBLOCKINDEXPROC glad_debug_glGetUniformBlockIndex = glad_debug_impl_glGetUniformBlockIndex;
PFNGLGETUNIFORMINDICESPROC glad_glGetUniformIndices = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const* uniformNames, GLuint * uniformIndices) {
    _pre_call_gl_callback("glGetUniformIndices", (GLADapiproc) glad_glGetUniformIndices, 4, program, uniformCount, uniformNames, uniformIndices);
    glad_glGetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
    _post_call_gl_callback(NULL, "glGetUniformIndices", (GLADapiproc) glad_glGetUniformIndices, 4, program, uniformCount, uniformNames, uniformIndices);
    
}
PFNGLGETUNIFORMINDICESPROC glad_debug_glGetUniformIndices = glad_debug_impl_glGetUniformIndices;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetUniformLocation(GLuint program, const GLchar * name) {
    GLint ret;
    _pre_call_gl_callback("glGetUniformLocation", (GLADapiproc) glad_glGetUniformLocation, 2, program, name);
    ret = glad_glGetUniformLocation(program, name);
    _post_call_gl_callback((void*) &ret, "glGetUniformLocation", (GLADapiproc) glad_glGetUniformLocation, 2, program, name);
    return ret;
}
PFNGLGETUNIFORMLOCATIONPROC glad_debug_glGetUniformLocation = glad_debug_impl_glGetUniformLocation;
PFNGLGETUNIFORMLOCATIONARBPROC glad_glGetUniformLocationARB = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetUniformLocationARB(GLhandleARB programObj, const GLcharARB * name) {
    GLint ret;
    _pre_call_gl_callback("glGetUniformLocationARB", (GLADapiproc) glad_glGetUniformLocationARB, 2, programObj, name);
    ret = glad_glGetUniformLocationARB(programObj, name);
    _post_call_gl_callback((void*) &ret, "glGetUniformLocationARB", (GLADapiproc) glad_glGetUniformLocationARB, 2, programObj, name);
    return ret;
}
PFNGLGETUNIFORMLOCATIONARBPROC glad_debug_glGetUniformLocationARB = glad_debug_impl_glGetUniformLocationARB;
PFNGLGETUNIFORMFVPROC glad_glGetUniformfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformfv(GLuint program, GLint location, GLfloat * params) {
    _pre_call_gl_callback("glGetUniformfv", (GLADapiproc) glad_glGetUniformfv, 3, program, location, params);
    glad_glGetUniformfv(program, location, params);
    _post_call_gl_callback(NULL, "glGetUniformfv", (GLADapiproc) glad_glGetUniformfv, 3, program, location, params);
    
}
PFNGLGETUNIFORMFVPROC glad_debug_glGetUniformfv = glad_debug_impl_glGetUniformfv;
PFNGLGETUNIFORMFVARBPROC glad_glGetUniformfvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformfvARB(GLhandleARB programObj, GLint location, GLfloat * params) {
    _pre_call_gl_callback("glGetUniformfvARB", (GLADapiproc) glad_glGetUniformfvARB, 3, programObj, location, params);
    glad_glGetUniformfvARB(programObj, location, params);
    _post_call_gl_callback(NULL, "glGetUniformfvARB", (GLADapiproc) glad_glGetUniformfvARB, 3, programObj, location, params);
    
}
PFNGLGETUNIFORMFVARBPROC glad_debug_glGetUniformfvARB = glad_debug_impl_glGetUniformfvARB;
PFNGLGETUNIFORMIVPROC glad_glGetUniformiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformiv(GLuint program, GLint location, GLint * params) {
    _pre_call_gl_callback("glGetUniformiv", (GLADapiproc) glad_glGetUniformiv, 3, program, location, params);
    glad_glGetUniformiv(program, location, params);
    _post_call_gl_callback(NULL, "glGetUniformiv", (GLADapiproc) glad_glGetUniformiv, 3, program, location, params);
    
}
PFNGLGETUNIFORMIVPROC glad_debug_glGetUniformiv = glad_debug_impl_glGetUniformiv;
PFNGLGETUNIFORMIVARBPROC glad_glGetUniformivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformivARB(GLhandleARB programObj, GLint location, GLint * params) {
    _pre_call_gl_callback("glGetUniformivARB", (GLADapiproc) glad_glGetUniformivARB, 3, programObj, location, params);
    glad_glGetUniformivARB(programObj, location, params);
    _post_call_gl_callback(NULL, "glGetUniformivARB", (GLADapiproc) glad_glGetUniformivARB, 3, programObj, location, params);
    
}
PFNGLGETUNIFORMIVARBPROC glad_debug_glGetUniformivARB = glad_debug_impl_glGetUniformivARB;
PFNGLGETUNIFORMUIVPROC glad_glGetUniformuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformuiv(GLuint program, GLint location, GLuint * params) {
    _pre_call_gl_callback("glGetUniformuiv", (GLADapiproc) glad_glGetUniformuiv, 3, program, location, params);
    glad_glGetUniformuiv(program, location, params);
    _post_call_gl_callback(NULL, "glGetUniformuiv", (GLADapiproc) glad_glGetUniformuiv, 3, program, location, params);
    
}
PFNGLGETUNIFORMUIVPROC glad_debug_glGetUniformuiv = glad_debug_impl_glGetUniformuiv;
PFNGLGETUNIFORMUIVEXTPROC glad_glGetUniformuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformuivEXT(GLuint program, GLint location, GLuint * params) {
    _pre_call_gl_callback("glGetUniformuivEXT", (GLADapiproc) glad_glGetUniformuivEXT, 3, program, location, params);
    glad_glGetUniformuivEXT(program, location, params);
    _post_call_gl_callback(NULL, "glGetUniformuivEXT", (GLADapiproc) glad_glGetUniformuivEXT, 3, program, location, params);
    
}
PFNGLGETUNIFORMUIVEXTPROC glad_debug_glGetUniformuivEXT = glad_debug_impl_glGetUniformuivEXT;
PFNGLGETVARYINGLOCATIONNVPROC glad_glGetVaryingLocationNV = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetVaryingLocationNV(GLuint program, const GLchar * name) {
    GLint ret;
    _pre_call_gl_callback("glGetVaryingLocationNV", (GLADapiproc) glad_glGetVaryingLocationNV, 2, program, name);
    ret = glad_glGetVaryingLocationNV(program, name);
    _post_call_gl_callback((void*) &ret, "glGetVaryingLocationNV", (GLADapiproc) glad_glGetVaryingLocationNV, 2, program, name);
    return ret;
}
PFNGLGETVARYINGLOCATIONNVPROC glad_debug_glGetVaryingLocationNV = glad_debug_impl_glGetVaryingLocationNV;
PFNGLGETVERTEXARRAYINTEGERI_VEXTPROC glad_glGetVertexArrayIntegeri_vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexArrayIntegeri_vEXT(GLuint vaobj, GLuint index, GLenum pname, GLint * param) {
    _pre_call_gl_callback("glGetVertexArrayIntegeri_vEXT", (GLADapiproc) glad_glGetVertexArrayIntegeri_vEXT, 4, vaobj, index, pname, param);
    glad_glGetVertexArrayIntegeri_vEXT(vaobj, index, pname, param);
    _post_call_gl_callback(NULL, "glGetVertexArrayIntegeri_vEXT", (GLADapiproc) glad_glGetVertexArrayIntegeri_vEXT, 4, vaobj, index, pname, param);
    
}
PFNGLGETVERTEXARRAYINTEGERI_VEXTPROC glad_debug_glGetVertexArrayIntegeri_vEXT = glad_debug_impl_glGetVertexArrayIntegeri_vEXT;
PFNGLGETVERTEXARRAYINTEGERVEXTPROC glad_glGetVertexArrayIntegervEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexArrayIntegervEXT(GLuint vaobj, GLenum pname, GLint * param) {
    _pre_call_gl_callback("glGetVertexArrayIntegervEXT", (GLADapiproc) glad_glGetVertexArrayIntegervEXT, 3, vaobj, pname, param);
    glad_glGetVertexArrayIntegervEXT(vaobj, pname, param);
    _post_call_gl_callback(NULL, "glGetVertexArrayIntegervEXT", (GLADapiproc) glad_glGetVertexArrayIntegervEXT, 3, vaobj, pname, param);
    
}
PFNGLGETVERTEXARRAYINTEGERVEXTPROC glad_debug_glGetVertexArrayIntegervEXT = glad_debug_impl_glGetVertexArrayIntegervEXT;
PFNGLGETVERTEXARRAYPOINTERI_VEXTPROC glad_glGetVertexArrayPointeri_vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexArrayPointeri_vEXT(GLuint vaobj, GLuint index, GLenum pname, void ** param) {
    _pre_call_gl_callback("glGetVertexArrayPointeri_vEXT", (GLADapiproc) glad_glGetVertexArrayPointeri_vEXT, 4, vaobj, index, pname, param);
    glad_glGetVertexArrayPointeri_vEXT(vaobj, index, pname, param);
    _post_call_gl_callback(NULL, "glGetVertexArrayPointeri_vEXT", (GLADapiproc) glad_glGetVertexArrayPointeri_vEXT, 4, vaobj, index, pname, param);
    
}
PFNGLGETVERTEXARRAYPOINTERI_VEXTPROC glad_debug_glGetVertexArrayPointeri_vEXT = glad_debug_impl_glGetVertexArrayPointeri_vEXT;
PFNGLGETVERTEXARRAYPOINTERVEXTPROC glad_glGetVertexArrayPointervEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexArrayPointervEXT(GLuint vaobj, GLenum pname, void ** param) {
    _pre_call_gl_callback("glGetVertexArrayPointervEXT", (GLADapiproc) glad_glGetVertexArrayPointervEXT, 3, vaobj, pname, param);
    glad_glGetVertexArrayPointervEXT(vaobj, pname, param);
    _post_call_gl_callback(NULL, "glGetVertexArrayPointervEXT", (GLADapiproc) glad_glGetVertexArrayPointervEXT, 3, vaobj, pname, param);
    
}
PFNGLGETVERTEXARRAYPOINTERVEXTPROC glad_debug_glGetVertexArrayPointervEXT = glad_debug_impl_glGetVertexArrayPointervEXT;
PFNGLGETVERTEXATTRIBIIVPROC glad_glGetVertexAttribIiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribIiv(GLuint index, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetVertexAttribIiv", (GLADapiproc) glad_glGetVertexAttribIiv, 3, index, pname, params);
    glad_glGetVertexAttribIiv(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribIiv", (GLADapiproc) glad_glGetVertexAttribIiv, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBIIVPROC glad_debug_glGetVertexAttribIiv = glad_debug_impl_glGetVertexAttribIiv;
PFNGLGETVERTEXATTRIBIIVEXTPROC glad_glGetVertexAttribIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribIivEXT(GLuint index, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetVertexAttribIivEXT", (GLADapiproc) glad_glGetVertexAttribIivEXT, 3, index, pname, params);
    glad_glGetVertexAttribIivEXT(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribIivEXT", (GLADapiproc) glad_glGetVertexAttribIivEXT, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBIIVEXTPROC glad_debug_glGetVertexAttribIivEXT = glad_debug_impl_glGetVertexAttribIivEXT;
PFNGLGETVERTEXATTRIBIUIVPROC glad_glGetVertexAttribIuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint * params) {
    _pre_call_gl_callback("glGetVertexAttribIuiv", (GLADapiproc) glad_glGetVertexAttribIuiv, 3, index, pname, params);
    glad_glGetVertexAttribIuiv(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribIuiv", (GLADapiproc) glad_glGetVertexAttribIuiv, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBIUIVPROC glad_debug_glGetVertexAttribIuiv = glad_debug_impl_glGetVertexAttribIuiv;
PFNGLGETVERTEXATTRIBIUIVEXTPROC glad_glGetVertexAttribIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribIuivEXT(GLuint index, GLenum pname, GLuint * params) {
    _pre_call_gl_callback("glGetVertexAttribIuivEXT", (GLADapiproc) glad_glGetVertexAttribIuivEXT, 3, index, pname, params);
    glad_glGetVertexAttribIuivEXT(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribIuivEXT", (GLADapiproc) glad_glGetVertexAttribIuivEXT, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBIUIVEXTPROC glad_debug_glGetVertexAttribIuivEXT = glad_debug_impl_glGetVertexAttribIuivEXT;
PFNGLGETVERTEXATTRIBPOINTERVPROC glad_glGetVertexAttribPointerv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribPointerv(GLuint index, GLenum pname, void ** pointer) {
    _pre_call_gl_callback("glGetVertexAttribPointerv", (GLADapiproc) glad_glGetVertexAttribPointerv, 3, index, pname, pointer);
    glad_glGetVertexAttribPointerv(index, pname, pointer);
    _post_call_gl_callback(NULL, "glGetVertexAttribPointerv", (GLADapiproc) glad_glGetVertexAttribPointerv, 3, index, pname, pointer);
    
}
PFNGLGETVERTEXATTRIBPOINTERVPROC glad_debug_glGetVertexAttribPointerv = glad_debug_impl_glGetVertexAttribPointerv;
PFNGLGETVERTEXATTRIBPOINTERVARBPROC glad_glGetVertexAttribPointervARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribPointervARB(GLuint index, GLenum pname, void ** pointer) {
    _pre_call_gl_callback("glGetVertexAttribPointervARB", (GLADapiproc) glad_glGetVertexAttribPointervARB, 3, index, pname, pointer);
    glad_glGetVertexAttribPointervARB(index, pname, pointer);
    _post_call_gl_callback(NULL, "glGetVertexAttribPointervARB", (GLADapiproc) glad_glGetVertexAttribPointervARB, 3, index, pname, pointer);
    
}
PFNGLGETVERTEXATTRIBPOINTERVARBPROC glad_debug_glGetVertexAttribPointervARB = glad_debug_impl_glGetVertexAttribPointervARB;
PFNGLGETVERTEXATTRIBPOINTERVNVPROC glad_glGetVertexAttribPointervNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribPointervNV(GLuint index, GLenum pname, void ** pointer) {
    _pre_call_gl_callback("glGetVertexAttribPointervNV", (GLADapiproc) glad_glGetVertexAttribPointervNV, 3, index, pname, pointer);
    glad_glGetVertexAttribPointervNV(index, pname, pointer);
    _post_call_gl_callback(NULL, "glGetVertexAttribPointervNV", (GLADapiproc) glad_glGetVertexAttribPointervNV, 3, index, pname, pointer);
    
}
PFNGLGETVERTEXATTRIBPOINTERVNVPROC glad_debug_glGetVertexAttribPointervNV = glad_debug_impl_glGetVertexAttribPointervNV;
PFNGLGETVERTEXATTRIBDVPROC glad_glGetVertexAttribdv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble * params) {
    _pre_call_gl_callback("glGetVertexAttribdv", (GLADapiproc) glad_glGetVertexAttribdv, 3, index, pname, params);
    glad_glGetVertexAttribdv(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribdv", (GLADapiproc) glad_glGetVertexAttribdv, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBDVPROC glad_debug_glGetVertexAttribdv = glad_debug_impl_glGetVertexAttribdv;
PFNGLGETVERTEXATTRIBDVARBPROC glad_glGetVertexAttribdvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribdvARB(GLuint index, GLenum pname, GLdouble * params) {
    _pre_call_gl_callback("glGetVertexAttribdvARB", (GLADapiproc) glad_glGetVertexAttribdvARB, 3, index, pname, params);
    glad_glGetVertexAttribdvARB(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribdvARB", (GLADapiproc) glad_glGetVertexAttribdvARB, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBDVARBPROC glad_debug_glGetVertexAttribdvARB = glad_debug_impl_glGetVertexAttribdvARB;
PFNGLGETVERTEXATTRIBDVNVPROC glad_glGetVertexAttribdvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribdvNV(GLuint index, GLenum pname, GLdouble * params) {
    _pre_call_gl_callback("glGetVertexAttribdvNV", (GLADapiproc) glad_glGetVertexAttribdvNV, 3, index, pname, params);
    glad_glGetVertexAttribdvNV(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribdvNV", (GLADapiproc) glad_glGetVertexAttribdvNV, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBDVNVPROC glad_debug_glGetVertexAttribdvNV = glad_debug_impl_glGetVertexAttribdvNV;
PFNGLGETVERTEXATTRIBFVPROC glad_glGetVertexAttribfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetVertexAttribfv", (GLADapiproc) glad_glGetVertexAttribfv, 3, index, pname, params);
    glad_glGetVertexAttribfv(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribfv", (GLADapiproc) glad_glGetVertexAttribfv, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBFVPROC glad_debug_glGetVertexAttribfv = glad_debug_impl_glGetVertexAttribfv;
PFNGLGETVERTEXATTRIBFVARBPROC glad_glGetVertexAttribfvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribfvARB(GLuint index, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetVertexAttribfvARB", (GLADapiproc) glad_glGetVertexAttribfvARB, 3, index, pname, params);
    glad_glGetVertexAttribfvARB(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribfvARB", (GLADapiproc) glad_glGetVertexAttribfvARB, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBFVARBPROC glad_debug_glGetVertexAttribfvARB = glad_debug_impl_glGetVertexAttribfvARB;
PFNGLGETVERTEXATTRIBFVNVPROC glad_glGetVertexAttribfvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribfvNV(GLuint index, GLenum pname, GLfloat * params) {
    _pre_call_gl_callback("glGetVertexAttribfvNV", (GLADapiproc) glad_glGetVertexAttribfvNV, 3, index, pname, params);
    glad_glGetVertexAttribfvNV(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribfvNV", (GLADapiproc) glad_glGetVertexAttribfvNV, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBFVNVPROC glad_debug_glGetVertexAttribfvNV = glad_debug_impl_glGetVertexAttribfvNV;
PFNGLGETVERTEXATTRIBIVPROC glad_glGetVertexAttribiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribiv(GLuint index, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetVertexAttribiv", (GLADapiproc) glad_glGetVertexAttribiv, 3, index, pname, params);
    glad_glGetVertexAttribiv(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribiv", (GLADapiproc) glad_glGetVertexAttribiv, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBIVPROC glad_debug_glGetVertexAttribiv = glad_debug_impl_glGetVertexAttribiv;
PFNGLGETVERTEXATTRIBIVARBPROC glad_glGetVertexAttribivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribivARB(GLuint index, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetVertexAttribivARB", (GLADapiproc) glad_glGetVertexAttribivARB, 3, index, pname, params);
    glad_glGetVertexAttribivARB(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribivARB", (GLADapiproc) glad_glGetVertexAttribivARB, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBIVARBPROC glad_debug_glGetVertexAttribivARB = glad_debug_impl_glGetVertexAttribivARB;
PFNGLGETVERTEXATTRIBIVNVPROC glad_glGetVertexAttribivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribivNV(GLuint index, GLenum pname, GLint * params) {
    _pre_call_gl_callback("glGetVertexAttribivNV", (GLADapiproc) glad_glGetVertexAttribivNV, 3, index, pname, params);
    glad_glGetVertexAttribivNV(index, pname, params);
    _post_call_gl_callback(NULL, "glGetVertexAttribivNV", (GLADapiproc) glad_glGetVertexAttribivNV, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBIVNVPROC glad_debug_glGetVertexAttribivNV = glad_debug_impl_glGetVertexAttribivNV;
PFNGLHINTPROC glad_glHint = NULL;
static void GLAD_API_PTR glad_debug_impl_glHint(GLenum target, GLenum mode) {
    _pre_call_gl_callback("glHint", (GLADapiproc) glad_glHint, 2, target, mode);
    glad_glHint(target, mode);
    _post_call_gl_callback(NULL, "glHint", (GLADapiproc) glad_glHint, 2, target, mode);
    
}
PFNGLHINTPROC glad_debug_glHint = glad_debug_impl_glHint;
PFNGLINDEXPOINTEREXTPROC glad_glIndexPointerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glIndexPointerEXT(GLenum type, GLsizei stride, GLsizei count, const void * pointer) {
    _pre_call_gl_callback("glIndexPointerEXT", (GLADapiproc) glad_glIndexPointerEXT, 4, type, stride, count, pointer);
    glad_glIndexPointerEXT(type, stride, count, pointer);
    _post_call_gl_callback(NULL, "glIndexPointerEXT", (GLADapiproc) glad_glIndexPointerEXT, 4, type, stride, count, pointer);
    
}
PFNGLINDEXPOINTEREXTPROC glad_debug_glIndexPointerEXT = glad_debug_impl_glIndexPointerEXT;
PFNGLISBUFFERPROC glad_glIsBuffer = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsBuffer(GLuint buffer) {
    GLboolean ret;
    _pre_call_gl_callback("glIsBuffer", (GLADapiproc) glad_glIsBuffer, 1, buffer);
    ret = glad_glIsBuffer(buffer);
    _post_call_gl_callback((void*) &ret, "glIsBuffer", (GLADapiproc) glad_glIsBuffer, 1, buffer);
    return ret;
}
PFNGLISBUFFERPROC glad_debug_glIsBuffer = glad_debug_impl_glIsBuffer;
PFNGLISBUFFERARBPROC glad_glIsBufferARB = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsBufferARB(GLuint buffer) {
    GLboolean ret;
    _pre_call_gl_callback("glIsBufferARB", (GLADapiproc) glad_glIsBufferARB, 1, buffer);
    ret = glad_glIsBufferARB(buffer);
    _post_call_gl_callback((void*) &ret, "glIsBufferARB", (GLADapiproc) glad_glIsBufferARB, 1, buffer);
    return ret;
}
PFNGLISBUFFERARBPROC glad_debug_glIsBufferARB = glad_debug_impl_glIsBufferARB;
PFNGLISENABLEDPROC glad_glIsEnabled = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsEnabled(GLenum cap) {
    GLboolean ret;
    _pre_call_gl_callback("glIsEnabled", (GLADapiproc) glad_glIsEnabled, 1, cap);
    ret = glad_glIsEnabled(cap);
    _post_call_gl_callback((void*) &ret, "glIsEnabled", (GLADapiproc) glad_glIsEnabled, 1, cap);
    return ret;
}
PFNGLISENABLEDPROC glad_debug_glIsEnabled = glad_debug_impl_glIsEnabled;
PFNGLISENABLEDINDEXEDEXTPROC glad_glIsEnabledIndexedEXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsEnabledIndexedEXT(GLenum target, GLuint index) {
    GLboolean ret;
    _pre_call_gl_callback("glIsEnabledIndexedEXT", (GLADapiproc) glad_glIsEnabledIndexedEXT, 2, target, index);
    ret = glad_glIsEnabledIndexedEXT(target, index);
    _post_call_gl_callback((void*) &ret, "glIsEnabledIndexedEXT", (GLADapiproc) glad_glIsEnabledIndexedEXT, 2, target, index);
    return ret;
}
PFNGLISENABLEDINDEXEDEXTPROC glad_debug_glIsEnabledIndexedEXT = glad_debug_impl_glIsEnabledIndexedEXT;
PFNGLISENABLEDIPROC glad_glIsEnabledi = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsEnabledi(GLenum target, GLuint index) {
    GLboolean ret;
    _pre_call_gl_callback("glIsEnabledi", (GLADapiproc) glad_glIsEnabledi, 2, target, index);
    ret = glad_glIsEnabledi(target, index);
    _post_call_gl_callback((void*) &ret, "glIsEnabledi", (GLADapiproc) glad_glIsEnabledi, 2, target, index);
    return ret;
}
PFNGLISENABLEDIPROC glad_debug_glIsEnabledi = glad_debug_impl_glIsEnabledi;
PFNGLISFRAMEBUFFERPROC glad_glIsFramebuffer = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsFramebuffer(GLuint framebuffer) {
    GLboolean ret;
    _pre_call_gl_callback("glIsFramebuffer", (GLADapiproc) glad_glIsFramebuffer, 1, framebuffer);
    ret = glad_glIsFramebuffer(framebuffer);
    _post_call_gl_callback((void*) &ret, "glIsFramebuffer", (GLADapiproc) glad_glIsFramebuffer, 1, framebuffer);
    return ret;
}
PFNGLISFRAMEBUFFERPROC glad_debug_glIsFramebuffer = glad_debug_impl_glIsFramebuffer;
PFNGLISFRAMEBUFFEREXTPROC glad_glIsFramebufferEXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsFramebufferEXT(GLuint framebuffer) {
    GLboolean ret;
    _pre_call_gl_callback("glIsFramebufferEXT", (GLADapiproc) glad_glIsFramebufferEXT, 1, framebuffer);
    ret = glad_glIsFramebufferEXT(framebuffer);
    _post_call_gl_callback((void*) &ret, "glIsFramebufferEXT", (GLADapiproc) glad_glIsFramebufferEXT, 1, framebuffer);
    return ret;
}
PFNGLISFRAMEBUFFEREXTPROC glad_debug_glIsFramebufferEXT = glad_debug_impl_glIsFramebufferEXT;
PFNGLISPROGRAMPROC glad_glIsProgram = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsProgram(GLuint program) {
    GLboolean ret;
    _pre_call_gl_callback("glIsProgram", (GLADapiproc) glad_glIsProgram, 1, program);
    ret = glad_glIsProgram(program);
    _post_call_gl_callback((void*) &ret, "glIsProgram", (GLADapiproc) glad_glIsProgram, 1, program);
    return ret;
}
PFNGLISPROGRAMPROC glad_debug_glIsProgram = glad_debug_impl_glIsProgram;
PFNGLISPROGRAMARBPROC glad_glIsProgramARB = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsProgramARB(GLuint program) {
    GLboolean ret;
    _pre_call_gl_callback("glIsProgramARB", (GLADapiproc) glad_glIsProgramARB, 1, program);
    ret = glad_glIsProgramARB(program);
    _post_call_gl_callback((void*) &ret, "glIsProgramARB", (GLADapiproc) glad_glIsProgramARB, 1, program);
    return ret;
}
PFNGLISPROGRAMARBPROC glad_debug_glIsProgramARB = glad_debug_impl_glIsProgramARB;
PFNGLISPROGRAMNVPROC glad_glIsProgramNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsProgramNV(GLuint id) {
    GLboolean ret;
    _pre_call_gl_callback("glIsProgramNV", (GLADapiproc) glad_glIsProgramNV, 1, id);
    ret = glad_glIsProgramNV(id);
    _post_call_gl_callback((void*) &ret, "glIsProgramNV", (GLADapiproc) glad_glIsProgramNV, 1, id);
    return ret;
}
PFNGLISPROGRAMNVPROC glad_debug_glIsProgramNV = glad_debug_impl_glIsProgramNV;
PFNGLISQUERYPROC glad_glIsQuery = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsQuery(GLuint id) {
    GLboolean ret;
    _pre_call_gl_callback("glIsQuery", (GLADapiproc) glad_glIsQuery, 1, id);
    ret = glad_glIsQuery(id);
    _post_call_gl_callback((void*) &ret, "glIsQuery", (GLADapiproc) glad_glIsQuery, 1, id);
    return ret;
}
PFNGLISQUERYPROC glad_debug_glIsQuery = glad_debug_impl_glIsQuery;
PFNGLISQUERYARBPROC glad_glIsQueryARB = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsQueryARB(GLuint id) {
    GLboolean ret;
    _pre_call_gl_callback("glIsQueryARB", (GLADapiproc) glad_glIsQueryARB, 1, id);
    ret = glad_glIsQueryARB(id);
    _post_call_gl_callback((void*) &ret, "glIsQueryARB", (GLADapiproc) glad_glIsQueryARB, 1, id);
    return ret;
}
PFNGLISQUERYARBPROC glad_debug_glIsQueryARB = glad_debug_impl_glIsQueryARB;
PFNGLISRENDERBUFFERPROC glad_glIsRenderbuffer = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsRenderbuffer(GLuint renderbuffer) {
    GLboolean ret;
    _pre_call_gl_callback("glIsRenderbuffer", (GLADapiproc) glad_glIsRenderbuffer, 1, renderbuffer);
    ret = glad_glIsRenderbuffer(renderbuffer);
    _post_call_gl_callback((void*) &ret, "glIsRenderbuffer", (GLADapiproc) glad_glIsRenderbuffer, 1, renderbuffer);
    return ret;
}
PFNGLISRENDERBUFFERPROC glad_debug_glIsRenderbuffer = glad_debug_impl_glIsRenderbuffer;
PFNGLISRENDERBUFFEREXTPROC glad_glIsRenderbufferEXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsRenderbufferEXT(GLuint renderbuffer) {
    GLboolean ret;
    _pre_call_gl_callback("glIsRenderbufferEXT", (GLADapiproc) glad_glIsRenderbufferEXT, 1, renderbuffer);
    ret = glad_glIsRenderbufferEXT(renderbuffer);
    _post_call_gl_callback((void*) &ret, "glIsRenderbufferEXT", (GLADapiproc) glad_glIsRenderbufferEXT, 1, renderbuffer);
    return ret;
}
PFNGLISRENDERBUFFEREXTPROC glad_debug_glIsRenderbufferEXT = glad_debug_impl_glIsRenderbufferEXT;
PFNGLISSAMPLERPROC glad_glIsSampler = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsSampler(GLuint sampler) {
    GLboolean ret;
    _pre_call_gl_callback("glIsSampler", (GLADapiproc) glad_glIsSampler, 1, sampler);
    ret = glad_glIsSampler(sampler);
    _post_call_gl_callback((void*) &ret, "glIsSampler", (GLADapiproc) glad_glIsSampler, 1, sampler);
    return ret;
}
PFNGLISSAMPLERPROC glad_debug_glIsSampler = glad_debug_impl_glIsSampler;
PFNGLISSHADERPROC glad_glIsShader = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsShader(GLuint shader) {
    GLboolean ret;
    _pre_call_gl_callback("glIsShader", (GLADapiproc) glad_glIsShader, 1, shader);
    ret = glad_glIsShader(shader);
    _post_call_gl_callback((void*) &ret, "glIsShader", (GLADapiproc) glad_glIsShader, 1, shader);
    return ret;
}
PFNGLISSHADERPROC glad_debug_glIsShader = glad_debug_impl_glIsShader;
PFNGLISSYNCPROC glad_glIsSync = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsSync(GLsync sync) {
    GLboolean ret;
    _pre_call_gl_callback("glIsSync", (GLADapiproc) glad_glIsSync, 1, sync);
    ret = glad_glIsSync(sync);
    _post_call_gl_callback((void*) &ret, "glIsSync", (GLADapiproc) glad_glIsSync, 1, sync);
    return ret;
}
PFNGLISSYNCPROC glad_debug_glIsSync = glad_debug_impl_glIsSync;
PFNGLISTEXTUREPROC glad_glIsTexture = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsTexture(GLuint texture) {
    GLboolean ret;
    _pre_call_gl_callback("glIsTexture", (GLADapiproc) glad_glIsTexture, 1, texture);
    ret = glad_glIsTexture(texture);
    _post_call_gl_callback((void*) &ret, "glIsTexture", (GLADapiproc) glad_glIsTexture, 1, texture);
    return ret;
}
PFNGLISTEXTUREPROC glad_debug_glIsTexture = glad_debug_impl_glIsTexture;
PFNGLISTEXTUREEXTPROC glad_glIsTextureEXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsTextureEXT(GLuint texture) {
    GLboolean ret;
    _pre_call_gl_callback("glIsTextureEXT", (GLADapiproc) glad_glIsTextureEXT, 1, texture);
    ret = glad_glIsTextureEXT(texture);
    _post_call_gl_callback((void*) &ret, "glIsTextureEXT", (GLADapiproc) glad_glIsTextureEXT, 1, texture);
    return ret;
}
PFNGLISTEXTUREEXTPROC glad_debug_glIsTextureEXT = glad_debug_impl_glIsTextureEXT;
PFNGLISVERTEXARRAYPROC glad_glIsVertexArray = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsVertexArray(GLuint array) {
    GLboolean ret;
    _pre_call_gl_callback("glIsVertexArray", (GLADapiproc) glad_glIsVertexArray, 1, array);
    ret = glad_glIsVertexArray(array);
    _post_call_gl_callback((void*) &ret, "glIsVertexArray", (GLADapiproc) glad_glIsVertexArray, 1, array);
    return ret;
}
PFNGLISVERTEXARRAYPROC glad_debug_glIsVertexArray = glad_debug_impl_glIsVertexArray;
PFNGLISVERTEXARRAYAPPLEPROC glad_glIsVertexArrayAPPLE = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsVertexArrayAPPLE(GLuint array) {
    GLboolean ret;
    _pre_call_gl_callback("glIsVertexArrayAPPLE", (GLADapiproc) glad_glIsVertexArrayAPPLE, 1, array);
    ret = glad_glIsVertexArrayAPPLE(array);
    _post_call_gl_callback((void*) &ret, "glIsVertexArrayAPPLE", (GLADapiproc) glad_glIsVertexArrayAPPLE, 1, array);
    return ret;
}
PFNGLISVERTEXARRAYAPPLEPROC glad_debug_glIsVertexArrayAPPLE = glad_debug_impl_glIsVertexArrayAPPLE;
PFNGLLINEWIDTHPROC glad_glLineWidth = NULL;
static void GLAD_API_PTR glad_debug_impl_glLineWidth(GLfloat width) {
    _pre_call_gl_callback("glLineWidth", (GLADapiproc) glad_glLineWidth, 1, width);
    glad_glLineWidth(width);
    _post_call_gl_callback(NULL, "glLineWidth", (GLADapiproc) glad_glLineWidth, 1, width);
    
}
PFNGLLINEWIDTHPROC glad_debug_glLineWidth = glad_debug_impl_glLineWidth;
PFNGLLINKPROGRAMPROC glad_glLinkProgram = NULL;
static void GLAD_API_PTR glad_debug_impl_glLinkProgram(GLuint program) {
    _pre_call_gl_callback("glLinkProgram", (GLADapiproc) glad_glLinkProgram, 1, program);
    glad_glLinkProgram(program);
    _post_call_gl_callback(NULL, "glLinkProgram", (GLADapiproc) glad_glLinkProgram, 1, program);
    
}
PFNGLLINKPROGRAMPROC glad_debug_glLinkProgram = glad_debug_impl_glLinkProgram;
PFNGLLINKPROGRAMARBPROC glad_glLinkProgramARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glLinkProgramARB(GLhandleARB programObj) {
    _pre_call_gl_callback("glLinkProgramARB", (GLADapiproc) glad_glLinkProgramARB, 1, programObj);
    glad_glLinkProgramARB(programObj);
    _post_call_gl_callback(NULL, "glLinkProgramARB", (GLADapiproc) glad_glLinkProgramARB, 1, programObj);
    
}
PFNGLLINKPROGRAMARBPROC glad_debug_glLinkProgramARB = glad_debug_impl_glLinkProgramARB;
PFNGLLOADPROGRAMNVPROC glad_glLoadProgramNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glLoadProgramNV(GLenum target, GLuint id, GLsizei len, const GLubyte * program) {
    _pre_call_gl_callback("glLoadProgramNV", (GLADapiproc) glad_glLoadProgramNV, 4, target, id, len, program);
    glad_glLoadProgramNV(target, id, len, program);
    _post_call_gl_callback(NULL, "glLoadProgramNV", (GLADapiproc) glad_glLoadProgramNV, 4, target, id, len, program);
    
}
PFNGLLOADPROGRAMNVPROC glad_debug_glLoadProgramNV = glad_debug_impl_glLoadProgramNV;
PFNGLLOGICOPPROC glad_glLogicOp = NULL;
static void GLAD_API_PTR glad_debug_impl_glLogicOp(GLenum opcode) {
    _pre_call_gl_callback("glLogicOp", (GLADapiproc) glad_glLogicOp, 1, opcode);
    glad_glLogicOp(opcode);
    _post_call_gl_callback(NULL, "glLogicOp", (GLADapiproc) glad_glLogicOp, 1, opcode);
    
}
PFNGLLOGICOPPROC glad_debug_glLogicOp = glad_debug_impl_glLogicOp;
PFNGLMAPBUFFERPROC glad_glMapBuffer = NULL;
static void * GLAD_API_PTR glad_debug_impl_glMapBuffer(GLenum target, GLenum access) {
    void * ret;
    _pre_call_gl_callback("glMapBuffer", (GLADapiproc) glad_glMapBuffer, 2, target, access);
    ret = glad_glMapBuffer(target, access);
    _post_call_gl_callback((void*) &ret, "glMapBuffer", (GLADapiproc) glad_glMapBuffer, 2, target, access);
    return ret;
}
PFNGLMAPBUFFERPROC glad_debug_glMapBuffer = glad_debug_impl_glMapBuffer;
PFNGLMAPBUFFERARBPROC glad_glMapBufferARB = NULL;
static void * GLAD_API_PTR glad_debug_impl_glMapBufferARB(GLenum target, GLenum access) {
    void * ret;
    _pre_call_gl_callback("glMapBufferARB", (GLADapiproc) glad_glMapBufferARB, 2, target, access);
    ret = glad_glMapBufferARB(target, access);
    _post_call_gl_callback((void*) &ret, "glMapBufferARB", (GLADapiproc) glad_glMapBufferARB, 2, target, access);
    return ret;
}
PFNGLMAPBUFFERARBPROC glad_debug_glMapBufferARB = glad_debug_impl_glMapBufferARB;
PFNGLMAPBUFFERRANGEPROC glad_glMapBufferRange = NULL;
static void * GLAD_API_PTR glad_debug_impl_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    void * ret;
    _pre_call_gl_callback("glMapBufferRange", (GLADapiproc) glad_glMapBufferRange, 4, target, offset, length, access);
    ret = glad_glMapBufferRange(target, offset, length, access);
    _post_call_gl_callback((void*) &ret, "glMapBufferRange", (GLADapiproc) glad_glMapBufferRange, 4, target, offset, length, access);
    return ret;
}
PFNGLMAPBUFFERRANGEPROC glad_debug_glMapBufferRange = glad_debug_impl_glMapBufferRange;
PFNGLMAPNAMEDBUFFEREXTPROC glad_glMapNamedBufferEXT = NULL;
static void * GLAD_API_PTR glad_debug_impl_glMapNamedBufferEXT(GLuint buffer, GLenum access) {
    void * ret;
    _pre_call_gl_callback("glMapNamedBufferEXT", (GLADapiproc) glad_glMapNamedBufferEXT, 2, buffer, access);
    ret = glad_glMapNamedBufferEXT(buffer, access);
    _post_call_gl_callback((void*) &ret, "glMapNamedBufferEXT", (GLADapiproc) glad_glMapNamedBufferEXT, 2, buffer, access);
    return ret;
}
PFNGLMAPNAMEDBUFFEREXTPROC glad_debug_glMapNamedBufferEXT = glad_debug_impl_glMapNamedBufferEXT;
PFNGLMAPNAMEDBUFFERRANGEEXTPROC glad_glMapNamedBufferRangeEXT = NULL;
static void * GLAD_API_PTR glad_debug_impl_glMapNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    void * ret;
    _pre_call_gl_callback("glMapNamedBufferRangeEXT", (GLADapiproc) glad_glMapNamedBufferRangeEXT, 4, buffer, offset, length, access);
    ret = glad_glMapNamedBufferRangeEXT(buffer, offset, length, access);
    _post_call_gl_callback((void*) &ret, "glMapNamedBufferRangeEXT", (GLADapiproc) glad_glMapNamedBufferRangeEXT, 4, buffer, offset, length, access);
    return ret;
}
PFNGLMAPNAMEDBUFFERRANGEEXTPROC glad_debug_glMapNamedBufferRangeEXT = glad_debug_impl_glMapNamedBufferRangeEXT;
PFNGLMATRIXFRUSTUMEXTPROC glad_glMatrixFrustumEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixFrustumEXT(GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    _pre_call_gl_callback("glMatrixFrustumEXT", (GLADapiproc) glad_glMatrixFrustumEXT, 7, mode, left, right, bottom, top, zNear, zFar);
    glad_glMatrixFrustumEXT(mode, left, right, bottom, top, zNear, zFar);
    _post_call_gl_callback(NULL, "glMatrixFrustumEXT", (GLADapiproc) glad_glMatrixFrustumEXT, 7, mode, left, right, bottom, top, zNear, zFar);
    
}
PFNGLMATRIXFRUSTUMEXTPROC glad_debug_glMatrixFrustumEXT = glad_debug_impl_glMatrixFrustumEXT;
PFNGLMATRIXLOADIDENTITYEXTPROC glad_glMatrixLoadIdentityEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoadIdentityEXT(GLenum mode) {
    _pre_call_gl_callback("glMatrixLoadIdentityEXT", (GLADapiproc) glad_glMatrixLoadIdentityEXT, 1, mode);
    glad_glMatrixLoadIdentityEXT(mode);
    _post_call_gl_callback(NULL, "glMatrixLoadIdentityEXT", (GLADapiproc) glad_glMatrixLoadIdentityEXT, 1, mode);
    
}
PFNGLMATRIXLOADIDENTITYEXTPROC glad_debug_glMatrixLoadIdentityEXT = glad_debug_impl_glMatrixLoadIdentityEXT;
PFNGLMATRIXLOADTRANSPOSEDEXTPROC glad_glMatrixLoadTransposedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoadTransposedEXT(GLenum mode, const GLdouble * m) {
    _pre_call_gl_callback("glMatrixLoadTransposedEXT", (GLADapiproc) glad_glMatrixLoadTransposedEXT, 2, mode, m);
    glad_glMatrixLoadTransposedEXT(mode, m);
    _post_call_gl_callback(NULL, "glMatrixLoadTransposedEXT", (GLADapiproc) glad_glMatrixLoadTransposedEXT, 2, mode, m);
    
}
PFNGLMATRIXLOADTRANSPOSEDEXTPROC glad_debug_glMatrixLoadTransposedEXT = glad_debug_impl_glMatrixLoadTransposedEXT;
PFNGLMATRIXLOADTRANSPOSEFEXTPROC glad_glMatrixLoadTransposefEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoadTransposefEXT(GLenum mode, const GLfloat * m) {
    _pre_call_gl_callback("glMatrixLoadTransposefEXT", (GLADapiproc) glad_glMatrixLoadTransposefEXT, 2, mode, m);
    glad_glMatrixLoadTransposefEXT(mode, m);
    _post_call_gl_callback(NULL, "glMatrixLoadTransposefEXT", (GLADapiproc) glad_glMatrixLoadTransposefEXT, 2, mode, m);
    
}
PFNGLMATRIXLOADTRANSPOSEFEXTPROC glad_debug_glMatrixLoadTransposefEXT = glad_debug_impl_glMatrixLoadTransposefEXT;
PFNGLMATRIXLOADDEXTPROC glad_glMatrixLoaddEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoaddEXT(GLenum mode, const GLdouble * m) {
    _pre_call_gl_callback("glMatrixLoaddEXT", (GLADapiproc) glad_glMatrixLoaddEXT, 2, mode, m);
    glad_glMatrixLoaddEXT(mode, m);
    _post_call_gl_callback(NULL, "glMatrixLoaddEXT", (GLADapiproc) glad_glMatrixLoaddEXT, 2, mode, m);
    
}
PFNGLMATRIXLOADDEXTPROC glad_debug_glMatrixLoaddEXT = glad_debug_impl_glMatrixLoaddEXT;
PFNGLMATRIXLOADFEXTPROC glad_glMatrixLoadfEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoadfEXT(GLenum mode, const GLfloat * m) {
    _pre_call_gl_callback("glMatrixLoadfEXT", (GLADapiproc) glad_glMatrixLoadfEXT, 2, mode, m);
    glad_glMatrixLoadfEXT(mode, m);
    _post_call_gl_callback(NULL, "glMatrixLoadfEXT", (GLADapiproc) glad_glMatrixLoadfEXT, 2, mode, m);
    
}
PFNGLMATRIXLOADFEXTPROC glad_debug_glMatrixLoadfEXT = glad_debug_impl_glMatrixLoadfEXT;
PFNGLMATRIXMULTTRANSPOSEDEXTPROC glad_glMatrixMultTransposedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMultTransposedEXT(GLenum mode, const GLdouble * m) {
    _pre_call_gl_callback("glMatrixMultTransposedEXT", (GLADapiproc) glad_glMatrixMultTransposedEXT, 2, mode, m);
    glad_glMatrixMultTransposedEXT(mode, m);
    _post_call_gl_callback(NULL, "glMatrixMultTransposedEXT", (GLADapiproc) glad_glMatrixMultTransposedEXT, 2, mode, m);
    
}
PFNGLMATRIXMULTTRANSPOSEDEXTPROC glad_debug_glMatrixMultTransposedEXT = glad_debug_impl_glMatrixMultTransposedEXT;
PFNGLMATRIXMULTTRANSPOSEFEXTPROC glad_glMatrixMultTransposefEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMultTransposefEXT(GLenum mode, const GLfloat * m) {
    _pre_call_gl_callback("glMatrixMultTransposefEXT", (GLADapiproc) glad_glMatrixMultTransposefEXT, 2, mode, m);
    glad_glMatrixMultTransposefEXT(mode, m);
    _post_call_gl_callback(NULL, "glMatrixMultTransposefEXT", (GLADapiproc) glad_glMatrixMultTransposefEXT, 2, mode, m);
    
}
PFNGLMATRIXMULTTRANSPOSEFEXTPROC glad_debug_glMatrixMultTransposefEXT = glad_debug_impl_glMatrixMultTransposefEXT;
PFNGLMATRIXMULTDEXTPROC glad_glMatrixMultdEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMultdEXT(GLenum mode, const GLdouble * m) {
    _pre_call_gl_callback("glMatrixMultdEXT", (GLADapiproc) glad_glMatrixMultdEXT, 2, mode, m);
    glad_glMatrixMultdEXT(mode, m);
    _post_call_gl_callback(NULL, "glMatrixMultdEXT", (GLADapiproc) glad_glMatrixMultdEXT, 2, mode, m);
    
}
PFNGLMATRIXMULTDEXTPROC glad_debug_glMatrixMultdEXT = glad_debug_impl_glMatrixMultdEXT;
PFNGLMATRIXMULTFEXTPROC glad_glMatrixMultfEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMultfEXT(GLenum mode, const GLfloat * m) {
    _pre_call_gl_callback("glMatrixMultfEXT", (GLADapiproc) glad_glMatrixMultfEXT, 2, mode, m);
    glad_glMatrixMultfEXT(mode, m);
    _post_call_gl_callback(NULL, "glMatrixMultfEXT", (GLADapiproc) glad_glMatrixMultfEXT, 2, mode, m);
    
}
PFNGLMATRIXMULTFEXTPROC glad_debug_glMatrixMultfEXT = glad_debug_impl_glMatrixMultfEXT;
PFNGLMATRIXORTHOEXTPROC glad_glMatrixOrthoEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixOrthoEXT(GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    _pre_call_gl_callback("glMatrixOrthoEXT", (GLADapiproc) glad_glMatrixOrthoEXT, 7, mode, left, right, bottom, top, zNear, zFar);
    glad_glMatrixOrthoEXT(mode, left, right, bottom, top, zNear, zFar);
    _post_call_gl_callback(NULL, "glMatrixOrthoEXT", (GLADapiproc) glad_glMatrixOrthoEXT, 7, mode, left, right, bottom, top, zNear, zFar);
    
}
PFNGLMATRIXORTHOEXTPROC glad_debug_glMatrixOrthoEXT = glad_debug_impl_glMatrixOrthoEXT;
PFNGLMATRIXPOPEXTPROC glad_glMatrixPopEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixPopEXT(GLenum mode) {
    _pre_call_gl_callback("glMatrixPopEXT", (GLADapiproc) glad_glMatrixPopEXT, 1, mode);
    glad_glMatrixPopEXT(mode);
    _post_call_gl_callback(NULL, "glMatrixPopEXT", (GLADapiproc) glad_glMatrixPopEXT, 1, mode);
    
}
PFNGLMATRIXPOPEXTPROC glad_debug_glMatrixPopEXT = glad_debug_impl_glMatrixPopEXT;
PFNGLMATRIXPUSHEXTPROC glad_glMatrixPushEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixPushEXT(GLenum mode) {
    _pre_call_gl_callback("glMatrixPushEXT", (GLADapiproc) glad_glMatrixPushEXT, 1, mode);
    glad_glMatrixPushEXT(mode);
    _post_call_gl_callback(NULL, "glMatrixPushEXT", (GLADapiproc) glad_glMatrixPushEXT, 1, mode);
    
}
PFNGLMATRIXPUSHEXTPROC glad_debug_glMatrixPushEXT = glad_debug_impl_glMatrixPushEXT;
PFNGLMATRIXROTATEDEXTPROC glad_glMatrixRotatedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixRotatedEXT(GLenum mode, GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
    _pre_call_gl_callback("glMatrixRotatedEXT", (GLADapiproc) glad_glMatrixRotatedEXT, 5, mode, angle, x, y, z);
    glad_glMatrixRotatedEXT(mode, angle, x, y, z);
    _post_call_gl_callback(NULL, "glMatrixRotatedEXT", (GLADapiproc) glad_glMatrixRotatedEXT, 5, mode, angle, x, y, z);
    
}
PFNGLMATRIXROTATEDEXTPROC glad_debug_glMatrixRotatedEXT = glad_debug_impl_glMatrixRotatedEXT;
PFNGLMATRIXROTATEFEXTPROC glad_glMatrixRotatefEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixRotatefEXT(GLenum mode, GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gl_callback("glMatrixRotatefEXT", (GLADapiproc) glad_glMatrixRotatefEXT, 5, mode, angle, x, y, z);
    glad_glMatrixRotatefEXT(mode, angle, x, y, z);
    _post_call_gl_callback(NULL, "glMatrixRotatefEXT", (GLADapiproc) glad_glMatrixRotatefEXT, 5, mode, angle, x, y, z);
    
}
PFNGLMATRIXROTATEFEXTPROC glad_debug_glMatrixRotatefEXT = glad_debug_impl_glMatrixRotatefEXT;
PFNGLMATRIXSCALEDEXTPROC glad_glMatrixScaledEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixScaledEXT(GLenum mode, GLdouble x, GLdouble y, GLdouble z) {
    _pre_call_gl_callback("glMatrixScaledEXT", (GLADapiproc) glad_glMatrixScaledEXT, 4, mode, x, y, z);
    glad_glMatrixScaledEXT(mode, x, y, z);
    _post_call_gl_callback(NULL, "glMatrixScaledEXT", (GLADapiproc) glad_glMatrixScaledEXT, 4, mode, x, y, z);
    
}
PFNGLMATRIXSCALEDEXTPROC glad_debug_glMatrixScaledEXT = glad_debug_impl_glMatrixScaledEXT;
PFNGLMATRIXSCALEFEXTPROC glad_glMatrixScalefEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixScalefEXT(GLenum mode, GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gl_callback("glMatrixScalefEXT", (GLADapiproc) glad_glMatrixScalefEXT, 4, mode, x, y, z);
    glad_glMatrixScalefEXT(mode, x, y, z);
    _post_call_gl_callback(NULL, "glMatrixScalefEXT", (GLADapiproc) glad_glMatrixScalefEXT, 4, mode, x, y, z);
    
}
PFNGLMATRIXSCALEFEXTPROC glad_debug_glMatrixScalefEXT = glad_debug_impl_glMatrixScalefEXT;
PFNGLMATRIXTRANSLATEDEXTPROC glad_glMatrixTranslatedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixTranslatedEXT(GLenum mode, GLdouble x, GLdouble y, GLdouble z) {
    _pre_call_gl_callback("glMatrixTranslatedEXT", (GLADapiproc) glad_glMatrixTranslatedEXT, 4, mode, x, y, z);
    glad_glMatrixTranslatedEXT(mode, x, y, z);
    _post_call_gl_callback(NULL, "glMatrixTranslatedEXT", (GLADapiproc) glad_glMatrixTranslatedEXT, 4, mode, x, y, z);
    
}
PFNGLMATRIXTRANSLATEDEXTPROC glad_debug_glMatrixTranslatedEXT = glad_debug_impl_glMatrixTranslatedEXT;
PFNGLMATRIXTRANSLATEFEXTPROC glad_glMatrixTranslatefEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixTranslatefEXT(GLenum mode, GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gl_callback("glMatrixTranslatefEXT", (GLADapiproc) glad_glMatrixTranslatefEXT, 4, mode, x, y, z);
    glad_glMatrixTranslatefEXT(mode, x, y, z);
    _post_call_gl_callback(NULL, "glMatrixTranslatefEXT", (GLADapiproc) glad_glMatrixTranslatefEXT, 4, mode, x, y, z);
    
}
PFNGLMATRIXTRANSLATEFEXTPROC glad_debug_glMatrixTranslatefEXT = glad_debug_impl_glMatrixTranslatefEXT;
PFNGLMULTIDRAWARRAYSPROC glad_glMultiDrawArrays = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawArrays(GLenum mode, const GLint * first, const GLsizei * count, GLsizei drawcount) {
    _pre_call_gl_callback("glMultiDrawArrays", (GLADapiproc) glad_glMultiDrawArrays, 4, mode, first, count, drawcount);
    glad_glMultiDrawArrays(mode, first, count, drawcount);
    _post_call_gl_callback(NULL, "glMultiDrawArrays", (GLADapiproc) glad_glMultiDrawArrays, 4, mode, first, count, drawcount);
    
}
PFNGLMULTIDRAWARRAYSPROC glad_debug_glMultiDrawArrays = glad_debug_impl_glMultiDrawArrays;
PFNGLMULTIDRAWARRAYSEXTPROC glad_glMultiDrawArraysEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawArraysEXT(GLenum mode, const GLint * first, const GLsizei * count, GLsizei primcount) {
    _pre_call_gl_callback("glMultiDrawArraysEXT", (GLADapiproc) glad_glMultiDrawArraysEXT, 4, mode, first, count, primcount);
    glad_glMultiDrawArraysEXT(mode, first, count, primcount);
    _post_call_gl_callback(NULL, "glMultiDrawArraysEXT", (GLADapiproc) glad_glMultiDrawArraysEXT, 4, mode, first, count, primcount);
    
}
PFNGLMULTIDRAWARRAYSEXTPROC glad_debug_glMultiDrawArraysEXT = glad_debug_impl_glMultiDrawArraysEXT;
PFNGLMULTIDRAWELEMENTSPROC glad_glMultiDrawElements = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawElements(GLenum mode, const GLsizei * count, GLenum type, const void *const* indices, GLsizei drawcount) {
    _pre_call_gl_callback("glMultiDrawElements", (GLADapiproc) glad_glMultiDrawElements, 5, mode, count, type, indices, drawcount);
    glad_glMultiDrawElements(mode, count, type, indices, drawcount);
    _post_call_gl_callback(NULL, "glMultiDrawElements", (GLADapiproc) glad_glMultiDrawElements, 5, mode, count, type, indices, drawcount);
    
}
PFNGLMULTIDRAWELEMENTSPROC glad_debug_glMultiDrawElements = glad_debug_impl_glMultiDrawElements;
PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC glad_glMultiDrawElementsBaseVertex = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei * count, GLenum type, const void *const* indices, GLsizei drawcount, const GLint * basevertex) {
    _pre_call_gl_callback("glMultiDrawElementsBaseVertex", (GLADapiproc) glad_glMultiDrawElementsBaseVertex, 6, mode, count, type, indices, drawcount, basevertex);
    glad_glMultiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex);
    _post_call_gl_callback(NULL, "glMultiDrawElementsBaseVertex", (GLADapiproc) glad_glMultiDrawElementsBaseVertex, 6, mode, count, type, indices, drawcount, basevertex);
    
}
PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC glad_debug_glMultiDrawElementsBaseVertex = glad_debug_impl_glMultiDrawElementsBaseVertex;
PFNGLMULTIDRAWELEMENTSEXTPROC glad_glMultiDrawElementsEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawElementsEXT(GLenum mode, const GLsizei * count, GLenum type, const void *const* indices, GLsizei primcount) {
    _pre_call_gl_callback("glMultiDrawElementsEXT", (GLADapiproc) glad_glMultiDrawElementsEXT, 5, mode, count, type, indices, primcount);
    glad_glMultiDrawElementsEXT(mode, count, type, indices, primcount);
    _post_call_gl_callback(NULL, "glMultiDrawElementsEXT", (GLADapiproc) glad_glMultiDrawElementsEXT, 5, mode, count, type, indices, primcount);
    
}
PFNGLMULTIDRAWELEMENTSEXTPROC glad_debug_glMultiDrawElementsEXT = glad_debug_impl_glMultiDrawElementsEXT;
PFNGLMULTITEXBUFFEREXTPROC glad_glMultiTexBufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexBufferEXT(GLenum texunit, GLenum target, GLenum internalformat, GLuint buffer) {
    _pre_call_gl_callback("glMultiTexBufferEXT", (GLADapiproc) glad_glMultiTexBufferEXT, 4, texunit, target, internalformat, buffer);
    glad_glMultiTexBufferEXT(texunit, target, internalformat, buffer);
    _post_call_gl_callback(NULL, "glMultiTexBufferEXT", (GLADapiproc) glad_glMultiTexBufferEXT, 4, texunit, target, internalformat, buffer);
    
}
PFNGLMULTITEXBUFFEREXTPROC glad_debug_glMultiTexBufferEXT = glad_debug_impl_glMultiTexBufferEXT;
PFNGLMULTITEXCOORD1DARBPROC glad_glMultiTexCoord1dARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord1dARB(GLenum target, GLdouble s) {
    _pre_call_gl_callback("glMultiTexCoord1dARB", (GLADapiproc) glad_glMultiTexCoord1dARB, 2, target, s);
    glad_glMultiTexCoord1dARB(target, s);
    _post_call_gl_callback(NULL, "glMultiTexCoord1dARB", (GLADapiproc) glad_glMultiTexCoord1dARB, 2, target, s);
    
}
PFNGLMULTITEXCOORD1DARBPROC glad_debug_glMultiTexCoord1dARB = glad_debug_impl_glMultiTexCoord1dARB;
PFNGLMULTITEXCOORD1DVARBPROC glad_glMultiTexCoord1dvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord1dvARB(GLenum target, const GLdouble * v) {
    _pre_call_gl_callback("glMultiTexCoord1dvARB", (GLADapiproc) glad_glMultiTexCoord1dvARB, 2, target, v);
    glad_glMultiTexCoord1dvARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord1dvARB", (GLADapiproc) glad_glMultiTexCoord1dvARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD1DVARBPROC glad_debug_glMultiTexCoord1dvARB = glad_debug_impl_glMultiTexCoord1dvARB;
PFNGLMULTITEXCOORD1FARBPROC glad_glMultiTexCoord1fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord1fARB(GLenum target, GLfloat s) {
    _pre_call_gl_callback("glMultiTexCoord1fARB", (GLADapiproc) glad_glMultiTexCoord1fARB, 2, target, s);
    glad_glMultiTexCoord1fARB(target, s);
    _post_call_gl_callback(NULL, "glMultiTexCoord1fARB", (GLADapiproc) glad_glMultiTexCoord1fARB, 2, target, s);
    
}
PFNGLMULTITEXCOORD1FARBPROC glad_debug_glMultiTexCoord1fARB = glad_debug_impl_glMultiTexCoord1fARB;
PFNGLMULTITEXCOORD1FVARBPROC glad_glMultiTexCoord1fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord1fvARB(GLenum target, const GLfloat * v) {
    _pre_call_gl_callback("glMultiTexCoord1fvARB", (GLADapiproc) glad_glMultiTexCoord1fvARB, 2, target, v);
    glad_glMultiTexCoord1fvARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord1fvARB", (GLADapiproc) glad_glMultiTexCoord1fvARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD1FVARBPROC glad_debug_glMultiTexCoord1fvARB = glad_debug_impl_glMultiTexCoord1fvARB;
PFNGLMULTITEXCOORD1IARBPROC glad_glMultiTexCoord1iARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord1iARB(GLenum target, GLint s) {
    _pre_call_gl_callback("glMultiTexCoord1iARB", (GLADapiproc) glad_glMultiTexCoord1iARB, 2, target, s);
    glad_glMultiTexCoord1iARB(target, s);
    _post_call_gl_callback(NULL, "glMultiTexCoord1iARB", (GLADapiproc) glad_glMultiTexCoord1iARB, 2, target, s);
    
}
PFNGLMULTITEXCOORD1IARBPROC glad_debug_glMultiTexCoord1iARB = glad_debug_impl_glMultiTexCoord1iARB;
PFNGLMULTITEXCOORD1IVARBPROC glad_glMultiTexCoord1ivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord1ivARB(GLenum target, const GLint * v) {
    _pre_call_gl_callback("glMultiTexCoord1ivARB", (GLADapiproc) glad_glMultiTexCoord1ivARB, 2, target, v);
    glad_glMultiTexCoord1ivARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord1ivARB", (GLADapiproc) glad_glMultiTexCoord1ivARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD1IVARBPROC glad_debug_glMultiTexCoord1ivARB = glad_debug_impl_glMultiTexCoord1ivARB;
PFNGLMULTITEXCOORD1SARBPROC glad_glMultiTexCoord1sARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord1sARB(GLenum target, GLshort s) {
    _pre_call_gl_callback("glMultiTexCoord1sARB", (GLADapiproc) glad_glMultiTexCoord1sARB, 2, target, s);
    glad_glMultiTexCoord1sARB(target, s);
    _post_call_gl_callback(NULL, "glMultiTexCoord1sARB", (GLADapiproc) glad_glMultiTexCoord1sARB, 2, target, s);
    
}
PFNGLMULTITEXCOORD1SARBPROC glad_debug_glMultiTexCoord1sARB = glad_debug_impl_glMultiTexCoord1sARB;
PFNGLMULTITEXCOORD1SVARBPROC glad_glMultiTexCoord1svARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord1svARB(GLenum target, const GLshort * v) {
    _pre_call_gl_callback("glMultiTexCoord1svARB", (GLADapiproc) glad_glMultiTexCoord1svARB, 2, target, v);
    glad_glMultiTexCoord1svARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord1svARB", (GLADapiproc) glad_glMultiTexCoord1svARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD1SVARBPROC glad_debug_glMultiTexCoord1svARB = glad_debug_impl_glMultiTexCoord1svARB;
PFNGLMULTITEXCOORD2DARBPROC glad_glMultiTexCoord2dARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord2dARB(GLenum target, GLdouble s, GLdouble t) {
    _pre_call_gl_callback("glMultiTexCoord2dARB", (GLADapiproc) glad_glMultiTexCoord2dARB, 3, target, s, t);
    glad_glMultiTexCoord2dARB(target, s, t);
    _post_call_gl_callback(NULL, "glMultiTexCoord2dARB", (GLADapiproc) glad_glMultiTexCoord2dARB, 3, target, s, t);
    
}
PFNGLMULTITEXCOORD2DARBPROC glad_debug_glMultiTexCoord2dARB = glad_debug_impl_glMultiTexCoord2dARB;
PFNGLMULTITEXCOORD2DVARBPROC glad_glMultiTexCoord2dvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord2dvARB(GLenum target, const GLdouble * v) {
    _pre_call_gl_callback("glMultiTexCoord2dvARB", (GLADapiproc) glad_glMultiTexCoord2dvARB, 2, target, v);
    glad_glMultiTexCoord2dvARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord2dvARB", (GLADapiproc) glad_glMultiTexCoord2dvARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD2DVARBPROC glad_debug_glMultiTexCoord2dvARB = glad_debug_impl_glMultiTexCoord2dvARB;
PFNGLMULTITEXCOORD2FARBPROC glad_glMultiTexCoord2fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t) {
    _pre_call_gl_callback("glMultiTexCoord2fARB", (GLADapiproc) glad_glMultiTexCoord2fARB, 3, target, s, t);
    glad_glMultiTexCoord2fARB(target, s, t);
    _post_call_gl_callback(NULL, "glMultiTexCoord2fARB", (GLADapiproc) glad_glMultiTexCoord2fARB, 3, target, s, t);
    
}
PFNGLMULTITEXCOORD2FARBPROC glad_debug_glMultiTexCoord2fARB = glad_debug_impl_glMultiTexCoord2fARB;
PFNGLMULTITEXCOORD2FVARBPROC glad_glMultiTexCoord2fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord2fvARB(GLenum target, const GLfloat * v) {
    _pre_call_gl_callback("glMultiTexCoord2fvARB", (GLADapiproc) glad_glMultiTexCoord2fvARB, 2, target, v);
    glad_glMultiTexCoord2fvARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord2fvARB", (GLADapiproc) glad_glMultiTexCoord2fvARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD2FVARBPROC glad_debug_glMultiTexCoord2fvARB = glad_debug_impl_glMultiTexCoord2fvARB;
PFNGLMULTITEXCOORD2IARBPROC glad_glMultiTexCoord2iARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord2iARB(GLenum target, GLint s, GLint t) {
    _pre_call_gl_callback("glMultiTexCoord2iARB", (GLADapiproc) glad_glMultiTexCoord2iARB, 3, target, s, t);
    glad_glMultiTexCoord2iARB(target, s, t);
    _post_call_gl_callback(NULL, "glMultiTexCoord2iARB", (GLADapiproc) glad_glMultiTexCoord2iARB, 3, target, s, t);
    
}
PFNGLMULTITEXCOORD2IARBPROC glad_debug_glMultiTexCoord2iARB = glad_debug_impl_glMultiTexCoord2iARB;
PFNGLMULTITEXCOORD2IVARBPROC glad_glMultiTexCoord2ivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord2ivARB(GLenum target, const GLint * v) {
    _pre_call_gl_callback("glMultiTexCoord2ivARB", (GLADapiproc) glad_glMultiTexCoord2ivARB, 2, target, v);
    glad_glMultiTexCoord2ivARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord2ivARB", (GLADapiproc) glad_glMultiTexCoord2ivARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD2IVARBPROC glad_debug_glMultiTexCoord2ivARB = glad_debug_impl_glMultiTexCoord2ivARB;
PFNGLMULTITEXCOORD2SARBPROC glad_glMultiTexCoord2sARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord2sARB(GLenum target, GLshort s, GLshort t) {
    _pre_call_gl_callback("glMultiTexCoord2sARB", (GLADapiproc) glad_glMultiTexCoord2sARB, 3, target, s, t);
    glad_glMultiTexCoord2sARB(target, s, t);
    _post_call_gl_callback(NULL, "glMultiTexCoord2sARB", (GLADapiproc) glad_glMultiTexCoord2sARB, 3, target, s, t);
    
}
PFNGLMULTITEXCOORD2SARBPROC glad_debug_glMultiTexCoord2sARB = glad_debug_impl_glMultiTexCoord2sARB;
PFNGLMULTITEXCOORD2SVARBPROC glad_glMultiTexCoord2svARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord2svARB(GLenum target, const GLshort * v) {
    _pre_call_gl_callback("glMultiTexCoord2svARB", (GLADapiproc) glad_glMultiTexCoord2svARB, 2, target, v);
    glad_glMultiTexCoord2svARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord2svARB", (GLADapiproc) glad_glMultiTexCoord2svARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD2SVARBPROC glad_debug_glMultiTexCoord2svARB = glad_debug_impl_glMultiTexCoord2svARB;
PFNGLMULTITEXCOORD3DARBPROC glad_glMultiTexCoord3dARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord3dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r) {
    _pre_call_gl_callback("glMultiTexCoord3dARB", (GLADapiproc) glad_glMultiTexCoord3dARB, 4, target, s, t, r);
    glad_glMultiTexCoord3dARB(target, s, t, r);
    _post_call_gl_callback(NULL, "glMultiTexCoord3dARB", (GLADapiproc) glad_glMultiTexCoord3dARB, 4, target, s, t, r);
    
}
PFNGLMULTITEXCOORD3DARBPROC glad_debug_glMultiTexCoord3dARB = glad_debug_impl_glMultiTexCoord3dARB;
PFNGLMULTITEXCOORD3DVARBPROC glad_glMultiTexCoord3dvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord3dvARB(GLenum target, const GLdouble * v) {
    _pre_call_gl_callback("glMultiTexCoord3dvARB", (GLADapiproc) glad_glMultiTexCoord3dvARB, 2, target, v);
    glad_glMultiTexCoord3dvARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord3dvARB", (GLADapiproc) glad_glMultiTexCoord3dvARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD3DVARBPROC glad_debug_glMultiTexCoord3dvARB = glad_debug_impl_glMultiTexCoord3dvARB;
PFNGLMULTITEXCOORD3FARBPROC glad_glMultiTexCoord3fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord3fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r) {
    _pre_call_gl_callback("glMultiTexCoord3fARB", (GLADapiproc) glad_glMultiTexCoord3fARB, 4, target, s, t, r);
    glad_glMultiTexCoord3fARB(target, s, t, r);
    _post_call_gl_callback(NULL, "glMultiTexCoord3fARB", (GLADapiproc) glad_glMultiTexCoord3fARB, 4, target, s, t, r);
    
}
PFNGLMULTITEXCOORD3FARBPROC glad_debug_glMultiTexCoord3fARB = glad_debug_impl_glMultiTexCoord3fARB;
PFNGLMULTITEXCOORD3FVARBPROC glad_glMultiTexCoord3fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord3fvARB(GLenum target, const GLfloat * v) {
    _pre_call_gl_callback("glMultiTexCoord3fvARB", (GLADapiproc) glad_glMultiTexCoord3fvARB, 2, target, v);
    glad_glMultiTexCoord3fvARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord3fvARB", (GLADapiproc) glad_glMultiTexCoord3fvARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD3FVARBPROC glad_debug_glMultiTexCoord3fvARB = glad_debug_impl_glMultiTexCoord3fvARB;
PFNGLMULTITEXCOORD3IARBPROC glad_glMultiTexCoord3iARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord3iARB(GLenum target, GLint s, GLint t, GLint r) {
    _pre_call_gl_callback("glMultiTexCoord3iARB", (GLADapiproc) glad_glMultiTexCoord3iARB, 4, target, s, t, r);
    glad_glMultiTexCoord3iARB(target, s, t, r);
    _post_call_gl_callback(NULL, "glMultiTexCoord3iARB", (GLADapiproc) glad_glMultiTexCoord3iARB, 4, target, s, t, r);
    
}
PFNGLMULTITEXCOORD3IARBPROC glad_debug_glMultiTexCoord3iARB = glad_debug_impl_glMultiTexCoord3iARB;
PFNGLMULTITEXCOORD3IVARBPROC glad_glMultiTexCoord3ivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord3ivARB(GLenum target, const GLint * v) {
    _pre_call_gl_callback("glMultiTexCoord3ivARB", (GLADapiproc) glad_glMultiTexCoord3ivARB, 2, target, v);
    glad_glMultiTexCoord3ivARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord3ivARB", (GLADapiproc) glad_glMultiTexCoord3ivARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD3IVARBPROC glad_debug_glMultiTexCoord3ivARB = glad_debug_impl_glMultiTexCoord3ivARB;
PFNGLMULTITEXCOORD3SARBPROC glad_glMultiTexCoord3sARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord3sARB(GLenum target, GLshort s, GLshort t, GLshort r) {
    _pre_call_gl_callback("glMultiTexCoord3sARB", (GLADapiproc) glad_glMultiTexCoord3sARB, 4, target, s, t, r);
    glad_glMultiTexCoord3sARB(target, s, t, r);
    _post_call_gl_callback(NULL, "glMultiTexCoord3sARB", (GLADapiproc) glad_glMultiTexCoord3sARB, 4, target, s, t, r);
    
}
PFNGLMULTITEXCOORD3SARBPROC glad_debug_glMultiTexCoord3sARB = glad_debug_impl_glMultiTexCoord3sARB;
PFNGLMULTITEXCOORD3SVARBPROC glad_glMultiTexCoord3svARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord3svARB(GLenum target, const GLshort * v) {
    _pre_call_gl_callback("glMultiTexCoord3svARB", (GLADapiproc) glad_glMultiTexCoord3svARB, 2, target, v);
    glad_glMultiTexCoord3svARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord3svARB", (GLADapiproc) glad_glMultiTexCoord3svARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD3SVARBPROC glad_debug_glMultiTexCoord3svARB = glad_debug_impl_glMultiTexCoord3svARB;
PFNGLMULTITEXCOORD4DARBPROC glad_glMultiTexCoord4dARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord4dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q) {
    _pre_call_gl_callback("glMultiTexCoord4dARB", (GLADapiproc) glad_glMultiTexCoord4dARB, 5, target, s, t, r, q);
    glad_glMultiTexCoord4dARB(target, s, t, r, q);
    _post_call_gl_callback(NULL, "glMultiTexCoord4dARB", (GLADapiproc) glad_glMultiTexCoord4dARB, 5, target, s, t, r, q);
    
}
PFNGLMULTITEXCOORD4DARBPROC glad_debug_glMultiTexCoord4dARB = glad_debug_impl_glMultiTexCoord4dARB;
PFNGLMULTITEXCOORD4DVARBPROC glad_glMultiTexCoord4dvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord4dvARB(GLenum target, const GLdouble * v) {
    _pre_call_gl_callback("glMultiTexCoord4dvARB", (GLADapiproc) glad_glMultiTexCoord4dvARB, 2, target, v);
    glad_glMultiTexCoord4dvARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord4dvARB", (GLADapiproc) glad_glMultiTexCoord4dvARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD4DVARBPROC glad_debug_glMultiTexCoord4dvARB = glad_debug_impl_glMultiTexCoord4dvARB;
PFNGLMULTITEXCOORD4FARBPROC glad_glMultiTexCoord4fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord4fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    _pre_call_gl_callback("glMultiTexCoord4fARB", (GLADapiproc) glad_glMultiTexCoord4fARB, 5, target, s, t, r, q);
    glad_glMultiTexCoord4fARB(target, s, t, r, q);
    _post_call_gl_callback(NULL, "glMultiTexCoord4fARB", (GLADapiproc) glad_glMultiTexCoord4fARB, 5, target, s, t, r, q);
    
}
PFNGLMULTITEXCOORD4FARBPROC glad_debug_glMultiTexCoord4fARB = glad_debug_impl_glMultiTexCoord4fARB;
PFNGLMULTITEXCOORD4FVARBPROC glad_glMultiTexCoord4fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord4fvARB(GLenum target, const GLfloat * v) {
    _pre_call_gl_callback("glMultiTexCoord4fvARB", (GLADapiproc) glad_glMultiTexCoord4fvARB, 2, target, v);
    glad_glMultiTexCoord4fvARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord4fvARB", (GLADapiproc) glad_glMultiTexCoord4fvARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD4FVARBPROC glad_debug_glMultiTexCoord4fvARB = glad_debug_impl_glMultiTexCoord4fvARB;
PFNGLMULTITEXCOORD4IARBPROC glad_glMultiTexCoord4iARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord4iARB(GLenum target, GLint s, GLint t, GLint r, GLint q) {
    _pre_call_gl_callback("glMultiTexCoord4iARB", (GLADapiproc) glad_glMultiTexCoord4iARB, 5, target, s, t, r, q);
    glad_glMultiTexCoord4iARB(target, s, t, r, q);
    _post_call_gl_callback(NULL, "glMultiTexCoord4iARB", (GLADapiproc) glad_glMultiTexCoord4iARB, 5, target, s, t, r, q);
    
}
PFNGLMULTITEXCOORD4IARBPROC glad_debug_glMultiTexCoord4iARB = glad_debug_impl_glMultiTexCoord4iARB;
PFNGLMULTITEXCOORD4IVARBPROC glad_glMultiTexCoord4ivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord4ivARB(GLenum target, const GLint * v) {
    _pre_call_gl_callback("glMultiTexCoord4ivARB", (GLADapiproc) glad_glMultiTexCoord4ivARB, 2, target, v);
    glad_glMultiTexCoord4ivARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord4ivARB", (GLADapiproc) glad_glMultiTexCoord4ivARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD4IVARBPROC glad_debug_glMultiTexCoord4ivARB = glad_debug_impl_glMultiTexCoord4ivARB;
PFNGLMULTITEXCOORD4SARBPROC glad_glMultiTexCoord4sARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord4sARB(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q) {
    _pre_call_gl_callback("glMultiTexCoord4sARB", (GLADapiproc) glad_glMultiTexCoord4sARB, 5, target, s, t, r, q);
    glad_glMultiTexCoord4sARB(target, s, t, r, q);
    _post_call_gl_callback(NULL, "glMultiTexCoord4sARB", (GLADapiproc) glad_glMultiTexCoord4sARB, 5, target, s, t, r, q);
    
}
PFNGLMULTITEXCOORD4SARBPROC glad_debug_glMultiTexCoord4sARB = glad_debug_impl_glMultiTexCoord4sARB;
PFNGLMULTITEXCOORD4SVARBPROC glad_glMultiTexCoord4svARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord4svARB(GLenum target, const GLshort * v) {
    _pre_call_gl_callback("glMultiTexCoord4svARB", (GLADapiproc) glad_glMultiTexCoord4svARB, 2, target, v);
    glad_glMultiTexCoord4svARB(target, v);
    _post_call_gl_callback(NULL, "glMultiTexCoord4svARB", (GLADapiproc) glad_glMultiTexCoord4svARB, 2, target, v);
    
}
PFNGLMULTITEXCOORD4SVARBPROC glad_debug_glMultiTexCoord4svARB = glad_debug_impl_glMultiTexCoord4svARB;
PFNGLMULTITEXCOORDPOINTEREXTPROC glad_glMultiTexCoordPointerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoordPointerEXT(GLenum texunit, GLint size, GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gl_callback("glMultiTexCoordPointerEXT", (GLADapiproc) glad_glMultiTexCoordPointerEXT, 5, texunit, size, type, stride, pointer);
    glad_glMultiTexCoordPointerEXT(texunit, size, type, stride, pointer);
    _post_call_gl_callback(NULL, "glMultiTexCoordPointerEXT", (GLADapiproc) glad_glMultiTexCoordPointerEXT, 5, texunit, size, type, stride, pointer);
    
}
PFNGLMULTITEXCOORDPOINTEREXTPROC glad_debug_glMultiTexCoordPointerEXT = glad_debug_impl_glMultiTexCoordPointerEXT;
PFNGLMULTITEXENVFEXTPROC glad_glMultiTexEnvfEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexEnvfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param) {
    _pre_call_gl_callback("glMultiTexEnvfEXT", (GLADapiproc) glad_glMultiTexEnvfEXT, 4, texunit, target, pname, param);
    glad_glMultiTexEnvfEXT(texunit, target, pname, param);
    _post_call_gl_callback(NULL, "glMultiTexEnvfEXT", (GLADapiproc) glad_glMultiTexEnvfEXT, 4, texunit, target, pname, param);
    
}
PFNGLMULTITEXENVFEXTPROC glad_debug_glMultiTexEnvfEXT = glad_debug_impl_glMultiTexEnvfEXT;
PFNGLMULTITEXENVFVEXTPROC glad_glMultiTexEnvfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexEnvfvEXT(GLenum texunit, GLenum target, GLenum pname, const GLfloat * params) {
    _pre_call_gl_callback("glMultiTexEnvfvEXT", (GLADapiproc) glad_glMultiTexEnvfvEXT, 4, texunit, target, pname, params);
    glad_glMultiTexEnvfvEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glMultiTexEnvfvEXT", (GLADapiproc) glad_glMultiTexEnvfvEXT, 4, texunit, target, pname, params);
    
}
PFNGLMULTITEXENVFVEXTPROC glad_debug_glMultiTexEnvfvEXT = glad_debug_impl_glMultiTexEnvfvEXT;
PFNGLMULTITEXENVIEXTPROC glad_glMultiTexEnviEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexEnviEXT(GLenum texunit, GLenum target, GLenum pname, GLint param) {
    _pre_call_gl_callback("glMultiTexEnviEXT", (GLADapiproc) glad_glMultiTexEnviEXT, 4, texunit, target, pname, param);
    glad_glMultiTexEnviEXT(texunit, target, pname, param);
    _post_call_gl_callback(NULL, "glMultiTexEnviEXT", (GLADapiproc) glad_glMultiTexEnviEXT, 4, texunit, target, pname, param);
    
}
PFNGLMULTITEXENVIEXTPROC glad_debug_glMultiTexEnviEXT = glad_debug_impl_glMultiTexEnviEXT;
PFNGLMULTITEXENVIVEXTPROC glad_glMultiTexEnvivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexEnvivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gl_callback("glMultiTexEnvivEXT", (GLADapiproc) glad_glMultiTexEnvivEXT, 4, texunit, target, pname, params);
    glad_glMultiTexEnvivEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glMultiTexEnvivEXT", (GLADapiproc) glad_glMultiTexEnvivEXT, 4, texunit, target, pname, params);
    
}
PFNGLMULTITEXENVIVEXTPROC glad_debug_glMultiTexEnvivEXT = glad_debug_impl_glMultiTexEnvivEXT;
PFNGLMULTITEXGENDEXTPROC glad_glMultiTexGendEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexGendEXT(GLenum texunit, GLenum coord, GLenum pname, GLdouble param) {
    _pre_call_gl_callback("glMultiTexGendEXT", (GLADapiproc) glad_glMultiTexGendEXT, 4, texunit, coord, pname, param);
    glad_glMultiTexGendEXT(texunit, coord, pname, param);
    _post_call_gl_callback(NULL, "glMultiTexGendEXT", (GLADapiproc) glad_glMultiTexGendEXT, 4, texunit, coord, pname, param);
    
}
PFNGLMULTITEXGENDEXTPROC glad_debug_glMultiTexGendEXT = glad_debug_impl_glMultiTexGendEXT;
PFNGLMULTITEXGENDVEXTPROC glad_glMultiTexGendvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexGendvEXT(GLenum texunit, GLenum coord, GLenum pname, const GLdouble * params) {
    _pre_call_gl_callback("glMultiTexGendvEXT", (GLADapiproc) glad_glMultiTexGendvEXT, 4, texunit, coord, pname, params);
    glad_glMultiTexGendvEXT(texunit, coord, pname, params);
    _post_call_gl_callback(NULL, "glMultiTexGendvEXT", (GLADapiproc) glad_glMultiTexGendvEXT, 4, texunit, coord, pname, params);
    
}
PFNGLMULTITEXGENDVEXTPROC glad_debug_glMultiTexGendvEXT = glad_debug_impl_glMultiTexGendvEXT;
PFNGLMULTITEXGENFEXTPROC glad_glMultiTexGenfEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexGenfEXT(GLenum texunit, GLenum coord, GLenum pname, GLfloat param) {
    _pre_call_gl_callback("glMultiTexGenfEXT", (GLADapiproc) glad_glMultiTexGenfEXT, 4, texunit, coord, pname, param);
    glad_glMultiTexGenfEXT(texunit, coord, pname, param);
    _post_call_gl_callback(NULL, "glMultiTexGenfEXT", (GLADapiproc) glad_glMultiTexGenfEXT, 4, texunit, coord, pname, param);
    
}
PFNGLMULTITEXGENFEXTPROC glad_debug_glMultiTexGenfEXT = glad_debug_impl_glMultiTexGenfEXT;
PFNGLMULTITEXGENFVEXTPROC glad_glMultiTexGenfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexGenfvEXT(GLenum texunit, GLenum coord, GLenum pname, const GLfloat * params) {
    _pre_call_gl_callback("glMultiTexGenfvEXT", (GLADapiproc) glad_glMultiTexGenfvEXT, 4, texunit, coord, pname, params);
    glad_glMultiTexGenfvEXT(texunit, coord, pname, params);
    _post_call_gl_callback(NULL, "glMultiTexGenfvEXT", (GLADapiproc) glad_glMultiTexGenfvEXT, 4, texunit, coord, pname, params);
    
}
PFNGLMULTITEXGENFVEXTPROC glad_debug_glMultiTexGenfvEXT = glad_debug_impl_glMultiTexGenfvEXT;
PFNGLMULTITEXGENIEXTPROC glad_glMultiTexGeniEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexGeniEXT(GLenum texunit, GLenum coord, GLenum pname, GLint param) {
    _pre_call_gl_callback("glMultiTexGeniEXT", (GLADapiproc) glad_glMultiTexGeniEXT, 4, texunit, coord, pname, param);
    glad_glMultiTexGeniEXT(texunit, coord, pname, param);
    _post_call_gl_callback(NULL, "glMultiTexGeniEXT", (GLADapiproc) glad_glMultiTexGeniEXT, 4, texunit, coord, pname, param);
    
}
PFNGLMULTITEXGENIEXTPROC glad_debug_glMultiTexGeniEXT = glad_debug_impl_glMultiTexGeniEXT;
PFNGLMULTITEXGENIVEXTPROC glad_glMultiTexGenivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexGenivEXT(GLenum texunit, GLenum coord, GLenum pname, const GLint * params) {
    _pre_call_gl_callback("glMultiTexGenivEXT", (GLADapiproc) glad_glMultiTexGenivEXT, 4, texunit, coord, pname, params);
    glad_glMultiTexGenivEXT(texunit, coord, pname, params);
    _post_call_gl_callback(NULL, "glMultiTexGenivEXT", (GLADapiproc) glad_glMultiTexGenivEXT, 4, texunit, coord, pname, params);
    
}
PFNGLMULTITEXGENIVEXTPROC glad_debug_glMultiTexGenivEXT = glad_debug_impl_glMultiTexGenivEXT;
PFNGLMULTITEXIMAGE1DEXTPROC glad_glMultiTexImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glMultiTexImage1DEXT", (GLADapiproc) glad_glMultiTexImage1DEXT, 9, texunit, target, level, internalformat, width, border, format, type, pixels);
    glad_glMultiTexImage1DEXT(texunit, target, level, internalformat, width, border, format, type, pixels);
    _post_call_gl_callback(NULL, "glMultiTexImage1DEXT", (GLADapiproc) glad_glMultiTexImage1DEXT, 9, texunit, target, level, internalformat, width, border, format, type, pixels);
    
}
PFNGLMULTITEXIMAGE1DEXTPROC glad_debug_glMultiTexImage1DEXT = glad_debug_impl_glMultiTexImage1DEXT;
PFNGLMULTITEXIMAGE2DEXTPROC glad_glMultiTexImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glMultiTexImage2DEXT", (GLADapiproc) glad_glMultiTexImage2DEXT, 10, texunit, target, level, internalformat, width, height, border, format, type, pixels);
    glad_glMultiTexImage2DEXT(texunit, target, level, internalformat, width, height, border, format, type, pixels);
    _post_call_gl_callback(NULL, "glMultiTexImage2DEXT", (GLADapiproc) glad_glMultiTexImage2DEXT, 10, texunit, target, level, internalformat, width, height, border, format, type, pixels);
    
}
PFNGLMULTITEXIMAGE2DEXTPROC glad_debug_glMultiTexImage2DEXT = glad_debug_impl_glMultiTexImage2DEXT;
PFNGLMULTITEXIMAGE3DEXTPROC glad_glMultiTexImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glMultiTexImage3DEXT", (GLADapiproc) glad_glMultiTexImage3DEXT, 11, texunit, target, level, internalformat, width, height, depth, border, format, type, pixels);
    glad_glMultiTexImage3DEXT(texunit, target, level, internalformat, width, height, depth, border, format, type, pixels);
    _post_call_gl_callback(NULL, "glMultiTexImage3DEXT", (GLADapiproc) glad_glMultiTexImage3DEXT, 11, texunit, target, level, internalformat, width, height, depth, border, format, type, pixels);
    
}
PFNGLMULTITEXIMAGE3DEXTPROC glad_debug_glMultiTexImage3DEXT = glad_debug_impl_glMultiTexImage3DEXT;
PFNGLMULTITEXPARAMETERIIVEXTPROC glad_glMultiTexParameterIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gl_callback("glMultiTexParameterIivEXT", (GLADapiproc) glad_glMultiTexParameterIivEXT, 4, texunit, target, pname, params);
    glad_glMultiTexParameterIivEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glMultiTexParameterIivEXT", (GLADapiproc) glad_glMultiTexParameterIivEXT, 4, texunit, target, pname, params);
    
}
PFNGLMULTITEXPARAMETERIIVEXTPROC glad_debug_glMultiTexParameterIivEXT = glad_debug_impl_glMultiTexParameterIivEXT;
PFNGLMULTITEXPARAMETERIUIVEXTPROC glad_glMultiTexParameterIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname, const GLuint * params) {
    _pre_call_gl_callback("glMultiTexParameterIuivEXT", (GLADapiproc) glad_glMultiTexParameterIuivEXT, 4, texunit, target, pname, params);
    glad_glMultiTexParameterIuivEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glMultiTexParameterIuivEXT", (GLADapiproc) glad_glMultiTexParameterIuivEXT, 4, texunit, target, pname, params);
    
}
PFNGLMULTITEXPARAMETERIUIVEXTPROC glad_debug_glMultiTexParameterIuivEXT = glad_debug_impl_glMultiTexParameterIuivEXT;
PFNGLMULTITEXPARAMETERFEXTPROC glad_glMultiTexParameterfEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexParameterfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param) {
    _pre_call_gl_callback("glMultiTexParameterfEXT", (GLADapiproc) glad_glMultiTexParameterfEXT, 4, texunit, target, pname, param);
    glad_glMultiTexParameterfEXT(texunit, target, pname, param);
    _post_call_gl_callback(NULL, "glMultiTexParameterfEXT", (GLADapiproc) glad_glMultiTexParameterfEXT, 4, texunit, target, pname, param);
    
}
PFNGLMULTITEXPARAMETERFEXTPROC glad_debug_glMultiTexParameterfEXT = glad_debug_impl_glMultiTexParameterfEXT;
PFNGLMULTITEXPARAMETERFVEXTPROC glad_glMultiTexParameterfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname, const GLfloat * params) {
    _pre_call_gl_callback("glMultiTexParameterfvEXT", (GLADapiproc) glad_glMultiTexParameterfvEXT, 4, texunit, target, pname, params);
    glad_glMultiTexParameterfvEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glMultiTexParameterfvEXT", (GLADapiproc) glad_glMultiTexParameterfvEXT, 4, texunit, target, pname, params);
    
}
PFNGLMULTITEXPARAMETERFVEXTPROC glad_debug_glMultiTexParameterfvEXT = glad_debug_impl_glMultiTexParameterfvEXT;
PFNGLMULTITEXPARAMETERIEXTPROC glad_glMultiTexParameteriEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexParameteriEXT(GLenum texunit, GLenum target, GLenum pname, GLint param) {
    _pre_call_gl_callback("glMultiTexParameteriEXT", (GLADapiproc) glad_glMultiTexParameteriEXT, 4, texunit, target, pname, param);
    glad_glMultiTexParameteriEXT(texunit, target, pname, param);
    _post_call_gl_callback(NULL, "glMultiTexParameteriEXT", (GLADapiproc) glad_glMultiTexParameteriEXT, 4, texunit, target, pname, param);
    
}
PFNGLMULTITEXPARAMETERIEXTPROC glad_debug_glMultiTexParameteriEXT = glad_debug_impl_glMultiTexParameteriEXT;
PFNGLMULTITEXPARAMETERIVEXTPROC glad_glMultiTexParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gl_callback("glMultiTexParameterivEXT", (GLADapiproc) glad_glMultiTexParameterivEXT, 4, texunit, target, pname, params);
    glad_glMultiTexParameterivEXT(texunit, target, pname, params);
    _post_call_gl_callback(NULL, "glMultiTexParameterivEXT", (GLADapiproc) glad_glMultiTexParameterivEXT, 4, texunit, target, pname, params);
    
}
PFNGLMULTITEXPARAMETERIVEXTPROC glad_debug_glMultiTexParameterivEXT = glad_debug_impl_glMultiTexParameterivEXT;
PFNGLMULTITEXRENDERBUFFEREXTPROC glad_glMultiTexRenderbufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexRenderbufferEXT(GLenum texunit, GLenum target, GLuint renderbuffer) {
    _pre_call_gl_callback("glMultiTexRenderbufferEXT", (GLADapiproc) glad_glMultiTexRenderbufferEXT, 3, texunit, target, renderbuffer);
    glad_glMultiTexRenderbufferEXT(texunit, target, renderbuffer);
    _post_call_gl_callback(NULL, "glMultiTexRenderbufferEXT", (GLADapiproc) glad_glMultiTexRenderbufferEXT, 3, texunit, target, renderbuffer);
    
}
PFNGLMULTITEXRENDERBUFFEREXTPROC glad_debug_glMultiTexRenderbufferEXT = glad_debug_impl_glMultiTexRenderbufferEXT;
PFNGLMULTITEXSUBIMAGE1DEXTPROC glad_glMultiTexSubImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glMultiTexSubImage1DEXT", (GLADapiproc) glad_glMultiTexSubImage1DEXT, 8, texunit, target, level, xoffset, width, format, type, pixels);
    glad_glMultiTexSubImage1DEXT(texunit, target, level, xoffset, width, format, type, pixels);
    _post_call_gl_callback(NULL, "glMultiTexSubImage1DEXT", (GLADapiproc) glad_glMultiTexSubImage1DEXT, 8, texunit, target, level, xoffset, width, format, type, pixels);
    
}
PFNGLMULTITEXSUBIMAGE1DEXTPROC glad_debug_glMultiTexSubImage1DEXT = glad_debug_impl_glMultiTexSubImage1DEXT;
PFNGLMULTITEXSUBIMAGE2DEXTPROC glad_glMultiTexSubImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glMultiTexSubImage2DEXT", (GLADapiproc) glad_glMultiTexSubImage2DEXT, 10, texunit, target, level, xoffset, yoffset, width, height, format, type, pixels);
    glad_glMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, width, height, format, type, pixels);
    _post_call_gl_callback(NULL, "glMultiTexSubImage2DEXT", (GLADapiproc) glad_glMultiTexSubImage2DEXT, 10, texunit, target, level, xoffset, yoffset, width, height, format, type, pixels);
    
}
PFNGLMULTITEXSUBIMAGE2DEXTPROC glad_debug_glMultiTexSubImage2DEXT = glad_debug_impl_glMultiTexSubImage2DEXT;
PFNGLMULTITEXSUBIMAGE3DEXTPROC glad_glMultiTexSubImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glMultiTexSubImage3DEXT", (GLADapiproc) glad_glMultiTexSubImage3DEXT, 12, texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    glad_glMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    _post_call_gl_callback(NULL, "glMultiTexSubImage3DEXT", (GLADapiproc) glad_glMultiTexSubImage3DEXT, 12, texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    
}
PFNGLMULTITEXSUBIMAGE3DEXTPROC glad_debug_glMultiTexSubImage3DEXT = glad_debug_impl_glMultiTexSubImage3DEXT;
PFNGLNAMEDBUFFERDATAEXTPROC glad_glNamedBufferDataEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedBufferDataEXT(GLuint buffer, GLsizeiptr size, const void * data, GLenum usage) {
    _pre_call_gl_callback("glNamedBufferDataEXT", (GLADapiproc) glad_glNamedBufferDataEXT, 4, buffer, size, data, usage);
    glad_glNamedBufferDataEXT(buffer, size, data, usage);
    _post_call_gl_callback(NULL, "glNamedBufferDataEXT", (GLADapiproc) glad_glNamedBufferDataEXT, 4, buffer, size, data, usage);
    
}
PFNGLNAMEDBUFFERDATAEXTPROC glad_debug_glNamedBufferDataEXT = glad_debug_impl_glNamedBufferDataEXT;
PFNGLNAMEDBUFFERSTORAGEEXTPROC glad_glNamedBufferStorageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void * data, GLbitfield flags) {
    _pre_call_gl_callback("glNamedBufferStorageEXT", (GLADapiproc) glad_glNamedBufferStorageEXT, 4, buffer, size, data, flags);
    glad_glNamedBufferStorageEXT(buffer, size, data, flags);
    _post_call_gl_callback(NULL, "glNamedBufferStorageEXT", (GLADapiproc) glad_glNamedBufferStorageEXT, 4, buffer, size, data, flags);
    
}
PFNGLNAMEDBUFFERSTORAGEEXTPROC glad_debug_glNamedBufferStorageEXT = glad_debug_impl_glNamedBufferStorageEXT;
PFNGLNAMEDBUFFERSUBDATAEXTPROC glad_glNamedBufferSubDataEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, const void * data) {
    _pre_call_gl_callback("glNamedBufferSubDataEXT", (GLADapiproc) glad_glNamedBufferSubDataEXT, 4, buffer, offset, size, data);
    glad_glNamedBufferSubDataEXT(buffer, offset, size, data);
    _post_call_gl_callback(NULL, "glNamedBufferSubDataEXT", (GLADapiproc) glad_glNamedBufferSubDataEXT, 4, buffer, offset, size, data);
    
}
PFNGLNAMEDBUFFERSUBDATAEXTPROC glad_debug_glNamedBufferSubDataEXT = glad_debug_impl_glNamedBufferSubDataEXT;
PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC glad_glNamedCopyBufferSubDataEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedCopyBufferSubDataEXT(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    _pre_call_gl_callback("glNamedCopyBufferSubDataEXT", (GLADapiproc) glad_glNamedCopyBufferSubDataEXT, 5, readBuffer, writeBuffer, readOffset, writeOffset, size);
    glad_glNamedCopyBufferSubDataEXT(readBuffer, writeBuffer, readOffset, writeOffset, size);
    _post_call_gl_callback(NULL, "glNamedCopyBufferSubDataEXT", (GLADapiproc) glad_glNamedCopyBufferSubDataEXT, 5, readBuffer, writeBuffer, readOffset, writeOffset, size);
    
}
PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC glad_debug_glNamedCopyBufferSubDataEXT = glad_debug_impl_glNamedCopyBufferSubDataEXT;
PFNGLNAMEDFRAMEBUFFERPARAMETERIEXTPROC glad_glNamedFramebufferParameteriEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedFramebufferParameteriEXT(GLuint framebuffer, GLenum pname, GLint param) {
    _pre_call_gl_callback("glNamedFramebufferParameteriEXT", (GLADapiproc) glad_glNamedFramebufferParameteriEXT, 3, framebuffer, pname, param);
    glad_glNamedFramebufferParameteriEXT(framebuffer, pname, param);
    _post_call_gl_callback(NULL, "glNamedFramebufferParameteriEXT", (GLADapiproc) glad_glNamedFramebufferParameteriEXT, 3, framebuffer, pname, param);
    
}
PFNGLNAMEDFRAMEBUFFERPARAMETERIEXTPROC glad_debug_glNamedFramebufferParameteriEXT = glad_debug_impl_glNamedFramebufferParameteriEXT;
PFNGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC glad_glNamedFramebufferRenderbufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedFramebufferRenderbufferEXT(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    _pre_call_gl_callback("glNamedFramebufferRenderbufferEXT", (GLADapiproc) glad_glNamedFramebufferRenderbufferEXT, 4, framebuffer, attachment, renderbuffertarget, renderbuffer);
    glad_glNamedFramebufferRenderbufferEXT(framebuffer, attachment, renderbuffertarget, renderbuffer);
    _post_call_gl_callback(NULL, "glNamedFramebufferRenderbufferEXT", (GLADapiproc) glad_glNamedFramebufferRenderbufferEXT, 4, framebuffer, attachment, renderbuffertarget, renderbuffer);
    
}
PFNGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC glad_debug_glNamedFramebufferRenderbufferEXT = glad_debug_impl_glNamedFramebufferRenderbufferEXT;
PFNGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC glad_glNamedFramebufferTexture1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedFramebufferTexture1DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    _pre_call_gl_callback("glNamedFramebufferTexture1DEXT", (GLADapiproc) glad_glNamedFramebufferTexture1DEXT, 5, framebuffer, attachment, textarget, texture, level);
    glad_glNamedFramebufferTexture1DEXT(framebuffer, attachment, textarget, texture, level);
    _post_call_gl_callback(NULL, "glNamedFramebufferTexture1DEXT", (GLADapiproc) glad_glNamedFramebufferTexture1DEXT, 5, framebuffer, attachment, textarget, texture, level);
    
}
PFNGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC glad_debug_glNamedFramebufferTexture1DEXT = glad_debug_impl_glNamedFramebufferTexture1DEXT;
PFNGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC glad_glNamedFramebufferTexture2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    _pre_call_gl_callback("glNamedFramebufferTexture2DEXT", (GLADapiproc) glad_glNamedFramebufferTexture2DEXT, 5, framebuffer, attachment, textarget, texture, level);
    glad_glNamedFramebufferTexture2DEXT(framebuffer, attachment, textarget, texture, level);
    _post_call_gl_callback(NULL, "glNamedFramebufferTexture2DEXT", (GLADapiproc) glad_glNamedFramebufferTexture2DEXT, 5, framebuffer, attachment, textarget, texture, level);
    
}
PFNGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC glad_debug_glNamedFramebufferTexture2DEXT = glad_debug_impl_glNamedFramebufferTexture2DEXT;
PFNGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC glad_glNamedFramebufferTexture3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedFramebufferTexture3DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) {
    _pre_call_gl_callback("glNamedFramebufferTexture3DEXT", (GLADapiproc) glad_glNamedFramebufferTexture3DEXT, 6, framebuffer, attachment, textarget, texture, level, zoffset);
    glad_glNamedFramebufferTexture3DEXT(framebuffer, attachment, textarget, texture, level, zoffset);
    _post_call_gl_callback(NULL, "glNamedFramebufferTexture3DEXT", (GLADapiproc) glad_glNamedFramebufferTexture3DEXT, 6, framebuffer, attachment, textarget, texture, level, zoffset);
    
}
PFNGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC glad_debug_glNamedFramebufferTexture3DEXT = glad_debug_impl_glNamedFramebufferTexture3DEXT;
PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC glad_glNamedFramebufferTextureEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
    _pre_call_gl_callback("glNamedFramebufferTextureEXT", (GLADapiproc) glad_glNamedFramebufferTextureEXT, 4, framebuffer, attachment, texture, level);
    glad_glNamedFramebufferTextureEXT(framebuffer, attachment, texture, level);
    _post_call_gl_callback(NULL, "glNamedFramebufferTextureEXT", (GLADapiproc) glad_glNamedFramebufferTextureEXT, 4, framebuffer, attachment, texture, level);
    
}
PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC glad_debug_glNamedFramebufferTextureEXT = glad_debug_impl_glNamedFramebufferTextureEXT;
PFNGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC glad_glNamedFramebufferTextureFaceEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedFramebufferTextureFaceEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLenum face) {
    _pre_call_gl_callback("glNamedFramebufferTextureFaceEXT", (GLADapiproc) glad_glNamedFramebufferTextureFaceEXT, 5, framebuffer, attachment, texture, level, face);
    glad_glNamedFramebufferTextureFaceEXT(framebuffer, attachment, texture, level, face);
    _post_call_gl_callback(NULL, "glNamedFramebufferTextureFaceEXT", (GLADapiproc) glad_glNamedFramebufferTextureFaceEXT, 5, framebuffer, attachment, texture, level, face);
    
}
PFNGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC glad_debug_glNamedFramebufferTextureFaceEXT = glad_debug_impl_glNamedFramebufferTextureFaceEXT;
PFNGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC glad_glNamedFramebufferTextureLayerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    _pre_call_gl_callback("glNamedFramebufferTextureLayerEXT", (GLADapiproc) glad_glNamedFramebufferTextureLayerEXT, 5, framebuffer, attachment, texture, level, layer);
    glad_glNamedFramebufferTextureLayerEXT(framebuffer, attachment, texture, level, layer);
    _post_call_gl_callback(NULL, "glNamedFramebufferTextureLayerEXT", (GLADapiproc) glad_glNamedFramebufferTextureLayerEXT, 5, framebuffer, attachment, texture, level, layer);
    
}
PFNGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC glad_debug_glNamedFramebufferTextureLayerEXT = glad_debug_impl_glNamedFramebufferTextureLayerEXT;
PFNGLNAMEDPROGRAMLOCALPARAMETER4DEXTPROC glad_glNamedProgramLocalParameter4dEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramLocalParameter4dEXT(GLuint program, GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    _pre_call_gl_callback("glNamedProgramLocalParameter4dEXT", (GLADapiproc) glad_glNamedProgramLocalParameter4dEXT, 7, program, target, index, x, y, z, w);
    glad_glNamedProgramLocalParameter4dEXT(program, target, index, x, y, z, w);
    _post_call_gl_callback(NULL, "glNamedProgramLocalParameter4dEXT", (GLADapiproc) glad_glNamedProgramLocalParameter4dEXT, 7, program, target, index, x, y, z, w);
    
}
PFNGLNAMEDPROGRAMLOCALPARAMETER4DEXTPROC glad_debug_glNamedProgramLocalParameter4dEXT = glad_debug_impl_glNamedProgramLocalParameter4dEXT;
PFNGLNAMEDPROGRAMLOCALPARAMETER4DVEXTPROC glad_glNamedProgramLocalParameter4dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramLocalParameter4dvEXT(GLuint program, GLenum target, GLuint index, const GLdouble * params) {
    _pre_call_gl_callback("glNamedProgramLocalParameter4dvEXT", (GLADapiproc) glad_glNamedProgramLocalParameter4dvEXT, 4, program, target, index, params);
    glad_glNamedProgramLocalParameter4dvEXT(program, target, index, params);
    _post_call_gl_callback(NULL, "glNamedProgramLocalParameter4dvEXT", (GLADapiproc) glad_glNamedProgramLocalParameter4dvEXT, 4, program, target, index, params);
    
}
PFNGLNAMEDPROGRAMLOCALPARAMETER4DVEXTPROC glad_debug_glNamedProgramLocalParameter4dvEXT = glad_debug_impl_glNamedProgramLocalParameter4dvEXT;
PFNGLNAMEDPROGRAMLOCALPARAMETER4FEXTPROC glad_glNamedProgramLocalParameter4fEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramLocalParameter4fEXT(GLuint program, GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    _pre_call_gl_callback("glNamedProgramLocalParameter4fEXT", (GLADapiproc) glad_glNamedProgramLocalParameter4fEXT, 7, program, target, index, x, y, z, w);
    glad_glNamedProgramLocalParameter4fEXT(program, target, index, x, y, z, w);
    _post_call_gl_callback(NULL, "glNamedProgramLocalParameter4fEXT", (GLADapiproc) glad_glNamedProgramLocalParameter4fEXT, 7, program, target, index, x, y, z, w);
    
}
PFNGLNAMEDPROGRAMLOCALPARAMETER4FEXTPROC glad_debug_glNamedProgramLocalParameter4fEXT = glad_debug_impl_glNamedProgramLocalParameter4fEXT;
PFNGLNAMEDPROGRAMLOCALPARAMETER4FVEXTPROC glad_glNamedProgramLocalParameter4fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramLocalParameter4fvEXT(GLuint program, GLenum target, GLuint index, const GLfloat * params) {
    _pre_call_gl_callback("glNamedProgramLocalParameter4fvEXT", (GLADapiproc) glad_glNamedProgramLocalParameter4fvEXT, 4, program, target, index, params);
    glad_glNamedProgramLocalParameter4fvEXT(program, target, index, params);
    _post_call_gl_callback(NULL, "glNamedProgramLocalParameter4fvEXT", (GLADapiproc) glad_glNamedProgramLocalParameter4fvEXT, 4, program, target, index, params);
    
}
PFNGLNAMEDPROGRAMLOCALPARAMETER4FVEXTPROC glad_debug_glNamedProgramLocalParameter4fvEXT = glad_debug_impl_glNamedProgramLocalParameter4fvEXT;
PFNGLNAMEDPROGRAMLOCALPARAMETERI4IEXTPROC glad_glNamedProgramLocalParameterI4iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramLocalParameterI4iEXT(GLuint program, GLenum target, GLuint index, GLint x, GLint y, GLint z, GLint w) {
    _pre_call_gl_callback("glNamedProgramLocalParameterI4iEXT", (GLADapiproc) glad_glNamedProgramLocalParameterI4iEXT, 7, program, target, index, x, y, z, w);
    glad_glNamedProgramLocalParameterI4iEXT(program, target, index, x, y, z, w);
    _post_call_gl_callback(NULL, "glNamedProgramLocalParameterI4iEXT", (GLADapiproc) glad_glNamedProgramLocalParameterI4iEXT, 7, program, target, index, x, y, z, w);
    
}
PFNGLNAMEDPROGRAMLOCALPARAMETERI4IEXTPROC glad_debug_glNamedProgramLocalParameterI4iEXT = glad_debug_impl_glNamedProgramLocalParameterI4iEXT;
PFNGLNAMEDPROGRAMLOCALPARAMETERI4IVEXTPROC glad_glNamedProgramLocalParameterI4ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramLocalParameterI4ivEXT(GLuint program, GLenum target, GLuint index, const GLint * params) {
    _pre_call_gl_callback("glNamedProgramLocalParameterI4ivEXT", (GLADapiproc) glad_glNamedProgramLocalParameterI4ivEXT, 4, program, target, index, params);
    glad_glNamedProgramLocalParameterI4ivEXT(program, target, index, params);
    _post_call_gl_callback(NULL, "glNamedProgramLocalParameterI4ivEXT", (GLADapiproc) glad_glNamedProgramLocalParameterI4ivEXT, 4, program, target, index, params);
    
}
PFNGLNAMEDPROGRAMLOCALPARAMETERI4IVEXTPROC glad_debug_glNamedProgramLocalParameterI4ivEXT = glad_debug_impl_glNamedProgramLocalParameterI4ivEXT;
PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIEXTPROC glad_glNamedProgramLocalParameterI4uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramLocalParameterI4uiEXT(GLuint program, GLenum target, GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    _pre_call_gl_callback("glNamedProgramLocalParameterI4uiEXT", (GLADapiproc) glad_glNamedProgramLocalParameterI4uiEXT, 7, program, target, index, x, y, z, w);
    glad_glNamedProgramLocalParameterI4uiEXT(program, target, index, x, y, z, w);
    _post_call_gl_callback(NULL, "glNamedProgramLocalParameterI4uiEXT", (GLADapiproc) glad_glNamedProgramLocalParameterI4uiEXT, 7, program, target, index, x, y, z, w);
    
}
PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIEXTPROC glad_debug_glNamedProgramLocalParameterI4uiEXT = glad_debug_impl_glNamedProgramLocalParameterI4uiEXT;
PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIVEXTPROC glad_glNamedProgramLocalParameterI4uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramLocalParameterI4uivEXT(GLuint program, GLenum target, GLuint index, const GLuint * params) {
    _pre_call_gl_callback("glNamedProgramLocalParameterI4uivEXT", (GLADapiproc) glad_glNamedProgramLocalParameterI4uivEXT, 4, program, target, index, params);
    glad_glNamedProgramLocalParameterI4uivEXT(program, target, index, params);
    _post_call_gl_callback(NULL, "glNamedProgramLocalParameterI4uivEXT", (GLADapiproc) glad_glNamedProgramLocalParameterI4uivEXT, 4, program, target, index, params);
    
}
PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIVEXTPROC glad_debug_glNamedProgramLocalParameterI4uivEXT = glad_debug_impl_glNamedProgramLocalParameterI4uivEXT;
PFNGLNAMEDPROGRAMLOCALPARAMETERS4FVEXTPROC glad_glNamedProgramLocalParameters4fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramLocalParameters4fvEXT(GLuint program, GLenum target, GLuint index, GLsizei count, const GLfloat * params) {
    _pre_call_gl_callback("glNamedProgramLocalParameters4fvEXT", (GLADapiproc) glad_glNamedProgramLocalParameters4fvEXT, 5, program, target, index, count, params);
    glad_glNamedProgramLocalParameters4fvEXT(program, target, index, count, params);
    _post_call_gl_callback(NULL, "glNamedProgramLocalParameters4fvEXT", (GLADapiproc) glad_glNamedProgramLocalParameters4fvEXT, 5, program, target, index, count, params);
    
}
PFNGLNAMEDPROGRAMLOCALPARAMETERS4FVEXTPROC glad_debug_glNamedProgramLocalParameters4fvEXT = glad_debug_impl_glNamedProgramLocalParameters4fvEXT;
PFNGLNAMEDPROGRAMLOCALPARAMETERSI4IVEXTPROC glad_glNamedProgramLocalParametersI4ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramLocalParametersI4ivEXT(GLuint program, GLenum target, GLuint index, GLsizei count, const GLint * params) {
    _pre_call_gl_callback("glNamedProgramLocalParametersI4ivEXT", (GLADapiproc) glad_glNamedProgramLocalParametersI4ivEXT, 5, program, target, index, count, params);
    glad_glNamedProgramLocalParametersI4ivEXT(program, target, index, count, params);
    _post_call_gl_callback(NULL, "glNamedProgramLocalParametersI4ivEXT", (GLADapiproc) glad_glNamedProgramLocalParametersI4ivEXT, 5, program, target, index, count, params);
    
}
PFNGLNAMEDPROGRAMLOCALPARAMETERSI4IVEXTPROC glad_debug_glNamedProgramLocalParametersI4ivEXT = glad_debug_impl_glNamedProgramLocalParametersI4ivEXT;
PFNGLNAMEDPROGRAMLOCALPARAMETERSI4UIVEXTPROC glad_glNamedProgramLocalParametersI4uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramLocalParametersI4uivEXT(GLuint program, GLenum target, GLuint index, GLsizei count, const GLuint * params) {
    _pre_call_gl_callback("glNamedProgramLocalParametersI4uivEXT", (GLADapiproc) glad_glNamedProgramLocalParametersI4uivEXT, 5, program, target, index, count, params);
    glad_glNamedProgramLocalParametersI4uivEXT(program, target, index, count, params);
    _post_call_gl_callback(NULL, "glNamedProgramLocalParametersI4uivEXT", (GLADapiproc) glad_glNamedProgramLocalParametersI4uivEXT, 5, program, target, index, count, params);
    
}
PFNGLNAMEDPROGRAMLOCALPARAMETERSI4UIVEXTPROC glad_debug_glNamedProgramLocalParametersI4uivEXT = glad_debug_impl_glNamedProgramLocalParametersI4uivEXT;
PFNGLNAMEDPROGRAMSTRINGEXTPROC glad_glNamedProgramStringEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedProgramStringEXT(GLuint program, GLenum target, GLenum format, GLsizei len, const void * string) {
    _pre_call_gl_callback("glNamedProgramStringEXT", (GLADapiproc) glad_glNamedProgramStringEXT, 5, program, target, format, len, string);
    glad_glNamedProgramStringEXT(program, target, format, len, string);
    _post_call_gl_callback(NULL, "glNamedProgramStringEXT", (GLADapiproc) glad_glNamedProgramStringEXT, 5, program, target, format, len, string);
    
}
PFNGLNAMEDPROGRAMSTRINGEXTPROC glad_debug_glNamedProgramStringEXT = glad_debug_impl_glNamedProgramStringEXT;
PFNGLNAMEDRENDERBUFFERSTORAGEEXTPROC glad_glNamedRenderbufferStorageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedRenderbufferStorageEXT(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glNamedRenderbufferStorageEXT", (GLADapiproc) glad_glNamedRenderbufferStorageEXT, 4, renderbuffer, internalformat, width, height);
    glad_glNamedRenderbufferStorageEXT(renderbuffer, internalformat, width, height);
    _post_call_gl_callback(NULL, "glNamedRenderbufferStorageEXT", (GLADapiproc) glad_glNamedRenderbufferStorageEXT, 4, renderbuffer, internalformat, width, height);
    
}
PFNGLNAMEDRENDERBUFFERSTORAGEEXTPROC glad_debug_glNamedRenderbufferStorageEXT = glad_debug_impl_glNamedRenderbufferStorageEXT;
PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXTPROC glad_glNamedRenderbufferStorageMultisampleCoverageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedRenderbufferStorageMultisampleCoverageEXT(GLuint renderbuffer, GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glNamedRenderbufferStorageMultisampleCoverageEXT", (GLADapiproc) glad_glNamedRenderbufferStorageMultisampleCoverageEXT, 6, renderbuffer, coverageSamples, colorSamples, internalformat, width, height);
    glad_glNamedRenderbufferStorageMultisampleCoverageEXT(renderbuffer, coverageSamples, colorSamples, internalformat, width, height);
    _post_call_gl_callback(NULL, "glNamedRenderbufferStorageMultisampleCoverageEXT", (GLADapiproc) glad_glNamedRenderbufferStorageMultisampleCoverageEXT, 6, renderbuffer, coverageSamples, colorSamples, internalformat, width, height);
    
}
PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXTPROC glad_debug_glNamedRenderbufferStorageMultisampleCoverageEXT = glad_debug_impl_glNamedRenderbufferStorageMultisampleCoverageEXT;
PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glad_glNamedRenderbufferStorageMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedRenderbufferStorageMultisampleEXT(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glNamedRenderbufferStorageMultisampleEXT", (GLADapiproc) glad_glNamedRenderbufferStorageMultisampleEXT, 5, renderbuffer, samples, internalformat, width, height);
    glad_glNamedRenderbufferStorageMultisampleEXT(renderbuffer, samples, internalformat, width, height);
    _post_call_gl_callback(NULL, "glNamedRenderbufferStorageMultisampleEXT", (GLADapiproc) glad_glNamedRenderbufferStorageMultisampleEXT, 5, renderbuffer, samples, internalformat, width, height);
    
}
PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glad_debug_glNamedRenderbufferStorageMultisampleEXT = glad_debug_impl_glNamedRenderbufferStorageMultisampleEXT;
PFNGLNORMALPOINTEREXTPROC glad_glNormalPointerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNormalPointerEXT(GLenum type, GLsizei stride, GLsizei count, const void * pointer) {
    _pre_call_gl_callback("glNormalPointerEXT", (GLADapiproc) glad_glNormalPointerEXT, 4, type, stride, count, pointer);
    glad_glNormalPointerEXT(type, stride, count, pointer);
    _post_call_gl_callback(NULL, "glNormalPointerEXT", (GLADapiproc) glad_glNormalPointerEXT, 4, type, stride, count, pointer);
    
}
PFNGLNORMALPOINTEREXTPROC glad_debug_glNormalPointerEXT = glad_debug_impl_glNormalPointerEXT;
PFNGLPIXELSTOREFPROC glad_glPixelStoref = NULL;
static void GLAD_API_PTR glad_debug_impl_glPixelStoref(GLenum pname, GLfloat param) {
    _pre_call_gl_callback("glPixelStoref", (GLADapiproc) glad_glPixelStoref, 2, pname, param);
    glad_glPixelStoref(pname, param);
    _post_call_gl_callback(NULL, "glPixelStoref", (GLADapiproc) glad_glPixelStoref, 2, pname, param);
    
}
PFNGLPIXELSTOREFPROC glad_debug_glPixelStoref = glad_debug_impl_glPixelStoref;
PFNGLPIXELSTOREIPROC glad_glPixelStorei = NULL;
static void GLAD_API_PTR glad_debug_impl_glPixelStorei(GLenum pname, GLint param) {
    _pre_call_gl_callback("glPixelStorei", (GLADapiproc) glad_glPixelStorei, 2, pname, param);
    glad_glPixelStorei(pname, param);
    _post_call_gl_callback(NULL, "glPixelStorei", (GLADapiproc) glad_glPixelStorei, 2, pname, param);
    
}
PFNGLPIXELSTOREIPROC glad_debug_glPixelStorei = glad_debug_impl_glPixelStorei;
PFNGLPOINTPARAMETERFPROC glad_glPointParameterf = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterf(GLenum pname, GLfloat param) {
    _pre_call_gl_callback("glPointParameterf", (GLADapiproc) glad_glPointParameterf, 2, pname, param);
    glad_glPointParameterf(pname, param);
    _post_call_gl_callback(NULL, "glPointParameterf", (GLADapiproc) glad_glPointParameterf, 2, pname, param);
    
}
PFNGLPOINTPARAMETERFPROC glad_debug_glPointParameterf = glad_debug_impl_glPointParameterf;
PFNGLPOINTPARAMETERFARBPROC glad_glPointParameterfARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterfARB(GLenum pname, GLfloat param) {
    _pre_call_gl_callback("glPointParameterfARB", (GLADapiproc) glad_glPointParameterfARB, 2, pname, param);
    glad_glPointParameterfARB(pname, param);
    _post_call_gl_callback(NULL, "glPointParameterfARB", (GLADapiproc) glad_glPointParameterfARB, 2, pname, param);
    
}
PFNGLPOINTPARAMETERFARBPROC glad_debug_glPointParameterfARB = glad_debug_impl_glPointParameterfARB;
PFNGLPOINTPARAMETERFEXTPROC glad_glPointParameterfEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterfEXT(GLenum pname, GLfloat param) {
    _pre_call_gl_callback("glPointParameterfEXT", (GLADapiproc) glad_glPointParameterfEXT, 2, pname, param);
    glad_glPointParameterfEXT(pname, param);
    _post_call_gl_callback(NULL, "glPointParameterfEXT", (GLADapiproc) glad_glPointParameterfEXT, 2, pname, param);
    
}
PFNGLPOINTPARAMETERFEXTPROC glad_debug_glPointParameterfEXT = glad_debug_impl_glPointParameterfEXT;
PFNGLPOINTPARAMETERFSGISPROC glad_glPointParameterfSGIS = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterfSGIS(GLenum pname, GLfloat param) {
    _pre_call_gl_callback("glPointParameterfSGIS", (GLADapiproc) glad_glPointParameterfSGIS, 2, pname, param);
    glad_glPointParameterfSGIS(pname, param);
    _post_call_gl_callback(NULL, "glPointParameterfSGIS", (GLADapiproc) glad_glPointParameterfSGIS, 2, pname, param);
    
}
PFNGLPOINTPARAMETERFSGISPROC glad_debug_glPointParameterfSGIS = glad_debug_impl_glPointParameterfSGIS;
PFNGLPOINTPARAMETERFVPROC glad_glPointParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterfv(GLenum pname, const GLfloat * params) {
    _pre_call_gl_callback("glPointParameterfv", (GLADapiproc) glad_glPointParameterfv, 2, pname, params);
    glad_glPointParameterfv(pname, params);
    _post_call_gl_callback(NULL, "glPointParameterfv", (GLADapiproc) glad_glPointParameterfv, 2, pname, params);
    
}
PFNGLPOINTPARAMETERFVPROC glad_debug_glPointParameterfv = glad_debug_impl_glPointParameterfv;
PFNGLPOINTPARAMETERFVARBPROC glad_glPointParameterfvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterfvARB(GLenum pname, const GLfloat * params) {
    _pre_call_gl_callback("glPointParameterfvARB", (GLADapiproc) glad_glPointParameterfvARB, 2, pname, params);
    glad_glPointParameterfvARB(pname, params);
    _post_call_gl_callback(NULL, "glPointParameterfvARB", (GLADapiproc) glad_glPointParameterfvARB, 2, pname, params);
    
}
PFNGLPOINTPARAMETERFVARBPROC glad_debug_glPointParameterfvARB = glad_debug_impl_glPointParameterfvARB;
PFNGLPOINTPARAMETERFVEXTPROC glad_glPointParameterfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterfvEXT(GLenum pname, const GLfloat * params) {
    _pre_call_gl_callback("glPointParameterfvEXT", (GLADapiproc) glad_glPointParameterfvEXT, 2, pname, params);
    glad_glPointParameterfvEXT(pname, params);
    _post_call_gl_callback(NULL, "glPointParameterfvEXT", (GLADapiproc) glad_glPointParameterfvEXT, 2, pname, params);
    
}
PFNGLPOINTPARAMETERFVEXTPROC glad_debug_glPointParameterfvEXT = glad_debug_impl_glPointParameterfvEXT;
PFNGLPOINTPARAMETERFVSGISPROC glad_glPointParameterfvSGIS = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterfvSGIS(GLenum pname, const GLfloat * params) {
    _pre_call_gl_callback("glPointParameterfvSGIS", (GLADapiproc) glad_glPointParameterfvSGIS, 2, pname, params);
    glad_glPointParameterfvSGIS(pname, params);
    _post_call_gl_callback(NULL, "glPointParameterfvSGIS", (GLADapiproc) glad_glPointParameterfvSGIS, 2, pname, params);
    
}
PFNGLPOINTPARAMETERFVSGISPROC glad_debug_glPointParameterfvSGIS = glad_debug_impl_glPointParameterfvSGIS;
PFNGLPOINTPARAMETERIPROC glad_glPointParameteri = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameteri(GLenum pname, GLint param) {
    _pre_call_gl_callback("glPointParameteri", (GLADapiproc) glad_glPointParameteri, 2, pname, param);
    glad_glPointParameteri(pname, param);
    _post_call_gl_callback(NULL, "glPointParameteri", (GLADapiproc) glad_glPointParameteri, 2, pname, param);
    
}
PFNGLPOINTPARAMETERIPROC glad_debug_glPointParameteri = glad_debug_impl_glPointParameteri;
PFNGLPOINTPARAMETERINVPROC glad_glPointParameteriNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameteriNV(GLenum pname, GLint param) {
    _pre_call_gl_callback("glPointParameteriNV", (GLADapiproc) glad_glPointParameteriNV, 2, pname, param);
    glad_glPointParameteriNV(pname, param);
    _post_call_gl_callback(NULL, "glPointParameteriNV", (GLADapiproc) glad_glPointParameteriNV, 2, pname, param);
    
}
PFNGLPOINTPARAMETERINVPROC glad_debug_glPointParameteriNV = glad_debug_impl_glPointParameteriNV;
PFNGLPOINTPARAMETERIVPROC glad_glPointParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameteriv(GLenum pname, const GLint * params) {
    _pre_call_gl_callback("glPointParameteriv", (GLADapiproc) glad_glPointParameteriv, 2, pname, params);
    glad_glPointParameteriv(pname, params);
    _post_call_gl_callback(NULL, "glPointParameteriv", (GLADapiproc) glad_glPointParameteriv, 2, pname, params);
    
}
PFNGLPOINTPARAMETERIVPROC glad_debug_glPointParameteriv = glad_debug_impl_glPointParameteriv;
PFNGLPOINTPARAMETERIVNVPROC glad_glPointParameterivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterivNV(GLenum pname, const GLint * params) {
    _pre_call_gl_callback("glPointParameterivNV", (GLADapiproc) glad_glPointParameterivNV, 2, pname, params);
    glad_glPointParameterivNV(pname, params);
    _post_call_gl_callback(NULL, "glPointParameterivNV", (GLADapiproc) glad_glPointParameterivNV, 2, pname, params);
    
}
PFNGLPOINTPARAMETERIVNVPROC glad_debug_glPointParameterivNV = glad_debug_impl_glPointParameterivNV;
PFNGLPOINTSIZEPROC glad_glPointSize = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointSize(GLfloat size) {
    _pre_call_gl_callback("glPointSize", (GLADapiproc) glad_glPointSize, 1, size);
    glad_glPointSize(size);
    _post_call_gl_callback(NULL, "glPointSize", (GLADapiproc) glad_glPointSize, 1, size);
    
}
PFNGLPOINTSIZEPROC glad_debug_glPointSize = glad_debug_impl_glPointSize;
PFNGLPOLYGONMODEPROC glad_glPolygonMode = NULL;
static void GLAD_API_PTR glad_debug_impl_glPolygonMode(GLenum face, GLenum mode) {
    _pre_call_gl_callback("glPolygonMode", (GLADapiproc) glad_glPolygonMode, 2, face, mode);
    glad_glPolygonMode(face, mode);
    _post_call_gl_callback(NULL, "glPolygonMode", (GLADapiproc) glad_glPolygonMode, 2, face, mode);
    
}
PFNGLPOLYGONMODEPROC glad_debug_glPolygonMode = glad_debug_impl_glPolygonMode;
PFNGLPOLYGONOFFSETPROC glad_glPolygonOffset = NULL;
static void GLAD_API_PTR glad_debug_impl_glPolygonOffset(GLfloat factor, GLfloat units) {
    _pre_call_gl_callback("glPolygonOffset", (GLADapiproc) glad_glPolygonOffset, 2, factor, units);
    glad_glPolygonOffset(factor, units);
    _post_call_gl_callback(NULL, "glPolygonOffset", (GLADapiproc) glad_glPolygonOffset, 2, factor, units);
    
}
PFNGLPOLYGONOFFSETPROC glad_debug_glPolygonOffset = glad_debug_impl_glPolygonOffset;
PFNGLPRIMITIVERESTARTINDEXPROC glad_glPrimitiveRestartIndex = NULL;
static void GLAD_API_PTR glad_debug_impl_glPrimitiveRestartIndex(GLuint index) {
    _pre_call_gl_callback("glPrimitiveRestartIndex", (GLADapiproc) glad_glPrimitiveRestartIndex, 1, index);
    glad_glPrimitiveRestartIndex(index);
    _post_call_gl_callback(NULL, "glPrimitiveRestartIndex", (GLADapiproc) glad_glPrimitiveRestartIndex, 1, index);
    
}
PFNGLPRIMITIVERESTARTINDEXPROC glad_debug_glPrimitiveRestartIndex = glad_debug_impl_glPrimitiveRestartIndex;
PFNGLPRIORITIZETEXTURESEXTPROC glad_glPrioritizeTexturesEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glPrioritizeTexturesEXT(GLsizei n, const GLuint * textures, const GLclampf * priorities) {
    _pre_call_gl_callback("glPrioritizeTexturesEXT", (GLADapiproc) glad_glPrioritizeTexturesEXT, 3, n, textures, priorities);
    glad_glPrioritizeTexturesEXT(n, textures, priorities);
    _post_call_gl_callback(NULL, "glPrioritizeTexturesEXT", (GLADapiproc) glad_glPrioritizeTexturesEXT, 3, n, textures, priorities);
    
}
PFNGLPRIORITIZETEXTURESEXTPROC glad_debug_glPrioritizeTexturesEXT = glad_debug_impl_glPrioritizeTexturesEXT;
PFNGLPROGRAMENVPARAMETER4DARBPROC glad_glProgramEnvParameter4dARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramEnvParameter4dARB(GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    _pre_call_gl_callback("glProgramEnvParameter4dARB", (GLADapiproc) glad_glProgramEnvParameter4dARB, 6, target, index, x, y, z, w);
    glad_glProgramEnvParameter4dARB(target, index, x, y, z, w);
    _post_call_gl_callback(NULL, "glProgramEnvParameter4dARB", (GLADapiproc) glad_glProgramEnvParameter4dARB, 6, target, index, x, y, z, w);
    
}
PFNGLPROGRAMENVPARAMETER4DARBPROC glad_debug_glProgramEnvParameter4dARB = glad_debug_impl_glProgramEnvParameter4dARB;
PFNGLPROGRAMENVPARAMETER4DVARBPROC glad_glProgramEnvParameter4dvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramEnvParameter4dvARB(GLenum target, GLuint index, const GLdouble * params) {
    _pre_call_gl_callback("glProgramEnvParameter4dvARB", (GLADapiproc) glad_glProgramEnvParameter4dvARB, 3, target, index, params);
    glad_glProgramEnvParameter4dvARB(target, index, params);
    _post_call_gl_callback(NULL, "glProgramEnvParameter4dvARB", (GLADapiproc) glad_glProgramEnvParameter4dvARB, 3, target, index, params);
    
}
PFNGLPROGRAMENVPARAMETER4DVARBPROC glad_debug_glProgramEnvParameter4dvARB = glad_debug_impl_glProgramEnvParameter4dvARB;
PFNGLPROGRAMENVPARAMETER4FARBPROC glad_glProgramEnvParameter4fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramEnvParameter4fARB(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    _pre_call_gl_callback("glProgramEnvParameter4fARB", (GLADapiproc) glad_glProgramEnvParameter4fARB, 6, target, index, x, y, z, w);
    glad_glProgramEnvParameter4fARB(target, index, x, y, z, w);
    _post_call_gl_callback(NULL, "glProgramEnvParameter4fARB", (GLADapiproc) glad_glProgramEnvParameter4fARB, 6, target, index, x, y, z, w);
    
}
PFNGLPROGRAMENVPARAMETER4FARBPROC glad_debug_glProgramEnvParameter4fARB = glad_debug_impl_glProgramEnvParameter4fARB;
PFNGLPROGRAMENVPARAMETER4FVARBPROC glad_glProgramEnvParameter4fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramEnvParameter4fvARB(GLenum target, GLuint index, const GLfloat * params) {
    _pre_call_gl_callback("glProgramEnvParameter4fvARB", (GLADapiproc) glad_glProgramEnvParameter4fvARB, 3, target, index, params);
    glad_glProgramEnvParameter4fvARB(target, index, params);
    _post_call_gl_callback(NULL, "glProgramEnvParameter4fvARB", (GLADapiproc) glad_glProgramEnvParameter4fvARB, 3, target, index, params);
    
}
PFNGLPROGRAMENVPARAMETER4FVARBPROC glad_debug_glProgramEnvParameter4fvARB = glad_debug_impl_glProgramEnvParameter4fvARB;
PFNGLPROGRAMLOCALPARAMETER4DARBPROC glad_glProgramLocalParameter4dARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramLocalParameter4dARB(GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    _pre_call_gl_callback("glProgramLocalParameter4dARB", (GLADapiproc) glad_glProgramLocalParameter4dARB, 6, target, index, x, y, z, w);
    glad_glProgramLocalParameter4dARB(target, index, x, y, z, w);
    _post_call_gl_callback(NULL, "glProgramLocalParameter4dARB", (GLADapiproc) glad_glProgramLocalParameter4dARB, 6, target, index, x, y, z, w);
    
}
PFNGLPROGRAMLOCALPARAMETER4DARBPROC glad_debug_glProgramLocalParameter4dARB = glad_debug_impl_glProgramLocalParameter4dARB;
PFNGLPROGRAMLOCALPARAMETER4DVARBPROC glad_glProgramLocalParameter4dvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramLocalParameter4dvARB(GLenum target, GLuint index, const GLdouble * params) {
    _pre_call_gl_callback("glProgramLocalParameter4dvARB", (GLADapiproc) glad_glProgramLocalParameter4dvARB, 3, target, index, params);
    glad_glProgramLocalParameter4dvARB(target, index, params);
    _post_call_gl_callback(NULL, "glProgramLocalParameter4dvARB", (GLADapiproc) glad_glProgramLocalParameter4dvARB, 3, target, index, params);
    
}
PFNGLPROGRAMLOCALPARAMETER4DVARBPROC glad_debug_glProgramLocalParameter4dvARB = glad_debug_impl_glProgramLocalParameter4dvARB;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC glad_glProgramLocalParameter4fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramLocalParameter4fARB(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    _pre_call_gl_callback("glProgramLocalParameter4fARB", (GLADapiproc) glad_glProgramLocalParameter4fARB, 6, target, index, x, y, z, w);
    glad_glProgramLocalParameter4fARB(target, index, x, y, z, w);
    _post_call_gl_callback(NULL, "glProgramLocalParameter4fARB", (GLADapiproc) glad_glProgramLocalParameter4fARB, 6, target, index, x, y, z, w);
    
}
PFNGLPROGRAMLOCALPARAMETER4FARBPROC glad_debug_glProgramLocalParameter4fARB = glad_debug_impl_glProgramLocalParameter4fARB;
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glad_glProgramLocalParameter4fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramLocalParameter4fvARB(GLenum target, GLuint index, const GLfloat * params) {
    _pre_call_gl_callback("glProgramLocalParameter4fvARB", (GLADapiproc) glad_glProgramLocalParameter4fvARB, 3, target, index, params);
    glad_glProgramLocalParameter4fvARB(target, index, params);
    _post_call_gl_callback(NULL, "glProgramLocalParameter4fvARB", (GLADapiproc) glad_glProgramLocalParameter4fvARB, 3, target, index, params);
    
}
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glad_debug_glProgramLocalParameter4fvARB = glad_debug_impl_glProgramLocalParameter4fvARB;
PFNGLPROGRAMPARAMETER4DNVPROC glad_glProgramParameter4dNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramParameter4dNV(GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    _pre_call_gl_callback("glProgramParameter4dNV", (GLADapiproc) glad_glProgramParameter4dNV, 6, target, index, x, y, z, w);
    glad_glProgramParameter4dNV(target, index, x, y, z, w);
    _post_call_gl_callback(NULL, "glProgramParameter4dNV", (GLADapiproc) glad_glProgramParameter4dNV, 6, target, index, x, y, z, w);
    
}
PFNGLPROGRAMPARAMETER4DNVPROC glad_debug_glProgramParameter4dNV = glad_debug_impl_glProgramParameter4dNV;
PFNGLPROGRAMPARAMETER4DVNVPROC glad_glProgramParameter4dvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramParameter4dvNV(GLenum target, GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glProgramParameter4dvNV", (GLADapiproc) glad_glProgramParameter4dvNV, 3, target, index, v);
    glad_glProgramParameter4dvNV(target, index, v);
    _post_call_gl_callback(NULL, "glProgramParameter4dvNV", (GLADapiproc) glad_glProgramParameter4dvNV, 3, target, index, v);
    
}
PFNGLPROGRAMPARAMETER4DVNVPROC glad_debug_glProgramParameter4dvNV = glad_debug_impl_glProgramParameter4dvNV;
PFNGLPROGRAMPARAMETER4FNVPROC glad_glProgramParameter4fNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramParameter4fNV(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    _pre_call_gl_callback("glProgramParameter4fNV", (GLADapiproc) glad_glProgramParameter4fNV, 6, target, index, x, y, z, w);
    glad_glProgramParameter4fNV(target, index, x, y, z, w);
    _post_call_gl_callback(NULL, "glProgramParameter4fNV", (GLADapiproc) glad_glProgramParameter4fNV, 6, target, index, x, y, z, w);
    
}
PFNGLPROGRAMPARAMETER4FNVPROC glad_debug_glProgramParameter4fNV = glad_debug_impl_glProgramParameter4fNV;
PFNGLPROGRAMPARAMETER4FVNVPROC glad_glProgramParameter4fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramParameter4fvNV(GLenum target, GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glProgramParameter4fvNV", (GLADapiproc) glad_glProgramParameter4fvNV, 3, target, index, v);
    glad_glProgramParameter4fvNV(target, index, v);
    _post_call_gl_callback(NULL, "glProgramParameter4fvNV", (GLADapiproc) glad_glProgramParameter4fvNV, 3, target, index, v);
    
}
PFNGLPROGRAMPARAMETER4FVNVPROC glad_debug_glProgramParameter4fvNV = glad_debug_impl_glProgramParameter4fvNV;
PFNGLPROGRAMPARAMETERIARBPROC glad_glProgramParameteriARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramParameteriARB(GLuint program, GLenum pname, GLint value) {
    _pre_call_gl_callback("glProgramParameteriARB", (GLADapiproc) glad_glProgramParameteriARB, 3, program, pname, value);
    glad_glProgramParameteriARB(program, pname, value);
    _post_call_gl_callback(NULL, "glProgramParameteriARB", (GLADapiproc) glad_glProgramParameteriARB, 3, program, pname, value);
    
}
PFNGLPROGRAMPARAMETERIARBPROC glad_debug_glProgramParameteriARB = glad_debug_impl_glProgramParameteriARB;
PFNGLPROGRAMPARAMETERS4DVNVPROC glad_glProgramParameters4dvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramParameters4dvNV(GLenum target, GLuint index, GLsizei count, const GLdouble * v) {
    _pre_call_gl_callback("glProgramParameters4dvNV", (GLADapiproc) glad_glProgramParameters4dvNV, 4, target, index, count, v);
    glad_glProgramParameters4dvNV(target, index, count, v);
    _post_call_gl_callback(NULL, "glProgramParameters4dvNV", (GLADapiproc) glad_glProgramParameters4dvNV, 4, target, index, count, v);
    
}
PFNGLPROGRAMPARAMETERS4DVNVPROC glad_debug_glProgramParameters4dvNV = glad_debug_impl_glProgramParameters4dvNV;
PFNGLPROGRAMPARAMETERS4FVNVPROC glad_glProgramParameters4fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramParameters4fvNV(GLenum target, GLuint index, GLsizei count, const GLfloat * v) {
    _pre_call_gl_callback("glProgramParameters4fvNV", (GLADapiproc) glad_glProgramParameters4fvNV, 4, target, index, count, v);
    glad_glProgramParameters4fvNV(target, index, count, v);
    _post_call_gl_callback(NULL, "glProgramParameters4fvNV", (GLADapiproc) glad_glProgramParameters4fvNV, 4, target, index, count, v);
    
}
PFNGLPROGRAMPARAMETERS4FVNVPROC glad_debug_glProgramParameters4fvNV = glad_debug_impl_glProgramParameters4fvNV;
PFNGLPROGRAMSTRINGARBPROC glad_glProgramStringARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramStringARB(GLenum target, GLenum format, GLsizei len, const void * string) {
    _pre_call_gl_callback("glProgramStringARB", (GLADapiproc) glad_glProgramStringARB, 4, target, format, len, string);
    glad_glProgramStringARB(target, format, len, string);
    _post_call_gl_callback(NULL, "glProgramStringARB", (GLADapiproc) glad_glProgramStringARB, 4, target, format, len, string);
    
}
PFNGLPROGRAMSTRINGARBPROC glad_debug_glProgramStringARB = glad_debug_impl_glProgramStringARB;
PFNGLPROGRAMUNIFORM1DEXTPROC glad_glProgramUniform1dEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1dEXT(GLuint program, GLint location, GLdouble x) {
    _pre_call_gl_callback("glProgramUniform1dEXT", (GLADapiproc) glad_glProgramUniform1dEXT, 3, program, location, x);
    glad_glProgramUniform1dEXT(program, location, x);
    _post_call_gl_callback(NULL, "glProgramUniform1dEXT", (GLADapiproc) glad_glProgramUniform1dEXT, 3, program, location, x);
    
}
PFNGLPROGRAMUNIFORM1DEXTPROC glad_debug_glProgramUniform1dEXT = glad_debug_impl_glProgramUniform1dEXT;
PFNGLPROGRAMUNIFORM1DVEXTPROC glad_glProgramUniform1dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniform1dvEXT", (GLADapiproc) glad_glProgramUniform1dvEXT, 4, program, location, count, value);
    glad_glProgramUniform1dvEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform1dvEXT", (GLADapiproc) glad_glProgramUniform1dvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1DVEXTPROC glad_debug_glProgramUniform1dvEXT = glad_debug_impl_glProgramUniform1dvEXT;
PFNGLPROGRAMUNIFORM1FEXTPROC glad_glProgramUniform1fEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1fEXT(GLuint program, GLint location, GLfloat v0) {
    _pre_call_gl_callback("glProgramUniform1fEXT", (GLADapiproc) glad_glProgramUniform1fEXT, 3, program, location, v0);
    glad_glProgramUniform1fEXT(program, location, v0);
    _post_call_gl_callback(NULL, "glProgramUniform1fEXT", (GLADapiproc) glad_glProgramUniform1fEXT, 3, program, location, v0);
    
}
PFNGLPROGRAMUNIFORM1FEXTPROC glad_debug_glProgramUniform1fEXT = glad_debug_impl_glProgramUniform1fEXT;
PFNGLPROGRAMUNIFORM1FVEXTPROC glad_glProgramUniform1fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniform1fvEXT", (GLADapiproc) glad_glProgramUniform1fvEXT, 4, program, location, count, value);
    glad_glProgramUniform1fvEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform1fvEXT", (GLADapiproc) glad_glProgramUniform1fvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1FVEXTPROC glad_debug_glProgramUniform1fvEXT = glad_debug_impl_glProgramUniform1fvEXT;
PFNGLPROGRAMUNIFORM1IEXTPROC glad_glProgramUniform1iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1iEXT(GLuint program, GLint location, GLint v0) {
    _pre_call_gl_callback("glProgramUniform1iEXT", (GLADapiproc) glad_glProgramUniform1iEXT, 3, program, location, v0);
    glad_glProgramUniform1iEXT(program, location, v0);
    _post_call_gl_callback(NULL, "glProgramUniform1iEXT", (GLADapiproc) glad_glProgramUniform1iEXT, 3, program, location, v0);
    
}
PFNGLPROGRAMUNIFORM1IEXTPROC glad_debug_glProgramUniform1iEXT = glad_debug_impl_glProgramUniform1iEXT;
PFNGLPROGRAMUNIFORM1IVEXTPROC glad_glProgramUniform1ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1ivEXT(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glProgramUniform1ivEXT", (GLADapiproc) glad_glProgramUniform1ivEXT, 4, program, location, count, value);
    glad_glProgramUniform1ivEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform1ivEXT", (GLADapiproc) glad_glProgramUniform1ivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1IVEXTPROC glad_debug_glProgramUniform1ivEXT = glad_debug_impl_glProgramUniform1ivEXT;
PFNGLPROGRAMUNIFORM1UIEXTPROC glad_glProgramUniform1uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1uiEXT(GLuint program, GLint location, GLuint v0) {
    _pre_call_gl_callback("glProgramUniform1uiEXT", (GLADapiproc) glad_glProgramUniform1uiEXT, 3, program, location, v0);
    glad_glProgramUniform1uiEXT(program, location, v0);
    _post_call_gl_callback(NULL, "glProgramUniform1uiEXT", (GLADapiproc) glad_glProgramUniform1uiEXT, 3, program, location, v0);
    
}
PFNGLPROGRAMUNIFORM1UIEXTPROC glad_debug_glProgramUniform1uiEXT = glad_debug_impl_glProgramUniform1uiEXT;
PFNGLPROGRAMUNIFORM1UIVEXTPROC glad_glProgramUniform1uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1uivEXT(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glProgramUniform1uivEXT", (GLADapiproc) glad_glProgramUniform1uivEXT, 4, program, location, count, value);
    glad_glProgramUniform1uivEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform1uivEXT", (GLADapiproc) glad_glProgramUniform1uivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1UIVEXTPROC glad_debug_glProgramUniform1uivEXT = glad_debug_impl_glProgramUniform1uivEXT;
PFNGLPROGRAMUNIFORM2DEXTPROC glad_glProgramUniform2dEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2dEXT(GLuint program, GLint location, GLdouble x, GLdouble y) {
    _pre_call_gl_callback("glProgramUniform2dEXT", (GLADapiproc) glad_glProgramUniform2dEXT, 4, program, location, x, y);
    glad_glProgramUniform2dEXT(program, location, x, y);
    _post_call_gl_callback(NULL, "glProgramUniform2dEXT", (GLADapiproc) glad_glProgramUniform2dEXT, 4, program, location, x, y);
    
}
PFNGLPROGRAMUNIFORM2DEXTPROC glad_debug_glProgramUniform2dEXT = glad_debug_impl_glProgramUniform2dEXT;
PFNGLPROGRAMUNIFORM2DVEXTPROC glad_glProgramUniform2dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniform2dvEXT", (GLADapiproc) glad_glProgramUniform2dvEXT, 4, program, location, count, value);
    glad_glProgramUniform2dvEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform2dvEXT", (GLADapiproc) glad_glProgramUniform2dvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2DVEXTPROC glad_debug_glProgramUniform2dvEXT = glad_debug_impl_glProgramUniform2dvEXT;
PFNGLPROGRAMUNIFORM2FEXTPROC glad_glProgramUniform2fEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1) {
    _pre_call_gl_callback("glProgramUniform2fEXT", (GLADapiproc) glad_glProgramUniform2fEXT, 4, program, location, v0, v1);
    glad_glProgramUniform2fEXT(program, location, v0, v1);
    _post_call_gl_callback(NULL, "glProgramUniform2fEXT", (GLADapiproc) glad_glProgramUniform2fEXT, 4, program, location, v0, v1);
    
}
PFNGLPROGRAMUNIFORM2FEXTPROC glad_debug_glProgramUniform2fEXT = glad_debug_impl_glProgramUniform2fEXT;
PFNGLPROGRAMUNIFORM2FVEXTPROC glad_glProgramUniform2fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniform2fvEXT", (GLADapiproc) glad_glProgramUniform2fvEXT, 4, program, location, count, value);
    glad_glProgramUniform2fvEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform2fvEXT", (GLADapiproc) glad_glProgramUniform2fvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2FVEXTPROC glad_debug_glProgramUniform2fvEXT = glad_debug_impl_glProgramUniform2fvEXT;
PFNGLPROGRAMUNIFORM2IEXTPROC glad_glProgramUniform2iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2iEXT(GLuint program, GLint location, GLint v0, GLint v1) {
    _pre_call_gl_callback("glProgramUniform2iEXT", (GLADapiproc) glad_glProgramUniform2iEXT, 4, program, location, v0, v1);
    glad_glProgramUniform2iEXT(program, location, v0, v1);
    _post_call_gl_callback(NULL, "glProgramUniform2iEXT", (GLADapiproc) glad_glProgramUniform2iEXT, 4, program, location, v0, v1);
    
}
PFNGLPROGRAMUNIFORM2IEXTPROC glad_debug_glProgramUniform2iEXT = glad_debug_impl_glProgramUniform2iEXT;
PFNGLPROGRAMUNIFORM2IVEXTPROC glad_glProgramUniform2ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2ivEXT(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glProgramUniform2ivEXT", (GLADapiproc) glad_glProgramUniform2ivEXT, 4, program, location, count, value);
    glad_glProgramUniform2ivEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform2ivEXT", (GLADapiproc) glad_glProgramUniform2ivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2IVEXTPROC glad_debug_glProgramUniform2ivEXT = glad_debug_impl_glProgramUniform2ivEXT;
PFNGLPROGRAMUNIFORM2UIEXTPROC glad_glProgramUniform2uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1) {
    _pre_call_gl_callback("glProgramUniform2uiEXT", (GLADapiproc) glad_glProgramUniform2uiEXT, 4, program, location, v0, v1);
    glad_glProgramUniform2uiEXT(program, location, v0, v1);
    _post_call_gl_callback(NULL, "glProgramUniform2uiEXT", (GLADapiproc) glad_glProgramUniform2uiEXT, 4, program, location, v0, v1);
    
}
PFNGLPROGRAMUNIFORM2UIEXTPROC glad_debug_glProgramUniform2uiEXT = glad_debug_impl_glProgramUniform2uiEXT;
PFNGLPROGRAMUNIFORM2UIVEXTPROC glad_glProgramUniform2uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2uivEXT(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glProgramUniform2uivEXT", (GLADapiproc) glad_glProgramUniform2uivEXT, 4, program, location, count, value);
    glad_glProgramUniform2uivEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform2uivEXT", (GLADapiproc) glad_glProgramUniform2uivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2UIVEXTPROC glad_debug_glProgramUniform2uivEXT = glad_debug_impl_glProgramUniform2uivEXT;
PFNGLPROGRAMUNIFORM3DEXTPROC glad_glProgramUniform3dEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3dEXT(GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z) {
    _pre_call_gl_callback("glProgramUniform3dEXT", (GLADapiproc) glad_glProgramUniform3dEXT, 5, program, location, x, y, z);
    glad_glProgramUniform3dEXT(program, location, x, y, z);
    _post_call_gl_callback(NULL, "glProgramUniform3dEXT", (GLADapiproc) glad_glProgramUniform3dEXT, 5, program, location, x, y, z);
    
}
PFNGLPROGRAMUNIFORM3DEXTPROC glad_debug_glProgramUniform3dEXT = glad_debug_impl_glProgramUniform3dEXT;
PFNGLPROGRAMUNIFORM3DVEXTPROC glad_glProgramUniform3dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniform3dvEXT", (GLADapiproc) glad_glProgramUniform3dvEXT, 4, program, location, count, value);
    glad_glProgramUniform3dvEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform3dvEXT", (GLADapiproc) glad_glProgramUniform3dvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3DVEXTPROC glad_debug_glProgramUniform3dvEXT = glad_debug_impl_glProgramUniform3dvEXT;
PFNGLPROGRAMUNIFORM3FEXTPROC glad_glProgramUniform3fEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    _pre_call_gl_callback("glProgramUniform3fEXT", (GLADapiproc) glad_glProgramUniform3fEXT, 5, program, location, v0, v1, v2);
    glad_glProgramUniform3fEXT(program, location, v0, v1, v2);
    _post_call_gl_callback(NULL, "glProgramUniform3fEXT", (GLADapiproc) glad_glProgramUniform3fEXT, 5, program, location, v0, v1, v2);
    
}
PFNGLPROGRAMUNIFORM3FEXTPROC glad_debug_glProgramUniform3fEXT = glad_debug_impl_glProgramUniform3fEXT;
PFNGLPROGRAMUNIFORM3FVEXTPROC glad_glProgramUniform3fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniform3fvEXT", (GLADapiproc) glad_glProgramUniform3fvEXT, 4, program, location, count, value);
    glad_glProgramUniform3fvEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform3fvEXT", (GLADapiproc) glad_glProgramUniform3fvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3FVEXTPROC glad_debug_glProgramUniform3fvEXT = glad_debug_impl_glProgramUniform3fvEXT;
PFNGLPROGRAMUNIFORM3IEXTPROC glad_glProgramUniform3iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3iEXT(GLuint program, GLint location, GLint v0, GLint v1, GLint v2) {
    _pre_call_gl_callback("glProgramUniform3iEXT", (GLADapiproc) glad_glProgramUniform3iEXT, 5, program, location, v0, v1, v2);
    glad_glProgramUniform3iEXT(program, location, v0, v1, v2);
    _post_call_gl_callback(NULL, "glProgramUniform3iEXT", (GLADapiproc) glad_glProgramUniform3iEXT, 5, program, location, v0, v1, v2);
    
}
PFNGLPROGRAMUNIFORM3IEXTPROC glad_debug_glProgramUniform3iEXT = glad_debug_impl_glProgramUniform3iEXT;
PFNGLPROGRAMUNIFORM3IVEXTPROC glad_glProgramUniform3ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3ivEXT(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glProgramUniform3ivEXT", (GLADapiproc) glad_glProgramUniform3ivEXT, 4, program, location, count, value);
    glad_glProgramUniform3ivEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform3ivEXT", (GLADapiproc) glad_glProgramUniform3ivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3IVEXTPROC glad_debug_glProgramUniform3ivEXT = glad_debug_impl_glProgramUniform3ivEXT;
PFNGLPROGRAMUNIFORM3UIEXTPROC glad_glProgramUniform3uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2) {
    _pre_call_gl_callback("glProgramUniform3uiEXT", (GLADapiproc) glad_glProgramUniform3uiEXT, 5, program, location, v0, v1, v2);
    glad_glProgramUniform3uiEXT(program, location, v0, v1, v2);
    _post_call_gl_callback(NULL, "glProgramUniform3uiEXT", (GLADapiproc) glad_glProgramUniform3uiEXT, 5, program, location, v0, v1, v2);
    
}
PFNGLPROGRAMUNIFORM3UIEXTPROC glad_debug_glProgramUniform3uiEXT = glad_debug_impl_glProgramUniform3uiEXT;
PFNGLPROGRAMUNIFORM3UIVEXTPROC glad_glProgramUniform3uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3uivEXT(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glProgramUniform3uivEXT", (GLADapiproc) glad_glProgramUniform3uivEXT, 4, program, location, count, value);
    glad_glProgramUniform3uivEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform3uivEXT", (GLADapiproc) glad_glProgramUniform3uivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3UIVEXTPROC glad_debug_glProgramUniform3uivEXT = glad_debug_impl_glProgramUniform3uivEXT;
PFNGLPROGRAMUNIFORM4DEXTPROC glad_glProgramUniform4dEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4dEXT(GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    _pre_call_gl_callback("glProgramUniform4dEXT", (GLADapiproc) glad_glProgramUniform4dEXT, 6, program, location, x, y, z, w);
    glad_glProgramUniform4dEXT(program, location, x, y, z, w);
    _post_call_gl_callback(NULL, "glProgramUniform4dEXT", (GLADapiproc) glad_glProgramUniform4dEXT, 6, program, location, x, y, z, w);
    
}
PFNGLPROGRAMUNIFORM4DEXTPROC glad_debug_glProgramUniform4dEXT = glad_debug_impl_glProgramUniform4dEXT;
PFNGLPROGRAMUNIFORM4DVEXTPROC glad_glProgramUniform4dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniform4dvEXT", (GLADapiproc) glad_glProgramUniform4dvEXT, 4, program, location, count, value);
    glad_glProgramUniform4dvEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform4dvEXT", (GLADapiproc) glad_glProgramUniform4dvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4DVEXTPROC glad_debug_glProgramUniform4dvEXT = glad_debug_impl_glProgramUniform4dvEXT;
PFNGLPROGRAMUNIFORM4FEXTPROC glad_glProgramUniform4fEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    _pre_call_gl_callback("glProgramUniform4fEXT", (GLADapiproc) glad_glProgramUniform4fEXT, 6, program, location, v0, v1, v2, v3);
    glad_glProgramUniform4fEXT(program, location, v0, v1, v2, v3);
    _post_call_gl_callback(NULL, "glProgramUniform4fEXT", (GLADapiproc) glad_glProgramUniform4fEXT, 6, program, location, v0, v1, v2, v3);
    
}
PFNGLPROGRAMUNIFORM4FEXTPROC glad_debug_glProgramUniform4fEXT = glad_debug_impl_glProgramUniform4fEXT;
PFNGLPROGRAMUNIFORM4FVEXTPROC glad_glProgramUniform4fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniform4fvEXT", (GLADapiproc) glad_glProgramUniform4fvEXT, 4, program, location, count, value);
    glad_glProgramUniform4fvEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform4fvEXT", (GLADapiproc) glad_glProgramUniform4fvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4FVEXTPROC glad_debug_glProgramUniform4fvEXT = glad_debug_impl_glProgramUniform4fvEXT;
PFNGLPROGRAMUNIFORM4IEXTPROC glad_glProgramUniform4iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4iEXT(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    _pre_call_gl_callback("glProgramUniform4iEXT", (GLADapiproc) glad_glProgramUniform4iEXT, 6, program, location, v0, v1, v2, v3);
    glad_glProgramUniform4iEXT(program, location, v0, v1, v2, v3);
    _post_call_gl_callback(NULL, "glProgramUniform4iEXT", (GLADapiproc) glad_glProgramUniform4iEXT, 6, program, location, v0, v1, v2, v3);
    
}
PFNGLPROGRAMUNIFORM4IEXTPROC glad_debug_glProgramUniform4iEXT = glad_debug_impl_glProgramUniform4iEXT;
PFNGLPROGRAMUNIFORM4IVEXTPROC glad_glProgramUniform4ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4ivEXT(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glProgramUniform4ivEXT", (GLADapiproc) glad_glProgramUniform4ivEXT, 4, program, location, count, value);
    glad_glProgramUniform4ivEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform4ivEXT", (GLADapiproc) glad_glProgramUniform4ivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4IVEXTPROC glad_debug_glProgramUniform4ivEXT = glad_debug_impl_glProgramUniform4ivEXT;
PFNGLPROGRAMUNIFORM4UIEXTPROC glad_glProgramUniform4uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    _pre_call_gl_callback("glProgramUniform4uiEXT", (GLADapiproc) glad_glProgramUniform4uiEXT, 6, program, location, v0, v1, v2, v3);
    glad_glProgramUniform4uiEXT(program, location, v0, v1, v2, v3);
    _post_call_gl_callback(NULL, "glProgramUniform4uiEXT", (GLADapiproc) glad_glProgramUniform4uiEXT, 6, program, location, v0, v1, v2, v3);
    
}
PFNGLPROGRAMUNIFORM4UIEXTPROC glad_debug_glProgramUniform4uiEXT = glad_debug_impl_glProgramUniform4uiEXT;
PFNGLPROGRAMUNIFORM4UIVEXTPROC glad_glProgramUniform4uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4uivEXT(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glProgramUniform4uivEXT", (GLADapiproc) glad_glProgramUniform4uivEXT, 4, program, location, count, value);
    glad_glProgramUniform4uivEXT(program, location, count, value);
    _post_call_gl_callback(NULL, "glProgramUniform4uivEXT", (GLADapiproc) glad_glProgramUniform4uivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4UIVEXTPROC glad_debug_glProgramUniform4uivEXT = glad_debug_impl_glProgramUniform4uivEXT;
PFNGLPROGRAMUNIFORMMATRIX2DVEXTPROC glad_glProgramUniformMatrix2dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniformMatrix2dvEXT", (GLADapiproc) glad_glProgramUniformMatrix2dvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2dvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix2dvEXT", (GLADapiproc) glad_glProgramUniformMatrix2dvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2DVEXTPROC glad_debug_glProgramUniformMatrix2dvEXT = glad_debug_impl_glProgramUniformMatrix2dvEXT;
PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC glad_glProgramUniformMatrix2fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniformMatrix2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2fvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC glad_debug_glProgramUniformMatrix2fvEXT = glad_debug_impl_glProgramUniformMatrix2fvEXT;
PFNGLPROGRAMUNIFORMMATRIX2X3DVEXTPROC glad_glProgramUniformMatrix2x3dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2x3dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniformMatrix2x3dvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x3dvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2x3dvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix2x3dvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x3dvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X3DVEXTPROC glad_debug_glProgramUniformMatrix2x3dvEXT = glad_debug_impl_glProgramUniformMatrix2x3dvEXT;
PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC glad_glProgramUniformMatrix2x3fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2x3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniformMatrix2x3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x3fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2x3fvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix2x3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x3fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC glad_debug_glProgramUniformMatrix2x3fvEXT = glad_debug_impl_glProgramUniformMatrix2x3fvEXT;
PFNGLPROGRAMUNIFORMMATRIX2X4DVEXTPROC glad_glProgramUniformMatrix2x4dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2x4dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniformMatrix2x4dvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x4dvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2x4dvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix2x4dvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x4dvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X4DVEXTPROC glad_debug_glProgramUniformMatrix2x4dvEXT = glad_debug_impl_glProgramUniformMatrix2x4dvEXT;
PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC glad_glProgramUniformMatrix2x4fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2x4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniformMatrix2x4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x4fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2x4fvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix2x4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x4fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC glad_debug_glProgramUniformMatrix2x4fvEXT = glad_debug_impl_glProgramUniformMatrix2x4fvEXT;
PFNGLPROGRAMUNIFORMMATRIX3DVEXTPROC glad_glProgramUniformMatrix3dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniformMatrix3dvEXT", (GLADapiproc) glad_glProgramUniformMatrix3dvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3dvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix3dvEXT", (GLADapiproc) glad_glProgramUniformMatrix3dvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3DVEXTPROC glad_debug_glProgramUniformMatrix3dvEXT = glad_debug_impl_glProgramUniformMatrix3dvEXT;
PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC glad_glProgramUniformMatrix3fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniformMatrix3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3fvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC glad_debug_glProgramUniformMatrix3fvEXT = glad_debug_impl_glProgramUniformMatrix3fvEXT;
PFNGLPROGRAMUNIFORMMATRIX3X2DVEXTPROC glad_glProgramUniformMatrix3x2dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3x2dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniformMatrix3x2dvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x2dvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3x2dvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix3x2dvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x2dvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X2DVEXTPROC glad_debug_glProgramUniformMatrix3x2dvEXT = glad_debug_impl_glProgramUniformMatrix3x2dvEXT;
PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC glad_glProgramUniformMatrix3x2fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3x2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniformMatrix3x2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x2fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3x2fvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix3x2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x2fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC glad_debug_glProgramUniformMatrix3x2fvEXT = glad_debug_impl_glProgramUniformMatrix3x2fvEXT;
PFNGLPROGRAMUNIFORMMATRIX3X4DVEXTPROC glad_glProgramUniformMatrix3x4dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3x4dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniformMatrix3x4dvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x4dvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3x4dvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix3x4dvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x4dvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X4DVEXTPROC glad_debug_glProgramUniformMatrix3x4dvEXT = glad_debug_impl_glProgramUniformMatrix3x4dvEXT;
PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC glad_glProgramUniformMatrix3x4fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3x4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniformMatrix3x4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x4fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3x4fvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix3x4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x4fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC glad_debug_glProgramUniformMatrix3x4fvEXT = glad_debug_impl_glProgramUniformMatrix3x4fvEXT;
PFNGLPROGRAMUNIFORMMATRIX4DVEXTPROC glad_glProgramUniformMatrix4dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniformMatrix4dvEXT", (GLADapiproc) glad_glProgramUniformMatrix4dvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4dvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix4dvEXT", (GLADapiproc) glad_glProgramUniformMatrix4dvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4DVEXTPROC glad_debug_glProgramUniformMatrix4dvEXT = glad_debug_impl_glProgramUniformMatrix4dvEXT;
PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC glad_glProgramUniformMatrix4fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniformMatrix4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4fvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC glad_debug_glProgramUniformMatrix4fvEXT = glad_debug_impl_glProgramUniformMatrix4fvEXT;
PFNGLPROGRAMUNIFORMMATRIX4X2DVEXTPROC glad_glProgramUniformMatrix4x2dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4x2dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniformMatrix4x2dvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x2dvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4x2dvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix4x2dvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x2dvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X2DVEXTPROC glad_debug_glProgramUniformMatrix4x2dvEXT = glad_debug_impl_glProgramUniformMatrix4x2dvEXT;
PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC glad_glProgramUniformMatrix4x2fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4x2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniformMatrix4x2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x2fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4x2fvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix4x2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x2fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC glad_debug_glProgramUniformMatrix4x2fvEXT = glad_debug_impl_glProgramUniformMatrix4x2fvEXT;
PFNGLPROGRAMUNIFORMMATRIX4X3DVEXTPROC glad_glProgramUniformMatrix4x3dvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4x3dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble * value) {
    _pre_call_gl_callback("glProgramUniformMatrix4x3dvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x3dvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4x3dvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix4x3dvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x3dvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X3DVEXTPROC glad_debug_glProgramUniformMatrix4x3dvEXT = glad_debug_impl_glProgramUniformMatrix4x3dvEXT;
PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC glad_glProgramUniformMatrix4x3fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4x3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glProgramUniformMatrix4x3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x3fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4x3fvEXT(program, location, count, transpose, value);
    _post_call_gl_callback(NULL, "glProgramUniformMatrix4x3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x3fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC glad_debug_glProgramUniformMatrix4x3fvEXT = glad_debug_impl_glProgramUniformMatrix4x3fvEXT;
PFNGLPROGRAMVERTEXLIMITNVPROC glad_glProgramVertexLimitNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramVertexLimitNV(GLenum target, GLint limit) {
    _pre_call_gl_callback("glProgramVertexLimitNV", (GLADapiproc) glad_glProgramVertexLimitNV, 2, target, limit);
    glad_glProgramVertexLimitNV(target, limit);
    _post_call_gl_callback(NULL, "glProgramVertexLimitNV", (GLADapiproc) glad_glProgramVertexLimitNV, 2, target, limit);
    
}
PFNGLPROGRAMVERTEXLIMITNVPROC glad_debug_glProgramVertexLimitNV = glad_debug_impl_glProgramVertexLimitNV;
PFNGLPROVOKINGVERTEXPROC glad_glProvokingVertex = NULL;
static void GLAD_API_PTR glad_debug_impl_glProvokingVertex(GLenum mode) {
    _pre_call_gl_callback("glProvokingVertex", (GLADapiproc) glad_glProvokingVertex, 1, mode);
    glad_glProvokingVertex(mode);
    _post_call_gl_callback(NULL, "glProvokingVertex", (GLADapiproc) glad_glProvokingVertex, 1, mode);
    
}
PFNGLPROVOKINGVERTEXPROC glad_debug_glProvokingVertex = glad_debug_impl_glProvokingVertex;
PFNGLPROVOKINGVERTEXEXTPROC glad_glProvokingVertexEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProvokingVertexEXT(GLenum mode) {
    _pre_call_gl_callback("glProvokingVertexEXT", (GLADapiproc) glad_glProvokingVertexEXT, 1, mode);
    glad_glProvokingVertexEXT(mode);
    _post_call_gl_callback(NULL, "glProvokingVertexEXT", (GLADapiproc) glad_glProvokingVertexEXT, 1, mode);
    
}
PFNGLPROVOKINGVERTEXEXTPROC glad_debug_glProvokingVertexEXT = glad_debug_impl_glProvokingVertexEXT;
PFNGLPUSHCLIENTATTRIBDEFAULTEXTPROC glad_glPushClientAttribDefaultEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glPushClientAttribDefaultEXT(GLbitfield mask) {
    _pre_call_gl_callback("glPushClientAttribDefaultEXT", (GLADapiproc) glad_glPushClientAttribDefaultEXT, 1, mask);
    glad_glPushClientAttribDefaultEXT(mask);
    _post_call_gl_callback(NULL, "glPushClientAttribDefaultEXT", (GLADapiproc) glad_glPushClientAttribDefaultEXT, 1, mask);
    
}
PFNGLPUSHCLIENTATTRIBDEFAULTEXTPROC glad_debug_glPushClientAttribDefaultEXT = glad_debug_impl_glPushClientAttribDefaultEXT;
PFNGLQUERYCOUNTERPROC glad_glQueryCounter = NULL;
static void GLAD_API_PTR glad_debug_impl_glQueryCounter(GLuint id, GLenum target) {
    _pre_call_gl_callback("glQueryCounter", (GLADapiproc) glad_glQueryCounter, 2, id, target);
    glad_glQueryCounter(id, target);
    _post_call_gl_callback(NULL, "glQueryCounter", (GLADapiproc) glad_glQueryCounter, 2, id, target);
    
}
PFNGLQUERYCOUNTERPROC glad_debug_glQueryCounter = glad_debug_impl_glQueryCounter;
PFNGLREADBUFFERPROC glad_glReadBuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glReadBuffer(GLenum src) {
    _pre_call_gl_callback("glReadBuffer", (GLADapiproc) glad_glReadBuffer, 1, src);
    glad_glReadBuffer(src);
    _post_call_gl_callback(NULL, "glReadBuffer", (GLADapiproc) glad_glReadBuffer, 1, src);
    
}
PFNGLREADBUFFERPROC glad_debug_glReadBuffer = glad_debug_impl_glReadBuffer;
PFNGLREADPIXELSPROC glad_glReadPixels = NULL;
static void GLAD_API_PTR glad_debug_impl_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void * pixels) {
    _pre_call_gl_callback("glReadPixels", (GLADapiproc) glad_glReadPixels, 7, x, y, width, height, format, type, pixels);
    glad_glReadPixels(x, y, width, height, format, type, pixels);
    _post_call_gl_callback(NULL, "glReadPixels", (GLADapiproc) glad_glReadPixels, 7, x, y, width, height, format, type, pixels);
    
}
PFNGLREADPIXELSPROC glad_debug_glReadPixels = glad_debug_impl_glReadPixels;
PFNGLRENDERBUFFERSTORAGEPROC glad_glRenderbufferStorage = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glRenderbufferStorage", (GLADapiproc) glad_glRenderbufferStorage, 4, target, internalformat, width, height);
    glad_glRenderbufferStorage(target, internalformat, width, height);
    _post_call_gl_callback(NULL, "glRenderbufferStorage", (GLADapiproc) glad_glRenderbufferStorage, 4, target, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEPROC glad_debug_glRenderbufferStorage = glad_debug_impl_glRenderbufferStorage;
PFNGLRENDERBUFFERSTORAGEEXTPROC glad_glRenderbufferStorageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageEXT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glRenderbufferStorageEXT", (GLADapiproc) glad_glRenderbufferStorageEXT, 4, target, internalformat, width, height);
    glad_glRenderbufferStorageEXT(target, internalformat, width, height);
    _post_call_gl_callback(NULL, "glRenderbufferStorageEXT", (GLADapiproc) glad_glRenderbufferStorageEXT, 4, target, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEEXTPROC glad_debug_glRenderbufferStorageEXT = glad_debug_impl_glRenderbufferStorageEXT;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_glRenderbufferStorageMultisample = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glRenderbufferStorageMultisample", (GLADapiproc) glad_glRenderbufferStorageMultisample, 5, target, samples, internalformat, width, height);
    glad_glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
    _post_call_gl_callback(NULL, "glRenderbufferStorageMultisample", (GLADapiproc) glad_glRenderbufferStorageMultisample, 5, target, samples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_debug_glRenderbufferStorageMultisample = glad_debug_impl_glRenderbufferStorageMultisample;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glad_glRenderbufferStorageMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisampleEXT(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glRenderbufferStorageMultisampleEXT", (GLADapiproc) glad_glRenderbufferStorageMultisampleEXT, 5, target, samples, internalformat, width, height);
    glad_glRenderbufferStorageMultisampleEXT(target, samples, internalformat, width, height);
    _post_call_gl_callback(NULL, "glRenderbufferStorageMultisampleEXT", (GLADapiproc) glad_glRenderbufferStorageMultisampleEXT, 5, target, samples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glad_debug_glRenderbufferStorageMultisampleEXT = glad_debug_impl_glRenderbufferStorageMultisampleEXT;
PFNGLREQUESTRESIDENTPROGRAMSNVPROC glad_glRequestResidentProgramsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glRequestResidentProgramsNV(GLsizei n, const GLuint * programs) {
    _pre_call_gl_callback("glRequestResidentProgramsNV", (GLADapiproc) glad_glRequestResidentProgramsNV, 2, n, programs);
    glad_glRequestResidentProgramsNV(n, programs);
    _post_call_gl_callback(NULL, "glRequestResidentProgramsNV", (GLADapiproc) glad_glRequestResidentProgramsNV, 2, n, programs);
    
}
PFNGLREQUESTRESIDENTPROGRAMSNVPROC glad_debug_glRequestResidentProgramsNV = glad_debug_impl_glRequestResidentProgramsNV;
PFNGLSAMPLECOVERAGEPROC glad_glSampleCoverage = NULL;
static void GLAD_API_PTR glad_debug_impl_glSampleCoverage(GLfloat value, GLboolean invert) {
    _pre_call_gl_callback("glSampleCoverage", (GLADapiproc) glad_glSampleCoverage, 2, value, invert);
    glad_glSampleCoverage(value, invert);
    _post_call_gl_callback(NULL, "glSampleCoverage", (GLADapiproc) glad_glSampleCoverage, 2, value, invert);
    
}
PFNGLSAMPLECOVERAGEPROC glad_debug_glSampleCoverage = glad_debug_impl_glSampleCoverage;
PFNGLSAMPLECOVERAGEARBPROC glad_glSampleCoverageARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glSampleCoverageARB(GLfloat value, GLboolean invert) {
    _pre_call_gl_callback("glSampleCoverageARB", (GLADapiproc) glad_glSampleCoverageARB, 2, value, invert);
    glad_glSampleCoverageARB(value, invert);
    _post_call_gl_callback(NULL, "glSampleCoverageARB", (GLADapiproc) glad_glSampleCoverageARB, 2, value, invert);
    
}
PFNGLSAMPLECOVERAGEARBPROC glad_debug_glSampleCoverageARB = glad_debug_impl_glSampleCoverageARB;
PFNGLSAMPLEMASKINDEXEDNVPROC glad_glSampleMaskIndexedNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glSampleMaskIndexedNV(GLuint index, GLbitfield mask) {
    _pre_call_gl_callback("glSampleMaskIndexedNV", (GLADapiproc) glad_glSampleMaskIndexedNV, 2, index, mask);
    glad_glSampleMaskIndexedNV(index, mask);
    _post_call_gl_callback(NULL, "glSampleMaskIndexedNV", (GLADapiproc) glad_glSampleMaskIndexedNV, 2, index, mask);
    
}
PFNGLSAMPLEMASKINDEXEDNVPROC glad_debug_glSampleMaskIndexedNV = glad_debug_impl_glSampleMaskIndexedNV;
PFNGLSAMPLEMASKIPROC glad_glSampleMaski = NULL;
static void GLAD_API_PTR glad_debug_impl_glSampleMaski(GLuint maskNumber, GLbitfield mask) {
    _pre_call_gl_callback("glSampleMaski", (GLADapiproc) glad_glSampleMaski, 2, maskNumber, mask);
    glad_glSampleMaski(maskNumber, mask);
    _post_call_gl_callback(NULL, "glSampleMaski", (GLADapiproc) glad_glSampleMaski, 2, maskNumber, mask);
    
}
PFNGLSAMPLEMASKIPROC glad_debug_glSampleMaski = glad_debug_impl_glSampleMaski;
PFNGLSAMPLERPARAMETERIIVPROC glad_glSamplerParameterIiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint * param) {
    _pre_call_gl_callback("glSamplerParameterIiv", (GLADapiproc) glad_glSamplerParameterIiv, 3, sampler, pname, param);
    glad_glSamplerParameterIiv(sampler, pname, param);
    _post_call_gl_callback(NULL, "glSamplerParameterIiv", (GLADapiproc) glad_glSamplerParameterIiv, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIIVPROC glad_debug_glSamplerParameterIiv = glad_debug_impl_glSamplerParameterIiv;
PFNGLSAMPLERPARAMETERIUIVPROC glad_glSamplerParameterIuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint * param) {
    _pre_call_gl_callback("glSamplerParameterIuiv", (GLADapiproc) glad_glSamplerParameterIuiv, 3, sampler, pname, param);
    glad_glSamplerParameterIuiv(sampler, pname, param);
    _post_call_gl_callback(NULL, "glSamplerParameterIuiv", (GLADapiproc) glad_glSamplerParameterIuiv, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIUIVPROC glad_debug_glSamplerParameterIuiv = glad_debug_impl_glSamplerParameterIuiv;
PFNGLSAMPLERPARAMETERFPROC glad_glSamplerParameterf = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
    _pre_call_gl_callback("glSamplerParameterf", (GLADapiproc) glad_glSamplerParameterf, 3, sampler, pname, param);
    glad_glSamplerParameterf(sampler, pname, param);
    _post_call_gl_callback(NULL, "glSamplerParameterf", (GLADapiproc) glad_glSamplerParameterf, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERFPROC glad_debug_glSamplerParameterf = glad_debug_impl_glSamplerParameterf;
PFNGLSAMPLERPARAMETERFVPROC glad_glSamplerParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat * param) {
    _pre_call_gl_callback("glSamplerParameterfv", (GLADapiproc) glad_glSamplerParameterfv, 3, sampler, pname, param);
    glad_glSamplerParameterfv(sampler, pname, param);
    _post_call_gl_callback(NULL, "glSamplerParameterfv", (GLADapiproc) glad_glSamplerParameterfv, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERFVPROC glad_debug_glSamplerParameterfv = glad_debug_impl_glSamplerParameterfv;
PFNGLSAMPLERPARAMETERIPROC glad_glSamplerParameteri = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
    _pre_call_gl_callback("glSamplerParameteri", (GLADapiproc) glad_glSamplerParameteri, 3, sampler, pname, param);
    glad_glSamplerParameteri(sampler, pname, param);
    _post_call_gl_callback(NULL, "glSamplerParameteri", (GLADapiproc) glad_glSamplerParameteri, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIPROC glad_debug_glSamplerParameteri = glad_debug_impl_glSamplerParameteri;
PFNGLSAMPLERPARAMETERIVPROC glad_glSamplerParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint * param) {
    _pre_call_gl_callback("glSamplerParameteriv", (GLADapiproc) glad_glSamplerParameteriv, 3, sampler, pname, param);
    glad_glSamplerParameteriv(sampler, pname, param);
    _post_call_gl_callback(NULL, "glSamplerParameteriv", (GLADapiproc) glad_glSamplerParameteriv, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIVPROC glad_debug_glSamplerParameteriv = glad_debug_impl_glSamplerParameteriv;
PFNGLSCISSORPROC glad_glScissor = NULL;
static void GLAD_API_PTR glad_debug_impl_glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glScissor", (GLADapiproc) glad_glScissor, 4, x, y, width, height);
    glad_glScissor(x, y, width, height);
    _post_call_gl_callback(NULL, "glScissor", (GLADapiproc) glad_glScissor, 4, x, y, width, height);
    
}
PFNGLSCISSORPROC glad_debug_glScissor = glad_debug_impl_glScissor;
PFNGLSHADERSOURCEPROC glad_glShaderSource = NULL;
static void GLAD_API_PTR glad_debug_impl_glShaderSource(GLuint shader, GLsizei count, const GLchar *const* string, const GLint * length) {
    _pre_call_gl_callback("glShaderSource", (GLADapiproc) glad_glShaderSource, 4, shader, count, string, length);
    glad_glShaderSource(shader, count, string, length);
    _post_call_gl_callback(NULL, "glShaderSource", (GLADapiproc) glad_glShaderSource, 4, shader, count, string, length);
    
}
PFNGLSHADERSOURCEPROC glad_debug_glShaderSource = glad_debug_impl_glShaderSource;
PFNGLSHADERSOURCEARBPROC glad_glShaderSourceARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glShaderSourceARB(GLhandleARB shaderObj, GLsizei count, const GLcharARB ** string, const GLint * length) {
    _pre_call_gl_callback("glShaderSourceARB", (GLADapiproc) glad_glShaderSourceARB, 4, shaderObj, count, string, length);
    glad_glShaderSourceARB(shaderObj, count, string, length);
    _post_call_gl_callback(NULL, "glShaderSourceARB", (GLADapiproc) glad_glShaderSourceARB, 4, shaderObj, count, string, length);
    
}
PFNGLSHADERSOURCEARBPROC glad_debug_glShaderSourceARB = glad_debug_impl_glShaderSourceARB;
PFNGLSTENCILFUNCPROC glad_glStencilFunc = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    _pre_call_gl_callback("glStencilFunc", (GLADapiproc) glad_glStencilFunc, 3, func, ref, mask);
    glad_glStencilFunc(func, ref, mask);
    _post_call_gl_callback(NULL, "glStencilFunc", (GLADapiproc) glad_glStencilFunc, 3, func, ref, mask);
    
}
PFNGLSTENCILFUNCPROC glad_debug_glStencilFunc = glad_debug_impl_glStencilFunc;
PFNGLSTENCILFUNCSEPARATEPROC glad_glStencilFuncSeparate = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    _pre_call_gl_callback("glStencilFuncSeparate", (GLADapiproc) glad_glStencilFuncSeparate, 4, face, func, ref, mask);
    glad_glStencilFuncSeparate(face, func, ref, mask);
    _post_call_gl_callback(NULL, "glStencilFuncSeparate", (GLADapiproc) glad_glStencilFuncSeparate, 4, face, func, ref, mask);
    
}
PFNGLSTENCILFUNCSEPARATEPROC glad_debug_glStencilFuncSeparate = glad_debug_impl_glStencilFuncSeparate;
PFNGLSTENCILFUNCSEPARATEATIPROC glad_glStencilFuncSeparateATI = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilFuncSeparateATI(GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask) {
    _pre_call_gl_callback("glStencilFuncSeparateATI", (GLADapiproc) glad_glStencilFuncSeparateATI, 4, frontfunc, backfunc, ref, mask);
    glad_glStencilFuncSeparateATI(frontfunc, backfunc, ref, mask);
    _post_call_gl_callback(NULL, "glStencilFuncSeparateATI", (GLADapiproc) glad_glStencilFuncSeparateATI, 4, frontfunc, backfunc, ref, mask);
    
}
PFNGLSTENCILFUNCSEPARATEATIPROC glad_debug_glStencilFuncSeparateATI = glad_debug_impl_glStencilFuncSeparateATI;
PFNGLSTENCILMASKPROC glad_glStencilMask = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilMask(GLuint mask) {
    _pre_call_gl_callback("glStencilMask", (GLADapiproc) glad_glStencilMask, 1, mask);
    glad_glStencilMask(mask);
    _post_call_gl_callback(NULL, "glStencilMask", (GLADapiproc) glad_glStencilMask, 1, mask);
    
}
PFNGLSTENCILMASKPROC glad_debug_glStencilMask = glad_debug_impl_glStencilMask;
PFNGLSTENCILMASKSEPARATEPROC glad_glStencilMaskSeparate = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilMaskSeparate(GLenum face, GLuint mask) {
    _pre_call_gl_callback("glStencilMaskSeparate", (GLADapiproc) glad_glStencilMaskSeparate, 2, face, mask);
    glad_glStencilMaskSeparate(face, mask);
    _post_call_gl_callback(NULL, "glStencilMaskSeparate", (GLADapiproc) glad_glStencilMaskSeparate, 2, face, mask);
    
}
PFNGLSTENCILMASKSEPARATEPROC glad_debug_glStencilMaskSeparate = glad_debug_impl_glStencilMaskSeparate;
PFNGLSTENCILOPPROC glad_glStencilOp = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    _pre_call_gl_callback("glStencilOp", (GLADapiproc) glad_glStencilOp, 3, fail, zfail, zpass);
    glad_glStencilOp(fail, zfail, zpass);
    _post_call_gl_callback(NULL, "glStencilOp", (GLADapiproc) glad_glStencilOp, 3, fail, zfail, zpass);
    
}
PFNGLSTENCILOPPROC glad_debug_glStencilOp = glad_debug_impl_glStencilOp;
PFNGLSTENCILOPSEPARATEPROC glad_glStencilOpSeparate = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    _pre_call_gl_callback("glStencilOpSeparate", (GLADapiproc) glad_glStencilOpSeparate, 4, face, sfail, dpfail, dppass);
    glad_glStencilOpSeparate(face, sfail, dpfail, dppass);
    _post_call_gl_callback(NULL, "glStencilOpSeparate", (GLADapiproc) glad_glStencilOpSeparate, 4, face, sfail, dpfail, dppass);
    
}
PFNGLSTENCILOPSEPARATEPROC glad_debug_glStencilOpSeparate = glad_debug_impl_glStencilOpSeparate;
PFNGLSTENCILOPSEPARATEATIPROC glad_glStencilOpSeparateATI = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilOpSeparateATI(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    _pre_call_gl_callback("glStencilOpSeparateATI", (GLADapiproc) glad_glStencilOpSeparateATI, 4, face, sfail, dpfail, dppass);
    glad_glStencilOpSeparateATI(face, sfail, dpfail, dppass);
    _post_call_gl_callback(NULL, "glStencilOpSeparateATI", (GLADapiproc) glad_glStencilOpSeparateATI, 4, face, sfail, dpfail, dppass);
    
}
PFNGLSTENCILOPSEPARATEATIPROC glad_debug_glStencilOpSeparateATI = glad_debug_impl_glStencilOpSeparateATI;
PFNGLTEXBUFFERPROC glad_glTexBuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer) {
    _pre_call_gl_callback("glTexBuffer", (GLADapiproc) glad_glTexBuffer, 3, target, internalformat, buffer);
    glad_glTexBuffer(target, internalformat, buffer);
    _post_call_gl_callback(NULL, "glTexBuffer", (GLADapiproc) glad_glTexBuffer, 3, target, internalformat, buffer);
    
}
PFNGLTEXBUFFERPROC glad_debug_glTexBuffer = glad_debug_impl_glTexBuffer;
PFNGLTEXBUFFERARBPROC glad_glTexBufferARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexBufferARB(GLenum target, GLenum internalformat, GLuint buffer) {
    _pre_call_gl_callback("glTexBufferARB", (GLADapiproc) glad_glTexBufferARB, 3, target, internalformat, buffer);
    glad_glTexBufferARB(target, internalformat, buffer);
    _post_call_gl_callback(NULL, "glTexBufferARB", (GLADapiproc) glad_glTexBufferARB, 3, target, internalformat, buffer);
    
}
PFNGLTEXBUFFERARBPROC glad_debug_glTexBufferARB = glad_debug_impl_glTexBufferARB;
PFNGLTEXBUFFEREXTPROC glad_glTexBufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexBufferEXT(GLenum target, GLenum internalformat, GLuint buffer) {
    _pre_call_gl_callback("glTexBufferEXT", (GLADapiproc) glad_glTexBufferEXT, 3, target, internalformat, buffer);
    glad_glTexBufferEXT(target, internalformat, buffer);
    _post_call_gl_callback(NULL, "glTexBufferEXT", (GLADapiproc) glad_glTexBufferEXT, 3, target, internalformat, buffer);
    
}
PFNGLTEXBUFFEREXTPROC glad_debug_glTexBufferEXT = glad_debug_impl_glTexBufferEXT;
PFNGLTEXCOORDPOINTEREXTPROC glad_glTexCoordPointerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexCoordPointerEXT(GLint size, GLenum type, GLsizei stride, GLsizei count, const void * pointer) {
    _pre_call_gl_callback("glTexCoordPointerEXT", (GLADapiproc) glad_glTexCoordPointerEXT, 5, size, type, stride, count, pointer);
    glad_glTexCoordPointerEXT(size, type, stride, count, pointer);
    _post_call_gl_callback(NULL, "glTexCoordPointerEXT", (GLADapiproc) glad_glTexCoordPointerEXT, 5, size, type, stride, count, pointer);
    
}
PFNGLTEXCOORDPOINTEREXTPROC glad_debug_glTexCoordPointerEXT = glad_debug_impl_glTexCoordPointerEXT;
PFNGLTEXIMAGE1DPROC glad_glTexImage1D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTexImage1D", (GLADapiproc) glad_glTexImage1D, 8, target, level, internalformat, width, border, format, type, pixels);
    glad_glTexImage1D(target, level, internalformat, width, border, format, type, pixels);
    _post_call_gl_callback(NULL, "glTexImage1D", (GLADapiproc) glad_glTexImage1D, 8, target, level, internalformat, width, border, format, type, pixels);
    
}
PFNGLTEXIMAGE1DPROC glad_debug_glTexImage1D = glad_debug_impl_glTexImage1D;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTexImage2D", (GLADapiproc) glad_glTexImage2D, 9, target, level, internalformat, width, height, border, format, type, pixels);
    glad_glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    _post_call_gl_callback(NULL, "glTexImage2D", (GLADapiproc) glad_glTexImage2D, 9, target, level, internalformat, width, height, border, format, type, pixels);
    
}
PFNGLTEXIMAGE2DPROC glad_debug_glTexImage2D = glad_debug_impl_glTexImage2D;
PFNGLTEXIMAGE2DMULTISAMPLEPROC glad_glTexImage2DMultisample = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    _pre_call_gl_callback("glTexImage2DMultisample", (GLADapiproc) glad_glTexImage2DMultisample, 6, target, samples, internalformat, width, height, fixedsamplelocations);
    glad_glTexImage2DMultisample(target, samples, internalformat, width, height, fixedsamplelocations);
    _post_call_gl_callback(NULL, "glTexImage2DMultisample", (GLADapiproc) glad_glTexImage2DMultisample, 6, target, samples, internalformat, width, height, fixedsamplelocations);
    
}
PFNGLTEXIMAGE2DMULTISAMPLEPROC glad_debug_glTexImage2DMultisample = glad_debug_impl_glTexImage2DMultisample;
PFNGLTEXIMAGE3DPROC glad_glTexImage3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTexImage3D", (GLADapiproc) glad_glTexImage3D, 10, target, level, internalformat, width, height, depth, border, format, type, pixels);
    glad_glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
    _post_call_gl_callback(NULL, "glTexImage3D", (GLADapiproc) glad_glTexImage3D, 10, target, level, internalformat, width, height, depth, border, format, type, pixels);
    
}
PFNGLTEXIMAGE3DPROC glad_debug_glTexImage3D = glad_debug_impl_glTexImage3D;
PFNGLTEXIMAGE3DEXTPROC glad_glTexImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexImage3DEXT(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTexImage3DEXT", (GLADapiproc) glad_glTexImage3DEXT, 10, target, level, internalformat, width, height, depth, border, format, type, pixels);
    glad_glTexImage3DEXT(target, level, internalformat, width, height, depth, border, format, type, pixels);
    _post_call_gl_callback(NULL, "glTexImage3DEXT", (GLADapiproc) glad_glTexImage3DEXT, 10, target, level, internalformat, width, height, depth, border, format, type, pixels);
    
}
PFNGLTEXIMAGE3DEXTPROC glad_debug_glTexImage3DEXT = glad_debug_impl_glTexImage3DEXT;
PFNGLTEXIMAGE3DMULTISAMPLEPROC glad_glTexImage3DMultisample = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    _pre_call_gl_callback("glTexImage3DMultisample", (GLADapiproc) glad_glTexImage3DMultisample, 7, target, samples, internalformat, width, height, depth, fixedsamplelocations);
    glad_glTexImage3DMultisample(target, samples, internalformat, width, height, depth, fixedsamplelocations);
    _post_call_gl_callback(NULL, "glTexImage3DMultisample", (GLADapiproc) glad_glTexImage3DMultisample, 7, target, samples, internalformat, width, height, depth, fixedsamplelocations);
    
}
PFNGLTEXIMAGE3DMULTISAMPLEPROC glad_debug_glTexImage3DMultisample = glad_debug_impl_glTexImage3DMultisample;
PFNGLTEXPARAMETERIIVPROC glad_glTexParameterIiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterIiv(GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gl_callback("glTexParameterIiv", (GLADapiproc) glad_glTexParameterIiv, 3, target, pname, params);
    glad_glTexParameterIiv(target, pname, params);
    _post_call_gl_callback(NULL, "glTexParameterIiv", (GLADapiproc) glad_glTexParameterIiv, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIIVPROC glad_debug_glTexParameterIiv = glad_debug_impl_glTexParameterIiv;
PFNGLTEXPARAMETERIIVEXTPROC glad_glTexParameterIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterIivEXT(GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gl_callback("glTexParameterIivEXT", (GLADapiproc) glad_glTexParameterIivEXT, 3, target, pname, params);
    glad_glTexParameterIivEXT(target, pname, params);
    _post_call_gl_callback(NULL, "glTexParameterIivEXT", (GLADapiproc) glad_glTexParameterIivEXT, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIIVEXTPROC glad_debug_glTexParameterIivEXT = glad_debug_impl_glTexParameterIivEXT;
PFNGLTEXPARAMETERIUIVPROC glad_glTexParameterIuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterIuiv(GLenum target, GLenum pname, const GLuint * params) {
    _pre_call_gl_callback("glTexParameterIuiv", (GLADapiproc) glad_glTexParameterIuiv, 3, target, pname, params);
    glad_glTexParameterIuiv(target, pname, params);
    _post_call_gl_callback(NULL, "glTexParameterIuiv", (GLADapiproc) glad_glTexParameterIuiv, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIUIVPROC glad_debug_glTexParameterIuiv = glad_debug_impl_glTexParameterIuiv;
PFNGLTEXPARAMETERIUIVEXTPROC glad_glTexParameterIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterIuivEXT(GLenum target, GLenum pname, const GLuint * params) {
    _pre_call_gl_callback("glTexParameterIuivEXT", (GLADapiproc) glad_glTexParameterIuivEXT, 3, target, pname, params);
    glad_glTexParameterIuivEXT(target, pname, params);
    _post_call_gl_callback(NULL, "glTexParameterIuivEXT", (GLADapiproc) glad_glTexParameterIuivEXT, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIUIVEXTPROC glad_debug_glTexParameterIuivEXT = glad_debug_impl_glTexParameterIuivEXT;
PFNGLTEXPARAMETERFPROC glad_glTexParameterf = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    _pre_call_gl_callback("glTexParameterf", (GLADapiproc) glad_glTexParameterf, 3, target, pname, param);
    glad_glTexParameterf(target, pname, param);
    _post_call_gl_callback(NULL, "glTexParameterf", (GLADapiproc) glad_glTexParameterf, 3, target, pname, param);
    
}
PFNGLTEXPARAMETERFPROC glad_debug_glTexParameterf = glad_debug_impl_glTexParameterf;
PFNGLTEXPARAMETERFVPROC glad_glTexParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterfv(GLenum target, GLenum pname, const GLfloat * params) {
    _pre_call_gl_callback("glTexParameterfv", (GLADapiproc) glad_glTexParameterfv, 3, target, pname, params);
    glad_glTexParameterfv(target, pname, params);
    _post_call_gl_callback(NULL, "glTexParameterfv", (GLADapiproc) glad_glTexParameterfv, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERFVPROC glad_debug_glTexParameterfv = glad_debug_impl_glTexParameterfv;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    _pre_call_gl_callback("glTexParameteri", (GLADapiproc) glad_glTexParameteri, 3, target, pname, param);
    glad_glTexParameteri(target, pname, param);
    _post_call_gl_callback(NULL, "glTexParameteri", (GLADapiproc) glad_glTexParameteri, 3, target, pname, param);
    
}
PFNGLTEXPARAMETERIPROC glad_debug_glTexParameteri = glad_debug_impl_glTexParameteri;
PFNGLTEXPARAMETERIVPROC glad_glTexParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameteriv(GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gl_callback("glTexParameteriv", (GLADapiproc) glad_glTexParameteriv, 3, target, pname, params);
    glad_glTexParameteriv(target, pname, params);
    _post_call_gl_callback(NULL, "glTexParameteriv", (GLADapiproc) glad_glTexParameteriv, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIVPROC glad_debug_glTexParameteriv = glad_debug_impl_glTexParameteriv;
PFNGLTEXRENDERBUFFERNVPROC glad_glTexRenderbufferNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexRenderbufferNV(GLenum target, GLuint renderbuffer) {
    _pre_call_gl_callback("glTexRenderbufferNV", (GLADapiproc) glad_glTexRenderbufferNV, 2, target, renderbuffer);
    glad_glTexRenderbufferNV(target, renderbuffer);
    _post_call_gl_callback(NULL, "glTexRenderbufferNV", (GLADapiproc) glad_glTexRenderbufferNV, 2, target, renderbuffer);
    
}
PFNGLTEXRENDERBUFFERNVPROC glad_debug_glTexRenderbufferNV = glad_debug_impl_glTexRenderbufferNV;
PFNGLTEXSUBIMAGE1DPROC glad_glTexSubImage1D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTexSubImage1D", (GLADapiproc) glad_glTexSubImage1D, 7, target, level, xoffset, width, format, type, pixels);
    glad_glTexSubImage1D(target, level, xoffset, width, format, type, pixels);
    _post_call_gl_callback(NULL, "glTexSubImage1D", (GLADapiproc) glad_glTexSubImage1D, 7, target, level, xoffset, width, format, type, pixels);
    
}
PFNGLTEXSUBIMAGE1DPROC glad_debug_glTexSubImage1D = glad_debug_impl_glTexSubImage1D;
PFNGLTEXSUBIMAGE1DEXTPROC glad_glTexSubImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexSubImage1DEXT(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTexSubImage1DEXT", (GLADapiproc) glad_glTexSubImage1DEXT, 7, target, level, xoffset, width, format, type, pixels);
    glad_glTexSubImage1DEXT(target, level, xoffset, width, format, type, pixels);
    _post_call_gl_callback(NULL, "glTexSubImage1DEXT", (GLADapiproc) glad_glTexSubImage1DEXT, 7, target, level, xoffset, width, format, type, pixels);
    
}
PFNGLTEXSUBIMAGE1DEXTPROC glad_debug_glTexSubImage1DEXT = glad_debug_impl_glTexSubImage1DEXT;
PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTexSubImage2D", (GLADapiproc) glad_glTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, type, pixels);
    glad_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    _post_call_gl_callback(NULL, "glTexSubImage2D", (GLADapiproc) glad_glTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, type, pixels);
    
}
PFNGLTEXSUBIMAGE2DPROC glad_debug_glTexSubImage2D = glad_debug_impl_glTexSubImage2D;
PFNGLTEXSUBIMAGE2DEXTPROC glad_glTexSubImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexSubImage2DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTexSubImage2DEXT", (GLADapiproc) glad_glTexSubImage2DEXT, 9, target, level, xoffset, yoffset, width, height, format, type, pixels);
    glad_glTexSubImage2DEXT(target, level, xoffset, yoffset, width, height, format, type, pixels);
    _post_call_gl_callback(NULL, "glTexSubImage2DEXT", (GLADapiproc) glad_glTexSubImage2DEXT, 9, target, level, xoffset, yoffset, width, height, format, type, pixels);
    
}
PFNGLTEXSUBIMAGE2DEXTPROC glad_debug_glTexSubImage2DEXT = glad_debug_impl_glTexSubImage2DEXT;
PFNGLTEXSUBIMAGE3DPROC glad_glTexSubImage3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTexSubImage3D", (GLADapiproc) glad_glTexSubImage3D, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    glad_glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    _post_call_gl_callback(NULL, "glTexSubImage3D", (GLADapiproc) glad_glTexSubImage3D, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    
}
PFNGLTEXSUBIMAGE3DPROC glad_debug_glTexSubImage3D = glad_debug_impl_glTexSubImage3D;
PFNGLTEXSUBIMAGE3DEXTPROC glad_glTexSubImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexSubImage3DEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTexSubImage3DEXT", (GLADapiproc) glad_glTexSubImage3DEXT, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    glad_glTexSubImage3DEXT(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    _post_call_gl_callback(NULL, "glTexSubImage3DEXT", (GLADapiproc) glad_glTexSubImage3DEXT, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    
}
PFNGLTEXSUBIMAGE3DEXTPROC glad_debug_glTexSubImage3DEXT = glad_debug_impl_glTexSubImage3DEXT;
PFNGLTEXTUREBUFFEREXTPROC glad_glTextureBufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureBufferEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer) {
    _pre_call_gl_callback("glTextureBufferEXT", (GLADapiproc) glad_glTextureBufferEXT, 4, texture, target, internalformat, buffer);
    glad_glTextureBufferEXT(texture, target, internalformat, buffer);
    _post_call_gl_callback(NULL, "glTextureBufferEXT", (GLADapiproc) glad_glTextureBufferEXT, 4, texture, target, internalformat, buffer);
    
}
PFNGLTEXTUREBUFFEREXTPROC glad_debug_glTextureBufferEXT = glad_debug_impl_glTextureBufferEXT;
PFNGLTEXTUREBUFFERRANGEEXTPROC glad_glTextureBufferRangeEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    _pre_call_gl_callback("glTextureBufferRangeEXT", (GLADapiproc) glad_glTextureBufferRangeEXT, 6, texture, target, internalformat, buffer, offset, size);
    glad_glTextureBufferRangeEXT(texture, target, internalformat, buffer, offset, size);
    _post_call_gl_callback(NULL, "glTextureBufferRangeEXT", (GLADapiproc) glad_glTextureBufferRangeEXT, 6, texture, target, internalformat, buffer, offset, size);
    
}
PFNGLTEXTUREBUFFERRANGEEXTPROC glad_debug_glTextureBufferRangeEXT = glad_debug_impl_glTextureBufferRangeEXT;
PFNGLTEXTUREIMAGE1DEXTPROC glad_glTextureImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTextureImage1DEXT", (GLADapiproc) glad_glTextureImage1DEXT, 9, texture, target, level, internalformat, width, border, format, type, pixels);
    glad_glTextureImage1DEXT(texture, target, level, internalformat, width, border, format, type, pixels);
    _post_call_gl_callback(NULL, "glTextureImage1DEXT", (GLADapiproc) glad_glTextureImage1DEXT, 9, texture, target, level, internalformat, width, border, format, type, pixels);
    
}
PFNGLTEXTUREIMAGE1DEXTPROC glad_debug_glTextureImage1DEXT = glad_debug_impl_glTextureImage1DEXT;
PFNGLTEXTUREIMAGE2DEXTPROC glad_glTextureImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTextureImage2DEXT", (GLADapiproc) glad_glTextureImage2DEXT, 10, texture, target, level, internalformat, width, height, border, format, type, pixels);
    glad_glTextureImage2DEXT(texture, target, level, internalformat, width, height, border, format, type, pixels);
    _post_call_gl_callback(NULL, "glTextureImage2DEXT", (GLADapiproc) glad_glTextureImage2DEXT, 10, texture, target, level, internalformat, width, height, border, format, type, pixels);
    
}
PFNGLTEXTUREIMAGE2DEXTPROC glad_debug_glTextureImage2DEXT = glad_debug_impl_glTextureImage2DEXT;
PFNGLTEXTUREIMAGE3DEXTPROC glad_glTextureImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTextureImage3DEXT", (GLADapiproc) glad_glTextureImage3DEXT, 11, texture, target, level, internalformat, width, height, depth, border, format, type, pixels);
    glad_glTextureImage3DEXT(texture, target, level, internalformat, width, height, depth, border, format, type, pixels);
    _post_call_gl_callback(NULL, "glTextureImage3DEXT", (GLADapiproc) glad_glTextureImage3DEXT, 11, texture, target, level, internalformat, width, height, depth, border, format, type, pixels);
    
}
PFNGLTEXTUREIMAGE3DEXTPROC glad_debug_glTextureImage3DEXT = glad_debug_impl_glTextureImage3DEXT;
PFNGLTEXTUREPAGECOMMITMENTEXTPROC glad_glTexturePageCommitmentEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexturePageCommitmentEXT(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit) {
    _pre_call_gl_callback("glTexturePageCommitmentEXT", (GLADapiproc) glad_glTexturePageCommitmentEXT, 9, texture, level, xoffset, yoffset, zoffset, width, height, depth, commit);
    glad_glTexturePageCommitmentEXT(texture, level, xoffset, yoffset, zoffset, width, height, depth, commit);
    _post_call_gl_callback(NULL, "glTexturePageCommitmentEXT", (GLADapiproc) glad_glTexturePageCommitmentEXT, 9, texture, level, xoffset, yoffset, zoffset, width, height, depth, commit);
    
}
PFNGLTEXTUREPAGECOMMITMENTEXTPROC glad_debug_glTexturePageCommitmentEXT = glad_debug_impl_glTexturePageCommitmentEXT;
PFNGLTEXTUREPARAMETERIIVEXTPROC glad_glTextureParameterIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gl_callback("glTextureParameterIivEXT", (GLADapiproc) glad_glTextureParameterIivEXT, 4, texture, target, pname, params);
    glad_glTextureParameterIivEXT(texture, target, pname, params);
    _post_call_gl_callback(NULL, "glTextureParameterIivEXT", (GLADapiproc) glad_glTextureParameterIivEXT, 4, texture, target, pname, params);
    
}
PFNGLTEXTUREPARAMETERIIVEXTPROC glad_debug_glTextureParameterIivEXT = glad_debug_impl_glTextureParameterIivEXT;
PFNGLTEXTUREPARAMETERIUIVEXTPROC glad_glTextureParameterIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname, const GLuint * params) {
    _pre_call_gl_callback("glTextureParameterIuivEXT", (GLADapiproc) glad_glTextureParameterIuivEXT, 4, texture, target, pname, params);
    glad_glTextureParameterIuivEXT(texture, target, pname, params);
    _post_call_gl_callback(NULL, "glTextureParameterIuivEXT", (GLADapiproc) glad_glTextureParameterIuivEXT, 4, texture, target, pname, params);
    
}
PFNGLTEXTUREPARAMETERIUIVEXTPROC glad_debug_glTextureParameterIuivEXT = glad_debug_impl_glTextureParameterIuivEXT;
PFNGLTEXTUREPARAMETERFEXTPROC glad_glTextureParameterfEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param) {
    _pre_call_gl_callback("glTextureParameterfEXT", (GLADapiproc) glad_glTextureParameterfEXT, 4, texture, target, pname, param);
    glad_glTextureParameterfEXT(texture, target, pname, param);
    _post_call_gl_callback(NULL, "glTextureParameterfEXT", (GLADapiproc) glad_glTextureParameterfEXT, 4, texture, target, pname, param);
    
}
PFNGLTEXTUREPARAMETERFEXTPROC glad_debug_glTextureParameterfEXT = glad_debug_impl_glTextureParameterfEXT;
PFNGLTEXTUREPARAMETERFVEXTPROC glad_glTextureParameterfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, const GLfloat * params) {
    _pre_call_gl_callback("glTextureParameterfvEXT", (GLADapiproc) glad_glTextureParameterfvEXT, 4, texture, target, pname, params);
    glad_glTextureParameterfvEXT(texture, target, pname, params);
    _post_call_gl_callback(NULL, "glTextureParameterfvEXT", (GLADapiproc) glad_glTextureParameterfvEXT, 4, texture, target, pname, params);
    
}
PFNGLTEXTUREPARAMETERFVEXTPROC glad_debug_glTextureParameterfvEXT = glad_debug_impl_glTextureParameterfvEXT;
PFNGLTEXTUREPARAMETERIEXTPROC glad_glTextureParameteriEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param) {
    _pre_call_gl_callback("glTextureParameteriEXT", (GLADapiproc) glad_glTextureParameteriEXT, 4, texture, target, pname, param);
    glad_glTextureParameteriEXT(texture, target, pname, param);
    _post_call_gl_callback(NULL, "glTextureParameteriEXT", (GLADapiproc) glad_glTextureParameteriEXT, 4, texture, target, pname, param);
    
}
PFNGLTEXTUREPARAMETERIEXTPROC glad_debug_glTextureParameteriEXT = glad_debug_impl_glTextureParameteriEXT;
PFNGLTEXTUREPARAMETERIVEXTPROC glad_glTextureParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gl_callback("glTextureParameterivEXT", (GLADapiproc) glad_glTextureParameterivEXT, 4, texture, target, pname, params);
    glad_glTextureParameterivEXT(texture, target, pname, params);
    _post_call_gl_callback(NULL, "glTextureParameterivEXT", (GLADapiproc) glad_glTextureParameterivEXT, 4, texture, target, pname, params);
    
}
PFNGLTEXTUREPARAMETERIVEXTPROC glad_debug_glTextureParameterivEXT = glad_debug_impl_glTextureParameterivEXT;
PFNGLTEXTURERENDERBUFFEREXTPROC glad_glTextureRenderbufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureRenderbufferEXT(GLuint texture, GLenum target, GLuint renderbuffer) {
    _pre_call_gl_callback("glTextureRenderbufferEXT", (GLADapiproc) glad_glTextureRenderbufferEXT, 3, texture, target, renderbuffer);
    glad_glTextureRenderbufferEXT(texture, target, renderbuffer);
    _post_call_gl_callback(NULL, "glTextureRenderbufferEXT", (GLADapiproc) glad_glTextureRenderbufferEXT, 3, texture, target, renderbuffer);
    
}
PFNGLTEXTURERENDERBUFFEREXTPROC glad_debug_glTextureRenderbufferEXT = glad_debug_impl_glTextureRenderbufferEXT;
PFNGLTEXTURESTORAGE1DEXTPROC glad_glTextureStorage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
    _pre_call_gl_callback("glTextureStorage1DEXT", (GLADapiproc) glad_glTextureStorage1DEXT, 5, texture, target, levels, internalformat, width);
    glad_glTextureStorage1DEXT(texture, target, levels, internalformat, width);
    _post_call_gl_callback(NULL, "glTextureStorage1DEXT", (GLADapiproc) glad_glTextureStorage1DEXT, 5, texture, target, levels, internalformat, width);
    
}
PFNGLTEXTURESTORAGE1DEXTPROC glad_debug_glTextureStorage1DEXT = glad_debug_impl_glTextureStorage1DEXT;
PFNGLTEXTURESTORAGE2DEXTPROC glad_glTextureStorage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glTextureStorage2DEXT", (GLADapiproc) glad_glTextureStorage2DEXT, 6, texture, target, levels, internalformat, width, height);
    glad_glTextureStorage2DEXT(texture, target, levels, internalformat, width, height);
    _post_call_gl_callback(NULL, "glTextureStorage2DEXT", (GLADapiproc) glad_glTextureStorage2DEXT, 6, texture, target, levels, internalformat, width, height);
    
}
PFNGLTEXTURESTORAGE2DEXTPROC glad_debug_glTextureStorage2DEXT = glad_debug_impl_glTextureStorage2DEXT;
PFNGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC glad_glTextureStorage2DMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorage2DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    _pre_call_gl_callback("glTextureStorage2DMultisampleEXT", (GLADapiproc) glad_glTextureStorage2DMultisampleEXT, 7, texture, target, samples, internalformat, width, height, fixedsamplelocations);
    glad_glTextureStorage2DMultisampleEXT(texture, target, samples, internalformat, width, height, fixedsamplelocations);
    _post_call_gl_callback(NULL, "glTextureStorage2DMultisampleEXT", (GLADapiproc) glad_glTextureStorage2DMultisampleEXT, 7, texture, target, samples, internalformat, width, height, fixedsamplelocations);
    
}
PFNGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC glad_debug_glTextureStorage2DMultisampleEXT = glad_debug_impl_glTextureStorage2DMultisampleEXT;
PFNGLTEXTURESTORAGE3DEXTPROC glad_glTextureStorage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    _pre_call_gl_callback("glTextureStorage3DEXT", (GLADapiproc) glad_glTextureStorage3DEXT, 7, texture, target, levels, internalformat, width, height, depth);
    glad_glTextureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);
    _post_call_gl_callback(NULL, "glTextureStorage3DEXT", (GLADapiproc) glad_glTextureStorage3DEXT, 7, texture, target, levels, internalformat, width, height, depth);
    
}
PFNGLTEXTURESTORAGE3DEXTPROC glad_debug_glTextureStorage3DEXT = glad_debug_impl_glTextureStorage3DEXT;
PFNGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC glad_glTextureStorage3DMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorage3DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    _pre_call_gl_callback("glTextureStorage3DMultisampleEXT", (GLADapiproc) glad_glTextureStorage3DMultisampleEXT, 8, texture, target, samples, internalformat, width, height, depth, fixedsamplelocations);
    glad_glTextureStorage3DMultisampleEXT(texture, target, samples, internalformat, width, height, depth, fixedsamplelocations);
    _post_call_gl_callback(NULL, "glTextureStorage3DMultisampleEXT", (GLADapiproc) glad_glTextureStorage3DMultisampleEXT, 8, texture, target, samples, internalformat, width, height, depth, fixedsamplelocations);
    
}
PFNGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC glad_debug_glTextureStorage3DMultisampleEXT = glad_debug_impl_glTextureStorage3DMultisampleEXT;
PFNGLTEXTURESUBIMAGE1DEXTPROC glad_glTextureSubImage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTextureSubImage1DEXT", (GLADapiproc) glad_glTextureSubImage1DEXT, 8, texture, target, level, xoffset, width, format, type, pixels);
    glad_glTextureSubImage1DEXT(texture, target, level, xoffset, width, format, type, pixels);
    _post_call_gl_callback(NULL, "glTextureSubImage1DEXT", (GLADapiproc) glad_glTextureSubImage1DEXT, 8, texture, target, level, xoffset, width, format, type, pixels);
    
}
PFNGLTEXTURESUBIMAGE1DEXTPROC glad_debug_glTextureSubImage1DEXT = glad_debug_impl_glTextureSubImage1DEXT;
PFNGLTEXTURESUBIMAGE2DEXTPROC glad_glTextureSubImage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTextureSubImage2DEXT", (GLADapiproc) glad_glTextureSubImage2DEXT, 10, texture, target, level, xoffset, yoffset, width, height, format, type, pixels);
    glad_glTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format, type, pixels);
    _post_call_gl_callback(NULL, "glTextureSubImage2DEXT", (GLADapiproc) glad_glTextureSubImage2DEXT, 10, texture, target, level, xoffset, yoffset, width, height, format, type, pixels);
    
}
PFNGLTEXTURESUBIMAGE2DEXTPROC glad_debug_glTextureSubImage2DEXT = glad_debug_impl_glTextureSubImage2DEXT;
PFNGLTEXTURESUBIMAGE3DEXTPROC glad_glTextureSubImage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gl_callback("glTextureSubImage3DEXT", (GLADapiproc) glad_glTextureSubImage3DEXT, 12, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    glad_glTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    _post_call_gl_callback(NULL, "glTextureSubImage3DEXT", (GLADapiproc) glad_glTextureSubImage3DEXT, 12, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    
}
PFNGLTEXTURESUBIMAGE3DEXTPROC glad_debug_glTextureSubImage3DEXT = glad_debug_impl_glTextureSubImage3DEXT;
PFNGLTRACKMATRIXNVPROC glad_glTrackMatrixNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glTrackMatrixNV(GLenum target, GLuint address, GLenum matrix, GLenum transform) {
    _pre_call_gl_callback("glTrackMatrixNV", (GLADapiproc) glad_glTrackMatrixNV, 4, target, address, matrix, transform);
    glad_glTrackMatrixNV(target, address, matrix, transform);
    _post_call_gl_callback(NULL, "glTrackMatrixNV", (GLADapiproc) glad_glTrackMatrixNV, 4, target, address, matrix, transform);
    
}
PFNGLTRACKMATRIXNVPROC glad_debug_glTrackMatrixNV = glad_debug_impl_glTrackMatrixNV;
PFNGLTRANSFORMFEEDBACKATTRIBSNVPROC glad_glTransformFeedbackAttribsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glTransformFeedbackAttribsNV(GLsizei count, const GLint * attribs, GLenum bufferMode) {
    _pre_call_gl_callback("glTransformFeedbackAttribsNV", (GLADapiproc) glad_glTransformFeedbackAttribsNV, 3, count, attribs, bufferMode);
    glad_glTransformFeedbackAttribsNV(count, attribs, bufferMode);
    _post_call_gl_callback(NULL, "glTransformFeedbackAttribsNV", (GLADapiproc) glad_glTransformFeedbackAttribsNV, 3, count, attribs, bufferMode);
    
}
PFNGLTRANSFORMFEEDBACKATTRIBSNVPROC glad_debug_glTransformFeedbackAttribsNV = glad_debug_impl_glTransformFeedbackAttribsNV;
PFNGLTRANSFORMFEEDBACKSTREAMATTRIBSNVPROC glad_glTransformFeedbackStreamAttribsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glTransformFeedbackStreamAttribsNV(GLsizei count, const GLint * attribs, GLsizei nbuffers, const GLint * bufstreams, GLenum bufferMode) {
    _pre_call_gl_callback("glTransformFeedbackStreamAttribsNV", (GLADapiproc) glad_glTransformFeedbackStreamAttribsNV, 5, count, attribs, nbuffers, bufstreams, bufferMode);
    glad_glTransformFeedbackStreamAttribsNV(count, attribs, nbuffers, bufstreams, bufferMode);
    _post_call_gl_callback(NULL, "glTransformFeedbackStreamAttribsNV", (GLADapiproc) glad_glTransformFeedbackStreamAttribsNV, 5, count, attribs, nbuffers, bufstreams, bufferMode);
    
}
PFNGLTRANSFORMFEEDBACKSTREAMATTRIBSNVPROC glad_debug_glTransformFeedbackStreamAttribsNV = glad_debug_impl_glTransformFeedbackStreamAttribsNV;
PFNGLTRANSFORMFEEDBACKVARYINGSPROC glad_glTransformFeedbackVaryings = NULL;
static void GLAD_API_PTR glad_debug_impl_glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const* varyings, GLenum bufferMode) {
    _pre_call_gl_callback("glTransformFeedbackVaryings", (GLADapiproc) glad_glTransformFeedbackVaryings, 4, program, count, varyings, bufferMode);
    glad_glTransformFeedbackVaryings(program, count, varyings, bufferMode);
    _post_call_gl_callback(NULL, "glTransformFeedbackVaryings", (GLADapiproc) glad_glTransformFeedbackVaryings, 4, program, count, varyings, bufferMode);
    
}
PFNGLTRANSFORMFEEDBACKVARYINGSPROC glad_debug_glTransformFeedbackVaryings = glad_debug_impl_glTransformFeedbackVaryings;
PFNGLTRANSFORMFEEDBACKVARYINGSEXTPROC glad_glTransformFeedbackVaryingsEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTransformFeedbackVaryingsEXT(GLuint program, GLsizei count, const GLchar *const* varyings, GLenum bufferMode) {
    _pre_call_gl_callback("glTransformFeedbackVaryingsEXT", (GLADapiproc) glad_glTransformFeedbackVaryingsEXT, 4, program, count, varyings, bufferMode);
    glad_glTransformFeedbackVaryingsEXT(program, count, varyings, bufferMode);
    _post_call_gl_callback(NULL, "glTransformFeedbackVaryingsEXT", (GLADapiproc) glad_glTransformFeedbackVaryingsEXT, 4, program, count, varyings, bufferMode);
    
}
PFNGLTRANSFORMFEEDBACKVARYINGSEXTPROC glad_debug_glTransformFeedbackVaryingsEXT = glad_debug_impl_glTransformFeedbackVaryingsEXT;
PFNGLTRANSFORMFEEDBACKVARYINGSNVPROC glad_glTransformFeedbackVaryingsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glTransformFeedbackVaryingsNV(GLuint program, GLsizei count, const GLint * locations, GLenum bufferMode) {
    _pre_call_gl_callback("glTransformFeedbackVaryingsNV", (GLADapiproc) glad_glTransformFeedbackVaryingsNV, 4, program, count, locations, bufferMode);
    glad_glTransformFeedbackVaryingsNV(program, count, locations, bufferMode);
    _post_call_gl_callback(NULL, "glTransformFeedbackVaryingsNV", (GLADapiproc) glad_glTransformFeedbackVaryingsNV, 4, program, count, locations, bufferMode);
    
}
PFNGLTRANSFORMFEEDBACKVARYINGSNVPROC glad_debug_glTransformFeedbackVaryingsNV = glad_debug_impl_glTransformFeedbackVaryingsNV;
PFNGLUNIFORM1FPROC glad_glUniform1f = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1f(GLint location, GLfloat v0) {
    _pre_call_gl_callback("glUniform1f", (GLADapiproc) glad_glUniform1f, 2, location, v0);
    glad_glUniform1f(location, v0);
    _post_call_gl_callback(NULL, "glUniform1f", (GLADapiproc) glad_glUniform1f, 2, location, v0);
    
}
PFNGLUNIFORM1FPROC glad_debug_glUniform1f = glad_debug_impl_glUniform1f;
PFNGLUNIFORM1FARBPROC glad_glUniform1fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1fARB(GLint location, GLfloat v0) {
    _pre_call_gl_callback("glUniform1fARB", (GLADapiproc) glad_glUniform1fARB, 2, location, v0);
    glad_glUniform1fARB(location, v0);
    _post_call_gl_callback(NULL, "glUniform1fARB", (GLADapiproc) glad_glUniform1fARB, 2, location, v0);
    
}
PFNGLUNIFORM1FARBPROC glad_debug_glUniform1fARB = glad_debug_impl_glUniform1fARB;
PFNGLUNIFORM1FVPROC glad_glUniform1fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1fv(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glUniform1fv", (GLADapiproc) glad_glUniform1fv, 3, location, count, value);
    glad_glUniform1fv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform1fv", (GLADapiproc) glad_glUniform1fv, 3, location, count, value);
    
}
PFNGLUNIFORM1FVPROC glad_debug_glUniform1fv = glad_debug_impl_glUniform1fv;
PFNGLUNIFORM1FVARBPROC glad_glUniform1fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1fvARB(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glUniform1fvARB", (GLADapiproc) glad_glUniform1fvARB, 3, location, count, value);
    glad_glUniform1fvARB(location, count, value);
    _post_call_gl_callback(NULL, "glUniform1fvARB", (GLADapiproc) glad_glUniform1fvARB, 3, location, count, value);
    
}
PFNGLUNIFORM1FVARBPROC glad_debug_glUniform1fvARB = glad_debug_impl_glUniform1fvARB;
PFNGLUNIFORM1IPROC glad_glUniform1i = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1i(GLint location, GLint v0) {
    _pre_call_gl_callback("glUniform1i", (GLADapiproc) glad_glUniform1i, 2, location, v0);
    glad_glUniform1i(location, v0);
    _post_call_gl_callback(NULL, "glUniform1i", (GLADapiproc) glad_glUniform1i, 2, location, v0);
    
}
PFNGLUNIFORM1IPROC glad_debug_glUniform1i = glad_debug_impl_glUniform1i;
PFNGLUNIFORM1IARBPROC glad_glUniform1iARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1iARB(GLint location, GLint v0) {
    _pre_call_gl_callback("glUniform1iARB", (GLADapiproc) glad_glUniform1iARB, 2, location, v0);
    glad_glUniform1iARB(location, v0);
    _post_call_gl_callback(NULL, "glUniform1iARB", (GLADapiproc) glad_glUniform1iARB, 2, location, v0);
    
}
PFNGLUNIFORM1IARBPROC glad_debug_glUniform1iARB = glad_debug_impl_glUniform1iARB;
PFNGLUNIFORM1IVPROC glad_glUniform1iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1iv(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glUniform1iv", (GLADapiproc) glad_glUniform1iv, 3, location, count, value);
    glad_glUniform1iv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform1iv", (GLADapiproc) glad_glUniform1iv, 3, location, count, value);
    
}
PFNGLUNIFORM1IVPROC glad_debug_glUniform1iv = glad_debug_impl_glUniform1iv;
PFNGLUNIFORM1IVARBPROC glad_glUniform1ivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1ivARB(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glUniform1ivARB", (GLADapiproc) glad_glUniform1ivARB, 3, location, count, value);
    glad_glUniform1ivARB(location, count, value);
    _post_call_gl_callback(NULL, "glUniform1ivARB", (GLADapiproc) glad_glUniform1ivARB, 3, location, count, value);
    
}
PFNGLUNIFORM1IVARBPROC glad_debug_glUniform1ivARB = glad_debug_impl_glUniform1ivARB;
PFNGLUNIFORM1UIPROC glad_glUniform1ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1ui(GLint location, GLuint v0) {
    _pre_call_gl_callback("glUniform1ui", (GLADapiproc) glad_glUniform1ui, 2, location, v0);
    glad_glUniform1ui(location, v0);
    _post_call_gl_callback(NULL, "glUniform1ui", (GLADapiproc) glad_glUniform1ui, 2, location, v0);
    
}
PFNGLUNIFORM1UIPROC glad_debug_glUniform1ui = glad_debug_impl_glUniform1ui;
PFNGLUNIFORM1UIEXTPROC glad_glUniform1uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1uiEXT(GLint location, GLuint v0) {
    _pre_call_gl_callback("glUniform1uiEXT", (GLADapiproc) glad_glUniform1uiEXT, 2, location, v0);
    glad_glUniform1uiEXT(location, v0);
    _post_call_gl_callback(NULL, "glUniform1uiEXT", (GLADapiproc) glad_glUniform1uiEXT, 2, location, v0);
    
}
PFNGLUNIFORM1UIEXTPROC glad_debug_glUniform1uiEXT = glad_debug_impl_glUniform1uiEXT;
PFNGLUNIFORM1UIVPROC glad_glUniform1uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1uiv(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glUniform1uiv", (GLADapiproc) glad_glUniform1uiv, 3, location, count, value);
    glad_glUniform1uiv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform1uiv", (GLADapiproc) glad_glUniform1uiv, 3, location, count, value);
    
}
PFNGLUNIFORM1UIVPROC glad_debug_glUniform1uiv = glad_debug_impl_glUniform1uiv;
PFNGLUNIFORM1UIVEXTPROC glad_glUniform1uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1uivEXT(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glUniform1uivEXT", (GLADapiproc) glad_glUniform1uivEXT, 3, location, count, value);
    glad_glUniform1uivEXT(location, count, value);
    _post_call_gl_callback(NULL, "glUniform1uivEXT", (GLADapiproc) glad_glUniform1uivEXT, 3, location, count, value);
    
}
PFNGLUNIFORM1UIVEXTPROC glad_debug_glUniform1uivEXT = glad_debug_impl_glUniform1uivEXT;
PFNGLUNIFORM2FPROC glad_glUniform2f = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    _pre_call_gl_callback("glUniform2f", (GLADapiproc) glad_glUniform2f, 3, location, v0, v1);
    glad_glUniform2f(location, v0, v1);
    _post_call_gl_callback(NULL, "glUniform2f", (GLADapiproc) glad_glUniform2f, 3, location, v0, v1);
    
}
PFNGLUNIFORM2FPROC glad_debug_glUniform2f = glad_debug_impl_glUniform2f;
PFNGLUNIFORM2FARBPROC glad_glUniform2fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2fARB(GLint location, GLfloat v0, GLfloat v1) {
    _pre_call_gl_callback("glUniform2fARB", (GLADapiproc) glad_glUniform2fARB, 3, location, v0, v1);
    glad_glUniform2fARB(location, v0, v1);
    _post_call_gl_callback(NULL, "glUniform2fARB", (GLADapiproc) glad_glUniform2fARB, 3, location, v0, v1);
    
}
PFNGLUNIFORM2FARBPROC glad_debug_glUniform2fARB = glad_debug_impl_glUniform2fARB;
PFNGLUNIFORM2FVPROC glad_glUniform2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2fv(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glUniform2fv", (GLADapiproc) glad_glUniform2fv, 3, location, count, value);
    glad_glUniform2fv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform2fv", (GLADapiproc) glad_glUniform2fv, 3, location, count, value);
    
}
PFNGLUNIFORM2FVPROC glad_debug_glUniform2fv = glad_debug_impl_glUniform2fv;
PFNGLUNIFORM2FVARBPROC glad_glUniform2fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2fvARB(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glUniform2fvARB", (GLADapiproc) glad_glUniform2fvARB, 3, location, count, value);
    glad_glUniform2fvARB(location, count, value);
    _post_call_gl_callback(NULL, "glUniform2fvARB", (GLADapiproc) glad_glUniform2fvARB, 3, location, count, value);
    
}
PFNGLUNIFORM2FVARBPROC glad_debug_glUniform2fvARB = glad_debug_impl_glUniform2fvARB;
PFNGLUNIFORM2IPROC glad_glUniform2i = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2i(GLint location, GLint v0, GLint v1) {
    _pre_call_gl_callback("glUniform2i", (GLADapiproc) glad_glUniform2i, 3, location, v0, v1);
    glad_glUniform2i(location, v0, v1);
    _post_call_gl_callback(NULL, "glUniform2i", (GLADapiproc) glad_glUniform2i, 3, location, v0, v1);
    
}
PFNGLUNIFORM2IPROC glad_debug_glUniform2i = glad_debug_impl_glUniform2i;
PFNGLUNIFORM2IARBPROC glad_glUniform2iARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2iARB(GLint location, GLint v0, GLint v1) {
    _pre_call_gl_callback("glUniform2iARB", (GLADapiproc) glad_glUniform2iARB, 3, location, v0, v1);
    glad_glUniform2iARB(location, v0, v1);
    _post_call_gl_callback(NULL, "glUniform2iARB", (GLADapiproc) glad_glUniform2iARB, 3, location, v0, v1);
    
}
PFNGLUNIFORM2IARBPROC glad_debug_glUniform2iARB = glad_debug_impl_glUniform2iARB;
PFNGLUNIFORM2IVPROC glad_glUniform2iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2iv(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glUniform2iv", (GLADapiproc) glad_glUniform2iv, 3, location, count, value);
    glad_glUniform2iv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform2iv", (GLADapiproc) glad_glUniform2iv, 3, location, count, value);
    
}
PFNGLUNIFORM2IVPROC glad_debug_glUniform2iv = glad_debug_impl_glUniform2iv;
PFNGLUNIFORM2IVARBPROC glad_glUniform2ivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2ivARB(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glUniform2ivARB", (GLADapiproc) glad_glUniform2ivARB, 3, location, count, value);
    glad_glUniform2ivARB(location, count, value);
    _post_call_gl_callback(NULL, "glUniform2ivARB", (GLADapiproc) glad_glUniform2ivARB, 3, location, count, value);
    
}
PFNGLUNIFORM2IVARBPROC glad_debug_glUniform2ivARB = glad_debug_impl_glUniform2ivARB;
PFNGLUNIFORM2UIPROC glad_glUniform2ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2ui(GLint location, GLuint v0, GLuint v1) {
    _pre_call_gl_callback("glUniform2ui", (GLADapiproc) glad_glUniform2ui, 3, location, v0, v1);
    glad_glUniform2ui(location, v0, v1);
    _post_call_gl_callback(NULL, "glUniform2ui", (GLADapiproc) glad_glUniform2ui, 3, location, v0, v1);
    
}
PFNGLUNIFORM2UIPROC glad_debug_glUniform2ui = glad_debug_impl_glUniform2ui;
PFNGLUNIFORM2UIEXTPROC glad_glUniform2uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2uiEXT(GLint location, GLuint v0, GLuint v1) {
    _pre_call_gl_callback("glUniform2uiEXT", (GLADapiproc) glad_glUniform2uiEXT, 3, location, v0, v1);
    glad_glUniform2uiEXT(location, v0, v1);
    _post_call_gl_callback(NULL, "glUniform2uiEXT", (GLADapiproc) glad_glUniform2uiEXT, 3, location, v0, v1);
    
}
PFNGLUNIFORM2UIEXTPROC glad_debug_glUniform2uiEXT = glad_debug_impl_glUniform2uiEXT;
PFNGLUNIFORM2UIVPROC glad_glUniform2uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2uiv(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glUniform2uiv", (GLADapiproc) glad_glUniform2uiv, 3, location, count, value);
    glad_glUniform2uiv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform2uiv", (GLADapiproc) glad_glUniform2uiv, 3, location, count, value);
    
}
PFNGLUNIFORM2UIVPROC glad_debug_glUniform2uiv = glad_debug_impl_glUniform2uiv;
PFNGLUNIFORM2UIVEXTPROC glad_glUniform2uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2uivEXT(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glUniform2uivEXT", (GLADapiproc) glad_glUniform2uivEXT, 3, location, count, value);
    glad_glUniform2uivEXT(location, count, value);
    _post_call_gl_callback(NULL, "glUniform2uivEXT", (GLADapiproc) glad_glUniform2uivEXT, 3, location, count, value);
    
}
PFNGLUNIFORM2UIVEXTPROC glad_debug_glUniform2uivEXT = glad_debug_impl_glUniform2uivEXT;
PFNGLUNIFORM3FPROC glad_glUniform3f = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    _pre_call_gl_callback("glUniform3f", (GLADapiproc) glad_glUniform3f, 4, location, v0, v1, v2);
    glad_glUniform3f(location, v0, v1, v2);
    _post_call_gl_callback(NULL, "glUniform3f", (GLADapiproc) glad_glUniform3f, 4, location, v0, v1, v2);
    
}
PFNGLUNIFORM3FPROC glad_debug_glUniform3f = glad_debug_impl_glUniform3f;
PFNGLUNIFORM3FARBPROC glad_glUniform3fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3fARB(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    _pre_call_gl_callback("glUniform3fARB", (GLADapiproc) glad_glUniform3fARB, 4, location, v0, v1, v2);
    glad_glUniform3fARB(location, v0, v1, v2);
    _post_call_gl_callback(NULL, "glUniform3fARB", (GLADapiproc) glad_glUniform3fARB, 4, location, v0, v1, v2);
    
}
PFNGLUNIFORM3FARBPROC glad_debug_glUniform3fARB = glad_debug_impl_glUniform3fARB;
PFNGLUNIFORM3FVPROC glad_glUniform3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3fv(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glUniform3fv", (GLADapiproc) glad_glUniform3fv, 3, location, count, value);
    glad_glUniform3fv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform3fv", (GLADapiproc) glad_glUniform3fv, 3, location, count, value);
    
}
PFNGLUNIFORM3FVPROC glad_debug_glUniform3fv = glad_debug_impl_glUniform3fv;
PFNGLUNIFORM3FVARBPROC glad_glUniform3fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3fvARB(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glUniform3fvARB", (GLADapiproc) glad_glUniform3fvARB, 3, location, count, value);
    glad_glUniform3fvARB(location, count, value);
    _post_call_gl_callback(NULL, "glUniform3fvARB", (GLADapiproc) glad_glUniform3fvARB, 3, location, count, value);
    
}
PFNGLUNIFORM3FVARBPROC glad_debug_glUniform3fvARB = glad_debug_impl_glUniform3fvARB;
PFNGLUNIFORM3IPROC glad_glUniform3i = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    _pre_call_gl_callback("glUniform3i", (GLADapiproc) glad_glUniform3i, 4, location, v0, v1, v2);
    glad_glUniform3i(location, v0, v1, v2);
    _post_call_gl_callback(NULL, "glUniform3i", (GLADapiproc) glad_glUniform3i, 4, location, v0, v1, v2);
    
}
PFNGLUNIFORM3IPROC glad_debug_glUniform3i = glad_debug_impl_glUniform3i;
PFNGLUNIFORM3IARBPROC glad_glUniform3iARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3iARB(GLint location, GLint v0, GLint v1, GLint v2) {
    _pre_call_gl_callback("glUniform3iARB", (GLADapiproc) glad_glUniform3iARB, 4, location, v0, v1, v2);
    glad_glUniform3iARB(location, v0, v1, v2);
    _post_call_gl_callback(NULL, "glUniform3iARB", (GLADapiproc) glad_glUniform3iARB, 4, location, v0, v1, v2);
    
}
PFNGLUNIFORM3IARBPROC glad_debug_glUniform3iARB = glad_debug_impl_glUniform3iARB;
PFNGLUNIFORM3IVPROC glad_glUniform3iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3iv(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glUniform3iv", (GLADapiproc) glad_glUniform3iv, 3, location, count, value);
    glad_glUniform3iv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform3iv", (GLADapiproc) glad_glUniform3iv, 3, location, count, value);
    
}
PFNGLUNIFORM3IVPROC glad_debug_glUniform3iv = glad_debug_impl_glUniform3iv;
PFNGLUNIFORM3IVARBPROC glad_glUniform3ivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3ivARB(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glUniform3ivARB", (GLADapiproc) glad_glUniform3ivARB, 3, location, count, value);
    glad_glUniform3ivARB(location, count, value);
    _post_call_gl_callback(NULL, "glUniform3ivARB", (GLADapiproc) glad_glUniform3ivARB, 3, location, count, value);
    
}
PFNGLUNIFORM3IVARBPROC glad_debug_glUniform3ivARB = glad_debug_impl_glUniform3ivARB;
PFNGLUNIFORM3UIPROC glad_glUniform3ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    _pre_call_gl_callback("glUniform3ui", (GLADapiproc) glad_glUniform3ui, 4, location, v0, v1, v2);
    glad_glUniform3ui(location, v0, v1, v2);
    _post_call_gl_callback(NULL, "glUniform3ui", (GLADapiproc) glad_glUniform3ui, 4, location, v0, v1, v2);
    
}
PFNGLUNIFORM3UIPROC glad_debug_glUniform3ui = glad_debug_impl_glUniform3ui;
PFNGLUNIFORM3UIEXTPROC glad_glUniform3uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3uiEXT(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    _pre_call_gl_callback("glUniform3uiEXT", (GLADapiproc) glad_glUniform3uiEXT, 4, location, v0, v1, v2);
    glad_glUniform3uiEXT(location, v0, v1, v2);
    _post_call_gl_callback(NULL, "glUniform3uiEXT", (GLADapiproc) glad_glUniform3uiEXT, 4, location, v0, v1, v2);
    
}
PFNGLUNIFORM3UIEXTPROC glad_debug_glUniform3uiEXT = glad_debug_impl_glUniform3uiEXT;
PFNGLUNIFORM3UIVPROC glad_glUniform3uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3uiv(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glUniform3uiv", (GLADapiproc) glad_glUniform3uiv, 3, location, count, value);
    glad_glUniform3uiv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform3uiv", (GLADapiproc) glad_glUniform3uiv, 3, location, count, value);
    
}
PFNGLUNIFORM3UIVPROC glad_debug_glUniform3uiv = glad_debug_impl_glUniform3uiv;
PFNGLUNIFORM3UIVEXTPROC glad_glUniform3uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3uivEXT(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glUniform3uivEXT", (GLADapiproc) glad_glUniform3uivEXT, 3, location, count, value);
    glad_glUniform3uivEXT(location, count, value);
    _post_call_gl_callback(NULL, "glUniform3uivEXT", (GLADapiproc) glad_glUniform3uivEXT, 3, location, count, value);
    
}
PFNGLUNIFORM3UIVEXTPROC glad_debug_glUniform3uivEXT = glad_debug_impl_glUniform3uivEXT;
PFNGLUNIFORM4FPROC glad_glUniform4f = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    _pre_call_gl_callback("glUniform4f", (GLADapiproc) glad_glUniform4f, 5, location, v0, v1, v2, v3);
    glad_glUniform4f(location, v0, v1, v2, v3);
    _post_call_gl_callback(NULL, "glUniform4f", (GLADapiproc) glad_glUniform4f, 5, location, v0, v1, v2, v3);
    
}
PFNGLUNIFORM4FPROC glad_debug_glUniform4f = glad_debug_impl_glUniform4f;
PFNGLUNIFORM4FARBPROC glad_glUniform4fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4fARB(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    _pre_call_gl_callback("glUniform4fARB", (GLADapiproc) glad_glUniform4fARB, 5, location, v0, v1, v2, v3);
    glad_glUniform4fARB(location, v0, v1, v2, v3);
    _post_call_gl_callback(NULL, "glUniform4fARB", (GLADapiproc) glad_glUniform4fARB, 5, location, v0, v1, v2, v3);
    
}
PFNGLUNIFORM4FARBPROC glad_debug_glUniform4fARB = glad_debug_impl_glUniform4fARB;
PFNGLUNIFORM4FVPROC glad_glUniform4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4fv(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glUniform4fv", (GLADapiproc) glad_glUniform4fv, 3, location, count, value);
    glad_glUniform4fv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform4fv", (GLADapiproc) glad_glUniform4fv, 3, location, count, value);
    
}
PFNGLUNIFORM4FVPROC glad_debug_glUniform4fv = glad_debug_impl_glUniform4fv;
PFNGLUNIFORM4FVARBPROC glad_glUniform4fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4fvARB(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gl_callback("glUniform4fvARB", (GLADapiproc) glad_glUniform4fvARB, 3, location, count, value);
    glad_glUniform4fvARB(location, count, value);
    _post_call_gl_callback(NULL, "glUniform4fvARB", (GLADapiproc) glad_glUniform4fvARB, 3, location, count, value);
    
}
PFNGLUNIFORM4FVARBPROC glad_debug_glUniform4fvARB = glad_debug_impl_glUniform4fvARB;
PFNGLUNIFORM4IPROC glad_glUniform4i = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    _pre_call_gl_callback("glUniform4i", (GLADapiproc) glad_glUniform4i, 5, location, v0, v1, v2, v3);
    glad_glUniform4i(location, v0, v1, v2, v3);
    _post_call_gl_callback(NULL, "glUniform4i", (GLADapiproc) glad_glUniform4i, 5, location, v0, v1, v2, v3);
    
}
PFNGLUNIFORM4IPROC glad_debug_glUniform4i = glad_debug_impl_glUniform4i;
PFNGLUNIFORM4IARBPROC glad_glUniform4iARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4iARB(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    _pre_call_gl_callback("glUniform4iARB", (GLADapiproc) glad_glUniform4iARB, 5, location, v0, v1, v2, v3);
    glad_glUniform4iARB(location, v0, v1, v2, v3);
    _post_call_gl_callback(NULL, "glUniform4iARB", (GLADapiproc) glad_glUniform4iARB, 5, location, v0, v1, v2, v3);
    
}
PFNGLUNIFORM4IARBPROC glad_debug_glUniform4iARB = glad_debug_impl_glUniform4iARB;
PFNGLUNIFORM4IVPROC glad_glUniform4iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4iv(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glUniform4iv", (GLADapiproc) glad_glUniform4iv, 3, location, count, value);
    glad_glUniform4iv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform4iv", (GLADapiproc) glad_glUniform4iv, 3, location, count, value);
    
}
PFNGLUNIFORM4IVPROC glad_debug_glUniform4iv = glad_debug_impl_glUniform4iv;
PFNGLUNIFORM4IVARBPROC glad_glUniform4ivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4ivARB(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gl_callback("glUniform4ivARB", (GLADapiproc) glad_glUniform4ivARB, 3, location, count, value);
    glad_glUniform4ivARB(location, count, value);
    _post_call_gl_callback(NULL, "glUniform4ivARB", (GLADapiproc) glad_glUniform4ivARB, 3, location, count, value);
    
}
PFNGLUNIFORM4IVARBPROC glad_debug_glUniform4ivARB = glad_debug_impl_glUniform4ivARB;
PFNGLUNIFORM4UIPROC glad_glUniform4ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    _pre_call_gl_callback("glUniform4ui", (GLADapiproc) glad_glUniform4ui, 5, location, v0, v1, v2, v3);
    glad_glUniform4ui(location, v0, v1, v2, v3);
    _post_call_gl_callback(NULL, "glUniform4ui", (GLADapiproc) glad_glUniform4ui, 5, location, v0, v1, v2, v3);
    
}
PFNGLUNIFORM4UIPROC glad_debug_glUniform4ui = glad_debug_impl_glUniform4ui;
PFNGLUNIFORM4UIEXTPROC glad_glUniform4uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4uiEXT(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    _pre_call_gl_callback("glUniform4uiEXT", (GLADapiproc) glad_glUniform4uiEXT, 5, location, v0, v1, v2, v3);
    glad_glUniform4uiEXT(location, v0, v1, v2, v3);
    _post_call_gl_callback(NULL, "glUniform4uiEXT", (GLADapiproc) glad_glUniform4uiEXT, 5, location, v0, v1, v2, v3);
    
}
PFNGLUNIFORM4UIEXTPROC glad_debug_glUniform4uiEXT = glad_debug_impl_glUniform4uiEXT;
PFNGLUNIFORM4UIVPROC glad_glUniform4uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4uiv(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glUniform4uiv", (GLADapiproc) glad_glUniform4uiv, 3, location, count, value);
    glad_glUniform4uiv(location, count, value);
    _post_call_gl_callback(NULL, "glUniform4uiv", (GLADapiproc) glad_glUniform4uiv, 3, location, count, value);
    
}
PFNGLUNIFORM4UIVPROC glad_debug_glUniform4uiv = glad_debug_impl_glUniform4uiv;
PFNGLUNIFORM4UIVEXTPROC glad_glUniform4uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4uivEXT(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gl_callback("glUniform4uivEXT", (GLADapiproc) glad_glUniform4uivEXT, 3, location, count, value);
    glad_glUniform4uivEXT(location, count, value);
    _post_call_gl_callback(NULL, "glUniform4uivEXT", (GLADapiproc) glad_glUniform4uivEXT, 3, location, count, value);
    
}
PFNGLUNIFORM4UIVEXTPROC glad_debug_glUniform4uivEXT = glad_debug_impl_glUniform4uivEXT;
PFNGLUNIFORMBLOCKBINDINGPROC glad_glUniformBlockBinding = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
    _pre_call_gl_callback("glUniformBlockBinding", (GLADapiproc) glad_glUniformBlockBinding, 3, program, uniformBlockIndex, uniformBlockBinding);
    glad_glUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
    _post_call_gl_callback(NULL, "glUniformBlockBinding", (GLADapiproc) glad_glUniformBlockBinding, 3, program, uniformBlockIndex, uniformBlockBinding);
    
}
PFNGLUNIFORMBLOCKBINDINGPROC glad_debug_glUniformBlockBinding = glad_debug_impl_glUniformBlockBinding;
PFNGLUNIFORMMATRIX2FVPROC glad_glUniformMatrix2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix2fv", (GLADapiproc) glad_glUniformMatrix2fv, 4, location, count, transpose, value);
    glad_glUniformMatrix2fv(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix2fv", (GLADapiproc) glad_glUniformMatrix2fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX2FVPROC glad_debug_glUniformMatrix2fv = glad_debug_impl_glUniformMatrix2fv;
PFNGLUNIFORMMATRIX2FVARBPROC glad_glUniformMatrix2fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix2fvARB(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix2fvARB", (GLADapiproc) glad_glUniformMatrix2fvARB, 4, location, count, transpose, value);
    glad_glUniformMatrix2fvARB(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix2fvARB", (GLADapiproc) glad_glUniformMatrix2fvARB, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX2FVARBPROC glad_debug_glUniformMatrix2fvARB = glad_debug_impl_glUniformMatrix2fvARB;
PFNGLUNIFORMMATRIX2X3FVPROC glad_glUniformMatrix2x3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix2x3fv", (GLADapiproc) glad_glUniformMatrix2x3fv, 4, location, count, transpose, value);
    glad_glUniformMatrix2x3fv(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix2x3fv", (GLADapiproc) glad_glUniformMatrix2x3fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX2X3FVPROC glad_debug_glUniformMatrix2x3fv = glad_debug_impl_glUniformMatrix2x3fv;
PFNGLUNIFORMMATRIX2X4FVPROC glad_glUniformMatrix2x4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix2x4fv", (GLADapiproc) glad_glUniformMatrix2x4fv, 4, location, count, transpose, value);
    glad_glUniformMatrix2x4fv(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix2x4fv", (GLADapiproc) glad_glUniformMatrix2x4fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX2X4FVPROC glad_debug_glUniformMatrix2x4fv = glad_debug_impl_glUniformMatrix2x4fv;
PFNGLUNIFORMMATRIX3FVPROC glad_glUniformMatrix3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix3fv", (GLADapiproc) glad_glUniformMatrix3fv, 4, location, count, transpose, value);
    glad_glUniformMatrix3fv(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix3fv", (GLADapiproc) glad_glUniformMatrix3fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX3FVPROC glad_debug_glUniformMatrix3fv = glad_debug_impl_glUniformMatrix3fv;
PFNGLUNIFORMMATRIX3FVARBPROC glad_glUniformMatrix3fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix3fvARB(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix3fvARB", (GLADapiproc) glad_glUniformMatrix3fvARB, 4, location, count, transpose, value);
    glad_glUniformMatrix3fvARB(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix3fvARB", (GLADapiproc) glad_glUniformMatrix3fvARB, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX3FVARBPROC glad_debug_glUniformMatrix3fvARB = glad_debug_impl_glUniformMatrix3fvARB;
PFNGLUNIFORMMATRIX3X2FVPROC glad_glUniformMatrix3x2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix3x2fv", (GLADapiproc) glad_glUniformMatrix3x2fv, 4, location, count, transpose, value);
    glad_glUniformMatrix3x2fv(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix3x2fv", (GLADapiproc) glad_glUniformMatrix3x2fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX3X2FVPROC glad_debug_glUniformMatrix3x2fv = glad_debug_impl_glUniformMatrix3x2fv;
PFNGLUNIFORMMATRIX3X4FVPROC glad_glUniformMatrix3x4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix3x4fv", (GLADapiproc) glad_glUniformMatrix3x4fv, 4, location, count, transpose, value);
    glad_glUniformMatrix3x4fv(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix3x4fv", (GLADapiproc) glad_glUniformMatrix3x4fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX3X4FVPROC glad_debug_glUniformMatrix3x4fv = glad_debug_impl_glUniformMatrix3x4fv;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix4fv", (GLADapiproc) glad_glUniformMatrix4fv, 4, location, count, transpose, value);
    glad_glUniformMatrix4fv(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix4fv", (GLADapiproc) glad_glUniformMatrix4fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX4FVPROC glad_debug_glUniformMatrix4fv = glad_debug_impl_glUniformMatrix4fv;
PFNGLUNIFORMMATRIX4FVARBPROC glad_glUniformMatrix4fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix4fvARB(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix4fvARB", (GLADapiproc) glad_glUniformMatrix4fvARB, 4, location, count, transpose, value);
    glad_glUniformMatrix4fvARB(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix4fvARB", (GLADapiproc) glad_glUniformMatrix4fvARB, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX4FVARBPROC glad_debug_glUniformMatrix4fvARB = glad_debug_impl_glUniformMatrix4fvARB;
PFNGLUNIFORMMATRIX4X2FVPROC glad_glUniformMatrix4x2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix4x2fv", (GLADapiproc) glad_glUniformMatrix4x2fv, 4, location, count, transpose, value);
    glad_glUniformMatrix4x2fv(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix4x2fv", (GLADapiproc) glad_glUniformMatrix4x2fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX4X2FVPROC glad_debug_glUniformMatrix4x2fv = glad_debug_impl_glUniformMatrix4x2fv;
PFNGLUNIFORMMATRIX4X3FVPROC glad_glUniformMatrix4x3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gl_callback("glUniformMatrix4x3fv", (GLADapiproc) glad_glUniformMatrix4x3fv, 4, location, count, transpose, value);
    glad_glUniformMatrix4x3fv(location, count, transpose, value);
    _post_call_gl_callback(NULL, "glUniformMatrix4x3fv", (GLADapiproc) glad_glUniformMatrix4x3fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX4X3FVPROC glad_debug_glUniformMatrix4x3fv = glad_debug_impl_glUniformMatrix4x3fv;
PFNGLUNMAPBUFFERPROC glad_glUnmapBuffer = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glUnmapBuffer(GLenum target) {
    GLboolean ret;
    _pre_call_gl_callback("glUnmapBuffer", (GLADapiproc) glad_glUnmapBuffer, 1, target);
    ret = glad_glUnmapBuffer(target);
    _post_call_gl_callback((void*) &ret, "glUnmapBuffer", (GLADapiproc) glad_glUnmapBuffer, 1, target);
    return ret;
}
PFNGLUNMAPBUFFERPROC glad_debug_glUnmapBuffer = glad_debug_impl_glUnmapBuffer;
PFNGLUNMAPBUFFERARBPROC glad_glUnmapBufferARB = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glUnmapBufferARB(GLenum target) {
    GLboolean ret;
    _pre_call_gl_callback("glUnmapBufferARB", (GLADapiproc) glad_glUnmapBufferARB, 1, target);
    ret = glad_glUnmapBufferARB(target);
    _post_call_gl_callback((void*) &ret, "glUnmapBufferARB", (GLADapiproc) glad_glUnmapBufferARB, 1, target);
    return ret;
}
PFNGLUNMAPBUFFERARBPROC glad_debug_glUnmapBufferARB = glad_debug_impl_glUnmapBufferARB;
PFNGLUNMAPNAMEDBUFFEREXTPROC glad_glUnmapNamedBufferEXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glUnmapNamedBufferEXT(GLuint buffer) {
    GLboolean ret;
    _pre_call_gl_callback("glUnmapNamedBufferEXT", (GLADapiproc) glad_glUnmapNamedBufferEXT, 1, buffer);
    ret = glad_glUnmapNamedBufferEXT(buffer);
    _post_call_gl_callback((void*) &ret, "glUnmapNamedBufferEXT", (GLADapiproc) glad_glUnmapNamedBufferEXT, 1, buffer);
    return ret;
}
PFNGLUNMAPNAMEDBUFFEREXTPROC glad_debug_glUnmapNamedBufferEXT = glad_debug_impl_glUnmapNamedBufferEXT;
PFNGLUSEPROGRAMPROC glad_glUseProgram = NULL;
static void GLAD_API_PTR glad_debug_impl_glUseProgram(GLuint program) {
    _pre_call_gl_callback("glUseProgram", (GLADapiproc) glad_glUseProgram, 1, program);
    glad_glUseProgram(program);
    _post_call_gl_callback(NULL, "glUseProgram", (GLADapiproc) glad_glUseProgram, 1, program);
    
}
PFNGLUSEPROGRAMPROC glad_debug_glUseProgram = glad_debug_impl_glUseProgram;
PFNGLUSEPROGRAMOBJECTARBPROC glad_glUseProgramObjectARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glUseProgramObjectARB(GLhandleARB programObj) {
    _pre_call_gl_callback("glUseProgramObjectARB", (GLADapiproc) glad_glUseProgramObjectARB, 1, programObj);
    glad_glUseProgramObjectARB(programObj);
    _post_call_gl_callback(NULL, "glUseProgramObjectARB", (GLADapiproc) glad_glUseProgramObjectARB, 1, programObj);
    
}
PFNGLUSEPROGRAMOBJECTARBPROC glad_debug_glUseProgramObjectARB = glad_debug_impl_glUseProgramObjectARB;
PFNGLVALIDATEPROGRAMPROC glad_glValidateProgram = NULL;
static void GLAD_API_PTR glad_debug_impl_glValidateProgram(GLuint program) {
    _pre_call_gl_callback("glValidateProgram", (GLADapiproc) glad_glValidateProgram, 1, program);
    glad_glValidateProgram(program);
    _post_call_gl_callback(NULL, "glValidateProgram", (GLADapiproc) glad_glValidateProgram, 1, program);
    
}
PFNGLVALIDATEPROGRAMPROC glad_debug_glValidateProgram = glad_debug_impl_glValidateProgram;
PFNGLVALIDATEPROGRAMARBPROC glad_glValidateProgramARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glValidateProgramARB(GLhandleARB programObj) {
    _pre_call_gl_callback("glValidateProgramARB", (GLADapiproc) glad_glValidateProgramARB, 1, programObj);
    glad_glValidateProgramARB(programObj);
    _post_call_gl_callback(NULL, "glValidateProgramARB", (GLADapiproc) glad_glValidateProgramARB, 1, programObj);
    
}
PFNGLVALIDATEPROGRAMARBPROC glad_debug_glValidateProgramARB = glad_debug_impl_glValidateProgramARB;
PFNGLVERTEXARRAYBINDVERTEXBUFFEREXTPROC glad_glVertexArrayBindVertexBufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayBindVertexBufferEXT(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    _pre_call_gl_callback("glVertexArrayBindVertexBufferEXT", (GLADapiproc) glad_glVertexArrayBindVertexBufferEXT, 5, vaobj, bindingindex, buffer, offset, stride);
    glad_glVertexArrayBindVertexBufferEXT(vaobj, bindingindex, buffer, offset, stride);
    _post_call_gl_callback(NULL, "glVertexArrayBindVertexBufferEXT", (GLADapiproc) glad_glVertexArrayBindVertexBufferEXT, 5, vaobj, bindingindex, buffer, offset, stride);
    
}
PFNGLVERTEXARRAYBINDVERTEXBUFFEREXTPROC glad_debug_glVertexArrayBindVertexBufferEXT = glad_debug_impl_glVertexArrayBindVertexBufferEXT;
PFNGLVERTEXARRAYCOLOROFFSETEXTPROC glad_glVertexArrayColorOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayColorOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArrayColorOffsetEXT", (GLADapiproc) glad_glVertexArrayColorOffsetEXT, 6, vaobj, buffer, size, type, stride, offset);
    glad_glVertexArrayColorOffsetEXT(vaobj, buffer, size, type, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArrayColorOffsetEXT", (GLADapiproc) glad_glVertexArrayColorOffsetEXT, 6, vaobj, buffer, size, type, stride, offset);
    
}
PFNGLVERTEXARRAYCOLOROFFSETEXTPROC glad_debug_glVertexArrayColorOffsetEXT = glad_debug_impl_glVertexArrayColorOffsetEXT;
PFNGLVERTEXARRAYEDGEFLAGOFFSETEXTPROC glad_glVertexArrayEdgeFlagOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayEdgeFlagOffsetEXT(GLuint vaobj, GLuint buffer, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArrayEdgeFlagOffsetEXT", (GLADapiproc) glad_glVertexArrayEdgeFlagOffsetEXT, 4, vaobj, buffer, stride, offset);
    glad_glVertexArrayEdgeFlagOffsetEXT(vaobj, buffer, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArrayEdgeFlagOffsetEXT", (GLADapiproc) glad_glVertexArrayEdgeFlagOffsetEXT, 4, vaobj, buffer, stride, offset);
    
}
PFNGLVERTEXARRAYEDGEFLAGOFFSETEXTPROC glad_debug_glVertexArrayEdgeFlagOffsetEXT = glad_debug_impl_glVertexArrayEdgeFlagOffsetEXT;
PFNGLVERTEXARRAYFOGCOORDOFFSETEXTPROC glad_glVertexArrayFogCoordOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayFogCoordOffsetEXT(GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArrayFogCoordOffsetEXT", (GLADapiproc) glad_glVertexArrayFogCoordOffsetEXT, 5, vaobj, buffer, type, stride, offset);
    glad_glVertexArrayFogCoordOffsetEXT(vaobj, buffer, type, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArrayFogCoordOffsetEXT", (GLADapiproc) glad_glVertexArrayFogCoordOffsetEXT, 5, vaobj, buffer, type, stride, offset);
    
}
PFNGLVERTEXARRAYFOGCOORDOFFSETEXTPROC glad_debug_glVertexArrayFogCoordOffsetEXT = glad_debug_impl_glVertexArrayFogCoordOffsetEXT;
PFNGLVERTEXARRAYINDEXOFFSETEXTPROC glad_glVertexArrayIndexOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayIndexOffsetEXT(GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArrayIndexOffsetEXT", (GLADapiproc) glad_glVertexArrayIndexOffsetEXT, 5, vaobj, buffer, type, stride, offset);
    glad_glVertexArrayIndexOffsetEXT(vaobj, buffer, type, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArrayIndexOffsetEXT", (GLADapiproc) glad_glVertexArrayIndexOffsetEXT, 5, vaobj, buffer, type, stride, offset);
    
}
PFNGLVERTEXARRAYINDEXOFFSETEXTPROC glad_debug_glVertexArrayIndexOffsetEXT = glad_debug_impl_glVertexArrayIndexOffsetEXT;
PFNGLVERTEXARRAYMULTITEXCOORDOFFSETEXTPROC glad_glVertexArrayMultiTexCoordOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayMultiTexCoordOffsetEXT(GLuint vaobj, GLuint buffer, GLenum texunit, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArrayMultiTexCoordOffsetEXT", (GLADapiproc) glad_glVertexArrayMultiTexCoordOffsetEXT, 7, vaobj, buffer, texunit, size, type, stride, offset);
    glad_glVertexArrayMultiTexCoordOffsetEXT(vaobj, buffer, texunit, size, type, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArrayMultiTexCoordOffsetEXT", (GLADapiproc) glad_glVertexArrayMultiTexCoordOffsetEXT, 7, vaobj, buffer, texunit, size, type, stride, offset);
    
}
PFNGLVERTEXARRAYMULTITEXCOORDOFFSETEXTPROC glad_debug_glVertexArrayMultiTexCoordOffsetEXT = glad_debug_impl_glVertexArrayMultiTexCoordOffsetEXT;
PFNGLVERTEXARRAYNORMALOFFSETEXTPROC glad_glVertexArrayNormalOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayNormalOffsetEXT(GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArrayNormalOffsetEXT", (GLADapiproc) glad_glVertexArrayNormalOffsetEXT, 5, vaobj, buffer, type, stride, offset);
    glad_glVertexArrayNormalOffsetEXT(vaobj, buffer, type, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArrayNormalOffsetEXT", (GLADapiproc) glad_glVertexArrayNormalOffsetEXT, 5, vaobj, buffer, type, stride, offset);
    
}
PFNGLVERTEXARRAYNORMALOFFSETEXTPROC glad_debug_glVertexArrayNormalOffsetEXT = glad_debug_impl_glVertexArrayNormalOffsetEXT;
PFNGLVERTEXARRAYSECONDARYCOLOROFFSETEXTPROC glad_glVertexArraySecondaryColorOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArraySecondaryColorOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArraySecondaryColorOffsetEXT", (GLADapiproc) glad_glVertexArraySecondaryColorOffsetEXT, 6, vaobj, buffer, size, type, stride, offset);
    glad_glVertexArraySecondaryColorOffsetEXT(vaobj, buffer, size, type, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArraySecondaryColorOffsetEXT", (GLADapiproc) glad_glVertexArraySecondaryColorOffsetEXT, 6, vaobj, buffer, size, type, stride, offset);
    
}
PFNGLVERTEXARRAYSECONDARYCOLOROFFSETEXTPROC glad_debug_glVertexArraySecondaryColorOffsetEXT = glad_debug_impl_glVertexArraySecondaryColorOffsetEXT;
PFNGLVERTEXARRAYTEXCOORDOFFSETEXTPROC glad_glVertexArrayTexCoordOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayTexCoordOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArrayTexCoordOffsetEXT", (GLADapiproc) glad_glVertexArrayTexCoordOffsetEXT, 6, vaobj, buffer, size, type, stride, offset);
    glad_glVertexArrayTexCoordOffsetEXT(vaobj, buffer, size, type, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArrayTexCoordOffsetEXT", (GLADapiproc) glad_glVertexArrayTexCoordOffsetEXT, 6, vaobj, buffer, size, type, stride, offset);
    
}
PFNGLVERTEXARRAYTEXCOORDOFFSETEXTPROC glad_debug_glVertexArrayTexCoordOffsetEXT = glad_debug_impl_glVertexArrayTexCoordOffsetEXT;
PFNGLVERTEXARRAYVERTEXATTRIBBINDINGEXTPROC glad_glVertexArrayVertexAttribBindingEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayVertexAttribBindingEXT(GLuint vaobj, GLuint attribindex, GLuint bindingindex) {
    _pre_call_gl_callback("glVertexArrayVertexAttribBindingEXT", (GLADapiproc) glad_glVertexArrayVertexAttribBindingEXT, 3, vaobj, attribindex, bindingindex);
    glad_glVertexArrayVertexAttribBindingEXT(vaobj, attribindex, bindingindex);
    _post_call_gl_callback(NULL, "glVertexArrayVertexAttribBindingEXT", (GLADapiproc) glad_glVertexArrayVertexAttribBindingEXT, 3, vaobj, attribindex, bindingindex);
    
}
PFNGLVERTEXARRAYVERTEXATTRIBBINDINGEXTPROC glad_debug_glVertexArrayVertexAttribBindingEXT = glad_debug_impl_glVertexArrayVertexAttribBindingEXT;
PFNGLVERTEXARRAYVERTEXATTRIBDIVISOREXTPROC glad_glVertexArrayVertexAttribDivisorEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayVertexAttribDivisorEXT(GLuint vaobj, GLuint index, GLuint divisor) {
    _pre_call_gl_callback("glVertexArrayVertexAttribDivisorEXT", (GLADapiproc) glad_glVertexArrayVertexAttribDivisorEXT, 3, vaobj, index, divisor);
    glad_glVertexArrayVertexAttribDivisorEXT(vaobj, index, divisor);
    _post_call_gl_callback(NULL, "glVertexArrayVertexAttribDivisorEXT", (GLADapiproc) glad_glVertexArrayVertexAttribDivisorEXT, 3, vaobj, index, divisor);
    
}
PFNGLVERTEXARRAYVERTEXATTRIBDIVISOREXTPROC glad_debug_glVertexArrayVertexAttribDivisorEXT = glad_debug_impl_glVertexArrayVertexAttribDivisorEXT;
PFNGLVERTEXARRAYVERTEXATTRIBFORMATEXTPROC glad_glVertexArrayVertexAttribFormatEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayVertexAttribFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    _pre_call_gl_callback("glVertexArrayVertexAttribFormatEXT", (GLADapiproc) glad_glVertexArrayVertexAttribFormatEXT, 6, vaobj, attribindex, size, type, normalized, relativeoffset);
    glad_glVertexArrayVertexAttribFormatEXT(vaobj, attribindex, size, type, normalized, relativeoffset);
    _post_call_gl_callback(NULL, "glVertexArrayVertexAttribFormatEXT", (GLADapiproc) glad_glVertexArrayVertexAttribFormatEXT, 6, vaobj, attribindex, size, type, normalized, relativeoffset);
    
}
PFNGLVERTEXARRAYVERTEXATTRIBFORMATEXTPROC glad_debug_glVertexArrayVertexAttribFormatEXT = glad_debug_impl_glVertexArrayVertexAttribFormatEXT;
PFNGLVERTEXARRAYVERTEXATTRIBIFORMATEXTPROC glad_glVertexArrayVertexAttribIFormatEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayVertexAttribIFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    _pre_call_gl_callback("glVertexArrayVertexAttribIFormatEXT", (GLADapiproc) glad_glVertexArrayVertexAttribIFormatEXT, 5, vaobj, attribindex, size, type, relativeoffset);
    glad_glVertexArrayVertexAttribIFormatEXT(vaobj, attribindex, size, type, relativeoffset);
    _post_call_gl_callback(NULL, "glVertexArrayVertexAttribIFormatEXT", (GLADapiproc) glad_glVertexArrayVertexAttribIFormatEXT, 5, vaobj, attribindex, size, type, relativeoffset);
    
}
PFNGLVERTEXARRAYVERTEXATTRIBIFORMATEXTPROC glad_debug_glVertexArrayVertexAttribIFormatEXT = glad_debug_impl_glVertexArrayVertexAttribIFormatEXT;
PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC glad_glVertexArrayVertexAttribIOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayVertexAttribIOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArrayVertexAttribIOffsetEXT", (GLADapiproc) glad_glVertexArrayVertexAttribIOffsetEXT, 7, vaobj, buffer, index, size, type, stride, offset);
    glad_glVertexArrayVertexAttribIOffsetEXT(vaobj, buffer, index, size, type, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArrayVertexAttribIOffsetEXT", (GLADapiproc) glad_glVertexArrayVertexAttribIOffsetEXT, 7, vaobj, buffer, index, size, type, stride, offset);
    
}
PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC glad_debug_glVertexArrayVertexAttribIOffsetEXT = glad_debug_impl_glVertexArrayVertexAttribIOffsetEXT;
PFNGLVERTEXARRAYVERTEXATTRIBLFORMATEXTPROC glad_glVertexArrayVertexAttribLFormatEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayVertexAttribLFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    _pre_call_gl_callback("glVertexArrayVertexAttribLFormatEXT", (GLADapiproc) glad_glVertexArrayVertexAttribLFormatEXT, 5, vaobj, attribindex, size, type, relativeoffset);
    glad_glVertexArrayVertexAttribLFormatEXT(vaobj, attribindex, size, type, relativeoffset);
    _post_call_gl_callback(NULL, "glVertexArrayVertexAttribLFormatEXT", (GLADapiproc) glad_glVertexArrayVertexAttribLFormatEXT, 5, vaobj, attribindex, size, type, relativeoffset);
    
}
PFNGLVERTEXARRAYVERTEXATTRIBLFORMATEXTPROC glad_debug_glVertexArrayVertexAttribLFormatEXT = glad_debug_impl_glVertexArrayVertexAttribLFormatEXT;
PFNGLVERTEXARRAYVERTEXATTRIBLOFFSETEXTPROC glad_glVertexArrayVertexAttribLOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayVertexAttribLOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArrayVertexAttribLOffsetEXT", (GLADapiproc) glad_glVertexArrayVertexAttribLOffsetEXT, 7, vaobj, buffer, index, size, type, stride, offset);
    glad_glVertexArrayVertexAttribLOffsetEXT(vaobj, buffer, index, size, type, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArrayVertexAttribLOffsetEXT", (GLADapiproc) glad_glVertexArrayVertexAttribLOffsetEXT, 7, vaobj, buffer, index, size, type, stride, offset);
    
}
PFNGLVERTEXARRAYVERTEXATTRIBLOFFSETEXTPROC glad_debug_glVertexArrayVertexAttribLOffsetEXT = glad_debug_impl_glVertexArrayVertexAttribLOffsetEXT;
PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC glad_glVertexArrayVertexAttribOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayVertexAttribOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArrayVertexAttribOffsetEXT", (GLADapiproc) glad_glVertexArrayVertexAttribOffsetEXT, 8, vaobj, buffer, index, size, type, normalized, stride, offset);
    glad_glVertexArrayVertexAttribOffsetEXT(vaobj, buffer, index, size, type, normalized, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArrayVertexAttribOffsetEXT", (GLADapiproc) glad_glVertexArrayVertexAttribOffsetEXT, 8, vaobj, buffer, index, size, type, normalized, stride, offset);
    
}
PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC glad_debug_glVertexArrayVertexAttribOffsetEXT = glad_debug_impl_glVertexArrayVertexAttribOffsetEXT;
PFNGLVERTEXARRAYVERTEXBINDINGDIVISOREXTPROC glad_glVertexArrayVertexBindingDivisorEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayVertexBindingDivisorEXT(GLuint vaobj, GLuint bindingindex, GLuint divisor) {
    _pre_call_gl_callback("glVertexArrayVertexBindingDivisorEXT", (GLADapiproc) glad_glVertexArrayVertexBindingDivisorEXT, 3, vaobj, bindingindex, divisor);
    glad_glVertexArrayVertexBindingDivisorEXT(vaobj, bindingindex, divisor);
    _post_call_gl_callback(NULL, "glVertexArrayVertexBindingDivisorEXT", (GLADapiproc) glad_glVertexArrayVertexBindingDivisorEXT, 3, vaobj, bindingindex, divisor);
    
}
PFNGLVERTEXARRAYVERTEXBINDINGDIVISOREXTPROC glad_debug_glVertexArrayVertexBindingDivisorEXT = glad_debug_impl_glVertexArrayVertexBindingDivisorEXT;
PFNGLVERTEXARRAYVERTEXOFFSETEXTPROC glad_glVertexArrayVertexOffsetEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexArrayVertexOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    _pre_call_gl_callback("glVertexArrayVertexOffsetEXT", (GLADapiproc) glad_glVertexArrayVertexOffsetEXT, 6, vaobj, buffer, size, type, stride, offset);
    glad_glVertexArrayVertexOffsetEXT(vaobj, buffer, size, type, stride, offset);
    _post_call_gl_callback(NULL, "glVertexArrayVertexOffsetEXT", (GLADapiproc) glad_glVertexArrayVertexOffsetEXT, 6, vaobj, buffer, size, type, stride, offset);
    
}
PFNGLVERTEXARRAYVERTEXOFFSETEXTPROC glad_debug_glVertexArrayVertexOffsetEXT = glad_debug_impl_glVertexArrayVertexOffsetEXT;
PFNGLVERTEXATTRIB1DPROC glad_glVertexAttrib1d = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1d(GLuint index, GLdouble x) {
    _pre_call_gl_callback("glVertexAttrib1d", (GLADapiproc) glad_glVertexAttrib1d, 2, index, x);
    glad_glVertexAttrib1d(index, x);
    _post_call_gl_callback(NULL, "glVertexAttrib1d", (GLADapiproc) glad_glVertexAttrib1d, 2, index, x);
    
}
PFNGLVERTEXATTRIB1DPROC glad_debug_glVertexAttrib1d = glad_debug_impl_glVertexAttrib1d;
PFNGLVERTEXATTRIB1DARBPROC glad_glVertexAttrib1dARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1dARB(GLuint index, GLdouble x) {
    _pre_call_gl_callback("glVertexAttrib1dARB", (GLADapiproc) glad_glVertexAttrib1dARB, 2, index, x);
    glad_glVertexAttrib1dARB(index, x);
    _post_call_gl_callback(NULL, "glVertexAttrib1dARB", (GLADapiproc) glad_glVertexAttrib1dARB, 2, index, x);
    
}
PFNGLVERTEXATTRIB1DARBPROC glad_debug_glVertexAttrib1dARB = glad_debug_impl_glVertexAttrib1dARB;
PFNGLVERTEXATTRIB1DNVPROC glad_glVertexAttrib1dNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1dNV(GLuint index, GLdouble x) {
    _pre_call_gl_callback("glVertexAttrib1dNV", (GLADapiproc) glad_glVertexAttrib1dNV, 2, index, x);
    glad_glVertexAttrib1dNV(index, x);
    _post_call_gl_callback(NULL, "glVertexAttrib1dNV", (GLADapiproc) glad_glVertexAttrib1dNV, 2, index, x);
    
}
PFNGLVERTEXATTRIB1DNVPROC glad_debug_glVertexAttrib1dNV = glad_debug_impl_glVertexAttrib1dNV;
PFNGLVERTEXATTRIB1DVPROC glad_glVertexAttrib1dv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1dv(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib1dv", (GLADapiproc) glad_glVertexAttrib1dv, 2, index, v);
    glad_glVertexAttrib1dv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib1dv", (GLADapiproc) glad_glVertexAttrib1dv, 2, index, v);
    
}
PFNGLVERTEXATTRIB1DVPROC glad_debug_glVertexAttrib1dv = glad_debug_impl_glVertexAttrib1dv;
PFNGLVERTEXATTRIB1DVARBPROC glad_glVertexAttrib1dvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1dvARB(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib1dvARB", (GLADapiproc) glad_glVertexAttrib1dvARB, 2, index, v);
    glad_glVertexAttrib1dvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib1dvARB", (GLADapiproc) glad_glVertexAttrib1dvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB1DVARBPROC glad_debug_glVertexAttrib1dvARB = glad_debug_impl_glVertexAttrib1dvARB;
PFNGLVERTEXATTRIB1DVNVPROC glad_glVertexAttrib1dvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1dvNV(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib1dvNV", (GLADapiproc) glad_glVertexAttrib1dvNV, 2, index, v);
    glad_glVertexAttrib1dvNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib1dvNV", (GLADapiproc) glad_glVertexAttrib1dvNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB1DVNVPROC glad_debug_glVertexAttrib1dvNV = glad_debug_impl_glVertexAttrib1dvNV;
PFNGLVERTEXATTRIB1FPROC glad_glVertexAttrib1f = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1f(GLuint index, GLfloat x) {
    _pre_call_gl_callback("glVertexAttrib1f", (GLADapiproc) glad_glVertexAttrib1f, 2, index, x);
    glad_glVertexAttrib1f(index, x);
    _post_call_gl_callback(NULL, "glVertexAttrib1f", (GLADapiproc) glad_glVertexAttrib1f, 2, index, x);
    
}
PFNGLVERTEXATTRIB1FPROC glad_debug_glVertexAttrib1f = glad_debug_impl_glVertexAttrib1f;
PFNGLVERTEXATTRIB1FARBPROC glad_glVertexAttrib1fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1fARB(GLuint index, GLfloat x) {
    _pre_call_gl_callback("glVertexAttrib1fARB", (GLADapiproc) glad_glVertexAttrib1fARB, 2, index, x);
    glad_glVertexAttrib1fARB(index, x);
    _post_call_gl_callback(NULL, "glVertexAttrib1fARB", (GLADapiproc) glad_glVertexAttrib1fARB, 2, index, x);
    
}
PFNGLVERTEXATTRIB1FARBPROC glad_debug_glVertexAttrib1fARB = glad_debug_impl_glVertexAttrib1fARB;
PFNGLVERTEXATTRIB1FNVPROC glad_glVertexAttrib1fNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1fNV(GLuint index, GLfloat x) {
    _pre_call_gl_callback("glVertexAttrib1fNV", (GLADapiproc) glad_glVertexAttrib1fNV, 2, index, x);
    glad_glVertexAttrib1fNV(index, x);
    _post_call_gl_callback(NULL, "glVertexAttrib1fNV", (GLADapiproc) glad_glVertexAttrib1fNV, 2, index, x);
    
}
PFNGLVERTEXATTRIB1FNVPROC glad_debug_glVertexAttrib1fNV = glad_debug_impl_glVertexAttrib1fNV;
PFNGLVERTEXATTRIB1FVPROC glad_glVertexAttrib1fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1fv(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib1fv", (GLADapiproc) glad_glVertexAttrib1fv, 2, index, v);
    glad_glVertexAttrib1fv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib1fv", (GLADapiproc) glad_glVertexAttrib1fv, 2, index, v);
    
}
PFNGLVERTEXATTRIB1FVPROC glad_debug_glVertexAttrib1fv = glad_debug_impl_glVertexAttrib1fv;
PFNGLVERTEXATTRIB1FVARBPROC glad_glVertexAttrib1fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1fvARB(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib1fvARB", (GLADapiproc) glad_glVertexAttrib1fvARB, 2, index, v);
    glad_glVertexAttrib1fvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib1fvARB", (GLADapiproc) glad_glVertexAttrib1fvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB1FVARBPROC glad_debug_glVertexAttrib1fvARB = glad_debug_impl_glVertexAttrib1fvARB;
PFNGLVERTEXATTRIB1FVNVPROC glad_glVertexAttrib1fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1fvNV(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib1fvNV", (GLADapiproc) glad_glVertexAttrib1fvNV, 2, index, v);
    glad_glVertexAttrib1fvNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib1fvNV", (GLADapiproc) glad_glVertexAttrib1fvNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB1FVNVPROC glad_debug_glVertexAttrib1fvNV = glad_debug_impl_glVertexAttrib1fvNV;
PFNGLVERTEXATTRIB1SPROC glad_glVertexAttrib1s = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1s(GLuint index, GLshort x) {
    _pre_call_gl_callback("glVertexAttrib1s", (GLADapiproc) glad_glVertexAttrib1s, 2, index, x);
    glad_glVertexAttrib1s(index, x);
    _post_call_gl_callback(NULL, "glVertexAttrib1s", (GLADapiproc) glad_glVertexAttrib1s, 2, index, x);
    
}
PFNGLVERTEXATTRIB1SPROC glad_debug_glVertexAttrib1s = glad_debug_impl_glVertexAttrib1s;
PFNGLVERTEXATTRIB1SARBPROC glad_glVertexAttrib1sARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1sARB(GLuint index, GLshort x) {
    _pre_call_gl_callback("glVertexAttrib1sARB", (GLADapiproc) glad_glVertexAttrib1sARB, 2, index, x);
    glad_glVertexAttrib1sARB(index, x);
    _post_call_gl_callback(NULL, "glVertexAttrib1sARB", (GLADapiproc) glad_glVertexAttrib1sARB, 2, index, x);
    
}
PFNGLVERTEXATTRIB1SARBPROC glad_debug_glVertexAttrib1sARB = glad_debug_impl_glVertexAttrib1sARB;
PFNGLVERTEXATTRIB1SNVPROC glad_glVertexAttrib1sNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1sNV(GLuint index, GLshort x) {
    _pre_call_gl_callback("glVertexAttrib1sNV", (GLADapiproc) glad_glVertexAttrib1sNV, 2, index, x);
    glad_glVertexAttrib1sNV(index, x);
    _post_call_gl_callback(NULL, "glVertexAttrib1sNV", (GLADapiproc) glad_glVertexAttrib1sNV, 2, index, x);
    
}
PFNGLVERTEXATTRIB1SNVPROC glad_debug_glVertexAttrib1sNV = glad_debug_impl_glVertexAttrib1sNV;
PFNGLVERTEXATTRIB1SVPROC glad_glVertexAttrib1sv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1sv(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib1sv", (GLADapiproc) glad_glVertexAttrib1sv, 2, index, v);
    glad_glVertexAttrib1sv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib1sv", (GLADapiproc) glad_glVertexAttrib1sv, 2, index, v);
    
}
PFNGLVERTEXATTRIB1SVPROC glad_debug_glVertexAttrib1sv = glad_debug_impl_glVertexAttrib1sv;
PFNGLVERTEXATTRIB1SVARBPROC glad_glVertexAttrib1svARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1svARB(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib1svARB", (GLADapiproc) glad_glVertexAttrib1svARB, 2, index, v);
    glad_glVertexAttrib1svARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib1svARB", (GLADapiproc) glad_glVertexAttrib1svARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB1SVARBPROC glad_debug_glVertexAttrib1svARB = glad_debug_impl_glVertexAttrib1svARB;
PFNGLVERTEXATTRIB1SVNVPROC glad_glVertexAttrib1svNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1svNV(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib1svNV", (GLADapiproc) glad_glVertexAttrib1svNV, 2, index, v);
    glad_glVertexAttrib1svNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib1svNV", (GLADapiproc) glad_glVertexAttrib1svNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB1SVNVPROC glad_debug_glVertexAttrib1svNV = glad_debug_impl_glVertexAttrib1svNV;
PFNGLVERTEXATTRIB2DPROC glad_glVertexAttrib2d = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2d(GLuint index, GLdouble x, GLdouble y) {
    _pre_call_gl_callback("glVertexAttrib2d", (GLADapiproc) glad_glVertexAttrib2d, 3, index, x, y);
    glad_glVertexAttrib2d(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttrib2d", (GLADapiproc) glad_glVertexAttrib2d, 3, index, x, y);
    
}
PFNGLVERTEXATTRIB2DPROC glad_debug_glVertexAttrib2d = glad_debug_impl_glVertexAttrib2d;
PFNGLVERTEXATTRIB2DARBPROC glad_glVertexAttrib2dARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2dARB(GLuint index, GLdouble x, GLdouble y) {
    _pre_call_gl_callback("glVertexAttrib2dARB", (GLADapiproc) glad_glVertexAttrib2dARB, 3, index, x, y);
    glad_glVertexAttrib2dARB(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttrib2dARB", (GLADapiproc) glad_glVertexAttrib2dARB, 3, index, x, y);
    
}
PFNGLVERTEXATTRIB2DARBPROC glad_debug_glVertexAttrib2dARB = glad_debug_impl_glVertexAttrib2dARB;
PFNGLVERTEXATTRIB2DNVPROC glad_glVertexAttrib2dNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2dNV(GLuint index, GLdouble x, GLdouble y) {
    _pre_call_gl_callback("glVertexAttrib2dNV", (GLADapiproc) glad_glVertexAttrib2dNV, 3, index, x, y);
    glad_glVertexAttrib2dNV(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttrib2dNV", (GLADapiproc) glad_glVertexAttrib2dNV, 3, index, x, y);
    
}
PFNGLVERTEXATTRIB2DNVPROC glad_debug_glVertexAttrib2dNV = glad_debug_impl_glVertexAttrib2dNV;
PFNGLVERTEXATTRIB2DVPROC glad_glVertexAttrib2dv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2dv(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib2dv", (GLADapiproc) glad_glVertexAttrib2dv, 2, index, v);
    glad_glVertexAttrib2dv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib2dv", (GLADapiproc) glad_glVertexAttrib2dv, 2, index, v);
    
}
PFNGLVERTEXATTRIB2DVPROC glad_debug_glVertexAttrib2dv = glad_debug_impl_glVertexAttrib2dv;
PFNGLVERTEXATTRIB2DVARBPROC glad_glVertexAttrib2dvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2dvARB(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib2dvARB", (GLADapiproc) glad_glVertexAttrib2dvARB, 2, index, v);
    glad_glVertexAttrib2dvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib2dvARB", (GLADapiproc) glad_glVertexAttrib2dvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB2DVARBPROC glad_debug_glVertexAttrib2dvARB = glad_debug_impl_glVertexAttrib2dvARB;
PFNGLVERTEXATTRIB2DVNVPROC glad_glVertexAttrib2dvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2dvNV(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib2dvNV", (GLADapiproc) glad_glVertexAttrib2dvNV, 2, index, v);
    glad_glVertexAttrib2dvNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib2dvNV", (GLADapiproc) glad_glVertexAttrib2dvNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB2DVNVPROC glad_debug_glVertexAttrib2dvNV = glad_debug_impl_glVertexAttrib2dvNV;
PFNGLVERTEXATTRIB2FPROC glad_glVertexAttrib2f = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
    _pre_call_gl_callback("glVertexAttrib2f", (GLADapiproc) glad_glVertexAttrib2f, 3, index, x, y);
    glad_glVertexAttrib2f(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttrib2f", (GLADapiproc) glad_glVertexAttrib2f, 3, index, x, y);
    
}
PFNGLVERTEXATTRIB2FPROC glad_debug_glVertexAttrib2f = glad_debug_impl_glVertexAttrib2f;
PFNGLVERTEXATTRIB2FARBPROC glad_glVertexAttrib2fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2fARB(GLuint index, GLfloat x, GLfloat y) {
    _pre_call_gl_callback("glVertexAttrib2fARB", (GLADapiproc) glad_glVertexAttrib2fARB, 3, index, x, y);
    glad_glVertexAttrib2fARB(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttrib2fARB", (GLADapiproc) glad_glVertexAttrib2fARB, 3, index, x, y);
    
}
PFNGLVERTEXATTRIB2FARBPROC glad_debug_glVertexAttrib2fARB = glad_debug_impl_glVertexAttrib2fARB;
PFNGLVERTEXATTRIB2FNVPROC glad_glVertexAttrib2fNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2fNV(GLuint index, GLfloat x, GLfloat y) {
    _pre_call_gl_callback("glVertexAttrib2fNV", (GLADapiproc) glad_glVertexAttrib2fNV, 3, index, x, y);
    glad_glVertexAttrib2fNV(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttrib2fNV", (GLADapiproc) glad_glVertexAttrib2fNV, 3, index, x, y);
    
}
PFNGLVERTEXATTRIB2FNVPROC glad_debug_glVertexAttrib2fNV = glad_debug_impl_glVertexAttrib2fNV;
PFNGLVERTEXATTRIB2FVPROC glad_glVertexAttrib2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2fv(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib2fv", (GLADapiproc) glad_glVertexAttrib2fv, 2, index, v);
    glad_glVertexAttrib2fv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib2fv", (GLADapiproc) glad_glVertexAttrib2fv, 2, index, v);
    
}
PFNGLVERTEXATTRIB2FVPROC glad_debug_glVertexAttrib2fv = glad_debug_impl_glVertexAttrib2fv;
PFNGLVERTEXATTRIB2FVARBPROC glad_glVertexAttrib2fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2fvARB(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib2fvARB", (GLADapiproc) glad_glVertexAttrib2fvARB, 2, index, v);
    glad_glVertexAttrib2fvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib2fvARB", (GLADapiproc) glad_glVertexAttrib2fvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB2FVARBPROC glad_debug_glVertexAttrib2fvARB = glad_debug_impl_glVertexAttrib2fvARB;
PFNGLVERTEXATTRIB2FVNVPROC glad_glVertexAttrib2fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2fvNV(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib2fvNV", (GLADapiproc) glad_glVertexAttrib2fvNV, 2, index, v);
    glad_glVertexAttrib2fvNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib2fvNV", (GLADapiproc) glad_glVertexAttrib2fvNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB2FVNVPROC glad_debug_glVertexAttrib2fvNV = glad_debug_impl_glVertexAttrib2fvNV;
PFNGLVERTEXATTRIB2SPROC glad_glVertexAttrib2s = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2s(GLuint index, GLshort x, GLshort y) {
    _pre_call_gl_callback("glVertexAttrib2s", (GLADapiproc) glad_glVertexAttrib2s, 3, index, x, y);
    glad_glVertexAttrib2s(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttrib2s", (GLADapiproc) glad_glVertexAttrib2s, 3, index, x, y);
    
}
PFNGLVERTEXATTRIB2SPROC glad_debug_glVertexAttrib2s = glad_debug_impl_glVertexAttrib2s;
PFNGLVERTEXATTRIB2SARBPROC glad_glVertexAttrib2sARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2sARB(GLuint index, GLshort x, GLshort y) {
    _pre_call_gl_callback("glVertexAttrib2sARB", (GLADapiproc) glad_glVertexAttrib2sARB, 3, index, x, y);
    glad_glVertexAttrib2sARB(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttrib2sARB", (GLADapiproc) glad_glVertexAttrib2sARB, 3, index, x, y);
    
}
PFNGLVERTEXATTRIB2SARBPROC glad_debug_glVertexAttrib2sARB = glad_debug_impl_glVertexAttrib2sARB;
PFNGLVERTEXATTRIB2SNVPROC glad_glVertexAttrib2sNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2sNV(GLuint index, GLshort x, GLshort y) {
    _pre_call_gl_callback("glVertexAttrib2sNV", (GLADapiproc) glad_glVertexAttrib2sNV, 3, index, x, y);
    glad_glVertexAttrib2sNV(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttrib2sNV", (GLADapiproc) glad_glVertexAttrib2sNV, 3, index, x, y);
    
}
PFNGLVERTEXATTRIB2SNVPROC glad_debug_glVertexAttrib2sNV = glad_debug_impl_glVertexAttrib2sNV;
PFNGLVERTEXATTRIB2SVPROC glad_glVertexAttrib2sv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2sv(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib2sv", (GLADapiproc) glad_glVertexAttrib2sv, 2, index, v);
    glad_glVertexAttrib2sv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib2sv", (GLADapiproc) glad_glVertexAttrib2sv, 2, index, v);
    
}
PFNGLVERTEXATTRIB2SVPROC glad_debug_glVertexAttrib2sv = glad_debug_impl_glVertexAttrib2sv;
PFNGLVERTEXATTRIB2SVARBPROC glad_glVertexAttrib2svARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2svARB(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib2svARB", (GLADapiproc) glad_glVertexAttrib2svARB, 2, index, v);
    glad_glVertexAttrib2svARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib2svARB", (GLADapiproc) glad_glVertexAttrib2svARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB2SVARBPROC glad_debug_glVertexAttrib2svARB = glad_debug_impl_glVertexAttrib2svARB;
PFNGLVERTEXATTRIB2SVNVPROC glad_glVertexAttrib2svNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2svNV(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib2svNV", (GLADapiproc) glad_glVertexAttrib2svNV, 2, index, v);
    glad_glVertexAttrib2svNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib2svNV", (GLADapiproc) glad_glVertexAttrib2svNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB2SVNVPROC glad_debug_glVertexAttrib2svNV = glad_debug_impl_glVertexAttrib2svNV;
PFNGLVERTEXATTRIB3DPROC glad_glVertexAttrib3d = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3d(GLuint index, GLdouble x, GLdouble y, GLdouble z) {
    _pre_call_gl_callback("glVertexAttrib3d", (GLADapiproc) glad_glVertexAttrib3d, 4, index, x, y, z);
    glad_glVertexAttrib3d(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttrib3d", (GLADapiproc) glad_glVertexAttrib3d, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIB3DPROC glad_debug_glVertexAttrib3d = glad_debug_impl_glVertexAttrib3d;
PFNGLVERTEXATTRIB3DARBPROC glad_glVertexAttrib3dARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3dARB(GLuint index, GLdouble x, GLdouble y, GLdouble z) {
    _pre_call_gl_callback("glVertexAttrib3dARB", (GLADapiproc) glad_glVertexAttrib3dARB, 4, index, x, y, z);
    glad_glVertexAttrib3dARB(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttrib3dARB", (GLADapiproc) glad_glVertexAttrib3dARB, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIB3DARBPROC glad_debug_glVertexAttrib3dARB = glad_debug_impl_glVertexAttrib3dARB;
PFNGLVERTEXATTRIB3DNVPROC glad_glVertexAttrib3dNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3dNV(GLuint index, GLdouble x, GLdouble y, GLdouble z) {
    _pre_call_gl_callback("glVertexAttrib3dNV", (GLADapiproc) glad_glVertexAttrib3dNV, 4, index, x, y, z);
    glad_glVertexAttrib3dNV(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttrib3dNV", (GLADapiproc) glad_glVertexAttrib3dNV, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIB3DNVPROC glad_debug_glVertexAttrib3dNV = glad_debug_impl_glVertexAttrib3dNV;
PFNGLVERTEXATTRIB3DVPROC glad_glVertexAttrib3dv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3dv(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib3dv", (GLADapiproc) glad_glVertexAttrib3dv, 2, index, v);
    glad_glVertexAttrib3dv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib3dv", (GLADapiproc) glad_glVertexAttrib3dv, 2, index, v);
    
}
PFNGLVERTEXATTRIB3DVPROC glad_debug_glVertexAttrib3dv = glad_debug_impl_glVertexAttrib3dv;
PFNGLVERTEXATTRIB3DVARBPROC glad_glVertexAttrib3dvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3dvARB(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib3dvARB", (GLADapiproc) glad_glVertexAttrib3dvARB, 2, index, v);
    glad_glVertexAttrib3dvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib3dvARB", (GLADapiproc) glad_glVertexAttrib3dvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB3DVARBPROC glad_debug_glVertexAttrib3dvARB = glad_debug_impl_glVertexAttrib3dvARB;
PFNGLVERTEXATTRIB3DVNVPROC glad_glVertexAttrib3dvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3dvNV(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib3dvNV", (GLADapiproc) glad_glVertexAttrib3dvNV, 2, index, v);
    glad_glVertexAttrib3dvNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib3dvNV", (GLADapiproc) glad_glVertexAttrib3dvNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB3DVNVPROC glad_debug_glVertexAttrib3dvNV = glad_debug_impl_glVertexAttrib3dvNV;
PFNGLVERTEXATTRIB3FPROC glad_glVertexAttrib3f = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gl_callback("glVertexAttrib3f", (GLADapiproc) glad_glVertexAttrib3f, 4, index, x, y, z);
    glad_glVertexAttrib3f(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttrib3f", (GLADapiproc) glad_glVertexAttrib3f, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIB3FPROC glad_debug_glVertexAttrib3f = glad_debug_impl_glVertexAttrib3f;
PFNGLVERTEXATTRIB3FARBPROC glad_glVertexAttrib3fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3fARB(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gl_callback("glVertexAttrib3fARB", (GLADapiproc) glad_glVertexAttrib3fARB, 4, index, x, y, z);
    glad_glVertexAttrib3fARB(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttrib3fARB", (GLADapiproc) glad_glVertexAttrib3fARB, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIB3FARBPROC glad_debug_glVertexAttrib3fARB = glad_debug_impl_glVertexAttrib3fARB;
PFNGLVERTEXATTRIB3FNVPROC glad_glVertexAttrib3fNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3fNV(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gl_callback("glVertexAttrib3fNV", (GLADapiproc) glad_glVertexAttrib3fNV, 4, index, x, y, z);
    glad_glVertexAttrib3fNV(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttrib3fNV", (GLADapiproc) glad_glVertexAttrib3fNV, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIB3FNVPROC glad_debug_glVertexAttrib3fNV = glad_debug_impl_glVertexAttrib3fNV;
PFNGLVERTEXATTRIB3FVPROC glad_glVertexAttrib3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3fv(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib3fv", (GLADapiproc) glad_glVertexAttrib3fv, 2, index, v);
    glad_glVertexAttrib3fv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib3fv", (GLADapiproc) glad_glVertexAttrib3fv, 2, index, v);
    
}
PFNGLVERTEXATTRIB3FVPROC glad_debug_glVertexAttrib3fv = glad_debug_impl_glVertexAttrib3fv;
PFNGLVERTEXATTRIB3FVARBPROC glad_glVertexAttrib3fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3fvARB(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib3fvARB", (GLADapiproc) glad_glVertexAttrib3fvARB, 2, index, v);
    glad_glVertexAttrib3fvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib3fvARB", (GLADapiproc) glad_glVertexAttrib3fvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB3FVARBPROC glad_debug_glVertexAttrib3fvARB = glad_debug_impl_glVertexAttrib3fvARB;
PFNGLVERTEXATTRIB3FVNVPROC glad_glVertexAttrib3fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3fvNV(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib3fvNV", (GLADapiproc) glad_glVertexAttrib3fvNV, 2, index, v);
    glad_glVertexAttrib3fvNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib3fvNV", (GLADapiproc) glad_glVertexAttrib3fvNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB3FVNVPROC glad_debug_glVertexAttrib3fvNV = glad_debug_impl_glVertexAttrib3fvNV;
PFNGLVERTEXATTRIB3SPROC glad_glVertexAttrib3s = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3s(GLuint index, GLshort x, GLshort y, GLshort z) {
    _pre_call_gl_callback("glVertexAttrib3s", (GLADapiproc) glad_glVertexAttrib3s, 4, index, x, y, z);
    glad_glVertexAttrib3s(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttrib3s", (GLADapiproc) glad_glVertexAttrib3s, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIB3SPROC glad_debug_glVertexAttrib3s = glad_debug_impl_glVertexAttrib3s;
PFNGLVERTEXATTRIB3SARBPROC glad_glVertexAttrib3sARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3sARB(GLuint index, GLshort x, GLshort y, GLshort z) {
    _pre_call_gl_callback("glVertexAttrib3sARB", (GLADapiproc) glad_glVertexAttrib3sARB, 4, index, x, y, z);
    glad_glVertexAttrib3sARB(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttrib3sARB", (GLADapiproc) glad_glVertexAttrib3sARB, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIB3SARBPROC glad_debug_glVertexAttrib3sARB = glad_debug_impl_glVertexAttrib3sARB;
PFNGLVERTEXATTRIB3SNVPROC glad_glVertexAttrib3sNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3sNV(GLuint index, GLshort x, GLshort y, GLshort z) {
    _pre_call_gl_callback("glVertexAttrib3sNV", (GLADapiproc) glad_glVertexAttrib3sNV, 4, index, x, y, z);
    glad_glVertexAttrib3sNV(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttrib3sNV", (GLADapiproc) glad_glVertexAttrib3sNV, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIB3SNVPROC glad_debug_glVertexAttrib3sNV = glad_debug_impl_glVertexAttrib3sNV;
PFNGLVERTEXATTRIB3SVPROC glad_glVertexAttrib3sv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3sv(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib3sv", (GLADapiproc) glad_glVertexAttrib3sv, 2, index, v);
    glad_glVertexAttrib3sv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib3sv", (GLADapiproc) glad_glVertexAttrib3sv, 2, index, v);
    
}
PFNGLVERTEXATTRIB3SVPROC glad_debug_glVertexAttrib3sv = glad_debug_impl_glVertexAttrib3sv;
PFNGLVERTEXATTRIB3SVARBPROC glad_glVertexAttrib3svARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3svARB(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib3svARB", (GLADapiproc) glad_glVertexAttrib3svARB, 2, index, v);
    glad_glVertexAttrib3svARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib3svARB", (GLADapiproc) glad_glVertexAttrib3svARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB3SVARBPROC glad_debug_glVertexAttrib3svARB = glad_debug_impl_glVertexAttrib3svARB;
PFNGLVERTEXATTRIB3SVNVPROC glad_glVertexAttrib3svNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3svNV(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib3svNV", (GLADapiproc) glad_glVertexAttrib3svNV, 2, index, v);
    glad_glVertexAttrib3svNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib3svNV", (GLADapiproc) glad_glVertexAttrib3svNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB3SVNVPROC glad_debug_glVertexAttrib3svNV = glad_debug_impl_glVertexAttrib3svNV;
PFNGLVERTEXATTRIB4NBVPROC glad_glVertexAttrib4Nbv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4Nbv(GLuint index, const GLbyte * v) {
    _pre_call_gl_callback("glVertexAttrib4Nbv", (GLADapiproc) glad_glVertexAttrib4Nbv, 2, index, v);
    glad_glVertexAttrib4Nbv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4Nbv", (GLADapiproc) glad_glVertexAttrib4Nbv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NBVPROC glad_debug_glVertexAttrib4Nbv = glad_debug_impl_glVertexAttrib4Nbv;
PFNGLVERTEXATTRIB4NBVARBPROC glad_glVertexAttrib4NbvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4NbvARB(GLuint index, const GLbyte * v) {
    _pre_call_gl_callback("glVertexAttrib4NbvARB", (GLADapiproc) glad_glVertexAttrib4NbvARB, 2, index, v);
    glad_glVertexAttrib4NbvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4NbvARB", (GLADapiproc) glad_glVertexAttrib4NbvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NBVARBPROC glad_debug_glVertexAttrib4NbvARB = glad_debug_impl_glVertexAttrib4NbvARB;
PFNGLVERTEXATTRIB4NIVPROC glad_glVertexAttrib4Niv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4Niv(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttrib4Niv", (GLADapiproc) glad_glVertexAttrib4Niv, 2, index, v);
    glad_glVertexAttrib4Niv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4Niv", (GLADapiproc) glad_glVertexAttrib4Niv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NIVPROC glad_debug_glVertexAttrib4Niv = glad_debug_impl_glVertexAttrib4Niv;
PFNGLVERTEXATTRIB4NIVARBPROC glad_glVertexAttrib4NivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4NivARB(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttrib4NivARB", (GLADapiproc) glad_glVertexAttrib4NivARB, 2, index, v);
    glad_glVertexAttrib4NivARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4NivARB", (GLADapiproc) glad_glVertexAttrib4NivARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NIVARBPROC glad_debug_glVertexAttrib4NivARB = glad_debug_impl_glVertexAttrib4NivARB;
PFNGLVERTEXATTRIB4NSVPROC glad_glVertexAttrib4Nsv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4Nsv(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib4Nsv", (GLADapiproc) glad_glVertexAttrib4Nsv, 2, index, v);
    glad_glVertexAttrib4Nsv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4Nsv", (GLADapiproc) glad_glVertexAttrib4Nsv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NSVPROC glad_debug_glVertexAttrib4Nsv = glad_debug_impl_glVertexAttrib4Nsv;
PFNGLVERTEXATTRIB4NSVARBPROC glad_glVertexAttrib4NsvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4NsvARB(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib4NsvARB", (GLADapiproc) glad_glVertexAttrib4NsvARB, 2, index, v);
    glad_glVertexAttrib4NsvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4NsvARB", (GLADapiproc) glad_glVertexAttrib4NsvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NSVARBPROC glad_debug_glVertexAttrib4NsvARB = glad_debug_impl_glVertexAttrib4NsvARB;
PFNGLVERTEXATTRIB4NUBPROC glad_glVertexAttrib4Nub = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4Nub(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w) {
    _pre_call_gl_callback("glVertexAttrib4Nub", (GLADapiproc) glad_glVertexAttrib4Nub, 5, index, x, y, z, w);
    glad_glVertexAttrib4Nub(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4Nub", (GLADapiproc) glad_glVertexAttrib4Nub, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4NUBPROC glad_debug_glVertexAttrib4Nub = glad_debug_impl_glVertexAttrib4Nub;
PFNGLVERTEXATTRIB4NUBARBPROC glad_glVertexAttrib4NubARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4NubARB(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w) {
    _pre_call_gl_callback("glVertexAttrib4NubARB", (GLADapiproc) glad_glVertexAttrib4NubARB, 5, index, x, y, z, w);
    glad_glVertexAttrib4NubARB(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4NubARB", (GLADapiproc) glad_glVertexAttrib4NubARB, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4NUBARBPROC glad_debug_glVertexAttrib4NubARB = glad_debug_impl_glVertexAttrib4NubARB;
PFNGLVERTEXATTRIB4NUBVPROC glad_glVertexAttrib4Nubv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4Nubv(GLuint index, const GLubyte * v) {
    _pre_call_gl_callback("glVertexAttrib4Nubv", (GLADapiproc) glad_glVertexAttrib4Nubv, 2, index, v);
    glad_glVertexAttrib4Nubv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4Nubv", (GLADapiproc) glad_glVertexAttrib4Nubv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NUBVPROC glad_debug_glVertexAttrib4Nubv = glad_debug_impl_glVertexAttrib4Nubv;
PFNGLVERTEXATTRIB4NUBVARBPROC glad_glVertexAttrib4NubvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4NubvARB(GLuint index, const GLubyte * v) {
    _pre_call_gl_callback("glVertexAttrib4NubvARB", (GLADapiproc) glad_glVertexAttrib4NubvARB, 2, index, v);
    glad_glVertexAttrib4NubvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4NubvARB", (GLADapiproc) glad_glVertexAttrib4NubvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NUBVARBPROC glad_debug_glVertexAttrib4NubvARB = glad_debug_impl_glVertexAttrib4NubvARB;
PFNGLVERTEXATTRIB4NUIVPROC glad_glVertexAttrib4Nuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4Nuiv(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttrib4Nuiv", (GLADapiproc) glad_glVertexAttrib4Nuiv, 2, index, v);
    glad_glVertexAttrib4Nuiv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4Nuiv", (GLADapiproc) glad_glVertexAttrib4Nuiv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NUIVPROC glad_debug_glVertexAttrib4Nuiv = glad_debug_impl_glVertexAttrib4Nuiv;
PFNGLVERTEXATTRIB4NUIVARBPROC glad_glVertexAttrib4NuivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4NuivARB(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttrib4NuivARB", (GLADapiproc) glad_glVertexAttrib4NuivARB, 2, index, v);
    glad_glVertexAttrib4NuivARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4NuivARB", (GLADapiproc) glad_glVertexAttrib4NuivARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NUIVARBPROC glad_debug_glVertexAttrib4NuivARB = glad_debug_impl_glVertexAttrib4NuivARB;
PFNGLVERTEXATTRIB4NUSVPROC glad_glVertexAttrib4Nusv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4Nusv(GLuint index, const GLushort * v) {
    _pre_call_gl_callback("glVertexAttrib4Nusv", (GLADapiproc) glad_glVertexAttrib4Nusv, 2, index, v);
    glad_glVertexAttrib4Nusv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4Nusv", (GLADapiproc) glad_glVertexAttrib4Nusv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NUSVPROC glad_debug_glVertexAttrib4Nusv = glad_debug_impl_glVertexAttrib4Nusv;
PFNGLVERTEXATTRIB4NUSVARBPROC glad_glVertexAttrib4NusvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4NusvARB(GLuint index, const GLushort * v) {
    _pre_call_gl_callback("glVertexAttrib4NusvARB", (GLADapiproc) glad_glVertexAttrib4NusvARB, 2, index, v);
    glad_glVertexAttrib4NusvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4NusvARB", (GLADapiproc) glad_glVertexAttrib4NusvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4NUSVARBPROC glad_debug_glVertexAttrib4NusvARB = glad_debug_impl_glVertexAttrib4NusvARB;
PFNGLVERTEXATTRIB4BVPROC glad_glVertexAttrib4bv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4bv(GLuint index, const GLbyte * v) {
    _pre_call_gl_callback("glVertexAttrib4bv", (GLADapiproc) glad_glVertexAttrib4bv, 2, index, v);
    glad_glVertexAttrib4bv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4bv", (GLADapiproc) glad_glVertexAttrib4bv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4BVPROC glad_debug_glVertexAttrib4bv = glad_debug_impl_glVertexAttrib4bv;
PFNGLVERTEXATTRIB4BVARBPROC glad_glVertexAttrib4bvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4bvARB(GLuint index, const GLbyte * v) {
    _pre_call_gl_callback("glVertexAttrib4bvARB", (GLADapiproc) glad_glVertexAttrib4bvARB, 2, index, v);
    glad_glVertexAttrib4bvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4bvARB", (GLADapiproc) glad_glVertexAttrib4bvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4BVARBPROC glad_debug_glVertexAttrib4bvARB = glad_debug_impl_glVertexAttrib4bvARB;
PFNGLVERTEXATTRIB4DPROC glad_glVertexAttrib4d = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    _pre_call_gl_callback("glVertexAttrib4d", (GLADapiproc) glad_glVertexAttrib4d, 5, index, x, y, z, w);
    glad_glVertexAttrib4d(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4d", (GLADapiproc) glad_glVertexAttrib4d, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4DPROC glad_debug_glVertexAttrib4d = glad_debug_impl_glVertexAttrib4d;
PFNGLVERTEXATTRIB4DARBPROC glad_glVertexAttrib4dARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4dARB(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    _pre_call_gl_callback("glVertexAttrib4dARB", (GLADapiproc) glad_glVertexAttrib4dARB, 5, index, x, y, z, w);
    glad_glVertexAttrib4dARB(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4dARB", (GLADapiproc) glad_glVertexAttrib4dARB, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4DARBPROC glad_debug_glVertexAttrib4dARB = glad_debug_impl_glVertexAttrib4dARB;
PFNGLVERTEXATTRIB4DNVPROC glad_glVertexAttrib4dNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4dNV(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    _pre_call_gl_callback("glVertexAttrib4dNV", (GLADapiproc) glad_glVertexAttrib4dNV, 5, index, x, y, z, w);
    glad_glVertexAttrib4dNV(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4dNV", (GLADapiproc) glad_glVertexAttrib4dNV, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4DNVPROC glad_debug_glVertexAttrib4dNV = glad_debug_impl_glVertexAttrib4dNV;
PFNGLVERTEXATTRIB4DVPROC glad_glVertexAttrib4dv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4dv(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib4dv", (GLADapiproc) glad_glVertexAttrib4dv, 2, index, v);
    glad_glVertexAttrib4dv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4dv", (GLADapiproc) glad_glVertexAttrib4dv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4DVPROC glad_debug_glVertexAttrib4dv = glad_debug_impl_glVertexAttrib4dv;
PFNGLVERTEXATTRIB4DVARBPROC glad_glVertexAttrib4dvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4dvARB(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib4dvARB", (GLADapiproc) glad_glVertexAttrib4dvARB, 2, index, v);
    glad_glVertexAttrib4dvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4dvARB", (GLADapiproc) glad_glVertexAttrib4dvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4DVARBPROC glad_debug_glVertexAttrib4dvARB = glad_debug_impl_glVertexAttrib4dvARB;
PFNGLVERTEXATTRIB4DVNVPROC glad_glVertexAttrib4dvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4dvNV(GLuint index, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttrib4dvNV", (GLADapiproc) glad_glVertexAttrib4dvNV, 2, index, v);
    glad_glVertexAttrib4dvNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4dvNV", (GLADapiproc) glad_glVertexAttrib4dvNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB4DVNVPROC glad_debug_glVertexAttrib4dvNV = glad_debug_impl_glVertexAttrib4dvNV;
PFNGLVERTEXATTRIB4FPROC glad_glVertexAttrib4f = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    _pre_call_gl_callback("glVertexAttrib4f", (GLADapiproc) glad_glVertexAttrib4f, 5, index, x, y, z, w);
    glad_glVertexAttrib4f(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4f", (GLADapiproc) glad_glVertexAttrib4f, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4FPROC glad_debug_glVertexAttrib4f = glad_debug_impl_glVertexAttrib4f;
PFNGLVERTEXATTRIB4FARBPROC glad_glVertexAttrib4fARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4fARB(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    _pre_call_gl_callback("glVertexAttrib4fARB", (GLADapiproc) glad_glVertexAttrib4fARB, 5, index, x, y, z, w);
    glad_glVertexAttrib4fARB(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4fARB", (GLADapiproc) glad_glVertexAttrib4fARB, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4FARBPROC glad_debug_glVertexAttrib4fARB = glad_debug_impl_glVertexAttrib4fARB;
PFNGLVERTEXATTRIB4FNVPROC glad_glVertexAttrib4fNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4fNV(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    _pre_call_gl_callback("glVertexAttrib4fNV", (GLADapiproc) glad_glVertexAttrib4fNV, 5, index, x, y, z, w);
    glad_glVertexAttrib4fNV(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4fNV", (GLADapiproc) glad_glVertexAttrib4fNV, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4FNVPROC glad_debug_glVertexAttrib4fNV = glad_debug_impl_glVertexAttrib4fNV;
PFNGLVERTEXATTRIB4FVPROC glad_glVertexAttrib4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4fv(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib4fv", (GLADapiproc) glad_glVertexAttrib4fv, 2, index, v);
    glad_glVertexAttrib4fv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4fv", (GLADapiproc) glad_glVertexAttrib4fv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4FVPROC glad_debug_glVertexAttrib4fv = glad_debug_impl_glVertexAttrib4fv;
PFNGLVERTEXATTRIB4FVARBPROC glad_glVertexAttrib4fvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4fvARB(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib4fvARB", (GLADapiproc) glad_glVertexAttrib4fvARB, 2, index, v);
    glad_glVertexAttrib4fvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4fvARB", (GLADapiproc) glad_glVertexAttrib4fvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4FVARBPROC glad_debug_glVertexAttrib4fvARB = glad_debug_impl_glVertexAttrib4fvARB;
PFNGLVERTEXATTRIB4FVNVPROC glad_glVertexAttrib4fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4fvNV(GLuint index, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttrib4fvNV", (GLADapiproc) glad_glVertexAttrib4fvNV, 2, index, v);
    glad_glVertexAttrib4fvNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4fvNV", (GLADapiproc) glad_glVertexAttrib4fvNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB4FVNVPROC glad_debug_glVertexAttrib4fvNV = glad_debug_impl_glVertexAttrib4fvNV;
PFNGLVERTEXATTRIB4IVPROC glad_glVertexAttrib4iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4iv(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttrib4iv", (GLADapiproc) glad_glVertexAttrib4iv, 2, index, v);
    glad_glVertexAttrib4iv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4iv", (GLADapiproc) glad_glVertexAttrib4iv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4IVPROC glad_debug_glVertexAttrib4iv = glad_debug_impl_glVertexAttrib4iv;
PFNGLVERTEXATTRIB4IVARBPROC glad_glVertexAttrib4ivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4ivARB(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttrib4ivARB", (GLADapiproc) glad_glVertexAttrib4ivARB, 2, index, v);
    glad_glVertexAttrib4ivARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4ivARB", (GLADapiproc) glad_glVertexAttrib4ivARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4IVARBPROC glad_debug_glVertexAttrib4ivARB = glad_debug_impl_glVertexAttrib4ivARB;
PFNGLVERTEXATTRIB4SPROC glad_glVertexAttrib4s = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4s(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w) {
    _pre_call_gl_callback("glVertexAttrib4s", (GLADapiproc) glad_glVertexAttrib4s, 5, index, x, y, z, w);
    glad_glVertexAttrib4s(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4s", (GLADapiproc) glad_glVertexAttrib4s, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4SPROC glad_debug_glVertexAttrib4s = glad_debug_impl_glVertexAttrib4s;
PFNGLVERTEXATTRIB4SARBPROC glad_glVertexAttrib4sARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4sARB(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w) {
    _pre_call_gl_callback("glVertexAttrib4sARB", (GLADapiproc) glad_glVertexAttrib4sARB, 5, index, x, y, z, w);
    glad_glVertexAttrib4sARB(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4sARB", (GLADapiproc) glad_glVertexAttrib4sARB, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4SARBPROC glad_debug_glVertexAttrib4sARB = glad_debug_impl_glVertexAttrib4sARB;
PFNGLVERTEXATTRIB4SNVPROC glad_glVertexAttrib4sNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4sNV(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w) {
    _pre_call_gl_callback("glVertexAttrib4sNV", (GLADapiproc) glad_glVertexAttrib4sNV, 5, index, x, y, z, w);
    glad_glVertexAttrib4sNV(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4sNV", (GLADapiproc) glad_glVertexAttrib4sNV, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4SNVPROC glad_debug_glVertexAttrib4sNV = glad_debug_impl_glVertexAttrib4sNV;
PFNGLVERTEXATTRIB4SVPROC glad_glVertexAttrib4sv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4sv(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib4sv", (GLADapiproc) glad_glVertexAttrib4sv, 2, index, v);
    glad_glVertexAttrib4sv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4sv", (GLADapiproc) glad_glVertexAttrib4sv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4SVPROC glad_debug_glVertexAttrib4sv = glad_debug_impl_glVertexAttrib4sv;
PFNGLVERTEXATTRIB4SVARBPROC glad_glVertexAttrib4svARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4svARB(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib4svARB", (GLADapiproc) glad_glVertexAttrib4svARB, 2, index, v);
    glad_glVertexAttrib4svARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4svARB", (GLADapiproc) glad_glVertexAttrib4svARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4SVARBPROC glad_debug_glVertexAttrib4svARB = glad_debug_impl_glVertexAttrib4svARB;
PFNGLVERTEXATTRIB4SVNVPROC glad_glVertexAttrib4svNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4svNV(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttrib4svNV", (GLADapiproc) glad_glVertexAttrib4svNV, 2, index, v);
    glad_glVertexAttrib4svNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4svNV", (GLADapiproc) glad_glVertexAttrib4svNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB4SVNVPROC glad_debug_glVertexAttrib4svNV = glad_debug_impl_glVertexAttrib4svNV;
PFNGLVERTEXATTRIB4UBNVPROC glad_glVertexAttrib4ubNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4ubNV(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w) {
    _pre_call_gl_callback("glVertexAttrib4ubNV", (GLADapiproc) glad_glVertexAttrib4ubNV, 5, index, x, y, z, w);
    glad_glVertexAttrib4ubNV(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttrib4ubNV", (GLADapiproc) glad_glVertexAttrib4ubNV, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4UBNVPROC glad_debug_glVertexAttrib4ubNV = glad_debug_impl_glVertexAttrib4ubNV;
PFNGLVERTEXATTRIB4UBVPROC glad_glVertexAttrib4ubv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4ubv(GLuint index, const GLubyte * v) {
    _pre_call_gl_callback("glVertexAttrib4ubv", (GLADapiproc) glad_glVertexAttrib4ubv, 2, index, v);
    glad_glVertexAttrib4ubv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4ubv", (GLADapiproc) glad_glVertexAttrib4ubv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4UBVPROC glad_debug_glVertexAttrib4ubv = glad_debug_impl_glVertexAttrib4ubv;
PFNGLVERTEXATTRIB4UBVARBPROC glad_glVertexAttrib4ubvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4ubvARB(GLuint index, const GLubyte * v) {
    _pre_call_gl_callback("glVertexAttrib4ubvARB", (GLADapiproc) glad_glVertexAttrib4ubvARB, 2, index, v);
    glad_glVertexAttrib4ubvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4ubvARB", (GLADapiproc) glad_glVertexAttrib4ubvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4UBVARBPROC glad_debug_glVertexAttrib4ubvARB = glad_debug_impl_glVertexAttrib4ubvARB;
PFNGLVERTEXATTRIB4UBVNVPROC glad_glVertexAttrib4ubvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4ubvNV(GLuint index, const GLubyte * v) {
    _pre_call_gl_callback("glVertexAttrib4ubvNV", (GLADapiproc) glad_glVertexAttrib4ubvNV, 2, index, v);
    glad_glVertexAttrib4ubvNV(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4ubvNV", (GLADapiproc) glad_glVertexAttrib4ubvNV, 2, index, v);
    
}
PFNGLVERTEXATTRIB4UBVNVPROC glad_debug_glVertexAttrib4ubvNV = glad_debug_impl_glVertexAttrib4ubvNV;
PFNGLVERTEXATTRIB4UIVPROC glad_glVertexAttrib4uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4uiv(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttrib4uiv", (GLADapiproc) glad_glVertexAttrib4uiv, 2, index, v);
    glad_glVertexAttrib4uiv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4uiv", (GLADapiproc) glad_glVertexAttrib4uiv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4UIVPROC glad_debug_glVertexAttrib4uiv = glad_debug_impl_glVertexAttrib4uiv;
PFNGLVERTEXATTRIB4UIVARBPROC glad_glVertexAttrib4uivARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4uivARB(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttrib4uivARB", (GLADapiproc) glad_glVertexAttrib4uivARB, 2, index, v);
    glad_glVertexAttrib4uivARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4uivARB", (GLADapiproc) glad_glVertexAttrib4uivARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4UIVARBPROC glad_debug_glVertexAttrib4uivARB = glad_debug_impl_glVertexAttrib4uivARB;
PFNGLVERTEXATTRIB4USVPROC glad_glVertexAttrib4usv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4usv(GLuint index, const GLushort * v) {
    _pre_call_gl_callback("glVertexAttrib4usv", (GLADapiproc) glad_glVertexAttrib4usv, 2, index, v);
    glad_glVertexAttrib4usv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4usv", (GLADapiproc) glad_glVertexAttrib4usv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4USVPROC glad_debug_glVertexAttrib4usv = glad_debug_impl_glVertexAttrib4usv;
PFNGLVERTEXATTRIB4USVARBPROC glad_glVertexAttrib4usvARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4usvARB(GLuint index, const GLushort * v) {
    _pre_call_gl_callback("glVertexAttrib4usvARB", (GLADapiproc) glad_glVertexAttrib4usvARB, 2, index, v);
    glad_glVertexAttrib4usvARB(index, v);
    _post_call_gl_callback(NULL, "glVertexAttrib4usvARB", (GLADapiproc) glad_glVertexAttrib4usvARB, 2, index, v);
    
}
PFNGLVERTEXATTRIB4USVARBPROC glad_debug_glVertexAttrib4usvARB = glad_debug_impl_glVertexAttrib4usvARB;
PFNGLVERTEXATTRIBDIVISORPROC glad_glVertexAttribDivisor = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribDivisor(GLuint index, GLuint divisor) {
    _pre_call_gl_callback("glVertexAttribDivisor", (GLADapiproc) glad_glVertexAttribDivisor, 2, index, divisor);
    glad_glVertexAttribDivisor(index, divisor);
    _post_call_gl_callback(NULL, "glVertexAttribDivisor", (GLADapiproc) glad_glVertexAttribDivisor, 2, index, divisor);
    
}
PFNGLVERTEXATTRIBDIVISORPROC glad_debug_glVertexAttribDivisor = glad_debug_impl_glVertexAttribDivisor;
PFNGLVERTEXATTRIBDIVISORARBPROC glad_glVertexAttribDivisorARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribDivisorARB(GLuint index, GLuint divisor) {
    _pre_call_gl_callback("glVertexAttribDivisorARB", (GLADapiproc) glad_glVertexAttribDivisorARB, 2, index, divisor);
    glad_glVertexAttribDivisorARB(index, divisor);
    _post_call_gl_callback(NULL, "glVertexAttribDivisorARB", (GLADapiproc) glad_glVertexAttribDivisorARB, 2, index, divisor);
    
}
PFNGLVERTEXATTRIBDIVISORARBPROC glad_debug_glVertexAttribDivisorARB = glad_debug_impl_glVertexAttribDivisorARB;
PFNGLVERTEXATTRIBI1IPROC glad_glVertexAttribI1i = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI1i(GLuint index, GLint x) {
    _pre_call_gl_callback("glVertexAttribI1i", (GLADapiproc) glad_glVertexAttribI1i, 2, index, x);
    glad_glVertexAttribI1i(index, x);
    _post_call_gl_callback(NULL, "glVertexAttribI1i", (GLADapiproc) glad_glVertexAttribI1i, 2, index, x);
    
}
PFNGLVERTEXATTRIBI1IPROC glad_debug_glVertexAttribI1i = glad_debug_impl_glVertexAttribI1i;
PFNGLVERTEXATTRIBI1IEXTPROC glad_glVertexAttribI1iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI1iEXT(GLuint index, GLint x) {
    _pre_call_gl_callback("glVertexAttribI1iEXT", (GLADapiproc) glad_glVertexAttribI1iEXT, 2, index, x);
    glad_glVertexAttribI1iEXT(index, x);
    _post_call_gl_callback(NULL, "glVertexAttribI1iEXT", (GLADapiproc) glad_glVertexAttribI1iEXT, 2, index, x);
    
}
PFNGLVERTEXATTRIBI1IEXTPROC glad_debug_glVertexAttribI1iEXT = glad_debug_impl_glVertexAttribI1iEXT;
PFNGLVERTEXATTRIBI1IVPROC glad_glVertexAttribI1iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI1iv(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttribI1iv", (GLADapiproc) glad_glVertexAttribI1iv, 2, index, v);
    glad_glVertexAttribI1iv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI1iv", (GLADapiproc) glad_glVertexAttribI1iv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI1IVPROC glad_debug_glVertexAttribI1iv = glad_debug_impl_glVertexAttribI1iv;
PFNGLVERTEXATTRIBI1IVEXTPROC glad_glVertexAttribI1ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI1ivEXT(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttribI1ivEXT", (GLADapiproc) glad_glVertexAttribI1ivEXT, 2, index, v);
    glad_glVertexAttribI1ivEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI1ivEXT", (GLADapiproc) glad_glVertexAttribI1ivEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI1IVEXTPROC glad_debug_glVertexAttribI1ivEXT = glad_debug_impl_glVertexAttribI1ivEXT;
PFNGLVERTEXATTRIBI1UIPROC glad_glVertexAttribI1ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI1ui(GLuint index, GLuint x) {
    _pre_call_gl_callback("glVertexAttribI1ui", (GLADapiproc) glad_glVertexAttribI1ui, 2, index, x);
    glad_glVertexAttribI1ui(index, x);
    _post_call_gl_callback(NULL, "glVertexAttribI1ui", (GLADapiproc) glad_glVertexAttribI1ui, 2, index, x);
    
}
PFNGLVERTEXATTRIBI1UIPROC glad_debug_glVertexAttribI1ui = glad_debug_impl_glVertexAttribI1ui;
PFNGLVERTEXATTRIBI1UIEXTPROC glad_glVertexAttribI1uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI1uiEXT(GLuint index, GLuint x) {
    _pre_call_gl_callback("glVertexAttribI1uiEXT", (GLADapiproc) glad_glVertexAttribI1uiEXT, 2, index, x);
    glad_glVertexAttribI1uiEXT(index, x);
    _post_call_gl_callback(NULL, "glVertexAttribI1uiEXT", (GLADapiproc) glad_glVertexAttribI1uiEXT, 2, index, x);
    
}
PFNGLVERTEXATTRIBI1UIEXTPROC glad_debug_glVertexAttribI1uiEXT = glad_debug_impl_glVertexAttribI1uiEXT;
PFNGLVERTEXATTRIBI1UIVPROC glad_glVertexAttribI1uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI1uiv(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttribI1uiv", (GLADapiproc) glad_glVertexAttribI1uiv, 2, index, v);
    glad_glVertexAttribI1uiv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI1uiv", (GLADapiproc) glad_glVertexAttribI1uiv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI1UIVPROC glad_debug_glVertexAttribI1uiv = glad_debug_impl_glVertexAttribI1uiv;
PFNGLVERTEXATTRIBI1UIVEXTPROC glad_glVertexAttribI1uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI1uivEXT(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttribI1uivEXT", (GLADapiproc) glad_glVertexAttribI1uivEXT, 2, index, v);
    glad_glVertexAttribI1uivEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI1uivEXT", (GLADapiproc) glad_glVertexAttribI1uivEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI1UIVEXTPROC glad_debug_glVertexAttribI1uivEXT = glad_debug_impl_glVertexAttribI1uivEXT;
PFNGLVERTEXATTRIBI2IPROC glad_glVertexAttribI2i = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI2i(GLuint index, GLint x, GLint y) {
    _pre_call_gl_callback("glVertexAttribI2i", (GLADapiproc) glad_glVertexAttribI2i, 3, index, x, y);
    glad_glVertexAttribI2i(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttribI2i", (GLADapiproc) glad_glVertexAttribI2i, 3, index, x, y);
    
}
PFNGLVERTEXATTRIBI2IPROC glad_debug_glVertexAttribI2i = glad_debug_impl_glVertexAttribI2i;
PFNGLVERTEXATTRIBI2IEXTPROC glad_glVertexAttribI2iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI2iEXT(GLuint index, GLint x, GLint y) {
    _pre_call_gl_callback("glVertexAttribI2iEXT", (GLADapiproc) glad_glVertexAttribI2iEXT, 3, index, x, y);
    glad_glVertexAttribI2iEXT(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttribI2iEXT", (GLADapiproc) glad_glVertexAttribI2iEXT, 3, index, x, y);
    
}
PFNGLVERTEXATTRIBI2IEXTPROC glad_debug_glVertexAttribI2iEXT = glad_debug_impl_glVertexAttribI2iEXT;
PFNGLVERTEXATTRIBI2IVPROC glad_glVertexAttribI2iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI2iv(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttribI2iv", (GLADapiproc) glad_glVertexAttribI2iv, 2, index, v);
    glad_glVertexAttribI2iv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI2iv", (GLADapiproc) glad_glVertexAttribI2iv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI2IVPROC glad_debug_glVertexAttribI2iv = glad_debug_impl_glVertexAttribI2iv;
PFNGLVERTEXATTRIBI2IVEXTPROC glad_glVertexAttribI2ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI2ivEXT(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttribI2ivEXT", (GLADapiproc) glad_glVertexAttribI2ivEXT, 2, index, v);
    glad_glVertexAttribI2ivEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI2ivEXT", (GLADapiproc) glad_glVertexAttribI2ivEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI2IVEXTPROC glad_debug_glVertexAttribI2ivEXT = glad_debug_impl_glVertexAttribI2ivEXT;
PFNGLVERTEXATTRIBI2UIPROC glad_glVertexAttribI2ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI2ui(GLuint index, GLuint x, GLuint y) {
    _pre_call_gl_callback("glVertexAttribI2ui", (GLADapiproc) glad_glVertexAttribI2ui, 3, index, x, y);
    glad_glVertexAttribI2ui(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttribI2ui", (GLADapiproc) glad_glVertexAttribI2ui, 3, index, x, y);
    
}
PFNGLVERTEXATTRIBI2UIPROC glad_debug_glVertexAttribI2ui = glad_debug_impl_glVertexAttribI2ui;
PFNGLVERTEXATTRIBI2UIEXTPROC glad_glVertexAttribI2uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI2uiEXT(GLuint index, GLuint x, GLuint y) {
    _pre_call_gl_callback("glVertexAttribI2uiEXT", (GLADapiproc) glad_glVertexAttribI2uiEXT, 3, index, x, y);
    glad_glVertexAttribI2uiEXT(index, x, y);
    _post_call_gl_callback(NULL, "glVertexAttribI2uiEXT", (GLADapiproc) glad_glVertexAttribI2uiEXT, 3, index, x, y);
    
}
PFNGLVERTEXATTRIBI2UIEXTPROC glad_debug_glVertexAttribI2uiEXT = glad_debug_impl_glVertexAttribI2uiEXT;
PFNGLVERTEXATTRIBI2UIVPROC glad_glVertexAttribI2uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI2uiv(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttribI2uiv", (GLADapiproc) glad_glVertexAttribI2uiv, 2, index, v);
    glad_glVertexAttribI2uiv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI2uiv", (GLADapiproc) glad_glVertexAttribI2uiv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI2UIVPROC glad_debug_glVertexAttribI2uiv = glad_debug_impl_glVertexAttribI2uiv;
PFNGLVERTEXATTRIBI2UIVEXTPROC glad_glVertexAttribI2uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI2uivEXT(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttribI2uivEXT", (GLADapiproc) glad_glVertexAttribI2uivEXT, 2, index, v);
    glad_glVertexAttribI2uivEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI2uivEXT", (GLADapiproc) glad_glVertexAttribI2uivEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI2UIVEXTPROC glad_debug_glVertexAttribI2uivEXT = glad_debug_impl_glVertexAttribI2uivEXT;
PFNGLVERTEXATTRIBI3IPROC glad_glVertexAttribI3i = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI3i(GLuint index, GLint x, GLint y, GLint z) {
    _pre_call_gl_callback("glVertexAttribI3i", (GLADapiproc) glad_glVertexAttribI3i, 4, index, x, y, z);
    glad_glVertexAttribI3i(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttribI3i", (GLADapiproc) glad_glVertexAttribI3i, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIBI3IPROC glad_debug_glVertexAttribI3i = glad_debug_impl_glVertexAttribI3i;
PFNGLVERTEXATTRIBI3IEXTPROC glad_glVertexAttribI3iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI3iEXT(GLuint index, GLint x, GLint y, GLint z) {
    _pre_call_gl_callback("glVertexAttribI3iEXT", (GLADapiproc) glad_glVertexAttribI3iEXT, 4, index, x, y, z);
    glad_glVertexAttribI3iEXT(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttribI3iEXT", (GLADapiproc) glad_glVertexAttribI3iEXT, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIBI3IEXTPROC glad_debug_glVertexAttribI3iEXT = glad_debug_impl_glVertexAttribI3iEXT;
PFNGLVERTEXATTRIBI3IVPROC glad_glVertexAttribI3iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI3iv(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttribI3iv", (GLADapiproc) glad_glVertexAttribI3iv, 2, index, v);
    glad_glVertexAttribI3iv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI3iv", (GLADapiproc) glad_glVertexAttribI3iv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI3IVPROC glad_debug_glVertexAttribI3iv = glad_debug_impl_glVertexAttribI3iv;
PFNGLVERTEXATTRIBI3IVEXTPROC glad_glVertexAttribI3ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI3ivEXT(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttribI3ivEXT", (GLADapiproc) glad_glVertexAttribI3ivEXT, 2, index, v);
    glad_glVertexAttribI3ivEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI3ivEXT", (GLADapiproc) glad_glVertexAttribI3ivEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI3IVEXTPROC glad_debug_glVertexAttribI3ivEXT = glad_debug_impl_glVertexAttribI3ivEXT;
PFNGLVERTEXATTRIBI3UIPROC glad_glVertexAttribI3ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI3ui(GLuint index, GLuint x, GLuint y, GLuint z) {
    _pre_call_gl_callback("glVertexAttribI3ui", (GLADapiproc) glad_glVertexAttribI3ui, 4, index, x, y, z);
    glad_glVertexAttribI3ui(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttribI3ui", (GLADapiproc) glad_glVertexAttribI3ui, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIBI3UIPROC glad_debug_glVertexAttribI3ui = glad_debug_impl_glVertexAttribI3ui;
PFNGLVERTEXATTRIBI3UIEXTPROC glad_glVertexAttribI3uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI3uiEXT(GLuint index, GLuint x, GLuint y, GLuint z) {
    _pre_call_gl_callback("glVertexAttribI3uiEXT", (GLADapiproc) glad_glVertexAttribI3uiEXT, 4, index, x, y, z);
    glad_glVertexAttribI3uiEXT(index, x, y, z);
    _post_call_gl_callback(NULL, "glVertexAttribI3uiEXT", (GLADapiproc) glad_glVertexAttribI3uiEXT, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIBI3UIEXTPROC glad_debug_glVertexAttribI3uiEXT = glad_debug_impl_glVertexAttribI3uiEXT;
PFNGLVERTEXATTRIBI3UIVPROC glad_glVertexAttribI3uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI3uiv(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttribI3uiv", (GLADapiproc) glad_glVertexAttribI3uiv, 2, index, v);
    glad_glVertexAttribI3uiv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI3uiv", (GLADapiproc) glad_glVertexAttribI3uiv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI3UIVPROC glad_debug_glVertexAttribI3uiv = glad_debug_impl_glVertexAttribI3uiv;
PFNGLVERTEXATTRIBI3UIVEXTPROC glad_glVertexAttribI3uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI3uivEXT(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttribI3uivEXT", (GLADapiproc) glad_glVertexAttribI3uivEXT, 2, index, v);
    glad_glVertexAttribI3uivEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI3uivEXT", (GLADapiproc) glad_glVertexAttribI3uivEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI3UIVEXTPROC glad_debug_glVertexAttribI3uivEXT = glad_debug_impl_glVertexAttribI3uivEXT;
PFNGLVERTEXATTRIBI4BVPROC glad_glVertexAttribI4bv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4bv(GLuint index, const GLbyte * v) {
    _pre_call_gl_callback("glVertexAttribI4bv", (GLADapiproc) glad_glVertexAttribI4bv, 2, index, v);
    glad_glVertexAttribI4bv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4bv", (GLADapiproc) glad_glVertexAttribI4bv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4BVPROC glad_debug_glVertexAttribI4bv = glad_debug_impl_glVertexAttribI4bv;
PFNGLVERTEXATTRIBI4BVEXTPROC glad_glVertexAttribI4bvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4bvEXT(GLuint index, const GLbyte * v) {
    _pre_call_gl_callback("glVertexAttribI4bvEXT", (GLADapiproc) glad_glVertexAttribI4bvEXT, 2, index, v);
    glad_glVertexAttribI4bvEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4bvEXT", (GLADapiproc) glad_glVertexAttribI4bvEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4BVEXTPROC glad_debug_glVertexAttribI4bvEXT = glad_debug_impl_glVertexAttribI4bvEXT;
PFNGLVERTEXATTRIBI4IPROC glad_glVertexAttribI4i = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    _pre_call_gl_callback("glVertexAttribI4i", (GLADapiproc) glad_glVertexAttribI4i, 5, index, x, y, z, w);
    glad_glVertexAttribI4i(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttribI4i", (GLADapiproc) glad_glVertexAttribI4i, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIBI4IPROC glad_debug_glVertexAttribI4i = glad_debug_impl_glVertexAttribI4i;
PFNGLVERTEXATTRIBI4IEXTPROC glad_glVertexAttribI4iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4iEXT(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    _pre_call_gl_callback("glVertexAttribI4iEXT", (GLADapiproc) glad_glVertexAttribI4iEXT, 5, index, x, y, z, w);
    glad_glVertexAttribI4iEXT(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttribI4iEXT", (GLADapiproc) glad_glVertexAttribI4iEXT, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIBI4IEXTPROC glad_debug_glVertexAttribI4iEXT = glad_debug_impl_glVertexAttribI4iEXT;
PFNGLVERTEXATTRIBI4IVPROC glad_glVertexAttribI4iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4iv(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttribI4iv", (GLADapiproc) glad_glVertexAttribI4iv, 2, index, v);
    glad_glVertexAttribI4iv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4iv", (GLADapiproc) glad_glVertexAttribI4iv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4IVPROC glad_debug_glVertexAttribI4iv = glad_debug_impl_glVertexAttribI4iv;
PFNGLVERTEXATTRIBI4IVEXTPROC glad_glVertexAttribI4ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4ivEXT(GLuint index, const GLint * v) {
    _pre_call_gl_callback("glVertexAttribI4ivEXT", (GLADapiproc) glad_glVertexAttribI4ivEXT, 2, index, v);
    glad_glVertexAttribI4ivEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4ivEXT", (GLADapiproc) glad_glVertexAttribI4ivEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4IVEXTPROC glad_debug_glVertexAttribI4ivEXT = glad_debug_impl_glVertexAttribI4ivEXT;
PFNGLVERTEXATTRIBI4SVPROC glad_glVertexAttribI4sv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4sv(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttribI4sv", (GLADapiproc) glad_glVertexAttribI4sv, 2, index, v);
    glad_glVertexAttribI4sv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4sv", (GLADapiproc) glad_glVertexAttribI4sv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4SVPROC glad_debug_glVertexAttribI4sv = glad_debug_impl_glVertexAttribI4sv;
PFNGLVERTEXATTRIBI4SVEXTPROC glad_glVertexAttribI4svEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4svEXT(GLuint index, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttribI4svEXT", (GLADapiproc) glad_glVertexAttribI4svEXT, 2, index, v);
    glad_glVertexAttribI4svEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4svEXT", (GLADapiproc) glad_glVertexAttribI4svEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4SVEXTPROC glad_debug_glVertexAttribI4svEXT = glad_debug_impl_glVertexAttribI4svEXT;
PFNGLVERTEXATTRIBI4UBVPROC glad_glVertexAttribI4ubv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4ubv(GLuint index, const GLubyte * v) {
    _pre_call_gl_callback("glVertexAttribI4ubv", (GLADapiproc) glad_glVertexAttribI4ubv, 2, index, v);
    glad_glVertexAttribI4ubv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4ubv", (GLADapiproc) glad_glVertexAttribI4ubv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4UBVPROC glad_debug_glVertexAttribI4ubv = glad_debug_impl_glVertexAttribI4ubv;
PFNGLVERTEXATTRIBI4UBVEXTPROC glad_glVertexAttribI4ubvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4ubvEXT(GLuint index, const GLubyte * v) {
    _pre_call_gl_callback("glVertexAttribI4ubvEXT", (GLADapiproc) glad_glVertexAttribI4ubvEXT, 2, index, v);
    glad_glVertexAttribI4ubvEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4ubvEXT", (GLADapiproc) glad_glVertexAttribI4ubvEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4UBVEXTPROC glad_debug_glVertexAttribI4ubvEXT = glad_debug_impl_glVertexAttribI4ubvEXT;
PFNGLVERTEXATTRIBI4UIPROC glad_glVertexAttribI4ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    _pre_call_gl_callback("glVertexAttribI4ui", (GLADapiproc) glad_glVertexAttribI4ui, 5, index, x, y, z, w);
    glad_glVertexAttribI4ui(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttribI4ui", (GLADapiproc) glad_glVertexAttribI4ui, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIBI4UIPROC glad_debug_glVertexAttribI4ui = glad_debug_impl_glVertexAttribI4ui;
PFNGLVERTEXATTRIBI4UIEXTPROC glad_glVertexAttribI4uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4uiEXT(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    _pre_call_gl_callback("glVertexAttribI4uiEXT", (GLADapiproc) glad_glVertexAttribI4uiEXT, 5, index, x, y, z, w);
    glad_glVertexAttribI4uiEXT(index, x, y, z, w);
    _post_call_gl_callback(NULL, "glVertexAttribI4uiEXT", (GLADapiproc) glad_glVertexAttribI4uiEXT, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIBI4UIEXTPROC glad_debug_glVertexAttribI4uiEXT = glad_debug_impl_glVertexAttribI4uiEXT;
PFNGLVERTEXATTRIBI4UIVPROC glad_glVertexAttribI4uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4uiv(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttribI4uiv", (GLADapiproc) glad_glVertexAttribI4uiv, 2, index, v);
    glad_glVertexAttribI4uiv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4uiv", (GLADapiproc) glad_glVertexAttribI4uiv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4UIVPROC glad_debug_glVertexAttribI4uiv = glad_debug_impl_glVertexAttribI4uiv;
PFNGLVERTEXATTRIBI4UIVEXTPROC glad_glVertexAttribI4uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4uivEXT(GLuint index, const GLuint * v) {
    _pre_call_gl_callback("glVertexAttribI4uivEXT", (GLADapiproc) glad_glVertexAttribI4uivEXT, 2, index, v);
    glad_glVertexAttribI4uivEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4uivEXT", (GLADapiproc) glad_glVertexAttribI4uivEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4UIVEXTPROC glad_debug_glVertexAttribI4uivEXT = glad_debug_impl_glVertexAttribI4uivEXT;
PFNGLVERTEXATTRIBI4USVPROC glad_glVertexAttribI4usv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4usv(GLuint index, const GLushort * v) {
    _pre_call_gl_callback("glVertexAttribI4usv", (GLADapiproc) glad_glVertexAttribI4usv, 2, index, v);
    glad_glVertexAttribI4usv(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4usv", (GLADapiproc) glad_glVertexAttribI4usv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4USVPROC glad_debug_glVertexAttribI4usv = glad_debug_impl_glVertexAttribI4usv;
PFNGLVERTEXATTRIBI4USVEXTPROC glad_glVertexAttribI4usvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4usvEXT(GLuint index, const GLushort * v) {
    _pre_call_gl_callback("glVertexAttribI4usvEXT", (GLADapiproc) glad_glVertexAttribI4usvEXT, 2, index, v);
    glad_glVertexAttribI4usvEXT(index, v);
    _post_call_gl_callback(NULL, "glVertexAttribI4usvEXT", (GLADapiproc) glad_glVertexAttribI4usvEXT, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4USVEXTPROC glad_debug_glVertexAttribI4usvEXT = glad_debug_impl_glVertexAttribI4usvEXT;
PFNGLVERTEXATTRIBIPOINTERPROC glad_glVertexAttribIPointer = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gl_callback("glVertexAttribIPointer", (GLADapiproc) glad_glVertexAttribIPointer, 5, index, size, type, stride, pointer);
    glad_glVertexAttribIPointer(index, size, type, stride, pointer);
    _post_call_gl_callback(NULL, "glVertexAttribIPointer", (GLADapiproc) glad_glVertexAttribIPointer, 5, index, size, type, stride, pointer);
    
}
PFNGLVERTEXATTRIBIPOINTERPROC glad_debug_glVertexAttribIPointer = glad_debug_impl_glVertexAttribIPointer;
PFNGLVERTEXATTRIBIPOINTEREXTPROC glad_glVertexAttribIPointerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribIPointerEXT(GLuint index, GLint size, GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gl_callback("glVertexAttribIPointerEXT", (GLADapiproc) glad_glVertexAttribIPointerEXT, 5, index, size, type, stride, pointer);
    glad_glVertexAttribIPointerEXT(index, size, type, stride, pointer);
    _post_call_gl_callback(NULL, "glVertexAttribIPointerEXT", (GLADapiproc) glad_glVertexAttribIPointerEXT, 5, index, size, type, stride, pointer);
    
}
PFNGLVERTEXATTRIBIPOINTEREXTPROC glad_debug_glVertexAttribIPointerEXT = glad_debug_impl_glVertexAttribIPointerEXT;
PFNGLVERTEXATTRIBP1UIPROC glad_glVertexAttribP1ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribP1ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    _pre_call_gl_callback("glVertexAttribP1ui", (GLADapiproc) glad_glVertexAttribP1ui, 4, index, type, normalized, value);
    glad_glVertexAttribP1ui(index, type, normalized, value);
    _post_call_gl_callback(NULL, "glVertexAttribP1ui", (GLADapiproc) glad_glVertexAttribP1ui, 4, index, type, normalized, value);
    
}
PFNGLVERTEXATTRIBP1UIPROC glad_debug_glVertexAttribP1ui = glad_debug_impl_glVertexAttribP1ui;
PFNGLVERTEXATTRIBP1UIVPROC glad_glVertexAttribP1uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribP1uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint * value) {
    _pre_call_gl_callback("glVertexAttribP1uiv", (GLADapiproc) glad_glVertexAttribP1uiv, 4, index, type, normalized, value);
    glad_glVertexAttribP1uiv(index, type, normalized, value);
    _post_call_gl_callback(NULL, "glVertexAttribP1uiv", (GLADapiproc) glad_glVertexAttribP1uiv, 4, index, type, normalized, value);
    
}
PFNGLVERTEXATTRIBP1UIVPROC glad_debug_glVertexAttribP1uiv = glad_debug_impl_glVertexAttribP1uiv;
PFNGLVERTEXATTRIBP2UIPROC glad_glVertexAttribP2ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribP2ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    _pre_call_gl_callback("glVertexAttribP2ui", (GLADapiproc) glad_glVertexAttribP2ui, 4, index, type, normalized, value);
    glad_glVertexAttribP2ui(index, type, normalized, value);
    _post_call_gl_callback(NULL, "glVertexAttribP2ui", (GLADapiproc) glad_glVertexAttribP2ui, 4, index, type, normalized, value);
    
}
PFNGLVERTEXATTRIBP2UIPROC glad_debug_glVertexAttribP2ui = glad_debug_impl_glVertexAttribP2ui;
PFNGLVERTEXATTRIBP2UIVPROC glad_glVertexAttribP2uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribP2uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint * value) {
    _pre_call_gl_callback("glVertexAttribP2uiv", (GLADapiproc) glad_glVertexAttribP2uiv, 4, index, type, normalized, value);
    glad_glVertexAttribP2uiv(index, type, normalized, value);
    _post_call_gl_callback(NULL, "glVertexAttribP2uiv", (GLADapiproc) glad_glVertexAttribP2uiv, 4, index, type, normalized, value);
    
}
PFNGLVERTEXATTRIBP2UIVPROC glad_debug_glVertexAttribP2uiv = glad_debug_impl_glVertexAttribP2uiv;
PFNGLVERTEXATTRIBP3UIPROC glad_glVertexAttribP3ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribP3ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    _pre_call_gl_callback("glVertexAttribP3ui", (GLADapiproc) glad_glVertexAttribP3ui, 4, index, type, normalized, value);
    glad_glVertexAttribP3ui(index, type, normalized, value);
    _post_call_gl_callback(NULL, "glVertexAttribP3ui", (GLADapiproc) glad_glVertexAttribP3ui, 4, index, type, normalized, value);
    
}
PFNGLVERTEXATTRIBP3UIPROC glad_debug_glVertexAttribP3ui = glad_debug_impl_glVertexAttribP3ui;
PFNGLVERTEXATTRIBP3UIVPROC glad_glVertexAttribP3uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribP3uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint * value) {
    _pre_call_gl_callback("glVertexAttribP3uiv", (GLADapiproc) glad_glVertexAttribP3uiv, 4, index, type, normalized, value);
    glad_glVertexAttribP3uiv(index, type, normalized, value);
    _post_call_gl_callback(NULL, "glVertexAttribP3uiv", (GLADapiproc) glad_glVertexAttribP3uiv, 4, index, type, normalized, value);
    
}
PFNGLVERTEXATTRIBP3UIVPROC glad_debug_glVertexAttribP3uiv = glad_debug_impl_glVertexAttribP3uiv;
PFNGLVERTEXATTRIBP4UIPROC glad_glVertexAttribP4ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribP4ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    _pre_call_gl_callback("glVertexAttribP4ui", (GLADapiproc) glad_glVertexAttribP4ui, 4, index, type, normalized, value);
    glad_glVertexAttribP4ui(index, type, normalized, value);
    _post_call_gl_callback(NULL, "glVertexAttribP4ui", (GLADapiproc) glad_glVertexAttribP4ui, 4, index, type, normalized, value);
    
}
PFNGLVERTEXATTRIBP4UIPROC glad_debug_glVertexAttribP4ui = glad_debug_impl_glVertexAttribP4ui;
PFNGLVERTEXATTRIBP4UIVPROC glad_glVertexAttribP4uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribP4uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint * value) {
    _pre_call_gl_callback("glVertexAttribP4uiv", (GLADapiproc) glad_glVertexAttribP4uiv, 4, index, type, normalized, value);
    glad_glVertexAttribP4uiv(index, type, normalized, value);
    _post_call_gl_callback(NULL, "glVertexAttribP4uiv", (GLADapiproc) glad_glVertexAttribP4uiv, 4, index, type, normalized, value);
    
}
PFNGLVERTEXATTRIBP4UIVPROC glad_debug_glVertexAttribP4uiv = glad_debug_impl_glVertexAttribP4uiv;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void * pointer) {
    _pre_call_gl_callback("glVertexAttribPointer", (GLADapiproc) glad_glVertexAttribPointer, 6, index, size, type, normalized, stride, pointer);
    glad_glVertexAttribPointer(index, size, type, normalized, stride, pointer);
    _post_call_gl_callback(NULL, "glVertexAttribPointer", (GLADapiproc) glad_glVertexAttribPointer, 6, index, size, type, normalized, stride, pointer);
    
}
PFNGLVERTEXATTRIBPOINTERPROC glad_debug_glVertexAttribPointer = glad_debug_impl_glVertexAttribPointer;
PFNGLVERTEXATTRIBPOINTERARBPROC glad_glVertexAttribPointerARB = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribPointerARB(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void * pointer) {
    _pre_call_gl_callback("glVertexAttribPointerARB", (GLADapiproc) glad_glVertexAttribPointerARB, 6, index, size, type, normalized, stride, pointer);
    glad_glVertexAttribPointerARB(index, size, type, normalized, stride, pointer);
    _post_call_gl_callback(NULL, "glVertexAttribPointerARB", (GLADapiproc) glad_glVertexAttribPointerARB, 6, index, size, type, normalized, stride, pointer);
    
}
PFNGLVERTEXATTRIBPOINTERARBPROC glad_debug_glVertexAttribPointerARB = glad_debug_impl_glVertexAttribPointerARB;
PFNGLVERTEXATTRIBPOINTERNVPROC glad_glVertexAttribPointerNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribPointerNV(GLuint index, GLint fsize, GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gl_callback("glVertexAttribPointerNV", (GLADapiproc) glad_glVertexAttribPointerNV, 5, index, fsize, type, stride, pointer);
    glad_glVertexAttribPointerNV(index, fsize, type, stride, pointer);
    _post_call_gl_callback(NULL, "glVertexAttribPointerNV", (GLADapiproc) glad_glVertexAttribPointerNV, 5, index, fsize, type, stride, pointer);
    
}
PFNGLVERTEXATTRIBPOINTERNVPROC glad_debug_glVertexAttribPointerNV = glad_debug_impl_glVertexAttribPointerNV;
PFNGLVERTEXATTRIBS1DVNVPROC glad_glVertexAttribs1dvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs1dvNV(GLuint index, GLsizei count, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttribs1dvNV", (GLADapiproc) glad_glVertexAttribs1dvNV, 3, index, count, v);
    glad_glVertexAttribs1dvNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs1dvNV", (GLADapiproc) glad_glVertexAttribs1dvNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS1DVNVPROC glad_debug_glVertexAttribs1dvNV = glad_debug_impl_glVertexAttribs1dvNV;
PFNGLVERTEXATTRIBS1FVNVPROC glad_glVertexAttribs1fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs1fvNV(GLuint index, GLsizei count, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttribs1fvNV", (GLADapiproc) glad_glVertexAttribs1fvNV, 3, index, count, v);
    glad_glVertexAttribs1fvNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs1fvNV", (GLADapiproc) glad_glVertexAttribs1fvNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS1FVNVPROC glad_debug_glVertexAttribs1fvNV = glad_debug_impl_glVertexAttribs1fvNV;
PFNGLVERTEXATTRIBS1SVNVPROC glad_glVertexAttribs1svNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs1svNV(GLuint index, GLsizei count, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttribs1svNV", (GLADapiproc) glad_glVertexAttribs1svNV, 3, index, count, v);
    glad_glVertexAttribs1svNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs1svNV", (GLADapiproc) glad_glVertexAttribs1svNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS1SVNVPROC glad_debug_glVertexAttribs1svNV = glad_debug_impl_glVertexAttribs1svNV;
PFNGLVERTEXATTRIBS2DVNVPROC glad_glVertexAttribs2dvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs2dvNV(GLuint index, GLsizei count, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttribs2dvNV", (GLADapiproc) glad_glVertexAttribs2dvNV, 3, index, count, v);
    glad_glVertexAttribs2dvNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs2dvNV", (GLADapiproc) glad_glVertexAttribs2dvNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS2DVNVPROC glad_debug_glVertexAttribs2dvNV = glad_debug_impl_glVertexAttribs2dvNV;
PFNGLVERTEXATTRIBS2FVNVPROC glad_glVertexAttribs2fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs2fvNV(GLuint index, GLsizei count, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttribs2fvNV", (GLADapiproc) glad_glVertexAttribs2fvNV, 3, index, count, v);
    glad_glVertexAttribs2fvNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs2fvNV", (GLADapiproc) glad_glVertexAttribs2fvNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS2FVNVPROC glad_debug_glVertexAttribs2fvNV = glad_debug_impl_glVertexAttribs2fvNV;
PFNGLVERTEXATTRIBS2SVNVPROC glad_glVertexAttribs2svNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs2svNV(GLuint index, GLsizei count, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttribs2svNV", (GLADapiproc) glad_glVertexAttribs2svNV, 3, index, count, v);
    glad_glVertexAttribs2svNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs2svNV", (GLADapiproc) glad_glVertexAttribs2svNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS2SVNVPROC glad_debug_glVertexAttribs2svNV = glad_debug_impl_glVertexAttribs2svNV;
PFNGLVERTEXATTRIBS3DVNVPROC glad_glVertexAttribs3dvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs3dvNV(GLuint index, GLsizei count, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttribs3dvNV", (GLADapiproc) glad_glVertexAttribs3dvNV, 3, index, count, v);
    glad_glVertexAttribs3dvNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs3dvNV", (GLADapiproc) glad_glVertexAttribs3dvNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS3DVNVPROC glad_debug_glVertexAttribs3dvNV = glad_debug_impl_glVertexAttribs3dvNV;
PFNGLVERTEXATTRIBS3FVNVPROC glad_glVertexAttribs3fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs3fvNV(GLuint index, GLsizei count, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttribs3fvNV", (GLADapiproc) glad_glVertexAttribs3fvNV, 3, index, count, v);
    glad_glVertexAttribs3fvNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs3fvNV", (GLADapiproc) glad_glVertexAttribs3fvNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS3FVNVPROC glad_debug_glVertexAttribs3fvNV = glad_debug_impl_glVertexAttribs3fvNV;
PFNGLVERTEXATTRIBS3SVNVPROC glad_glVertexAttribs3svNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs3svNV(GLuint index, GLsizei count, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttribs3svNV", (GLADapiproc) glad_glVertexAttribs3svNV, 3, index, count, v);
    glad_glVertexAttribs3svNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs3svNV", (GLADapiproc) glad_glVertexAttribs3svNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS3SVNVPROC glad_debug_glVertexAttribs3svNV = glad_debug_impl_glVertexAttribs3svNV;
PFNGLVERTEXATTRIBS4DVNVPROC glad_glVertexAttribs4dvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs4dvNV(GLuint index, GLsizei count, const GLdouble * v) {
    _pre_call_gl_callback("glVertexAttribs4dvNV", (GLADapiproc) glad_glVertexAttribs4dvNV, 3, index, count, v);
    glad_glVertexAttribs4dvNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs4dvNV", (GLADapiproc) glad_glVertexAttribs4dvNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS4DVNVPROC glad_debug_glVertexAttribs4dvNV = glad_debug_impl_glVertexAttribs4dvNV;
PFNGLVERTEXATTRIBS4FVNVPROC glad_glVertexAttribs4fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs4fvNV(GLuint index, GLsizei count, const GLfloat * v) {
    _pre_call_gl_callback("glVertexAttribs4fvNV", (GLADapiproc) glad_glVertexAttribs4fvNV, 3, index, count, v);
    glad_glVertexAttribs4fvNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs4fvNV", (GLADapiproc) glad_glVertexAttribs4fvNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS4FVNVPROC glad_debug_glVertexAttribs4fvNV = glad_debug_impl_glVertexAttribs4fvNV;
PFNGLVERTEXATTRIBS4SVNVPROC glad_glVertexAttribs4svNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs4svNV(GLuint index, GLsizei count, const GLshort * v) {
    _pre_call_gl_callback("glVertexAttribs4svNV", (GLADapiproc) glad_glVertexAttribs4svNV, 3, index, count, v);
    glad_glVertexAttribs4svNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs4svNV", (GLADapiproc) glad_glVertexAttribs4svNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS4SVNVPROC glad_debug_glVertexAttribs4svNV = glad_debug_impl_glVertexAttribs4svNV;
PFNGLVERTEXATTRIBS4UBVNVPROC glad_glVertexAttribs4ubvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribs4ubvNV(GLuint index, GLsizei count, const GLubyte * v) {
    _pre_call_gl_callback("glVertexAttribs4ubvNV", (GLADapiproc) glad_glVertexAttribs4ubvNV, 3, index, count, v);
    glad_glVertexAttribs4ubvNV(index, count, v);
    _post_call_gl_callback(NULL, "glVertexAttribs4ubvNV", (GLADapiproc) glad_glVertexAttribs4ubvNV, 3, index, count, v);
    
}
PFNGLVERTEXATTRIBS4UBVNVPROC glad_debug_glVertexAttribs4ubvNV = glad_debug_impl_glVertexAttribs4ubvNV;
PFNGLVERTEXPOINTEREXTPROC glad_glVertexPointerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexPointerEXT(GLint size, GLenum type, GLsizei stride, GLsizei count, const void * pointer) {
    _pre_call_gl_callback("glVertexPointerEXT", (GLADapiproc) glad_glVertexPointerEXT, 5, size, type, stride, count, pointer);
    glad_glVertexPointerEXT(size, type, stride, count, pointer);
    _post_call_gl_callback(NULL, "glVertexPointerEXT", (GLADapiproc) glad_glVertexPointerEXT, 5, size, type, stride, count, pointer);
    
}
PFNGLVERTEXPOINTEREXTPROC glad_debug_glVertexPointerEXT = glad_debug_impl_glVertexPointerEXT;
PFNGLVIEWPORTPROC glad_glViewport = NULL;
static void GLAD_API_PTR glad_debug_impl_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gl_callback("glViewport", (GLADapiproc) glad_glViewport, 4, x, y, width, height);
    glad_glViewport(x, y, width, height);
    _post_call_gl_callback(NULL, "glViewport", (GLADapiproc) glad_glViewport, 4, x, y, width, height);
    
}
PFNGLVIEWPORTPROC glad_debug_glViewport = glad_debug_impl_glViewport;
PFNGLWAITSYNCPROC glad_glWaitSync = NULL;
static void GLAD_API_PTR glad_debug_impl_glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    _pre_call_gl_callback("glWaitSync", (GLADapiproc) glad_glWaitSync, 3, sync, flags, timeout);
    glad_glWaitSync(sync, flags, timeout);
    _post_call_gl_callback(NULL, "glWaitSync", (GLADapiproc) glad_glWaitSync, 3, sync, flags, timeout);
    
}
PFNGLWAITSYNCPROC glad_debug_glWaitSync = glad_debug_impl_glWaitSync;


static void glad_gl_load_GL_VERSION_1_0( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_0) return;
    glad_glBlendFunc = (PFNGLBLENDFUNCPROC) load(userptr, "glBlendFunc");
    glad_glClear = (PFNGLCLEARPROC) load(userptr, "glClear");
    glad_glClearColor = (PFNGLCLEARCOLORPROC) load(userptr, "glClearColor");
    glad_glClearDepth = (PFNGLCLEARDEPTHPROC) load(userptr, "glClearDepth");
    glad_glClearStencil = (PFNGLCLEARSTENCILPROC) load(userptr, "glClearStencil");
    glad_glColorMask = (PFNGLCOLORMASKPROC) load(userptr, "glColorMask");
    glad_glCullFace = (PFNGLCULLFACEPROC) load(userptr, "glCullFace");
    glad_glDepthFunc = (PFNGLDEPTHFUNCPROC) load(userptr, "glDepthFunc");
    glad_glDepthMask = (PFNGLDEPTHMASKPROC) load(userptr, "glDepthMask");
    glad_glDepthRange = (PFNGLDEPTHRANGEPROC) load(userptr, "glDepthRange");
    glad_glDisable = (PFNGLDISABLEPROC) load(userptr, "glDisable");
    glad_glDrawBuffer = (PFNGLDRAWBUFFERPROC) load(userptr, "glDrawBuffer");
    glad_glEnable = (PFNGLENABLEPROC) load(userptr, "glEnable");
    glad_glFinish = (PFNGLFINISHPROC) load(userptr, "glFinish");
    glad_glFlush = (PFNGLFLUSHPROC) load(userptr, "glFlush");
    glad_glFrontFace = (PFNGLFRONTFACEPROC) load(userptr, "glFrontFace");
    glad_glGetBooleanv = (PFNGLGETBOOLEANVPROC) load(userptr, "glGetBooleanv");
    glad_glGetDoublev = (PFNGLGETDOUBLEVPROC) load(userptr, "glGetDoublev");
    glad_glGetError = (PFNGLGETERRORPROC) load(userptr, "glGetError");
    glad_glGetFloatv = (PFNGLGETFLOATVPROC) load(userptr, "glGetFloatv");
    glad_glGetIntegerv = (PFNGLGETINTEGERVPROC) load(userptr, "glGetIntegerv");
    glad_glGetString = (PFNGLGETSTRINGPROC) load(userptr, "glGetString");
    glad_glGetTexImage = (PFNGLGETTEXIMAGEPROC) load(userptr, "glGetTexImage");
    glad_glGetTexLevelParameterfv = (PFNGLGETTEXLEVELPARAMETERFVPROC) load(userptr, "glGetTexLevelParameterfv");
    glad_glGetTexLevelParameteriv = (PFNGLGETTEXLEVELPARAMETERIVPROC) load(userptr, "glGetTexLevelParameteriv");
    glad_glGetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC) load(userptr, "glGetTexParameterfv");
    glad_glGetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC) load(userptr, "glGetTexParameteriv");
    glad_glHint = (PFNGLHINTPROC) load(userptr, "glHint");
    glad_glIsEnabled = (PFNGLISENABLEDPROC) load(userptr, "glIsEnabled");
    glad_glLineWidth = (PFNGLLINEWIDTHPROC) load(userptr, "glLineWidth");
    glad_glLogicOp = (PFNGLLOGICOPPROC) load(userptr, "glLogicOp");
    glad_glPixelStoref = (PFNGLPIXELSTOREFPROC) load(userptr, "glPixelStoref");
    glad_glPixelStorei = (PFNGLPIXELSTOREIPROC) load(userptr, "glPixelStorei");
    glad_glPointSize = (PFNGLPOINTSIZEPROC) load(userptr, "glPointSize");
    glad_glPolygonMode = (PFNGLPOLYGONMODEPROC) load(userptr, "glPolygonMode");
    glad_glReadBuffer = (PFNGLREADBUFFERPROC) load(userptr, "glReadBuffer");
    glad_glReadPixels = (PFNGLREADPIXELSPROC) load(userptr, "glReadPixels");
    glad_glScissor = (PFNGLSCISSORPROC) load(userptr, "glScissor");
    glad_glStencilFunc = (PFNGLSTENCILFUNCPROC) load(userptr, "glStencilFunc");
    glad_glStencilMask = (PFNGLSTENCILMASKPROC) load(userptr, "glStencilMask");
    glad_glStencilOp = (PFNGLSTENCILOPPROC) load(userptr, "glStencilOp");
    glad_glTexImage1D = (PFNGLTEXIMAGE1DPROC) load(userptr, "glTexImage1D");
    glad_glTexImage2D = (PFNGLTEXIMAGE2DPROC) load(userptr, "glTexImage2D");
    glad_glTexParameterf = (PFNGLTEXPARAMETERFPROC) load(userptr, "glTexParameterf");
    glad_glTexParameterfv = (PFNGLTEXPARAMETERFVPROC) load(userptr, "glTexParameterfv");
    glad_glTexParameteri = (PFNGLTEXPARAMETERIPROC) load(userptr, "glTexParameteri");
    glad_glTexParameteriv = (PFNGLTEXPARAMETERIVPROC) load(userptr, "glTexParameteriv");
    glad_glViewport = (PFNGLVIEWPORTPROC) load(userptr, "glViewport");
}
static void glad_gl_load_GL_VERSION_1_1( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_1) return;
    glad_glBindTexture = (PFNGLBINDTEXTUREPROC) load(userptr, "glBindTexture");
    glad_glCopyTexImage1D = (PFNGLCOPYTEXIMAGE1DPROC) load(userptr, "glCopyTexImage1D");
    glad_glCopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC) load(userptr, "glCopyTexImage2D");
    glad_glCopyTexSubImage1D = (PFNGLCOPYTEXSUBIMAGE1DPROC) load(userptr, "glCopyTexSubImage1D");
    glad_glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC) load(userptr, "glCopyTexSubImage2D");
    glad_glDeleteTextures = (PFNGLDELETETEXTURESPROC) load(userptr, "glDeleteTextures");
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC) load(userptr, "glDrawArrays");
    glad_glDrawElements = (PFNGLDRAWELEMENTSPROC) load(userptr, "glDrawElements");
    glad_glGenTextures = (PFNGLGENTEXTURESPROC) load(userptr, "glGenTextures");
    glad_glIsTexture = (PFNGLISTEXTUREPROC) load(userptr, "glIsTexture");
    glad_glPolygonOffset = (PFNGLPOLYGONOFFSETPROC) load(userptr, "glPolygonOffset");
    glad_glTexSubImage1D = (PFNGLTEXSUBIMAGE1DPROC) load(userptr, "glTexSubImage1D");
    glad_glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC) load(userptr, "glTexSubImage2D");
}
static void glad_gl_load_GL_VERSION_1_2( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_2) return;
    glad_glCopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC) load(userptr, "glCopyTexSubImage3D");
    glad_glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC) load(userptr, "glDrawRangeElements");
    glad_glTexImage3D = (PFNGLTEXIMAGE3DPROC) load(userptr, "glTexImage3D");
    glad_glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC) load(userptr, "glTexSubImage3D");
}
static void glad_gl_load_GL_VERSION_1_3( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_3) return;
    glad_glActiveTexture = (PFNGLACTIVETEXTUREPROC) load(userptr, "glActiveTexture");
    glad_glCompressedTexImage1D = (PFNGLCOMPRESSEDTEXIMAGE1DPROC) load(userptr, "glCompressedTexImage1D");
    glad_glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC) load(userptr, "glCompressedTexImage2D");
    glad_glCompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC) load(userptr, "glCompressedTexImage3D");
    glad_glCompressedTexSubImage1D = (PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC) load(userptr, "glCompressedTexSubImage1D");
    glad_glCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC) load(userptr, "glCompressedTexSubImage2D");
    glad_glCompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC) load(userptr, "glCompressedTexSubImage3D");
    glad_glGetCompressedTexImage = (PFNGLGETCOMPRESSEDTEXIMAGEPROC) load(userptr, "glGetCompressedTexImage");
    glad_glSampleCoverage = (PFNGLSAMPLECOVERAGEPROC) load(userptr, "glSampleCoverage");
}
static void glad_gl_load_GL_VERSION_1_4( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_4) return;
    glad_glBlendColor = (PFNGLBLENDCOLORPROC) load(userptr, "glBlendColor");
    glad_glBlendEquation = (PFNGLBLENDEQUATIONPROC) load(userptr, "glBlendEquation");
    glad_glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC) load(userptr, "glBlendFuncSeparate");
    glad_glMultiDrawArrays = (PFNGLMULTIDRAWARRAYSPROC) load(userptr, "glMultiDrawArrays");
    glad_glMultiDrawElements = (PFNGLMULTIDRAWELEMENTSPROC) load(userptr, "glMultiDrawElements");
    glad_glPointParameterf = (PFNGLPOINTPARAMETERFPROC) load(userptr, "glPointParameterf");
    glad_glPointParameterfv = (PFNGLPOINTPARAMETERFVPROC) load(userptr, "glPointParameterfv");
    glad_glPointParameteri = (PFNGLPOINTPARAMETERIPROC) load(userptr, "glPointParameteri");
    glad_glPointParameteriv = (PFNGLPOINTPARAMETERIVPROC) load(userptr, "glPointParameteriv");
}
static void glad_gl_load_GL_VERSION_1_5( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_5) return;
    glad_glBeginQuery = (PFNGLBEGINQUERYPROC) load(userptr, "glBeginQuery");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC) load(userptr, "glBindBuffer");
    glad_glBufferData = (PFNGLBUFFERDATAPROC) load(userptr, "glBufferData");
    glad_glBufferSubData = (PFNGLBUFFERSUBDATAPROC) load(userptr, "glBufferSubData");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC) load(userptr, "glDeleteBuffers");
    glad_glDeleteQueries = (PFNGLDELETEQUERIESPROC) load(userptr, "glDeleteQueries");
    glad_glEndQuery = (PFNGLENDQUERYPROC) load(userptr, "glEndQuery");
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC) load(userptr, "glGenBuffers");
    glad_glGenQueries = (PFNGLGENQUERIESPROC) load(userptr, "glGenQueries");
    glad_glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC) load(userptr, "glGetBufferParameteriv");
    glad_glGetBufferPointerv = (PFNGLGETBUFFERPOINTERVPROC) load(userptr, "glGetBufferPointerv");
    glad_glGetBufferSubData = (PFNGLGETBUFFERSUBDATAPROC) load(userptr, "glGetBufferSubData");
    glad_glGetQueryObjectiv = (PFNGLGETQUERYOBJECTIVPROC) load(userptr, "glGetQueryObjectiv");
    glad_glGetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC) load(userptr, "glGetQueryObjectuiv");
    glad_glGetQueryiv = (PFNGLGETQUERYIVPROC) load(userptr, "glGetQueryiv");
    glad_glIsBuffer = (PFNGLISBUFFERPROC) load(userptr, "glIsBuffer");
    glad_glIsQuery = (PFNGLISQUERYPROC) load(userptr, "glIsQuery");
    glad_glMapBuffer = (PFNGLMAPBUFFERPROC) load(userptr, "glMapBuffer");
    glad_glUnmapBuffer = (PFNGLUNMAPBUFFERPROC) load(userptr, "glUnmapBuffer");
}
static void glad_gl_load_GL_VERSION_2_0( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_2_0) return;
    glad_glAttachShader = (PFNGLATTACHSHADERPROC) load(userptr, "glAttachShader");
    glad_glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC) load(userptr, "glBindAttribLocation");
    glad_glBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC) load(userptr, "glBlendEquationSeparate");
    glad_glCompileShader = (PFNGLCOMPILESHADERPROC) load(userptr, "glCompileShader");
    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC) load(userptr, "glCreateProgram");
    glad_glCreateShader = (PFNGLCREATESHADERPROC) load(userptr, "glCreateShader");
    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC) load(userptr, "glDeleteProgram");
    glad_glDeleteShader = (PFNGLDELETESHADERPROC) load(userptr, "glDeleteShader");
    glad_glDetachShader = (PFNGLDETACHSHADERPROC) load(userptr, "glDetachShader");
    glad_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC) load(userptr, "glDisableVertexAttribArray");
    glad_glDrawBuffers = (PFNGLDRAWBUFFERSPROC) load(userptr, "glDrawBuffers");
    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) load(userptr, "glEnableVertexAttribArray");
    glad_glGetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC) load(userptr, "glGetActiveAttrib");
    glad_glGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC) load(userptr, "glGetActiveUniform");
    glad_glGetAttachedShaders = (PFNGLGETATTACHEDSHADERSPROC) load(userptr, "glGetAttachedShaders");
    glad_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC) load(userptr, "glGetAttribLocation");
    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC) load(userptr, "glGetProgramInfoLog");
    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC) load(userptr, "glGetProgramiv");
    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) load(userptr, "glGetShaderInfoLog");
    glad_glGetShaderSource = (PFNGLGETSHADERSOURCEPROC) load(userptr, "glGetShaderSource");
    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC) load(userptr, "glGetShaderiv");
    glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) load(userptr, "glGetUniformLocation");
    glad_glGetUniformfv = (PFNGLGETUNIFORMFVPROC) load(userptr, "glGetUniformfv");
    glad_glGetUniformiv = (PFNGLGETUNIFORMIVPROC) load(userptr, "glGetUniformiv");
    glad_glGetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC) load(userptr, "glGetVertexAttribPointerv");
    glad_glGetVertexAttribdv = (PFNGLGETVERTEXATTRIBDVPROC) load(userptr, "glGetVertexAttribdv");
    glad_glGetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC) load(userptr, "glGetVertexAttribfv");
    glad_glGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC) load(userptr, "glGetVertexAttribiv");
    glad_glIsProgram = (PFNGLISPROGRAMPROC) load(userptr, "glIsProgram");
    glad_glIsShader = (PFNGLISSHADERPROC) load(userptr, "glIsShader");
    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC) load(userptr, "glLinkProgram");
    glad_glShaderSource = (PFNGLSHADERSOURCEPROC) load(userptr, "glShaderSource");
    glad_glStencilFuncSeparate = (PFNGLSTENCILFUNCSEPARATEPROC) load(userptr, "glStencilFuncSeparate");
    glad_glStencilMaskSeparate = (PFNGLSTENCILMASKSEPARATEPROC) load(userptr, "glStencilMaskSeparate");
    glad_glStencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC) load(userptr, "glStencilOpSeparate");
    glad_glUniform1f = (PFNGLUNIFORM1FPROC) load(userptr, "glUniform1f");
    glad_glUniform1fv = (PFNGLUNIFORM1FVPROC) load(userptr, "glUniform1fv");
    glad_glUniform1i = (PFNGLUNIFORM1IPROC) load(userptr, "glUniform1i");
    glad_glUniform1iv = (PFNGLUNIFORM1IVPROC) load(userptr, "glUniform1iv");
    glad_glUniform2f = (PFNGLUNIFORM2FPROC) load(userptr, "glUniform2f");
    glad_glUniform2fv = (PFNGLUNIFORM2FVPROC) load(userptr, "glUniform2fv");
    glad_glUniform2i = (PFNGLUNIFORM2IPROC) load(userptr, "glUniform2i");
    glad_glUniform2iv = (PFNGLUNIFORM2IVPROC) load(userptr, "glUniform2iv");
    glad_glUniform3f = (PFNGLUNIFORM3FPROC) load(userptr, "glUniform3f");
    glad_glUniform3fv = (PFNGLUNIFORM3FVPROC) load(userptr, "glUniform3fv");
    glad_glUniform3i = (PFNGLUNIFORM3IPROC) load(userptr, "glUniform3i");
    glad_glUniform3iv = (PFNGLUNIFORM3IVPROC) load(userptr, "glUniform3iv");
    glad_glUniform4f = (PFNGLUNIFORM4FPROC) load(userptr, "glUniform4f");
    glad_glUniform4fv = (PFNGLUNIFORM4FVPROC) load(userptr, "glUniform4fv");
    glad_glUniform4i = (PFNGLUNIFORM4IPROC) load(userptr, "glUniform4i");
    glad_glUniform4iv = (PFNGLUNIFORM4IVPROC) load(userptr, "glUniform4iv");
    glad_glUniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC) load(userptr, "glUniformMatrix2fv");
    glad_glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC) load(userptr, "glUniformMatrix3fv");
    glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC) load(userptr, "glUniformMatrix4fv");
    glad_glUseProgram = (PFNGLUSEPROGRAMPROC) load(userptr, "glUseProgram");
    glad_glValidateProgram = (PFNGLVALIDATEPROGRAMPROC) load(userptr, "glValidateProgram");
    glad_glVertexAttrib1d = (PFNGLVERTEXATTRIB1DPROC) load(userptr, "glVertexAttrib1d");
    glad_glVertexAttrib1dv = (PFNGLVERTEXATTRIB1DVPROC) load(userptr, "glVertexAttrib1dv");
    glad_glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC) load(userptr, "glVertexAttrib1f");
    glad_glVertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC) load(userptr, "glVertexAttrib1fv");
    glad_glVertexAttrib1s = (PFNGLVERTEXATTRIB1SPROC) load(userptr, "glVertexAttrib1s");
    glad_glVertexAttrib1sv = (PFNGLVERTEXATTRIB1SVPROC) load(userptr, "glVertexAttrib1sv");
    glad_glVertexAttrib2d = (PFNGLVERTEXATTRIB2DPROC) load(userptr, "glVertexAttrib2d");
    glad_glVertexAttrib2dv = (PFNGLVERTEXATTRIB2DVPROC) load(userptr, "glVertexAttrib2dv");
    glad_glVertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC) load(userptr, "glVertexAttrib2f");
    glad_glVertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC) load(userptr, "glVertexAttrib2fv");
    glad_glVertexAttrib2s = (PFNGLVERTEXATTRIB2SPROC) load(userptr, "glVertexAttrib2s");
    glad_glVertexAttrib2sv = (PFNGLVERTEXATTRIB2SVPROC) load(userptr, "glVertexAttrib2sv");
    glad_glVertexAttrib3d = (PFNGLVERTEXATTRIB3DPROC) load(userptr, "glVertexAttrib3d");
    glad_glVertexAttrib3dv = (PFNGLVERTEXATTRIB3DVPROC) load(userptr, "glVertexAttrib3dv");
    glad_glVertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC) load(userptr, "glVertexAttrib3f");
    glad_glVertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC) load(userptr, "glVertexAttrib3fv");
    glad_glVertexAttrib3s = (PFNGLVERTEXATTRIB3SPROC) load(userptr, "glVertexAttrib3s");
    glad_glVertexAttrib3sv = (PFNGLVERTEXATTRIB3SVPROC) load(userptr, "glVertexAttrib3sv");
    glad_glVertexAttrib4Nbv = (PFNGLVERTEXATTRIB4NBVPROC) load(userptr, "glVertexAttrib4Nbv");
    glad_glVertexAttrib4Niv = (PFNGLVERTEXATTRIB4NIVPROC) load(userptr, "glVertexAttrib4Niv");
    glad_glVertexAttrib4Nsv = (PFNGLVERTEXATTRIB4NSVPROC) load(userptr, "glVertexAttrib4Nsv");
    glad_glVertexAttrib4Nub = (PFNGLVERTEXATTRIB4NUBPROC) load(userptr, "glVertexAttrib4Nub");
    glad_glVertexAttrib4Nubv = (PFNGLVERTEXATTRIB4NUBVPROC) load(userptr, "glVertexAttrib4Nubv");
    glad_glVertexAttrib4Nuiv = (PFNGLVERTEXATTRIB4NUIVPROC) load(userptr, "glVertexAttrib4Nuiv");
    glad_glVertexAttrib4Nusv = (PFNGLVERTEXATTRIB4NUSVPROC) load(userptr, "glVertexAttrib4Nusv");
    glad_glVertexAttrib4bv = (PFNGLVERTEXATTRIB4BVPROC) load(userptr, "glVertexAttrib4bv");
    glad_glVertexAttrib4d = (PFNGLVERTEXATTRIB4DPROC) load(userptr, "glVertexAttrib4d");
    glad_glVertexAttrib4dv = (PFNGLVERTEXATTRIB4DVPROC) load(userptr, "glVertexAttrib4dv");
    glad_glVertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC) load(userptr, "glVertexAttrib4f");
    glad_glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC) load(userptr, "glVertexAttrib4fv");
    glad_glVertexAttrib4iv = (PFNGLVERTEXATTRIB4IVPROC) load(userptr, "glVertexAttrib4iv");
    glad_glVertexAttrib4s = (PFNGLVERTEXATTRIB4SPROC) load(userptr, "glVertexAttrib4s");
    glad_glVertexAttrib4sv = (PFNGLVERTEXATTRIB4SVPROC) load(userptr, "glVertexAttrib4sv");
    glad_glVertexAttrib4ubv = (PFNGLVERTEXATTRIB4UBVPROC) load(userptr, "glVertexAttrib4ubv");
    glad_glVertexAttrib4uiv = (PFNGLVERTEXATTRIB4UIVPROC) load(userptr, "glVertexAttrib4uiv");
    glad_glVertexAttrib4usv = (PFNGLVERTEXATTRIB4USVPROC) load(userptr, "glVertexAttrib4usv");
    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC) load(userptr, "glVertexAttribPointer");
}
static void glad_gl_load_GL_VERSION_2_1( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_2_1) return;
    glad_glUniformMatrix2x3fv = (PFNGLUNIFORMMATRIX2X3FVPROC) load(userptr, "glUniformMatrix2x3fv");
    glad_glUniformMatrix2x4fv = (PFNGLUNIFORMMATRIX2X4FVPROC) load(userptr, "glUniformMatrix2x4fv");
    glad_glUniformMatrix3x2fv = (PFNGLUNIFORMMATRIX3X2FVPROC) load(userptr, "glUniformMatrix3x2fv");
    glad_glUniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC) load(userptr, "glUniformMatrix3x4fv");
    glad_glUniformMatrix4x2fv = (PFNGLUNIFORMMATRIX4X2FVPROC) load(userptr, "glUniformMatrix4x2fv");
    glad_glUniformMatrix4x3fv = (PFNGLUNIFORMMATRIX4X3FVPROC) load(userptr, "glUniformMatrix4x3fv");
}
static void glad_gl_load_GL_VERSION_3_0( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_3_0) return;
    glad_glBeginConditionalRender = (PFNGLBEGINCONDITIONALRENDERPROC) load(userptr, "glBeginConditionalRender");
    glad_glBeginTransformFeedback = (PFNGLBEGINTRANSFORMFEEDBACKPROC) load(userptr, "glBeginTransformFeedback");
    glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC) load(userptr, "glBindBufferBase");
    glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC) load(userptr, "glBindBufferRange");
    glad_glBindFragDataLocation = (PFNGLBINDFRAGDATALOCATIONPROC) load(userptr, "glBindFragDataLocation");
    glad_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) load(userptr, "glBindFramebuffer");
    glad_glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) load(userptr, "glBindRenderbuffer");
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) load(userptr, "glBindVertexArray");
    glad_glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC) load(userptr, "glBlitFramebuffer");
    glad_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) load(userptr, "glCheckFramebufferStatus");
    glad_glClampColor = (PFNGLCLAMPCOLORPROC) load(userptr, "glClampColor");
    glad_glClearBufferfi = (PFNGLCLEARBUFFERFIPROC) load(userptr, "glClearBufferfi");
    glad_glClearBufferfv = (PFNGLCLEARBUFFERFVPROC) load(userptr, "glClearBufferfv");
    glad_glClearBufferiv = (PFNGLCLEARBUFFERIVPROC) load(userptr, "glClearBufferiv");
    glad_glClearBufferuiv = (PFNGLCLEARBUFFERUIVPROC) load(userptr, "glClearBufferuiv");
    glad_glColorMaski = (PFNGLCOLORMASKIPROC) load(userptr, "glColorMaski");
    glad_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) load(userptr, "glDeleteFramebuffers");
    glad_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) load(userptr, "glDeleteRenderbuffers");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) load(userptr, "glDeleteVertexArrays");
    glad_glDisablei = (PFNGLDISABLEIPROC) load(userptr, "glDisablei");
    glad_glEnablei = (PFNGLENABLEIPROC) load(userptr, "glEnablei");
    glad_glEndConditionalRender = (PFNGLENDCONDITIONALRENDERPROC) load(userptr, "glEndConditionalRender");
    glad_glEndTransformFeedback = (PFNGLENDTRANSFORMFEEDBACKPROC) load(userptr, "glEndTransformFeedback");
    glad_glFlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC) load(userptr, "glFlushMappedBufferRange");
    glad_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) load(userptr, "glFramebufferRenderbuffer");
    glad_glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC) load(userptr, "glFramebufferTexture1D");
    glad_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) load(userptr, "glFramebufferTexture2D");
    glad_glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC) load(userptr, "glFramebufferTexture3D");
    glad_glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC) load(userptr, "glFramebufferTextureLayer");
    glad_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) load(userptr, "glGenFramebuffers");
    glad_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) load(userptr, "glGenRenderbuffers");
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) load(userptr, "glGenVertexArrays");
    glad_glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC) load(userptr, "glGenerateMipmap");
    glad_glGetBooleani_v = (PFNGLGETBOOLEANI_VPROC) load(userptr, "glGetBooleani_v");
    glad_glGetFragDataLocation = (PFNGLGETFRAGDATALOCATIONPROC) load(userptr, "glGetFragDataLocation");
    glad_glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) load(userptr, "glGetFramebufferAttachmentParameteriv");
    glad_glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC) load(userptr, "glGetIntegeri_v");
    glad_glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) load(userptr, "glGetRenderbufferParameteriv");
    glad_glGetStringi = (PFNGLGETSTRINGIPROC) load(userptr, "glGetStringi");
    glad_glGetTexParameterIiv = (PFNGLGETTEXPARAMETERIIVPROC) load(userptr, "glGetTexParameterIiv");
    glad_glGetTexParameterIuiv = (PFNGLGETTEXPARAMETERIUIVPROC) load(userptr, "glGetTexParameterIuiv");
    glad_glGetTransformFeedbackVarying = (PFNGLGETTRANSFORMFEEDBACKVARYINGPROC) load(userptr, "glGetTransformFeedbackVarying");
    glad_glGetUniformuiv = (PFNGLGETUNIFORMUIVPROC) load(userptr, "glGetUniformuiv");
    glad_glGetVertexAttribIiv = (PFNGLGETVERTEXATTRIBIIVPROC) load(userptr, "glGetVertexAttribIiv");
    glad_glGetVertexAttribIuiv = (PFNGLGETVERTEXATTRIBIUIVPROC) load(userptr, "glGetVertexAttribIuiv");
    glad_glIsEnabledi = (PFNGLISENABLEDIPROC) load(userptr, "glIsEnabledi");
    glad_glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC) load(userptr, "glIsFramebuffer");
    glad_glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC) load(userptr, "glIsRenderbuffer");
    glad_glIsVertexArray = (PFNGLISVERTEXARRAYPROC) load(userptr, "glIsVertexArray");
    glad_glMapBufferRange = (PFNGLMAPBUFFERRANGEPROC) load(userptr, "glMapBufferRange");
    glad_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) load(userptr, "glRenderbufferStorage");
    glad_glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) load(userptr, "glRenderbufferStorageMultisample");
    glad_glTexParameterIiv = (PFNGLTEXPARAMETERIIVPROC) load(userptr, "glTexParameterIiv");
    glad_glTexParameterIuiv = (PFNGLTEXPARAMETERIUIVPROC) load(userptr, "glTexParameterIuiv");
    glad_glTransformFeedbackVaryings = (PFNGLTRANSFORMFEEDBACKVARYINGSPROC) load(userptr, "glTransformFeedbackVaryings");
    glad_glUniform1ui = (PFNGLUNIFORM1UIPROC) load(userptr, "glUniform1ui");
    glad_glUniform1uiv = (PFNGLUNIFORM1UIVPROC) load(userptr, "glUniform1uiv");
    glad_glUniform2ui = (PFNGLUNIFORM2UIPROC) load(userptr, "glUniform2ui");
    glad_glUniform2uiv = (PFNGLUNIFORM2UIVPROC) load(userptr, "glUniform2uiv");
    glad_glUniform3ui = (PFNGLUNIFORM3UIPROC) load(userptr, "glUniform3ui");
    glad_glUniform3uiv = (PFNGLUNIFORM3UIVPROC) load(userptr, "glUniform3uiv");
    glad_glUniform4ui = (PFNGLUNIFORM4UIPROC) load(userptr, "glUniform4ui");
    glad_glUniform4uiv = (PFNGLUNIFORM4UIVPROC) load(userptr, "glUniform4uiv");
    glad_glVertexAttribI1i = (PFNGLVERTEXATTRIBI1IPROC) load(userptr, "glVertexAttribI1i");
    glad_glVertexAttribI1iv = (PFNGLVERTEXATTRIBI1IVPROC) load(userptr, "glVertexAttribI1iv");
    glad_glVertexAttribI1ui = (PFNGLVERTEXATTRIBI1UIPROC) load(userptr, "glVertexAttribI1ui");
    glad_glVertexAttribI1uiv = (PFNGLVERTEXATTRIBI1UIVPROC) load(userptr, "glVertexAttribI1uiv");
    glad_glVertexAttribI2i = (PFNGLVERTEXATTRIBI2IPROC) load(userptr, "glVertexAttribI2i");
    glad_glVertexAttribI2iv = (PFNGLVERTEXATTRIBI2IVPROC) load(userptr, "glVertexAttribI2iv");
    glad_glVertexAttribI2ui = (PFNGLVERTEXATTRIBI2UIPROC) load(userptr, "glVertexAttribI2ui");
    glad_glVertexAttribI2uiv = (PFNGLVERTEXATTRIBI2UIVPROC) load(userptr, "glVertexAttribI2uiv");
    glad_glVertexAttribI3i = (PFNGLVERTEXATTRIBI3IPROC) load(userptr, "glVertexAttribI3i");
    glad_glVertexAttribI3iv = (PFNGLVERTEXATTRIBI3IVPROC) load(userptr, "glVertexAttribI3iv");
    glad_glVertexAttribI3ui = (PFNGLVERTEXATTRIBI3UIPROC) load(userptr, "glVertexAttribI3ui");
    glad_glVertexAttribI3uiv = (PFNGLVERTEXATTRIBI3UIVPROC) load(userptr, "glVertexAttribI3uiv");
    glad_glVertexAttribI4bv = (PFNGLVERTEXATTRIBI4BVPROC) load(userptr, "glVertexAttribI4bv");
    glad_glVertexAttribI4i = (PFNGLVERTEXATTRIBI4IPROC) load(userptr, "glVertexAttribI4i");
    glad_glVertexAttribI4iv = (PFNGLVERTEXATTRIBI4IVPROC) load(userptr, "glVertexAttribI4iv");
    glad_glVertexAttribI4sv = (PFNGLVERTEXATTRIBI4SVPROC) load(userptr, "glVertexAttribI4sv");
    glad_glVertexAttribI4ubv = (PFNGLVERTEXATTRIBI4UBVPROC) load(userptr, "glVertexAttribI4ubv");
    glad_glVertexAttribI4ui = (PFNGLVERTEXATTRIBI4UIPROC) load(userptr, "glVertexAttribI4ui");
    glad_glVertexAttribI4uiv = (PFNGLVERTEXATTRIBI4UIVPROC) load(userptr, "glVertexAttribI4uiv");
    glad_glVertexAttribI4usv = (PFNGLVERTEXATTRIBI4USVPROC) load(userptr, "glVertexAttribI4usv");
    glad_glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC) load(userptr, "glVertexAttribIPointer");
}
static void glad_gl_load_GL_VERSION_3_1( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_3_1) return;
    glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC) load(userptr, "glBindBufferBase");
    glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC) load(userptr, "glBindBufferRange");
    glad_glCopyBufferSubData = (PFNGLCOPYBUFFERSUBDATAPROC) load(userptr, "glCopyBufferSubData");
    glad_glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC) load(userptr, "glDrawArraysInstanced");
    glad_glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC) load(userptr, "glDrawElementsInstanced");
    glad_glGetActiveUniformBlockName = (PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC) load(userptr, "glGetActiveUniformBlockName");
    glad_glGetActiveUniformBlockiv = (PFNGLGETACTIVEUNIFORMBLOCKIVPROC) load(userptr, "glGetActiveUniformBlockiv");
    glad_glGetActiveUniformName = (PFNGLGETACTIVEUNIFORMNAMEPROC) load(userptr, "glGetActiveUniformName");
    glad_glGetActiveUniformsiv = (PFNGLGETACTIVEUNIFORMSIVPROC) load(userptr, "glGetActiveUniformsiv");
    glad_glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC) load(userptr, "glGetIntegeri_v");
    glad_glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC) load(userptr, "glGetUniformBlockIndex");
    glad_glGetUniformIndices = (PFNGLGETUNIFORMINDICESPROC) load(userptr, "glGetUniformIndices");
    glad_glPrimitiveRestartIndex = (PFNGLPRIMITIVERESTARTINDEXPROC) load(userptr, "glPrimitiveRestartIndex");
    glad_glTexBuffer = (PFNGLTEXBUFFERPROC) load(userptr, "glTexBuffer");
    glad_glUniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC) load(userptr, "glUniformBlockBinding");
}
static void glad_gl_load_GL_VERSION_3_2( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_3_2) return;
    glad_glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC) load(userptr, "glClientWaitSync");
    glad_glDeleteSync = (PFNGLDELETESYNCPROC) load(userptr, "glDeleteSync");
    glad_glDrawElementsBaseVertex = (PFNGLDRAWELEMENTSBASEVERTEXPROC) load(userptr, "glDrawElementsBaseVertex");
    glad_glDrawElementsInstancedBaseVertex = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC) load(userptr, "glDrawElementsInstancedBaseVertex");
    glad_glDrawRangeElementsBaseVertex = (PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC) load(userptr, "glDrawRangeElementsBaseVertex");
    glad_glFenceSync = (PFNGLFENCESYNCPROC) load(userptr, "glFenceSync");
    glad_glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC) load(userptr, "glFramebufferTexture");
    glad_glGetBufferParameteri64v = (PFNGLGETBUFFERPARAMETERI64VPROC) load(userptr, "glGetBufferParameteri64v");
    glad_glGetInteger64i_v = (PFNGLGETINTEGER64I_VPROC) load(userptr, "glGetInteger64i_v");
    glad_glGetInteger64v = (PFNGLGETINTEGER64VPROC) load(userptr, "glGetInteger64v");
    glad_glGetMultisamplefv = (PFNGLGETMULTISAMPLEFVPROC) load(userptr, "glGetMultisamplefv");
    glad_glGetSynciv = (PFNGLGETSYNCIVPROC) load(userptr, "glGetSynciv");
    glad_glIsSync = (PFNGLISSYNCPROC) load(userptr, "glIsSync");
    glad_glMultiDrawElementsBaseVertex = (PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC) load(userptr, "glMultiDrawElementsBaseVertex");
    glad_glProvokingVertex = (PFNGLPROVOKINGVERTEXPROC) load(userptr, "glProvokingVertex");
    glad_glSampleMaski = (PFNGLSAMPLEMASKIPROC) load(userptr, "glSampleMaski");
    glad_glTexImage2DMultisample = (PFNGLTEXIMAGE2DMULTISAMPLEPROC) load(userptr, "glTexImage2DMultisample");
    glad_glTexImage3DMultisample = (PFNGLTEXIMAGE3DMULTISAMPLEPROC) load(userptr, "glTexImage3DMultisample");
    glad_glWaitSync = (PFNGLWAITSYNCPROC) load(userptr, "glWaitSync");
}
static void glad_gl_load_GL_VERSION_3_3( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_3_3) return;
    glad_glBindFragDataLocationIndexed = (PFNGLBINDFRAGDATALOCATIONINDEXEDPROC) load(userptr, "glBindFragDataLocationIndexed");
    glad_glBindSampler = (PFNGLBINDSAMPLERPROC) load(userptr, "glBindSampler");
    glad_glDeleteSamplers = (PFNGLDELETESAMPLERSPROC) load(userptr, "glDeleteSamplers");
    glad_glGenSamplers = (PFNGLGENSAMPLERSPROC) load(userptr, "glGenSamplers");
    glad_glGetFragDataIndex = (PFNGLGETFRAGDATAINDEXPROC) load(userptr, "glGetFragDataIndex");
    glad_glGetQueryObjecti64v = (PFNGLGETQUERYOBJECTI64VPROC) load(userptr, "glGetQueryObjecti64v");
    glad_glGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VPROC) load(userptr, "glGetQueryObjectui64v");
    glad_glGetSamplerParameterIiv = (PFNGLGETSAMPLERPARAMETERIIVPROC) load(userptr, "glGetSamplerParameterIiv");
    glad_glGetSamplerParameterIuiv = (PFNGLGETSAMPLERPARAMETERIUIVPROC) load(userptr, "glGetSamplerParameterIuiv");
    glad_glGetSamplerParameterfv = (PFNGLGETSAMPLERPARAMETERFVPROC) load(userptr, "glGetSamplerParameterfv");
    glad_glGetSamplerParameteriv = (PFNGLGETSAMPLERPARAMETERIVPROC) load(userptr, "glGetSamplerParameteriv");
    glad_glIsSampler = (PFNGLISSAMPLERPROC) load(userptr, "glIsSampler");
    glad_glQueryCounter = (PFNGLQUERYCOUNTERPROC) load(userptr, "glQueryCounter");
    glad_glSamplerParameterIiv = (PFNGLSAMPLERPARAMETERIIVPROC) load(userptr, "glSamplerParameterIiv");
    glad_glSamplerParameterIuiv = (PFNGLSAMPLERPARAMETERIUIVPROC) load(userptr, "glSamplerParameterIuiv");
    glad_glSamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC) load(userptr, "glSamplerParameterf");
    glad_glSamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC) load(userptr, "glSamplerParameterfv");
    glad_glSamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC) load(userptr, "glSamplerParameteri");
    glad_glSamplerParameteriv = (PFNGLSAMPLERPARAMETERIVPROC) load(userptr, "glSamplerParameteriv");
    glad_glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC) load(userptr, "glVertexAttribDivisor");
    glad_glVertexAttribP1ui = (PFNGLVERTEXATTRIBP1UIPROC) load(userptr, "glVertexAttribP1ui");
    glad_glVertexAttribP1uiv = (PFNGLVERTEXATTRIBP1UIVPROC) load(userptr, "glVertexAttribP1uiv");
    glad_glVertexAttribP2ui = (PFNGLVERTEXATTRIBP2UIPROC) load(userptr, "glVertexAttribP2ui");
    glad_glVertexAttribP2uiv = (PFNGLVERTEXATTRIBP2UIVPROC) load(userptr, "glVertexAttribP2uiv");
    glad_glVertexAttribP3ui = (PFNGLVERTEXATTRIBP3UIPROC) load(userptr, "glVertexAttribP3ui");
    glad_glVertexAttribP3uiv = (PFNGLVERTEXATTRIBP3UIVPROC) load(userptr, "glVertexAttribP3uiv");
    glad_glVertexAttribP4ui = (PFNGLVERTEXATTRIBP4UIPROC) load(userptr, "glVertexAttribP4ui");
    glad_glVertexAttribP4uiv = (PFNGLVERTEXATTRIBP4UIVPROC) load(userptr, "glVertexAttribP4uiv");
}
static void glad_gl_load_GL_APPLE_flush_buffer_range( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_APPLE_flush_buffer_range) return;
    glad_glBufferParameteriAPPLE = (PFNGLBUFFERPARAMETERIAPPLEPROC) load(userptr, "glBufferParameteriAPPLE");
    glad_glFlushMappedBufferRangeAPPLE = (PFNGLFLUSHMAPPEDBUFFERRANGEAPPLEPROC) load(userptr, "glFlushMappedBufferRangeAPPLE");
}
static void glad_gl_load_GL_APPLE_vertex_array_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_APPLE_vertex_array_object) return;
    glad_glBindVertexArrayAPPLE = (PFNGLBINDVERTEXARRAYAPPLEPROC) load(userptr, "glBindVertexArrayAPPLE");
    glad_glDeleteVertexArraysAPPLE = (PFNGLDELETEVERTEXARRAYSAPPLEPROC) load(userptr, "glDeleteVertexArraysAPPLE");
    glad_glGenVertexArraysAPPLE = (PFNGLGENVERTEXARRAYSAPPLEPROC) load(userptr, "glGenVertexArraysAPPLE");
    glad_glIsVertexArrayAPPLE = (PFNGLISVERTEXARRAYAPPLEPROC) load(userptr, "glIsVertexArrayAPPLE");
}
static void glad_gl_load_GL_ARB_blend_func_extended( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_blend_func_extended) return;
    glad_glBindFragDataLocationIndexed = (PFNGLBINDFRAGDATALOCATIONINDEXEDPROC) load(userptr, "glBindFragDataLocationIndexed");
    glad_glGetFragDataIndex = (PFNGLGETFRAGDATAINDEXPROC) load(userptr, "glGetFragDataIndex");
}
static void glad_gl_load_GL_ARB_color_buffer_float( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_color_buffer_float) return;
    glad_glClampColorARB = (PFNGLCLAMPCOLORARBPROC) load(userptr, "glClampColorARB");
}
static void glad_gl_load_GL_ARB_copy_buffer( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_copy_buffer) return;
    glad_glCopyBufferSubData = (PFNGLCOPYBUFFERSUBDATAPROC) load(userptr, "glCopyBufferSubData");
}
static void glad_gl_load_GL_ARB_draw_buffers( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_draw_buffers) return;
    glad_glDrawBuffersARB = (PFNGLDRAWBUFFERSARBPROC) load(userptr, "glDrawBuffersARB");
}
static void glad_gl_load_GL_ARB_draw_elements_base_vertex( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_draw_elements_base_vertex) return;
    glad_glDrawElementsBaseVertex = (PFNGLDRAWELEMENTSBASEVERTEXPROC) load(userptr, "glDrawElementsBaseVertex");
    glad_glDrawElementsInstancedBaseVertex = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC) load(userptr, "glDrawElementsInstancedBaseVertex");
    glad_glDrawRangeElementsBaseVertex = (PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC) load(userptr, "glDrawRangeElementsBaseVertex");
    glad_glMultiDrawElementsBaseVertex = (PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC) load(userptr, "glMultiDrawElementsBaseVertex");
}
static void glad_gl_load_GL_ARB_draw_instanced( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_draw_instanced) return;
    glad_glDrawArraysInstancedARB = (PFNGLDRAWARRAYSINSTANCEDARBPROC) load(userptr, "glDrawArraysInstancedARB");
    glad_glDrawElementsInstancedARB = (PFNGLDRAWELEMENTSINSTANCEDARBPROC) load(userptr, "glDrawElementsInstancedARB");
}
static void glad_gl_load_GL_ARB_framebuffer_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_framebuffer_object) return;
    glad_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) load(userptr, "glBindFramebuffer");
    glad_glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) load(userptr, "glBindRenderbuffer");
    glad_glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC) load(userptr, "glBlitFramebuffer");
    glad_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) load(userptr, "glCheckFramebufferStatus");
    glad_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) load(userptr, "glDeleteFramebuffers");
    glad_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) load(userptr, "glDeleteRenderbuffers");
    glad_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) load(userptr, "glFramebufferRenderbuffer");
    glad_glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC) load(userptr, "glFramebufferTexture1D");
    glad_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) load(userptr, "glFramebufferTexture2D");
    glad_glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC) load(userptr, "glFramebufferTexture3D");
    glad_glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC) load(userptr, "glFramebufferTextureLayer");
    glad_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) load(userptr, "glGenFramebuffers");
    glad_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) load(userptr, "glGenRenderbuffers");
    glad_glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC) load(userptr, "glGenerateMipmap");
    glad_glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) load(userptr, "glGetFramebufferAttachmentParameteriv");
    glad_glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) load(userptr, "glGetRenderbufferParameteriv");
    glad_glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC) load(userptr, "glIsFramebuffer");
    glad_glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC) load(userptr, "glIsRenderbuffer");
    glad_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) load(userptr, "glRenderbufferStorage");
    glad_glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) load(userptr, "glRenderbufferStorageMultisample");
}
static void glad_gl_load_GL_ARB_geometry_shader4( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_geometry_shader4) return;
    glad_glFramebufferTextureARB = (PFNGLFRAMEBUFFERTEXTUREARBPROC) load(userptr, "glFramebufferTextureARB");
    glad_glFramebufferTextureFaceARB = (PFNGLFRAMEBUFFERTEXTUREFACEARBPROC) load(userptr, "glFramebufferTextureFaceARB");
    glad_glFramebufferTextureLayerARB = (PFNGLFRAMEBUFFERTEXTURELAYERARBPROC) load(userptr, "glFramebufferTextureLayerARB");
    glad_glProgramParameteriARB = (PFNGLPROGRAMPARAMETERIARBPROC) load(userptr, "glProgramParameteriARB");
}
static void glad_gl_load_GL_ARB_imaging( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_imaging) return;
    glad_glBlendColor = (PFNGLBLENDCOLORPROC) load(userptr, "glBlendColor");
    glad_glBlendEquation = (PFNGLBLENDEQUATIONPROC) load(userptr, "glBlendEquation");
}
static void glad_gl_load_GL_ARB_instanced_arrays( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_instanced_arrays) return;
    glad_glVertexAttribDivisorARB = (PFNGLVERTEXATTRIBDIVISORARBPROC) load(userptr, "glVertexAttribDivisorARB");
}
static void glad_gl_load_GL_ARB_map_buffer_range( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_map_buffer_range) return;
    glad_glFlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC) load(userptr, "glFlushMappedBufferRange");
    glad_glMapBufferRange = (PFNGLMAPBUFFERRANGEPROC) load(userptr, "glMapBufferRange");
}
static void glad_gl_load_GL_ARB_multisample( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_multisample) return;
    glad_glSampleCoverageARB = (PFNGLSAMPLECOVERAGEARBPROC) load(userptr, "glSampleCoverageARB");
}
static void glad_gl_load_GL_ARB_multitexture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_multitexture) return;
    glad_glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC) load(userptr, "glActiveTextureARB");
    glad_glClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC) load(userptr, "glClientActiveTextureARB");
    glad_glMultiTexCoord1dARB = (PFNGLMULTITEXCOORD1DARBPROC) load(userptr, "glMultiTexCoord1dARB");
    glad_glMultiTexCoord1dvARB = (PFNGLMULTITEXCOORD1DVARBPROC) load(userptr, "glMultiTexCoord1dvARB");
    glad_glMultiTexCoord1fARB = (PFNGLMULTITEXCOORD1FARBPROC) load(userptr, "glMultiTexCoord1fARB");
    glad_glMultiTexCoord1fvARB = (PFNGLMULTITEXCOORD1FVARBPROC) load(userptr, "glMultiTexCoord1fvARB");
    glad_glMultiTexCoord1iARB = (PFNGLMULTITEXCOORD1IARBPROC) load(userptr, "glMultiTexCoord1iARB");
    glad_glMultiTexCoord1ivARB = (PFNGLMULTITEXCOORD1IVARBPROC) load(userptr, "glMultiTexCoord1ivARB");
    glad_glMultiTexCoord1sARB = (PFNGLMULTITEXCOORD1SARBPROC) load(userptr, "glMultiTexCoord1sARB");
    glad_glMultiTexCoord1svARB = (PFNGLMULTITEXCOORD1SVARBPROC) load(userptr, "glMultiTexCoord1svARB");
    glad_glMultiTexCoord2dARB = (PFNGLMULTITEXCOORD2DARBPROC) load(userptr, "glMultiTexCoord2dARB");
    glad_glMultiTexCoord2dvARB = (PFNGLMULTITEXCOORD2DVARBPROC) load(userptr, "glMultiTexCoord2dvARB");
    glad_glMultiTexCoord2fARB = (PFNGLMULTITEXCOORD2FARBPROC) load(userptr, "glMultiTexCoord2fARB");
    glad_glMultiTexCoord2fvARB = (PFNGLMULTITEXCOORD2FVARBPROC) load(userptr, "glMultiTexCoord2fvARB");
    glad_glMultiTexCoord2iARB = (PFNGLMULTITEXCOORD2IARBPROC) load(userptr, "glMultiTexCoord2iARB");
    glad_glMultiTexCoord2ivARB = (PFNGLMULTITEXCOORD2IVARBPROC) load(userptr, "glMultiTexCoord2ivARB");
    glad_glMultiTexCoord2sARB = (PFNGLMULTITEXCOORD2SARBPROC) load(userptr, "glMultiTexCoord2sARB");
    glad_glMultiTexCoord2svARB = (PFNGLMULTITEXCOORD2SVARBPROC) load(userptr, "glMultiTexCoord2svARB");
    glad_glMultiTexCoord3dARB = (PFNGLMULTITEXCOORD3DARBPROC) load(userptr, "glMultiTexCoord3dARB");
    glad_glMultiTexCoord3dvARB = (PFNGLMULTITEXCOORD3DVARBPROC) load(userptr, "glMultiTexCoord3dvARB");
    glad_glMultiTexCoord3fARB = (PFNGLMULTITEXCOORD3FARBPROC) load(userptr, "glMultiTexCoord3fARB");
    glad_glMultiTexCoord3fvARB = (PFNGLMULTITEXCOORD3FVARBPROC) load(userptr, "glMultiTexCoord3fvARB");
    glad_glMultiTexCoord3iARB = (PFNGLMULTITEXCOORD3IARBPROC) load(userptr, "glMultiTexCoord3iARB");
    glad_glMultiTexCoord3ivARB = (PFNGLMULTITEXCOORD3IVARBPROC) load(userptr, "glMultiTexCoord3ivARB");
    glad_glMultiTexCoord3sARB = (PFNGLMULTITEXCOORD3SARBPROC) load(userptr, "glMultiTexCoord3sARB");
    glad_glMultiTexCoord3svARB = (PFNGLMULTITEXCOORD3SVARBPROC) load(userptr, "glMultiTexCoord3svARB");
    glad_glMultiTexCoord4dARB = (PFNGLMULTITEXCOORD4DARBPROC) load(userptr, "glMultiTexCoord4dARB");
    glad_glMultiTexCoord4dvARB = (PFNGLMULTITEXCOORD4DVARBPROC) load(userptr, "glMultiTexCoord4dvARB");
    glad_glMultiTexCoord4fARB = (PFNGLMULTITEXCOORD4FARBPROC) load(userptr, "glMultiTexCoord4fARB");
    glad_glMultiTexCoord4fvARB = (PFNGLMULTITEXCOORD4FVARBPROC) load(userptr, "glMultiTexCoord4fvARB");
    glad_glMultiTexCoord4iARB = (PFNGLMULTITEXCOORD4IARBPROC) load(userptr, "glMultiTexCoord4iARB");
    glad_glMultiTexCoord4ivARB = (PFNGLMULTITEXCOORD4IVARBPROC) load(userptr, "glMultiTexCoord4ivARB");
    glad_glMultiTexCoord4sARB = (PFNGLMULTITEXCOORD4SARBPROC) load(userptr, "glMultiTexCoord4sARB");
    glad_glMultiTexCoord4svARB = (PFNGLMULTITEXCOORD4SVARBPROC) load(userptr, "glMultiTexCoord4svARB");
}
static void glad_gl_load_GL_ARB_occlusion_query( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_occlusion_query) return;
    glad_glBeginQueryARB = (PFNGLBEGINQUERYARBPROC) load(userptr, "glBeginQueryARB");
    glad_glDeleteQueriesARB = (PFNGLDELETEQUERIESARBPROC) load(userptr, "glDeleteQueriesARB");
    glad_glEndQueryARB = (PFNGLENDQUERYARBPROC) load(userptr, "glEndQueryARB");
    glad_glGenQueriesARB = (PFNGLGENQUERIESARBPROC) load(userptr, "glGenQueriesARB");
    glad_glGetQueryObjectivARB = (PFNGLGETQUERYOBJECTIVARBPROC) load(userptr, "glGetQueryObjectivARB");
    glad_glGetQueryObjectuivARB = (PFNGLGETQUERYOBJECTUIVARBPROC) load(userptr, "glGetQueryObjectuivARB");
    glad_glGetQueryivARB = (PFNGLGETQUERYIVARBPROC) load(userptr, "glGetQueryivARB");
    glad_glIsQueryARB = (PFNGLISQUERYARBPROC) load(userptr, "glIsQueryARB");
}
static void glad_gl_load_GL_ARB_point_parameters( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_point_parameters) return;
    glad_glPointParameterfARB = (PFNGLPOINTPARAMETERFARBPROC) load(userptr, "glPointParameterfARB");
    glad_glPointParameterfvARB = (PFNGLPOINTPARAMETERFVARBPROC) load(userptr, "glPointParameterfvARB");
}
static void glad_gl_load_GL_ARB_provoking_vertex( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_provoking_vertex) return;
    glad_glProvokingVertex = (PFNGLPROVOKINGVERTEXPROC) load(userptr, "glProvokingVertex");
}
static void glad_gl_load_GL_ARB_sampler_objects( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_sampler_objects) return;
    glad_glBindSampler = (PFNGLBINDSAMPLERPROC) load(userptr, "glBindSampler");
    glad_glDeleteSamplers = (PFNGLDELETESAMPLERSPROC) load(userptr, "glDeleteSamplers");
    glad_glGenSamplers = (PFNGLGENSAMPLERSPROC) load(userptr, "glGenSamplers");
    glad_glGetSamplerParameterIiv = (PFNGLGETSAMPLERPARAMETERIIVPROC) load(userptr, "glGetSamplerParameterIiv");
    glad_glGetSamplerParameterIuiv = (PFNGLGETSAMPLERPARAMETERIUIVPROC) load(userptr, "glGetSamplerParameterIuiv");
    glad_glGetSamplerParameterfv = (PFNGLGETSAMPLERPARAMETERFVPROC) load(userptr, "glGetSamplerParameterfv");
    glad_glGetSamplerParameteriv = (PFNGLGETSAMPLERPARAMETERIVPROC) load(userptr, "glGetSamplerParameteriv");
    glad_glIsSampler = (PFNGLISSAMPLERPROC) load(userptr, "glIsSampler");
    glad_glSamplerParameterIiv = (PFNGLSAMPLERPARAMETERIIVPROC) load(userptr, "glSamplerParameterIiv");
    glad_glSamplerParameterIuiv = (PFNGLSAMPLERPARAMETERIUIVPROC) load(userptr, "glSamplerParameterIuiv");
    glad_glSamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC) load(userptr, "glSamplerParameterf");
    glad_glSamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC) load(userptr, "glSamplerParameterfv");
    glad_glSamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC) load(userptr, "glSamplerParameteri");
    glad_glSamplerParameteriv = (PFNGLSAMPLERPARAMETERIVPROC) load(userptr, "glSamplerParameteriv");
}
static void glad_gl_load_GL_ARB_shader_objects( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_shader_objects) return;
    glad_glAttachObjectARB = (PFNGLATTACHOBJECTARBPROC) load(userptr, "glAttachObjectARB");
    glad_glCompileShaderARB = (PFNGLCOMPILESHADERARBPROC) load(userptr, "glCompileShaderARB");
    glad_glCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC) load(userptr, "glCreateProgramObjectARB");
    glad_glCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC) load(userptr, "glCreateShaderObjectARB");
    glad_glDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC) load(userptr, "glDeleteObjectARB");
    glad_glDetachObjectARB = (PFNGLDETACHOBJECTARBPROC) load(userptr, "glDetachObjectARB");
    glad_glGetActiveUniformARB = (PFNGLGETACTIVEUNIFORMARBPROC) load(userptr, "glGetActiveUniformARB");
    glad_glGetAttachedObjectsARB = (PFNGLGETATTACHEDOBJECTSARBPROC) load(userptr, "glGetAttachedObjectsARB");
    glad_glGetHandleARB = (PFNGLGETHANDLEARBPROC) load(userptr, "glGetHandleARB");
    glad_glGetInfoLogARB = (PFNGLGETINFOLOGARBPROC) load(userptr, "glGetInfoLogARB");
    glad_glGetObjectParameterfvARB = (PFNGLGETOBJECTPARAMETERFVARBPROC) load(userptr, "glGetObjectParameterfvARB");
    glad_glGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC) load(userptr, "glGetObjectParameterivARB");
    glad_glGetShaderSourceARB = (PFNGLGETSHADERSOURCEARBPROC) load(userptr, "glGetShaderSourceARB");
    glad_glGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC) load(userptr, "glGetUniformLocationARB");
    glad_glGetUniformfvARB = (PFNGLGETUNIFORMFVARBPROC) load(userptr, "glGetUniformfvARB");
    glad_glGetUniformivARB = (PFNGLGETUNIFORMIVARBPROC) load(userptr, "glGetUniformivARB");
    glad_glLinkProgramARB = (PFNGLLINKPROGRAMARBPROC) load(userptr, "glLinkProgramARB");
    glad_glShaderSourceARB = (PFNGLSHADERSOURCEARBPROC) load(userptr, "glShaderSourceARB");
    glad_glUniform1fARB = (PFNGLUNIFORM1FARBPROC) load(userptr, "glUniform1fARB");
    glad_glUniform1fvARB = (PFNGLUNIFORM1FVARBPROC) load(userptr, "glUniform1fvARB");
    glad_glUniform1iARB = (PFNGLUNIFORM1IARBPROC) load(userptr, "glUniform1iARB");
    glad_glUniform1ivARB = (PFNGLUNIFORM1IVARBPROC) load(userptr, "glUniform1ivARB");
    glad_glUniform2fARB = (PFNGLUNIFORM2FARBPROC) load(userptr, "glUniform2fARB");
    glad_glUniform2fvARB = (PFNGLUNIFORM2FVARBPROC) load(userptr, "glUniform2fvARB");
    glad_glUniform2iARB = (PFNGLUNIFORM2IARBPROC) load(userptr, "glUniform2iARB");
    glad_glUniform2ivARB = (PFNGLUNIFORM2IVARBPROC) load(userptr, "glUniform2ivARB");
    glad_glUniform3fARB = (PFNGLUNIFORM3FARBPROC) load(userptr, "glUniform3fARB");
    glad_glUniform3fvARB = (PFNGLUNIFORM3FVARBPROC) load(userptr, "glUniform3fvARB");
    glad_glUniform3iARB = (PFNGLUNIFORM3IARBPROC) load(userptr, "glUniform3iARB");
    glad_glUniform3ivARB = (PFNGLUNIFORM3IVARBPROC) load(userptr, "glUniform3ivARB");
    glad_glUniform4fARB = (PFNGLUNIFORM4FARBPROC) load(userptr, "glUniform4fARB");
    glad_glUniform4fvARB = (PFNGLUNIFORM4FVARBPROC) load(userptr, "glUniform4fvARB");
    glad_glUniform4iARB = (PFNGLUNIFORM4IARBPROC) load(userptr, "glUniform4iARB");
    glad_glUniform4ivARB = (PFNGLUNIFORM4IVARBPROC) load(userptr, "glUniform4ivARB");
    glad_glUniformMatrix2fvARB = (PFNGLUNIFORMMATRIX2FVARBPROC) load(userptr, "glUniformMatrix2fvARB");
    glad_glUniformMatrix3fvARB = (PFNGLUNIFORMMATRIX3FVARBPROC) load(userptr, "glUniformMatrix3fvARB");
    glad_glUniformMatrix4fvARB = (PFNGLUNIFORMMATRIX4FVARBPROC) load(userptr, "glUniformMatrix4fvARB");
    glad_glUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC) load(userptr, "glUseProgramObjectARB");
    glad_glValidateProgramARB = (PFNGLVALIDATEPROGRAMARBPROC) load(userptr, "glValidateProgramARB");
}
static void glad_gl_load_GL_ARB_sync( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_sync) return;
    glad_glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC) load(userptr, "glClientWaitSync");
    glad_glDeleteSync = (PFNGLDELETESYNCPROC) load(userptr, "glDeleteSync");
    glad_glFenceSync = (PFNGLFENCESYNCPROC) load(userptr, "glFenceSync");
    glad_glGetInteger64v = (PFNGLGETINTEGER64VPROC) load(userptr, "glGetInteger64v");
    glad_glGetSynciv = (PFNGLGETSYNCIVPROC) load(userptr, "glGetSynciv");
    glad_glIsSync = (PFNGLISSYNCPROC) load(userptr, "glIsSync");
    glad_glWaitSync = (PFNGLWAITSYNCPROC) load(userptr, "glWaitSync");
}
static void glad_gl_load_GL_ARB_texture_buffer_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_texture_buffer_object) return;
    glad_glTexBufferARB = (PFNGLTEXBUFFERARBPROC) load(userptr, "glTexBufferARB");
}
static void glad_gl_load_GL_ARB_texture_compression( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_texture_compression) return;
    glad_glCompressedTexImage1DARB = (PFNGLCOMPRESSEDTEXIMAGE1DARBPROC) load(userptr, "glCompressedTexImage1DARB");
    glad_glCompressedTexImage2DARB = (PFNGLCOMPRESSEDTEXIMAGE2DARBPROC) load(userptr, "glCompressedTexImage2DARB");
    glad_glCompressedTexImage3DARB = (PFNGLCOMPRESSEDTEXIMAGE3DARBPROC) load(userptr, "glCompressedTexImage3DARB");
    glad_glCompressedTexSubImage1DARB = (PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC) load(userptr, "glCompressedTexSubImage1DARB");
    glad_glCompressedTexSubImage2DARB = (PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC) load(userptr, "glCompressedTexSubImage2DARB");
    glad_glCompressedTexSubImage3DARB = (PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC) load(userptr, "glCompressedTexSubImage3DARB");
    glad_glGetCompressedTexImageARB = (PFNGLGETCOMPRESSEDTEXIMAGEARBPROC) load(userptr, "glGetCompressedTexImageARB");
}
static void glad_gl_load_GL_ARB_texture_multisample( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_texture_multisample) return;
    glad_glGetMultisamplefv = (PFNGLGETMULTISAMPLEFVPROC) load(userptr, "glGetMultisamplefv");
    glad_glSampleMaski = (PFNGLSAMPLEMASKIPROC) load(userptr, "glSampleMaski");
    glad_glTexImage2DMultisample = (PFNGLTEXIMAGE2DMULTISAMPLEPROC) load(userptr, "glTexImage2DMultisample");
    glad_glTexImage3DMultisample = (PFNGLTEXIMAGE3DMULTISAMPLEPROC) load(userptr, "glTexImage3DMultisample");
}
static void glad_gl_load_GL_ARB_timer_query( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_timer_query) return;
    glad_glGetQueryObjecti64v = (PFNGLGETQUERYOBJECTI64VPROC) load(userptr, "glGetQueryObjecti64v");
    glad_glGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VPROC) load(userptr, "glGetQueryObjectui64v");
    glad_glQueryCounter = (PFNGLQUERYCOUNTERPROC) load(userptr, "glQueryCounter");
}
static void glad_gl_load_GL_ARB_uniform_buffer_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_uniform_buffer_object) return;
    glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC) load(userptr, "glBindBufferBase");
    glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC) load(userptr, "glBindBufferRange");
    glad_glGetActiveUniformBlockName = (PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC) load(userptr, "glGetActiveUniformBlockName");
    glad_glGetActiveUniformBlockiv = (PFNGLGETACTIVEUNIFORMBLOCKIVPROC) load(userptr, "glGetActiveUniformBlockiv");
    glad_glGetActiveUniformName = (PFNGLGETACTIVEUNIFORMNAMEPROC) load(userptr, "glGetActiveUniformName");
    glad_glGetActiveUniformsiv = (PFNGLGETACTIVEUNIFORMSIVPROC) load(userptr, "glGetActiveUniformsiv");
    glad_glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC) load(userptr, "glGetIntegeri_v");
    glad_glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC) load(userptr, "glGetUniformBlockIndex");
    glad_glGetUniformIndices = (PFNGLGETUNIFORMINDICESPROC) load(userptr, "glGetUniformIndices");
    glad_glUniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC) load(userptr, "glUniformBlockBinding");
}
static void glad_gl_load_GL_ARB_vertex_array_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_vertex_array_object) return;
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) load(userptr, "glBindVertexArray");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) load(userptr, "glDeleteVertexArrays");
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) load(userptr, "glGenVertexArrays");
    glad_glIsVertexArray = (PFNGLISVERTEXARRAYPROC) load(userptr, "glIsVertexArray");
}
static void glad_gl_load_GL_ARB_vertex_buffer_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_vertex_buffer_object) return;
    glad_glBindBufferARB = (PFNGLBINDBUFFERARBPROC) load(userptr, "glBindBufferARB");
    glad_glBufferDataARB = (PFNGLBUFFERDATAARBPROC) load(userptr, "glBufferDataARB");
    glad_glBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC) load(userptr, "glBufferSubDataARB");
    glad_glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC) load(userptr, "glDeleteBuffersARB");
    glad_glGenBuffersARB = (PFNGLGENBUFFERSARBPROC) load(userptr, "glGenBuffersARB");
    glad_glGetBufferParameterivARB = (PFNGLGETBUFFERPARAMETERIVARBPROC) load(userptr, "glGetBufferParameterivARB");
    glad_glGetBufferPointervARB = (PFNGLGETBUFFERPOINTERVARBPROC) load(userptr, "glGetBufferPointervARB");
    glad_glGetBufferSubDataARB = (PFNGLGETBUFFERSUBDATAARBPROC) load(userptr, "glGetBufferSubDataARB");
    glad_glIsBufferARB = (PFNGLISBUFFERARBPROC) load(userptr, "glIsBufferARB");
    glad_glMapBufferARB = (PFNGLMAPBUFFERARBPROC) load(userptr, "glMapBufferARB");
    glad_glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC) load(userptr, "glUnmapBufferARB");
}
static void glad_gl_load_GL_ARB_vertex_program( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_vertex_program) return;
    glad_glBindProgramARB = (PFNGLBINDPROGRAMARBPROC) load(userptr, "glBindProgramARB");
    glad_glDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC) load(userptr, "glDeleteProgramsARB");
    glad_glDisableVertexAttribArrayARB = (PFNGLDISABLEVERTEXATTRIBARRAYARBPROC) load(userptr, "glDisableVertexAttribArrayARB");
    glad_glEnableVertexAttribArrayARB = (PFNGLENABLEVERTEXATTRIBARRAYARBPROC) load(userptr, "glEnableVertexAttribArrayARB");
    glad_glGenProgramsARB = (PFNGLGENPROGRAMSARBPROC) load(userptr, "glGenProgramsARB");
    glad_glGetProgramEnvParameterdvARB = (PFNGLGETPROGRAMENVPARAMETERDVARBPROC) load(userptr, "glGetProgramEnvParameterdvARB");
    glad_glGetProgramEnvParameterfvARB = (PFNGLGETPROGRAMENVPARAMETERFVARBPROC) load(userptr, "glGetProgramEnvParameterfvARB");
    glad_glGetProgramLocalParameterdvARB = (PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC) load(userptr, "glGetProgramLocalParameterdvARB");
    glad_glGetProgramLocalParameterfvARB = (PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC) load(userptr, "glGetProgramLocalParameterfvARB");
    glad_glGetProgramStringARB = (PFNGLGETPROGRAMSTRINGARBPROC) load(userptr, "glGetProgramStringARB");
    glad_glGetProgramivARB = (PFNGLGETPROGRAMIVARBPROC) load(userptr, "glGetProgramivARB");
    glad_glGetVertexAttribPointervARB = (PFNGLGETVERTEXATTRIBPOINTERVARBPROC) load(userptr, "glGetVertexAttribPointervARB");
    glad_glGetVertexAttribdvARB = (PFNGLGETVERTEXATTRIBDVARBPROC) load(userptr, "glGetVertexAttribdvARB");
    glad_glGetVertexAttribfvARB = (PFNGLGETVERTEXATTRIBFVARBPROC) load(userptr, "glGetVertexAttribfvARB");
    glad_glGetVertexAttribivARB = (PFNGLGETVERTEXATTRIBIVARBPROC) load(userptr, "glGetVertexAttribivARB");
    glad_glIsProgramARB = (PFNGLISPROGRAMARBPROC) load(userptr, "glIsProgramARB");
    glad_glProgramEnvParameter4dARB = (PFNGLPROGRAMENVPARAMETER4DARBPROC) load(userptr, "glProgramEnvParameter4dARB");
    glad_glProgramEnvParameter4dvARB = (PFNGLPROGRAMENVPARAMETER4DVARBPROC) load(userptr, "glProgramEnvParameter4dvARB");
    glad_glProgramEnvParameter4fARB = (PFNGLPROGRAMENVPARAMETER4FARBPROC) load(userptr, "glProgramEnvParameter4fARB");
    glad_glProgramEnvParameter4fvARB = (PFNGLPROGRAMENVPARAMETER4FVARBPROC) load(userptr, "glProgramEnvParameter4fvARB");
    glad_glProgramLocalParameter4dARB = (PFNGLPROGRAMLOCALPARAMETER4DARBPROC) load(userptr, "glProgramLocalParameter4dARB");
    glad_glProgramLocalParameter4dvARB = (PFNGLPROGRAMLOCALPARAMETER4DVARBPROC) load(userptr, "glProgramLocalParameter4dvARB");
    glad_glProgramLocalParameter4fARB = (PFNGLPROGRAMLOCALPARAMETER4FARBPROC) load(userptr, "glProgramLocalParameter4fARB");
    glad_glProgramLocalParameter4fvARB = (PFNGLPROGRAMLOCALPARAMETER4FVARBPROC) load(userptr, "glProgramLocalParameter4fvARB");
    glad_glProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC) load(userptr, "glProgramStringARB");
    glad_glVertexAttrib1dARB = (PFNGLVERTEXATTRIB1DARBPROC) load(userptr, "glVertexAttrib1dARB");
    glad_glVertexAttrib1dvARB = (PFNGLVERTEXATTRIB1DVARBPROC) load(userptr, "glVertexAttrib1dvARB");
    glad_glVertexAttrib1fARB = (PFNGLVERTEXATTRIB1FARBPROC) load(userptr, "glVertexAttrib1fARB");
    glad_glVertexAttrib1fvARB = (PFNGLVERTEXATTRIB1FVARBPROC) load(userptr, "glVertexAttrib1fvARB");
    glad_glVertexAttrib1sARB = (PFNGLVERTEXATTRIB1SARBPROC) load(userptr, "glVertexAttrib1sARB");
    glad_glVertexAttrib1svARB = (PFNGLVERTEXATTRIB1SVARBPROC) load(userptr, "glVertexAttrib1svARB");
    glad_glVertexAttrib2dARB = (PFNGLVERTEXATTRIB2DARBPROC) load(userptr, "glVertexAttrib2dARB");
    glad_glVertexAttrib2dvARB = (PFNGLVERTEXATTRIB2DVARBPROC) load(userptr, "glVertexAttrib2dvARB");
    glad_glVertexAttrib2fARB = (PFNGLVERTEXATTRIB2FARBPROC) load(userptr, "glVertexAttrib2fARB");
    glad_glVertexAttrib2fvARB = (PFNGLVERTEXATTRIB2FVARBPROC) load(userptr, "glVertexAttrib2fvARB");
    glad_glVertexAttrib2sARB = (PFNGLVERTEXATTRIB2SARBPROC) load(userptr, "glVertexAttrib2sARB");
    glad_glVertexAttrib2svARB = (PFNGLVERTEXATTRIB2SVARBPROC) load(userptr, "glVertexAttrib2svARB");
    glad_glVertexAttrib3dARB = (PFNGLVERTEXATTRIB3DARBPROC) load(userptr, "glVertexAttrib3dARB");
    glad_glVertexAttrib3dvARB = (PFNGLVERTEXATTRIB3DVARBPROC) load(userptr, "glVertexAttrib3dvARB");
    glad_glVertexAttrib3fARB = (PFNGLVERTEXATTRIB3FARBPROC) load(userptr, "glVertexAttrib3fARB");
    glad_glVertexAttrib3fvARB = (PFNGLVERTEXATTRIB3FVARBPROC) load(userptr, "glVertexAttrib3fvARB");
    glad_glVertexAttrib3sARB = (PFNGLVERTEXATTRIB3SARBPROC) load(userptr, "glVertexAttrib3sARB");
    glad_glVertexAttrib3svARB = (PFNGLVERTEXATTRIB3SVARBPROC) load(userptr, "glVertexAttrib3svARB");
    glad_glVertexAttrib4NbvARB = (PFNGLVERTEXATTRIB4NBVARBPROC) load(userptr, "glVertexAttrib4NbvARB");
    glad_glVertexAttrib4NivARB = (PFNGLVERTEXATTRIB4NIVARBPROC) load(userptr, "glVertexAttrib4NivARB");
    glad_glVertexAttrib4NsvARB = (PFNGLVERTEXATTRIB4NSVARBPROC) load(userptr, "glVertexAttrib4NsvARB");
    glad_glVertexAttrib4NubARB = (PFNGLVERTEXATTRIB4NUBARBPROC) load(userptr, "glVertexAttrib4NubARB");
    glad_glVertexAttrib4NubvARB = (PFNGLVERTEXATTRIB4NUBVARBPROC) load(userptr, "glVertexAttrib4NubvARB");
    glad_glVertexAttrib4NuivARB = (PFNGLVERTEXATTRIB4NUIVARBPROC) load(userptr, "glVertexAttrib4NuivARB");
    glad_glVertexAttrib4NusvARB = (PFNGLVERTEXATTRIB4NUSVARBPROC) load(userptr, "glVertexAttrib4NusvARB");
    glad_glVertexAttrib4bvARB = (PFNGLVERTEXATTRIB4BVARBPROC) load(userptr, "glVertexAttrib4bvARB");
    glad_glVertexAttrib4dARB = (PFNGLVERTEXATTRIB4DARBPROC) load(userptr, "glVertexAttrib4dARB");
    glad_glVertexAttrib4dvARB = (PFNGLVERTEXATTRIB4DVARBPROC) load(userptr, "glVertexAttrib4dvARB");
    glad_glVertexAttrib4fARB = (PFNGLVERTEXATTRIB4FARBPROC) load(userptr, "glVertexAttrib4fARB");
    glad_glVertexAttrib4fvARB = (PFNGLVERTEXATTRIB4FVARBPROC) load(userptr, "glVertexAttrib4fvARB");
    glad_glVertexAttrib4ivARB = (PFNGLVERTEXATTRIB4IVARBPROC) load(userptr, "glVertexAttrib4ivARB");
    glad_glVertexAttrib4sARB = (PFNGLVERTEXATTRIB4SARBPROC) load(userptr, "glVertexAttrib4sARB");
    glad_glVertexAttrib4svARB = (PFNGLVERTEXATTRIB4SVARBPROC) load(userptr, "glVertexAttrib4svARB");
    glad_glVertexAttrib4ubvARB = (PFNGLVERTEXATTRIB4UBVARBPROC) load(userptr, "glVertexAttrib4ubvARB");
    glad_glVertexAttrib4uivARB = (PFNGLVERTEXATTRIB4UIVARBPROC) load(userptr, "glVertexAttrib4uivARB");
    glad_glVertexAttrib4usvARB = (PFNGLVERTEXATTRIB4USVARBPROC) load(userptr, "glVertexAttrib4usvARB");
    glad_glVertexAttribPointerARB = (PFNGLVERTEXATTRIBPOINTERARBPROC) load(userptr, "glVertexAttribPointerARB");
}
static void glad_gl_load_GL_ARB_vertex_shader( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_vertex_shader) return;
    glad_glBindAttribLocationARB = (PFNGLBINDATTRIBLOCATIONARBPROC) load(userptr, "glBindAttribLocationARB");
    glad_glDisableVertexAttribArrayARB = (PFNGLDISABLEVERTEXATTRIBARRAYARBPROC) load(userptr, "glDisableVertexAttribArrayARB");
    glad_glEnableVertexAttribArrayARB = (PFNGLENABLEVERTEXATTRIBARRAYARBPROC) load(userptr, "glEnableVertexAttribArrayARB");
    glad_glGetActiveAttribARB = (PFNGLGETACTIVEATTRIBARBPROC) load(userptr, "glGetActiveAttribARB");
    glad_glGetAttribLocationARB = (PFNGLGETATTRIBLOCATIONARBPROC) load(userptr, "glGetAttribLocationARB");
    glad_glGetVertexAttribPointervARB = (PFNGLGETVERTEXATTRIBPOINTERVARBPROC) load(userptr, "glGetVertexAttribPointervARB");
    glad_glGetVertexAttribdvARB = (PFNGLGETVERTEXATTRIBDVARBPROC) load(userptr, "glGetVertexAttribdvARB");
    glad_glGetVertexAttribfvARB = (PFNGLGETVERTEXATTRIBFVARBPROC) load(userptr, "glGetVertexAttribfvARB");
    glad_glGetVertexAttribivARB = (PFNGLGETVERTEXATTRIBIVARBPROC) load(userptr, "glGetVertexAttribivARB");
    glad_glVertexAttrib1dARB = (PFNGLVERTEXATTRIB1DARBPROC) load(userptr, "glVertexAttrib1dARB");
    glad_glVertexAttrib1dvARB = (PFNGLVERTEXATTRIB1DVARBPROC) load(userptr, "glVertexAttrib1dvARB");
    glad_glVertexAttrib1fARB = (PFNGLVERTEXATTRIB1FARBPROC) load(userptr, "glVertexAttrib1fARB");
    glad_glVertexAttrib1fvARB = (PFNGLVERTEXATTRIB1FVARBPROC) load(userptr, "glVertexAttrib1fvARB");
    glad_glVertexAttrib1sARB = (PFNGLVERTEXATTRIB1SARBPROC) load(userptr, "glVertexAttrib1sARB");
    glad_glVertexAttrib1svARB = (PFNGLVERTEXATTRIB1SVARBPROC) load(userptr, "glVertexAttrib1svARB");
    glad_glVertexAttrib2dARB = (PFNGLVERTEXATTRIB2DARBPROC) load(userptr, "glVertexAttrib2dARB");
    glad_glVertexAttrib2dvARB = (PFNGLVERTEXATTRIB2DVARBPROC) load(userptr, "glVertexAttrib2dvARB");
    glad_glVertexAttrib2fARB = (PFNGLVERTEXATTRIB2FARBPROC) load(userptr, "glVertexAttrib2fARB");
    glad_glVertexAttrib2fvARB = (PFNGLVERTEXATTRIB2FVARBPROC) load(userptr, "glVertexAttrib2fvARB");
    glad_glVertexAttrib2sARB = (PFNGLVERTEXATTRIB2SARBPROC) load(userptr, "glVertexAttrib2sARB");
    glad_glVertexAttrib2svARB = (PFNGLVERTEXATTRIB2SVARBPROC) load(userptr, "glVertexAttrib2svARB");
    glad_glVertexAttrib3dARB = (PFNGLVERTEXATTRIB3DARBPROC) load(userptr, "glVertexAttrib3dARB");
    glad_glVertexAttrib3dvARB = (PFNGLVERTEXATTRIB3DVARBPROC) load(userptr, "glVertexAttrib3dvARB");
    glad_glVertexAttrib3fARB = (PFNGLVERTEXATTRIB3FARBPROC) load(userptr, "glVertexAttrib3fARB");
    glad_glVertexAttrib3fvARB = (PFNGLVERTEXATTRIB3FVARBPROC) load(userptr, "glVertexAttrib3fvARB");
    glad_glVertexAttrib3sARB = (PFNGLVERTEXATTRIB3SARBPROC) load(userptr, "glVertexAttrib3sARB");
    glad_glVertexAttrib3svARB = (PFNGLVERTEXATTRIB3SVARBPROC) load(userptr, "glVertexAttrib3svARB");
    glad_glVertexAttrib4NbvARB = (PFNGLVERTEXATTRIB4NBVARBPROC) load(userptr, "glVertexAttrib4NbvARB");
    glad_glVertexAttrib4NivARB = (PFNGLVERTEXATTRIB4NIVARBPROC) load(userptr, "glVertexAttrib4NivARB");
    glad_glVertexAttrib4NsvARB = (PFNGLVERTEXATTRIB4NSVARBPROC) load(userptr, "glVertexAttrib4NsvARB");
    glad_glVertexAttrib4NubARB = (PFNGLVERTEXATTRIB4NUBARBPROC) load(userptr, "glVertexAttrib4NubARB");
    glad_glVertexAttrib4NubvARB = (PFNGLVERTEXATTRIB4NUBVARBPROC) load(userptr, "glVertexAttrib4NubvARB");
    glad_glVertexAttrib4NuivARB = (PFNGLVERTEXATTRIB4NUIVARBPROC) load(userptr, "glVertexAttrib4NuivARB");
    glad_glVertexAttrib4NusvARB = (PFNGLVERTEXATTRIB4NUSVARBPROC) load(userptr, "glVertexAttrib4NusvARB");
    glad_glVertexAttrib4bvARB = (PFNGLVERTEXATTRIB4BVARBPROC) load(userptr, "glVertexAttrib4bvARB");
    glad_glVertexAttrib4dARB = (PFNGLVERTEXATTRIB4DARBPROC) load(userptr, "glVertexAttrib4dARB");
    glad_glVertexAttrib4dvARB = (PFNGLVERTEXATTRIB4DVARBPROC) load(userptr, "glVertexAttrib4dvARB");
    glad_glVertexAttrib4fARB = (PFNGLVERTEXATTRIB4FARBPROC) load(userptr, "glVertexAttrib4fARB");
    glad_glVertexAttrib4fvARB = (PFNGLVERTEXATTRIB4FVARBPROC) load(userptr, "glVertexAttrib4fvARB");
    glad_glVertexAttrib4ivARB = (PFNGLVERTEXATTRIB4IVARBPROC) load(userptr, "glVertexAttrib4ivARB");
    glad_glVertexAttrib4sARB = (PFNGLVERTEXATTRIB4SARBPROC) load(userptr, "glVertexAttrib4sARB");
    glad_glVertexAttrib4svARB = (PFNGLVERTEXATTRIB4SVARBPROC) load(userptr, "glVertexAttrib4svARB");
    glad_glVertexAttrib4ubvARB = (PFNGLVERTEXATTRIB4UBVARBPROC) load(userptr, "glVertexAttrib4ubvARB");
    glad_glVertexAttrib4uivARB = (PFNGLVERTEXATTRIB4UIVARBPROC) load(userptr, "glVertexAttrib4uivARB");
    glad_glVertexAttrib4usvARB = (PFNGLVERTEXATTRIB4USVARBPROC) load(userptr, "glVertexAttrib4usvARB");
    glad_glVertexAttribPointerARB = (PFNGLVERTEXATTRIBPOINTERARBPROC) load(userptr, "glVertexAttribPointerARB");
}
static void glad_gl_load_GL_ARB_vertex_type_2_10_10_10_rev( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_vertex_type_2_10_10_10_rev) return;
    glad_glVertexAttribP1ui = (PFNGLVERTEXATTRIBP1UIPROC) load(userptr, "glVertexAttribP1ui");
    glad_glVertexAttribP1uiv = (PFNGLVERTEXATTRIBP1UIVPROC) load(userptr, "glVertexAttribP1uiv");
    glad_glVertexAttribP2ui = (PFNGLVERTEXATTRIBP2UIPROC) load(userptr, "glVertexAttribP2ui");
    glad_glVertexAttribP2uiv = (PFNGLVERTEXATTRIBP2UIVPROC) load(userptr, "glVertexAttribP2uiv");
    glad_glVertexAttribP3ui = (PFNGLVERTEXATTRIBP3UIPROC) load(userptr, "glVertexAttribP3ui");
    glad_glVertexAttribP3uiv = (PFNGLVERTEXATTRIBP3UIVPROC) load(userptr, "glVertexAttribP3uiv");
    glad_glVertexAttribP4ui = (PFNGLVERTEXATTRIBP4UIPROC) load(userptr, "glVertexAttribP4ui");
    glad_glVertexAttribP4uiv = (PFNGLVERTEXATTRIBP4UIVPROC) load(userptr, "glVertexAttribP4uiv");
}
static void glad_gl_load_GL_ATI_draw_buffers( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ATI_draw_buffers) return;
    glad_glDrawBuffersATI = (PFNGLDRAWBUFFERSATIPROC) load(userptr, "glDrawBuffersATI");
}
static void glad_gl_load_GL_ATI_separate_stencil( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ATI_separate_stencil) return;
    glad_glStencilFuncSeparateATI = (PFNGLSTENCILFUNCSEPARATEATIPROC) load(userptr, "glStencilFuncSeparateATI");
    glad_glStencilOpSeparateATI = (PFNGLSTENCILOPSEPARATEATIPROC) load(userptr, "glStencilOpSeparateATI");
}
static void glad_gl_load_GL_EXT_blend_color( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_blend_color) return;
    glad_glBlendColorEXT = (PFNGLBLENDCOLOREXTPROC) load(userptr, "glBlendColorEXT");
}
static void glad_gl_load_GL_EXT_blend_equation_separate( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_blend_equation_separate) return;
    glad_glBlendEquationSeparateEXT = (PFNGLBLENDEQUATIONSEPARATEEXTPROC) load(userptr, "glBlendEquationSeparateEXT");
}
static void glad_gl_load_GL_EXT_blend_func_separate( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_blend_func_separate) return;
    glad_glBlendFuncSeparateEXT = (PFNGLBLENDFUNCSEPARATEEXTPROC) load(userptr, "glBlendFuncSeparateEXT");
}
static void glad_gl_load_GL_EXT_blend_minmax( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_blend_minmax) return;
    glad_glBlendEquationEXT = (PFNGLBLENDEQUATIONEXTPROC) load(userptr, "glBlendEquationEXT");
}
static void glad_gl_load_GL_EXT_copy_texture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_copy_texture) return;
    glad_glCopyTexImage1DEXT = (PFNGLCOPYTEXIMAGE1DEXTPROC) load(userptr, "glCopyTexImage1DEXT");
    glad_glCopyTexImage2DEXT = (PFNGLCOPYTEXIMAGE2DEXTPROC) load(userptr, "glCopyTexImage2DEXT");
    glad_glCopyTexSubImage1DEXT = (PFNGLCOPYTEXSUBIMAGE1DEXTPROC) load(userptr, "glCopyTexSubImage1DEXT");
    glad_glCopyTexSubImage2DEXT = (PFNGLCOPYTEXSUBIMAGE2DEXTPROC) load(userptr, "glCopyTexSubImage2DEXT");
    glad_glCopyTexSubImage3DEXT = (PFNGLCOPYTEXSUBIMAGE3DEXTPROC) load(userptr, "glCopyTexSubImage3DEXT");
}
static void glad_gl_load_GL_EXT_direct_state_access( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_direct_state_access) return;
    glad_glBindMultiTextureEXT = (PFNGLBINDMULTITEXTUREEXTPROC) load(userptr, "glBindMultiTextureEXT");
    glad_glCheckNamedFramebufferStatusEXT = (PFNGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC) load(userptr, "glCheckNamedFramebufferStatusEXT");
    glad_glClearNamedBufferDataEXT = (PFNGLCLEARNAMEDBUFFERDATAEXTPROC) load(userptr, "glClearNamedBufferDataEXT");
    glad_glClearNamedBufferSubDataEXT = (PFNGLCLEARNAMEDBUFFERSUBDATAEXTPROC) load(userptr, "glClearNamedBufferSubDataEXT");
    glad_glClientAttribDefaultEXT = (PFNGLCLIENTATTRIBDEFAULTEXTPROC) load(userptr, "glClientAttribDefaultEXT");
    glad_glCompressedMultiTexImage1DEXT = (PFNGLCOMPRESSEDMULTITEXIMAGE1DEXTPROC) load(userptr, "glCompressedMultiTexImage1DEXT");
    glad_glCompressedMultiTexImage2DEXT = (PFNGLCOMPRESSEDMULTITEXIMAGE2DEXTPROC) load(userptr, "glCompressedMultiTexImage2DEXT");
    glad_glCompressedMultiTexImage3DEXT = (PFNGLCOMPRESSEDMULTITEXIMAGE3DEXTPROC) load(userptr, "glCompressedMultiTexImage3DEXT");
    glad_glCompressedMultiTexSubImage1DEXT = (PFNGLCOMPRESSEDMULTITEXSUBIMAGE1DEXTPROC) load(userptr, "glCompressedMultiTexSubImage1DEXT");
    glad_glCompressedMultiTexSubImage2DEXT = (PFNGLCOMPRESSEDMULTITEXSUBIMAGE2DEXTPROC) load(userptr, "glCompressedMultiTexSubImage2DEXT");
    glad_glCompressedMultiTexSubImage3DEXT = (PFNGLCOMPRESSEDMULTITEXSUBIMAGE3DEXTPROC) load(userptr, "glCompressedMultiTexSubImage3DEXT");
    glad_glCompressedTextureImage1DEXT = (PFNGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC) load(userptr, "glCompressedTextureImage1DEXT");
    glad_glCompressedTextureImage2DEXT = (PFNGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC) load(userptr, "glCompressedTextureImage2DEXT");
    glad_glCompressedTextureImage3DEXT = (PFNGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC) load(userptr, "glCompressedTextureImage3DEXT");
    glad_glCompressedTextureSubImage1DEXT = (PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC) load(userptr, "glCompressedTextureSubImage1DEXT");
    glad_glCompressedTextureSubImage2DEXT = (PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC) load(userptr, "glCompressedTextureSubImage2DEXT");
    glad_glCompressedTextureSubImage3DEXT = (PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC) load(userptr, "glCompressedTextureSubImage3DEXT");
    glad_glCopyMultiTexImage1DEXT = (PFNGLCOPYMULTITEXIMAGE1DEXTPROC) load(userptr, "glCopyMultiTexImage1DEXT");
    glad_glCopyMultiTexImage2DEXT = (PFNGLCOPYMULTITEXIMAGE2DEXTPROC) load(userptr, "glCopyMultiTexImage2DEXT");
    glad_glCopyMultiTexSubImage1DEXT = (PFNGLCOPYMULTITEXSUBIMAGE1DEXTPROC) load(userptr, "glCopyMultiTexSubImage1DEXT");
    glad_glCopyMultiTexSubImage2DEXT = (PFNGLCOPYMULTITEXSUBIMAGE2DEXTPROC) load(userptr, "glCopyMultiTexSubImage2DEXT");
    glad_glCopyMultiTexSubImage3DEXT = (PFNGLCOPYMULTITEXSUBIMAGE3DEXTPROC) load(userptr, "glCopyMultiTexSubImage3DEXT");
    glad_glCopyTextureImage1DEXT = (PFNGLCOPYTEXTUREIMAGE1DEXTPROC) load(userptr, "glCopyTextureImage1DEXT");
    glad_glCopyTextureImage2DEXT = (PFNGLCOPYTEXTUREIMAGE2DEXTPROC) load(userptr, "glCopyTextureImage2DEXT");
    glad_glCopyTextureSubImage1DEXT = (PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC) load(userptr, "glCopyTextureSubImage1DEXT");
    glad_glCopyTextureSubImage2DEXT = (PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC) load(userptr, "glCopyTextureSubImage2DEXT");
    glad_glCopyTextureSubImage3DEXT = (PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC) load(userptr, "glCopyTextureSubImage3DEXT");
    glad_glDisableClientStateIndexedEXT = (PFNGLDISABLECLIENTSTATEINDEXEDEXTPROC) load(userptr, "glDisableClientStateIndexedEXT");
    glad_glDisableClientStateiEXT = (PFNGLDISABLECLIENTSTATEIEXTPROC) load(userptr, "glDisableClientStateiEXT");
    glad_glDisableIndexedEXT = (PFNGLDISABLEINDEXEDEXTPROC) load(userptr, "glDisableIndexedEXT");
    glad_glDisableVertexArrayAttribEXT = (PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC) load(userptr, "glDisableVertexArrayAttribEXT");
    glad_glDisableVertexArrayEXT = (PFNGLDISABLEVERTEXARRAYEXTPROC) load(userptr, "glDisableVertexArrayEXT");
    glad_glEnableClientStateIndexedEXT = (PFNGLENABLECLIENTSTATEINDEXEDEXTPROC) load(userptr, "glEnableClientStateIndexedEXT");
    glad_glEnableClientStateiEXT = (PFNGLENABLECLIENTSTATEIEXTPROC) load(userptr, "glEnableClientStateiEXT");
    glad_glEnableIndexedEXT = (PFNGLENABLEINDEXEDEXTPROC) load(userptr, "glEnableIndexedEXT");
    glad_glEnableVertexArrayAttribEXT = (PFNGLENABLEVERTEXARRAYATTRIBEXTPROC) load(userptr, "glEnableVertexArrayAttribEXT");
    glad_glEnableVertexArrayEXT = (PFNGLENABLEVERTEXARRAYEXTPROC) load(userptr, "glEnableVertexArrayEXT");
    glad_glFlushMappedNamedBufferRangeEXT = (PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEEXTPROC) load(userptr, "glFlushMappedNamedBufferRangeEXT");
    glad_glFramebufferDrawBufferEXT = (PFNGLFRAMEBUFFERDRAWBUFFEREXTPROC) load(userptr, "glFramebufferDrawBufferEXT");
    glad_glFramebufferDrawBuffersEXT = (PFNGLFRAMEBUFFERDRAWBUFFERSEXTPROC) load(userptr, "glFramebufferDrawBuffersEXT");
    glad_glFramebufferReadBufferEXT = (PFNGLFRAMEBUFFERREADBUFFEREXTPROC) load(userptr, "glFramebufferReadBufferEXT");
    glad_glGenerateMultiTexMipmapEXT = (PFNGLGENERATEMULTITEXMIPMAPEXTPROC) load(userptr, "glGenerateMultiTexMipmapEXT");
    glad_glGenerateTextureMipmapEXT = (PFNGLGENERATETEXTUREMIPMAPEXTPROC) load(userptr, "glGenerateTextureMipmapEXT");
    glad_glGetBooleanIndexedvEXT = (PFNGLGETBOOLEANINDEXEDVEXTPROC) load(userptr, "glGetBooleanIndexedvEXT");
    glad_glGetCompressedMultiTexImageEXT = (PFNGLGETCOMPRESSEDMULTITEXIMAGEEXTPROC) load(userptr, "glGetCompressedMultiTexImageEXT");
    glad_glGetCompressedTextureImageEXT = (PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC) load(userptr, "glGetCompressedTextureImageEXT");
    glad_glGetDoubleIndexedvEXT = (PFNGLGETDOUBLEINDEXEDVEXTPROC) load(userptr, "glGetDoubleIndexedvEXT");
    glad_glGetDoublei_vEXT = (PFNGLGETDOUBLEI_VEXTPROC) load(userptr, "glGetDoublei_vEXT");
    glad_glGetFloatIndexedvEXT = (PFNGLGETFLOATINDEXEDVEXTPROC) load(userptr, "glGetFloatIndexedvEXT");
    glad_glGetFloati_vEXT = (PFNGLGETFLOATI_VEXTPROC) load(userptr, "glGetFloati_vEXT");
    glad_glGetFramebufferParameterivEXT = (PFNGLGETFRAMEBUFFERPARAMETERIVEXTPROC) load(userptr, "glGetFramebufferParameterivEXT");
    glad_glGetIntegerIndexedvEXT = (PFNGLGETINTEGERINDEXEDVEXTPROC) load(userptr, "glGetIntegerIndexedvEXT");
    glad_glGetMultiTexEnvfvEXT = (PFNGLGETMULTITEXENVFVEXTPROC) load(userptr, "glGetMultiTexEnvfvEXT");
    glad_glGetMultiTexEnvivEXT = (PFNGLGETMULTITEXENVIVEXTPROC) load(userptr, "glGetMultiTexEnvivEXT");
    glad_glGetMultiTexGendvEXT = (PFNGLGETMULTITEXGENDVEXTPROC) load(userptr, "glGetMultiTexGendvEXT");
    glad_glGetMultiTexGenfvEXT = (PFNGLGETMULTITEXGENFVEXTPROC) load(userptr, "glGetMultiTexGenfvEXT");
    glad_glGetMultiTexGenivEXT = (PFNGLGETMULTITEXGENIVEXTPROC) load(userptr, "glGetMultiTexGenivEXT");
    glad_glGetMultiTexImageEXT = (PFNGLGETMULTITEXIMAGEEXTPROC) load(userptr, "glGetMultiTexImageEXT");
    glad_glGetMultiTexLevelParameterfvEXT = (PFNGLGETMULTITEXLEVELPARAMETERFVEXTPROC) load(userptr, "glGetMultiTexLevelParameterfvEXT");
    glad_glGetMultiTexLevelParameterivEXT = (PFNGLGETMULTITEXLEVELPARAMETERIVEXTPROC) load(userptr, "glGetMultiTexLevelParameterivEXT");
    glad_glGetMultiTexParameterIivEXT = (PFNGLGETMULTITEXPARAMETERIIVEXTPROC) load(userptr, "glGetMultiTexParameterIivEXT");
    glad_glGetMultiTexParameterIuivEXT = (PFNGLGETMULTITEXPARAMETERIUIVEXTPROC) load(userptr, "glGetMultiTexParameterIuivEXT");
    glad_glGetMultiTexParameterfvEXT = (PFNGLGETMULTITEXPARAMETERFVEXTPROC) load(userptr, "glGetMultiTexParameterfvEXT");
    glad_glGetMultiTexParameterivEXT = (PFNGLGETMULTITEXPARAMETERIVEXTPROC) load(userptr, "glGetMultiTexParameterivEXT");
    glad_glGetNamedBufferParameterivEXT = (PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC) load(userptr, "glGetNamedBufferParameterivEXT");
    glad_glGetNamedBufferPointervEXT = (PFNGLGETNAMEDBUFFERPOINTERVEXTPROC) load(userptr, "glGetNamedBufferPointervEXT");
    glad_glGetNamedBufferSubDataEXT = (PFNGLGETNAMEDBUFFERSUBDATAEXTPROC) load(userptr, "glGetNamedBufferSubDataEXT");
    glad_glGetNamedFramebufferAttachmentParameterivEXT = (PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC) load(userptr, "glGetNamedFramebufferAttachmentParameterivEXT");
    glad_glGetNamedFramebufferParameterivEXT = (PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVEXTPROC) load(userptr, "glGetNamedFramebufferParameterivEXT");
    glad_glGetNamedProgramLocalParameterIivEXT = (PFNGLGETNAMEDPROGRAMLOCALPARAMETERIIVEXTPROC) load(userptr, "glGetNamedProgramLocalParameterIivEXT");
    glad_glGetNamedProgramLocalParameterIuivEXT = (PFNGLGETNAMEDPROGRAMLOCALPARAMETERIUIVEXTPROC) load(userptr, "glGetNamedProgramLocalParameterIuivEXT");
    glad_glGetNamedProgramLocalParameterdvEXT = (PFNGLGETNAMEDPROGRAMLOCALPARAMETERDVEXTPROC) load(userptr, "glGetNamedProgramLocalParameterdvEXT");
    glad_glGetNamedProgramLocalParameterfvEXT = (PFNGLGETNAMEDPROGRAMLOCALPARAMETERFVEXTPROC) load(userptr, "glGetNamedProgramLocalParameterfvEXT");
    glad_glGetNamedProgramStringEXT = (PFNGLGETNAMEDPROGRAMSTRINGEXTPROC) load(userptr, "glGetNamedProgramStringEXT");
    glad_glGetNamedProgramivEXT = (PFNGLGETNAMEDPROGRAMIVEXTPROC) load(userptr, "glGetNamedProgramivEXT");
    glad_glGetNamedRenderbufferParameterivEXT = (PFNGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC) load(userptr, "glGetNamedRenderbufferParameterivEXT");
    glad_glGetPointerIndexedvEXT = (PFNGLGETPOINTERINDEXEDVEXTPROC) load(userptr, "glGetPointerIndexedvEXT");
    glad_glGetPointeri_vEXT = (PFNGLGETPOINTERI_VEXTPROC) load(userptr, "glGetPointeri_vEXT");
    glad_glGetTextureImageEXT = (PFNGLGETTEXTUREIMAGEEXTPROC) load(userptr, "glGetTextureImageEXT");
    glad_glGetTextureLevelParameterfvEXT = (PFNGLGETTEXTURELEVELPARAMETERFVEXTPROC) load(userptr, "glGetTextureLevelParameterfvEXT");
    glad_glGetTextureLevelParameterivEXT = (PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC) load(userptr, "glGetTextureLevelParameterivEXT");
    glad_glGetTextureParameterIivEXT = (PFNGLGETTEXTUREPARAMETERIIVEXTPROC) load(userptr, "glGetTextureParameterIivEXT");
    glad_glGetTextureParameterIuivEXT = (PFNGLGETTEXTUREPARAMETERIUIVEXTPROC) load(userptr, "glGetTextureParameterIuivEXT");
    glad_glGetTextureParameterfvEXT = (PFNGLGETTEXTUREPARAMETERFVEXTPROC) load(userptr, "glGetTextureParameterfvEXT");
    glad_glGetTextureParameterivEXT = (PFNGLGETTEXTUREPARAMETERIVEXTPROC) load(userptr, "glGetTextureParameterivEXT");
    glad_glGetVertexArrayIntegeri_vEXT = (PFNGLGETVERTEXARRAYINTEGERI_VEXTPROC) load(userptr, "glGetVertexArrayIntegeri_vEXT");
    glad_glGetVertexArrayIntegervEXT = (PFNGLGETVERTEXARRAYINTEGERVEXTPROC) load(userptr, "glGetVertexArrayIntegervEXT");
    glad_glGetVertexArrayPointeri_vEXT = (PFNGLGETVERTEXARRAYPOINTERI_VEXTPROC) load(userptr, "glGetVertexArrayPointeri_vEXT");
    glad_glGetVertexArrayPointervEXT = (PFNGLGETVERTEXARRAYPOINTERVEXTPROC) load(userptr, "glGetVertexArrayPointervEXT");
    glad_glIsEnabledIndexedEXT = (PFNGLISENABLEDINDEXEDEXTPROC) load(userptr, "glIsEnabledIndexedEXT");
    glad_glMapNamedBufferEXT = (PFNGLMAPNAMEDBUFFEREXTPROC) load(userptr, "glMapNamedBufferEXT");
    glad_glMapNamedBufferRangeEXT = (PFNGLMAPNAMEDBUFFERRANGEEXTPROC) load(userptr, "glMapNamedBufferRangeEXT");
    glad_glMatrixFrustumEXT = (PFNGLMATRIXFRUSTUMEXTPROC) load(userptr, "glMatrixFrustumEXT");
    glad_glMatrixLoadIdentityEXT = (PFNGLMATRIXLOADIDENTITYEXTPROC) load(userptr, "glMatrixLoadIdentityEXT");
    glad_glMatrixLoadTransposedEXT = (PFNGLMATRIXLOADTRANSPOSEDEXTPROC) load(userptr, "glMatrixLoadTransposedEXT");
    glad_glMatrixLoadTransposefEXT = (PFNGLMATRIXLOADTRANSPOSEFEXTPROC) load(userptr, "glMatrixLoadTransposefEXT");
    glad_glMatrixLoaddEXT = (PFNGLMATRIXLOADDEXTPROC) load(userptr, "glMatrixLoaddEXT");
    glad_glMatrixLoadfEXT = (PFNGLMATRIXLOADFEXTPROC) load(userptr, "glMatrixLoadfEXT");
    glad_glMatrixMultTransposedEXT = (PFNGLMATRIXMULTTRANSPOSEDEXTPROC) load(userptr, "glMatrixMultTransposedEXT");
    glad_glMatrixMultTransposefEXT = (PFNGLMATRIXMULTTRANSPOSEFEXTPROC) load(userptr, "glMatrixMultTransposefEXT");
    glad_glMatrixMultdEXT = (PFNGLMATRIXMULTDEXTPROC) load(userptr, "glMatrixMultdEXT");
    glad_glMatrixMultfEXT = (PFNGLMATRIXMULTFEXTPROC) load(userptr, "glMatrixMultfEXT");
    glad_glMatrixOrthoEXT = (PFNGLMATRIXORTHOEXTPROC) load(userptr, "glMatrixOrthoEXT");
    glad_glMatrixPopEXT = (PFNGLMATRIXPOPEXTPROC) load(userptr, "glMatrixPopEXT");
    glad_glMatrixPushEXT = (PFNGLMATRIXPUSHEXTPROC) load(userptr, "glMatrixPushEXT");
    glad_glMatrixRotatedEXT = (PFNGLMATRIXROTATEDEXTPROC) load(userptr, "glMatrixRotatedEXT");
    glad_glMatrixRotatefEXT = (PFNGLMATRIXROTATEFEXTPROC) load(userptr, "glMatrixRotatefEXT");
    glad_glMatrixScaledEXT = (PFNGLMATRIXSCALEDEXTPROC) load(userptr, "glMatrixScaledEXT");
    glad_glMatrixScalefEXT = (PFNGLMATRIXSCALEFEXTPROC) load(userptr, "glMatrixScalefEXT");
    glad_glMatrixTranslatedEXT = (PFNGLMATRIXTRANSLATEDEXTPROC) load(userptr, "glMatrixTranslatedEXT");
    glad_glMatrixTranslatefEXT = (PFNGLMATRIXTRANSLATEFEXTPROC) load(userptr, "glMatrixTranslatefEXT");
    glad_glMultiTexBufferEXT = (PFNGLMULTITEXBUFFEREXTPROC) load(userptr, "glMultiTexBufferEXT");
    glad_glMultiTexCoordPointerEXT = (PFNGLMULTITEXCOORDPOINTEREXTPROC) load(userptr, "glMultiTexCoordPointerEXT");
    glad_glMultiTexEnvfEXT = (PFNGLMULTITEXENVFEXTPROC) load(userptr, "glMultiTexEnvfEXT");
    glad_glMultiTexEnvfvEXT = (PFNGLMULTITEXENVFVEXTPROC) load(userptr, "glMultiTexEnvfvEXT");
    glad_glMultiTexEnviEXT = (PFNGLMULTITEXENVIEXTPROC) load(userptr, "glMultiTexEnviEXT");
    glad_glMultiTexEnvivEXT = (PFNGLMULTITEXENVIVEXTPROC) load(userptr, "glMultiTexEnvivEXT");
    glad_glMultiTexGendEXT = (PFNGLMULTITEXGENDEXTPROC) load(userptr, "glMultiTexGendEXT");
    glad_glMultiTexGendvEXT = (PFNGLMULTITEXGENDVEXTPROC) load(userptr, "glMultiTexGendvEXT");
    glad_glMultiTexGenfEXT = (PFNGLMULTITEXGENFEXTPROC) load(userptr, "glMultiTexGenfEXT");
    glad_glMultiTexGenfvEXT = (PFNGLMULTITEXGENFVEXTPROC) load(userptr, "glMultiTexGenfvEXT");
    glad_glMultiTexGeniEXT = (PFNGLMULTITEXGENIEXTPROC) load(userptr, "glMultiTexGeniEXT");
    glad_glMultiTexGenivEXT = (PFNGLMULTITEXGENIVEXTPROC) load(userptr, "glMultiTexGenivEXT");
    glad_glMultiTexImage1DEXT = (PFNGLMULTITEXIMAGE1DEXTPROC) load(userptr, "glMultiTexImage1DEXT");
    glad_glMultiTexImage2DEXT = (PFNGLMULTITEXIMAGE2DEXTPROC) load(userptr, "glMultiTexImage2DEXT");
    glad_glMultiTexImage3DEXT = (PFNGLMULTITEXIMAGE3DEXTPROC) load(userptr, "glMultiTexImage3DEXT");
    glad_glMultiTexParameterIivEXT = (PFNGLMULTITEXPARAMETERIIVEXTPROC) load(userptr, "glMultiTexParameterIivEXT");
    glad_glMultiTexParameterIuivEXT = (PFNGLMULTITEXPARAMETERIUIVEXTPROC) load(userptr, "glMultiTexParameterIuivEXT");
    glad_glMultiTexParameterfEXT = (PFNGLMULTITEXPARAMETERFEXTPROC) load(userptr, "glMultiTexParameterfEXT");
    glad_glMultiTexParameterfvEXT = (PFNGLMULTITEXPARAMETERFVEXTPROC) load(userptr, "glMultiTexParameterfvEXT");
    glad_glMultiTexParameteriEXT = (PFNGLMULTITEXPARAMETERIEXTPROC) load(userptr, "glMultiTexParameteriEXT");
    glad_glMultiTexParameterivEXT = (PFNGLMULTITEXPARAMETERIVEXTPROC) load(userptr, "glMultiTexParameterivEXT");
    glad_glMultiTexRenderbufferEXT = (PFNGLMULTITEXRENDERBUFFEREXTPROC) load(userptr, "glMultiTexRenderbufferEXT");
    glad_glMultiTexSubImage1DEXT = (PFNGLMULTITEXSUBIMAGE1DEXTPROC) load(userptr, "glMultiTexSubImage1DEXT");
    glad_glMultiTexSubImage2DEXT = (PFNGLMULTITEXSUBIMAGE2DEXTPROC) load(userptr, "glMultiTexSubImage2DEXT");
    glad_glMultiTexSubImage3DEXT = (PFNGLMULTITEXSUBIMAGE3DEXTPROC) load(userptr, "glMultiTexSubImage3DEXT");
    glad_glNamedBufferDataEXT = (PFNGLNAMEDBUFFERDATAEXTPROC) load(userptr, "glNamedBufferDataEXT");
    glad_glNamedBufferStorageEXT = (PFNGLNAMEDBUFFERSTORAGEEXTPROC) load(userptr, "glNamedBufferStorageEXT");
    glad_glNamedBufferSubDataEXT = (PFNGLNAMEDBUFFERSUBDATAEXTPROC) load(userptr, "glNamedBufferSubDataEXT");
    glad_glNamedCopyBufferSubDataEXT = (PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC) load(userptr, "glNamedCopyBufferSubDataEXT");
    glad_glNamedFramebufferParameteriEXT = (PFNGLNAMEDFRAMEBUFFERPARAMETERIEXTPROC) load(userptr, "glNamedFramebufferParameteriEXT");
    glad_glNamedFramebufferRenderbufferEXT = (PFNGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC) load(userptr, "glNamedFramebufferRenderbufferEXT");
    glad_glNamedFramebufferTexture1DEXT = (PFNGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC) load(userptr, "glNamedFramebufferTexture1DEXT");
    glad_glNamedFramebufferTexture2DEXT = (PFNGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC) load(userptr, "glNamedFramebufferTexture2DEXT");
    glad_glNamedFramebufferTexture3DEXT = (PFNGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC) load(userptr, "glNamedFramebufferTexture3DEXT");
    glad_glNamedFramebufferTextureEXT = (PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC) load(userptr, "glNamedFramebufferTextureEXT");
    glad_glNamedFramebufferTextureFaceEXT = (PFNGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC) load(userptr, "glNamedFramebufferTextureFaceEXT");
    glad_glNamedFramebufferTextureLayerEXT = (PFNGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC) load(userptr, "glNamedFramebufferTextureLayerEXT");
    glad_glNamedProgramLocalParameter4dEXT = (PFNGLNAMEDPROGRAMLOCALPARAMETER4DEXTPROC) load(userptr, "glNamedProgramLocalParameter4dEXT");
    glad_glNamedProgramLocalParameter4dvEXT = (PFNGLNAMEDPROGRAMLOCALPARAMETER4DVEXTPROC) load(userptr, "glNamedProgramLocalParameter4dvEXT");
    glad_glNamedProgramLocalParameter4fEXT = (PFNGLNAMEDPROGRAMLOCALPARAMETER4FEXTPROC) load(userptr, "glNamedProgramLocalParameter4fEXT");
    glad_glNamedProgramLocalParameter4fvEXT = (PFNGLNAMEDPROGRAMLOCALPARAMETER4FVEXTPROC) load(userptr, "glNamedProgramLocalParameter4fvEXT");
    glad_glNamedProgramLocalParameterI4iEXT = (PFNGLNAMEDPROGRAMLOCALPARAMETERI4IEXTPROC) load(userptr, "glNamedProgramLocalParameterI4iEXT");
    glad_glNamedProgramLocalParameterI4ivEXT = (PFNGLNAMEDPROGRAMLOCALPARAMETERI4IVEXTPROC) load(userptr, "glNamedProgramLocalParameterI4ivEXT");
    glad_glNamedProgramLocalParameterI4uiEXT = (PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIEXTPROC) load(userptr, "glNamedProgramLocalParameterI4uiEXT");
    glad_glNamedProgramLocalParameterI4uivEXT = (PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIVEXTPROC) load(userptr, "glNamedProgramLocalParameterI4uivEXT");
    glad_glNamedProgramLocalParameters4fvEXT = (PFNGLNAMEDPROGRAMLOCALPARAMETERS4FVEXTPROC) load(userptr, "glNamedProgramLocalParameters4fvEXT");
    glad_glNamedProgramLocalParametersI4ivEXT = (PFNGLNAMEDPROGRAMLOCALPARAMETERSI4IVEXTPROC) load(userptr, "glNamedProgramLocalParametersI4ivEXT");
    glad_glNamedProgramLocalParametersI4uivEXT = (PFNGLNAMEDPROGRAMLOCALPARAMETERSI4UIVEXTPROC) load(userptr, "glNamedProgramLocalParametersI4uivEXT");
    glad_glNamedProgramStringEXT = (PFNGLNAMEDPROGRAMSTRINGEXTPROC) load(userptr, "glNamedProgramStringEXT");
    glad_glNamedRenderbufferStorageEXT = (PFNGLNAMEDRENDERBUFFERSTORAGEEXTPROC) load(userptr, "glNamedRenderbufferStorageEXT");
    glad_glNamedRenderbufferStorageMultisampleCoverageEXT = (PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXTPROC) load(userptr, "glNamedRenderbufferStorageMultisampleCoverageEXT");
    glad_glNamedRenderbufferStorageMultisampleEXT = (PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) load(userptr, "glNamedRenderbufferStorageMultisampleEXT");
    glad_glProgramUniform1dEXT = (PFNGLPROGRAMUNIFORM1DEXTPROC) load(userptr, "glProgramUniform1dEXT");
    glad_glProgramUniform1dvEXT = (PFNGLPROGRAMUNIFORM1DVEXTPROC) load(userptr, "glProgramUniform1dvEXT");
    glad_glProgramUniform1fEXT = (PFNGLPROGRAMUNIFORM1FEXTPROC) load(userptr, "glProgramUniform1fEXT");
    glad_glProgramUniform1fvEXT = (PFNGLPROGRAMUNIFORM1FVEXTPROC) load(userptr, "glProgramUniform1fvEXT");
    glad_glProgramUniform1iEXT = (PFNGLPROGRAMUNIFORM1IEXTPROC) load(userptr, "glProgramUniform1iEXT");
    glad_glProgramUniform1ivEXT = (PFNGLPROGRAMUNIFORM1IVEXTPROC) load(userptr, "glProgramUniform1ivEXT");
    glad_glProgramUniform1uiEXT = (PFNGLPROGRAMUNIFORM1UIEXTPROC) load(userptr, "glProgramUniform1uiEXT");
    glad_glProgramUniform1uivEXT = (PFNGLPROGRAMUNIFORM1UIVEXTPROC) load(userptr, "glProgramUniform1uivEXT");
    glad_glProgramUniform2dEXT = (PFNGLPROGRAMUNIFORM2DEXTPROC) load(userptr, "glProgramUniform2dEXT");
    glad_glProgramUniform2dvEXT = (PFNGLPROGRAMUNIFORM2DVEXTPROC) load(userptr, "glProgramUniform2dvEXT");
    glad_glProgramUniform2fEXT = (PFNGLPROGRAMUNIFORM2FEXTPROC) load(userptr, "glProgramUniform2fEXT");
    glad_glProgramUniform2fvEXT = (PFNGLPROGRAMUNIFORM2FVEXTPROC) load(userptr, "glProgramUniform2fvEXT");
    glad_glProgramUniform2iEXT = (PFNGLPROGRAMUNIFORM2IEXTPROC) load(userptr, "glProgramUniform2iEXT");
    glad_glProgramUniform2ivEXT = (PFNGLPROGRAMUNIFORM2IVEXTPROC) load(userptr, "glProgramUniform2ivEXT");
    glad_glProgramUniform2uiEXT = (PFNGLPROGRAMUNIFORM2UIEXTPROC) load(userptr, "glProgramUniform2uiEXT");
    glad_glProgramUniform2uivEXT = (PFNGLPROGRAMUNIFORM2UIVEXTPROC) load(userptr, "glProgramUniform2uivEXT");
    glad_glProgramUniform3dEXT = (PFNGLPROGRAMUNIFORM3DEXTPROC) load(userptr, "glProgramUniform3dEXT");
    glad_glProgramUniform3dvEXT = (PFNGLPROGRAMUNIFORM3DVEXTPROC) load(userptr, "glProgramUniform3dvEXT");
    glad_glProgramUniform3fEXT = (PFNGLPROGRAMUNIFORM3FEXTPROC) load(userptr, "glProgramUniform3fEXT");
    glad_glProgramUniform3fvEXT = (PFNGLPROGRAMUNIFORM3FVEXTPROC) load(userptr, "glProgramUniform3fvEXT");
    glad_glProgramUniform3iEXT = (PFNGLPROGRAMUNIFORM3IEXTPROC) load(userptr, "glProgramUniform3iEXT");
    glad_glProgramUniform3ivEXT = (PFNGLPROGRAMUNIFORM3IVEXTPROC) load(userptr, "glProgramUniform3ivEXT");
    glad_glProgramUniform3uiEXT = (PFNGLPROGRAMUNIFORM3UIEXTPROC) load(userptr, "glProgramUniform3uiEXT");
    glad_glProgramUniform3uivEXT = (PFNGLPROGRAMUNIFORM3UIVEXTPROC) load(userptr, "glProgramUniform3uivEXT");
    glad_glProgramUniform4dEXT = (PFNGLPROGRAMUNIFORM4DEXTPROC) load(userptr, "glProgramUniform4dEXT");
    glad_glProgramUniform4dvEXT = (PFNGLPROGRAMUNIFORM4DVEXTPROC) load(userptr, "glProgramUniform4dvEXT");
    glad_glProgramUniform4fEXT = (PFNGLPROGRAMUNIFORM4FEXTPROC) load(userptr, "glProgramUniform4fEXT");
    glad_glProgramUniform4fvEXT = (PFNGLPROGRAMUNIFORM4FVEXTPROC) load(userptr, "glProgramUniform4fvEXT");
    glad_glProgramUniform4iEXT = (PFNGLPROGRAMUNIFORM4IEXTPROC) load(userptr, "glProgramUniform4iEXT");
    glad_glProgramUniform4ivEXT = (PFNGLPROGRAMUNIFORM4IVEXTPROC) load(userptr, "glProgramUniform4ivEXT");
    glad_glProgramUniform4uiEXT = (PFNGLPROGRAMUNIFORM4UIEXTPROC) load(userptr, "glProgramUniform4uiEXT");
    glad_glProgramUniform4uivEXT = (PFNGLPROGRAMUNIFORM4UIVEXTPROC) load(userptr, "glProgramUniform4uivEXT");
    glad_glProgramUniformMatrix2dvEXT = (PFNGLPROGRAMUNIFORMMATRIX2DVEXTPROC) load(userptr, "glProgramUniformMatrix2dvEXT");
    glad_glProgramUniformMatrix2fvEXT = (PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC) load(userptr, "glProgramUniformMatrix2fvEXT");
    glad_glProgramUniformMatrix2x3dvEXT = (PFNGLPROGRAMUNIFORMMATRIX2X3DVEXTPROC) load(userptr, "glProgramUniformMatrix2x3dvEXT");
    glad_glProgramUniformMatrix2x3fvEXT = (PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC) load(userptr, "glProgramUniformMatrix2x3fvEXT");
    glad_glProgramUniformMatrix2x4dvEXT = (PFNGLPROGRAMUNIFORMMATRIX2X4DVEXTPROC) load(userptr, "glProgramUniformMatrix2x4dvEXT");
    glad_glProgramUniformMatrix2x4fvEXT = (PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC) load(userptr, "glProgramUniformMatrix2x4fvEXT");
    glad_glProgramUniformMatrix3dvEXT = (PFNGLPROGRAMUNIFORMMATRIX3DVEXTPROC) load(userptr, "glProgramUniformMatrix3dvEXT");
    glad_glProgramUniformMatrix3fvEXT = (PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC) load(userptr, "glProgramUniformMatrix3fvEXT");
    glad_glProgramUniformMatrix3x2dvEXT = (PFNGLPROGRAMUNIFORMMATRIX3X2DVEXTPROC) load(userptr, "glProgramUniformMatrix3x2dvEXT");
    glad_glProgramUniformMatrix3x2fvEXT = (PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC) load(userptr, "glProgramUniformMatrix3x2fvEXT");
    glad_glProgramUniformMatrix3x4dvEXT = (PFNGLPROGRAMUNIFORMMATRIX3X4DVEXTPROC) load(userptr, "glProgramUniformMatrix3x4dvEXT");
    glad_glProgramUniformMatrix3x4fvEXT = (PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC) load(userptr, "glProgramUniformMatrix3x4fvEXT");
    glad_glProgramUniformMatrix4dvEXT = (PFNGLPROGRAMUNIFORMMATRIX4DVEXTPROC) load(userptr, "glProgramUniformMatrix4dvEXT");
    glad_glProgramUniformMatrix4fvEXT = (PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC) load(userptr, "glProgramUniformMatrix4fvEXT");
    glad_glProgramUniformMatrix4x2dvEXT = (PFNGLPROGRAMUNIFORMMATRIX4X2DVEXTPROC) load(userptr, "glProgramUniformMatrix4x2dvEXT");
    glad_glProgramUniformMatrix4x2fvEXT = (PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC) load(userptr, "glProgramUniformMatrix4x2fvEXT");
    glad_glProgramUniformMatrix4x3dvEXT = (PFNGLPROGRAMUNIFORMMATRIX4X3DVEXTPROC) load(userptr, "glProgramUniformMatrix4x3dvEXT");
    glad_glProgramUniformMatrix4x3fvEXT = (PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC) load(userptr, "glProgramUniformMatrix4x3fvEXT");
    glad_glPushClientAttribDefaultEXT = (PFNGLPUSHCLIENTATTRIBDEFAULTEXTPROC) load(userptr, "glPushClientAttribDefaultEXT");
    glad_glTextureBufferEXT = (PFNGLTEXTUREBUFFEREXTPROC) load(userptr, "glTextureBufferEXT");
    glad_glTextureBufferRangeEXT = (PFNGLTEXTUREBUFFERRANGEEXTPROC) load(userptr, "glTextureBufferRangeEXT");
    glad_glTextureImage1DEXT = (PFNGLTEXTUREIMAGE1DEXTPROC) load(userptr, "glTextureImage1DEXT");
    glad_glTextureImage2DEXT = (PFNGLTEXTUREIMAGE2DEXTPROC) load(userptr, "glTextureImage2DEXT");
    glad_glTextureImage3DEXT = (PFNGLTEXTUREIMAGE3DEXTPROC) load(userptr, "glTextureImage3DEXT");
    glad_glTexturePageCommitmentEXT = (PFNGLTEXTUREPAGECOMMITMENTEXTPROC) load(userptr, "glTexturePageCommitmentEXT");
    glad_glTextureParameterIivEXT = (PFNGLTEXTUREPARAMETERIIVEXTPROC) load(userptr, "glTextureParameterIivEXT");
    glad_glTextureParameterIuivEXT = (PFNGLTEXTUREPARAMETERIUIVEXTPROC) load(userptr, "glTextureParameterIuivEXT");
    glad_glTextureParameterfEXT = (PFNGLTEXTUREPARAMETERFEXTPROC) load(userptr, "glTextureParameterfEXT");
    glad_glTextureParameterfvEXT = (PFNGLTEXTUREPARAMETERFVEXTPROC) load(userptr, "glTextureParameterfvEXT");
    glad_glTextureParameteriEXT = (PFNGLTEXTUREPARAMETERIEXTPROC) load(userptr, "glTextureParameteriEXT");
    glad_glTextureParameterivEXT = (PFNGLTEXTUREPARAMETERIVEXTPROC) load(userptr, "glTextureParameterivEXT");
    glad_glTextureRenderbufferEXT = (PFNGLTEXTURERENDERBUFFEREXTPROC) load(userptr, "glTextureRenderbufferEXT");
    glad_glTextureStorage1DEXT = (PFNGLTEXTURESTORAGE1DEXTPROC) load(userptr, "glTextureStorage1DEXT");
    glad_glTextureStorage2DEXT = (PFNGLTEXTURESTORAGE2DEXTPROC) load(userptr, "glTextureStorage2DEXT");
    glad_glTextureStorage2DMultisampleEXT = (PFNGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC) load(userptr, "glTextureStorage2DMultisampleEXT");
    glad_glTextureStorage3DEXT = (PFNGLTEXTURESTORAGE3DEXTPROC) load(userptr, "glTextureStorage3DEXT");
    glad_glTextureStorage3DMultisampleEXT = (PFNGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC) load(userptr, "glTextureStorage3DMultisampleEXT");
    glad_glTextureSubImage1DEXT = (PFNGLTEXTURESUBIMAGE1DEXTPROC) load(userptr, "glTextureSubImage1DEXT");
    glad_glTextureSubImage2DEXT = (PFNGLTEXTURESUBIMAGE2DEXTPROC) load(userptr, "glTextureSubImage2DEXT");
    glad_glTextureSubImage3DEXT = (PFNGLTEXTURESUBIMAGE3DEXTPROC) load(userptr, "glTextureSubImage3DEXT");
    glad_glUnmapNamedBufferEXT = (PFNGLUNMAPNAMEDBUFFEREXTPROC) load(userptr, "glUnmapNamedBufferEXT");
    glad_glVertexArrayBindVertexBufferEXT = (PFNGLVERTEXARRAYBINDVERTEXBUFFEREXTPROC) load(userptr, "glVertexArrayBindVertexBufferEXT");
    glad_glVertexArrayColorOffsetEXT = (PFNGLVERTEXARRAYCOLOROFFSETEXTPROC) load(userptr, "glVertexArrayColorOffsetEXT");
    glad_glVertexArrayEdgeFlagOffsetEXT = (PFNGLVERTEXARRAYEDGEFLAGOFFSETEXTPROC) load(userptr, "glVertexArrayEdgeFlagOffsetEXT");
    glad_glVertexArrayFogCoordOffsetEXT = (PFNGLVERTEXARRAYFOGCOORDOFFSETEXTPROC) load(userptr, "glVertexArrayFogCoordOffsetEXT");
    glad_glVertexArrayIndexOffsetEXT = (PFNGLVERTEXARRAYINDEXOFFSETEXTPROC) load(userptr, "glVertexArrayIndexOffsetEXT");
    glad_glVertexArrayMultiTexCoordOffsetEXT = (PFNGLVERTEXARRAYMULTITEXCOORDOFFSETEXTPROC) load(userptr, "glVertexArrayMultiTexCoordOffsetEXT");
    glad_glVertexArrayNormalOffsetEXT = (PFNGLVERTEXARRAYNORMALOFFSETEXTPROC) load(userptr, "glVertexArrayNormalOffsetEXT");
    glad_glVertexArraySecondaryColorOffsetEXT = (PFNGLVERTEXARRAYSECONDARYCOLOROFFSETEXTPROC) load(userptr, "glVertexArraySecondaryColorOffsetEXT");
    glad_glVertexArrayTexCoordOffsetEXT = (PFNGLVERTEXARRAYTEXCOORDOFFSETEXTPROC) load(userptr, "glVertexArrayTexCoordOffsetEXT");
    glad_glVertexArrayVertexAttribBindingEXT = (PFNGLVERTEXARRAYVERTEXATTRIBBINDINGEXTPROC) load(userptr, "glVertexArrayVertexAttribBindingEXT");
    glad_glVertexArrayVertexAttribDivisorEXT = (PFNGLVERTEXARRAYVERTEXATTRIBDIVISOREXTPROC) load(userptr, "glVertexArrayVertexAttribDivisorEXT");
    glad_glVertexArrayVertexAttribFormatEXT = (PFNGLVERTEXARRAYVERTEXATTRIBFORMATEXTPROC) load(userptr, "glVertexArrayVertexAttribFormatEXT");
    glad_glVertexArrayVertexAttribIFormatEXT = (PFNGLVERTEXARRAYVERTEXATTRIBIFORMATEXTPROC) load(userptr, "glVertexArrayVertexAttribIFormatEXT");
    glad_glVertexArrayVertexAttribIOffsetEXT = (PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC) load(userptr, "glVertexArrayVertexAttribIOffsetEXT");
    glad_glVertexArrayVertexAttribLFormatEXT = (PFNGLVERTEXARRAYVERTEXATTRIBLFORMATEXTPROC) load(userptr, "glVertexArrayVertexAttribLFormatEXT");
    glad_glVertexArrayVertexAttribLOffsetEXT = (PFNGLVERTEXARRAYVERTEXATTRIBLOFFSETEXTPROC) load(userptr, "glVertexArrayVertexAttribLOffsetEXT");
    glad_glVertexArrayVertexAttribOffsetEXT = (PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC) load(userptr, "glVertexArrayVertexAttribOffsetEXT");
    glad_glVertexArrayVertexBindingDivisorEXT = (PFNGLVERTEXARRAYVERTEXBINDINGDIVISOREXTPROC) load(userptr, "glVertexArrayVertexBindingDivisorEXT");
    glad_glVertexArrayVertexOffsetEXT = (PFNGLVERTEXARRAYVERTEXOFFSETEXTPROC) load(userptr, "glVertexArrayVertexOffsetEXT");
}
static void glad_gl_load_GL_EXT_draw_buffers2( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_draw_buffers2) return;
    glad_glColorMaskIndexedEXT = (PFNGLCOLORMASKINDEXEDEXTPROC) load(userptr, "glColorMaskIndexedEXT");
    glad_glDisableIndexedEXT = (PFNGLDISABLEINDEXEDEXTPROC) load(userptr, "glDisableIndexedEXT");
    glad_glEnableIndexedEXT = (PFNGLENABLEINDEXEDEXTPROC) load(userptr, "glEnableIndexedEXT");
    glad_glGetBooleanIndexedvEXT = (PFNGLGETBOOLEANINDEXEDVEXTPROC) load(userptr, "glGetBooleanIndexedvEXT");
    glad_glGetIntegerIndexedvEXT = (PFNGLGETINTEGERINDEXEDVEXTPROC) load(userptr, "glGetIntegerIndexedvEXT");
    glad_glIsEnabledIndexedEXT = (PFNGLISENABLEDINDEXEDEXTPROC) load(userptr, "glIsEnabledIndexedEXT");
}
static void glad_gl_load_GL_EXT_draw_instanced( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_draw_instanced) return;
    glad_glDrawArraysInstancedEXT = (PFNGLDRAWARRAYSINSTANCEDEXTPROC) load(userptr, "glDrawArraysInstancedEXT");
    glad_glDrawElementsInstancedEXT = (PFNGLDRAWELEMENTSINSTANCEDEXTPROC) load(userptr, "glDrawElementsInstancedEXT");
}
static void glad_gl_load_GL_EXT_draw_range_elements( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_draw_range_elements) return;
    glad_glDrawRangeElementsEXT = (PFNGLDRAWRANGEELEMENTSEXTPROC) load(userptr, "glDrawRangeElementsEXT");
}
static void glad_gl_load_GL_EXT_framebuffer_blit( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_framebuffer_blit) return;
    glad_glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC) load(userptr, "glBlitFramebufferEXT");
}
static void glad_gl_load_GL_EXT_framebuffer_multisample( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_framebuffer_multisample) return;
    glad_glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) load(userptr, "glRenderbufferStorageMultisampleEXT");
}
static void glad_gl_load_GL_EXT_framebuffer_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_framebuffer_object) return;
    glad_glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC) load(userptr, "glBindFramebufferEXT");
    glad_glBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC) load(userptr, "glBindRenderbufferEXT");
    glad_glCheckFramebufferStatusEXT = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC) load(userptr, "glCheckFramebufferStatusEXT");
    glad_glDeleteFramebuffersEXT = (PFNGLDELETEFRAMEBUFFERSEXTPROC) load(userptr, "glDeleteFramebuffersEXT");
    glad_glDeleteRenderbuffersEXT = (PFNGLDELETERENDERBUFFERSEXTPROC) load(userptr, "glDeleteRenderbuffersEXT");
    glad_glFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC) load(userptr, "glFramebufferRenderbufferEXT");
    glad_glFramebufferTexture1DEXT = (PFNGLFRAMEBUFFERTEXTURE1DEXTPROC) load(userptr, "glFramebufferTexture1DEXT");
    glad_glFramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC) load(userptr, "glFramebufferTexture2DEXT");
    glad_glFramebufferTexture3DEXT = (PFNGLFRAMEBUFFERTEXTURE3DEXTPROC) load(userptr, "glFramebufferTexture3DEXT");
    glad_glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC) load(userptr, "glGenFramebuffersEXT");
    glad_glGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC) load(userptr, "glGenRenderbuffersEXT");
    glad_glGenerateMipmapEXT = (PFNGLGENERATEMIPMAPEXTPROC) load(userptr, "glGenerateMipmapEXT");
    glad_glGetFramebufferAttachmentParameterivEXT = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC) load(userptr, "glGetFramebufferAttachmentParameterivEXT");
    glad_glGetRenderbufferParameterivEXT = (PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC) load(userptr, "glGetRenderbufferParameterivEXT");
    glad_glIsFramebufferEXT = (PFNGLISFRAMEBUFFEREXTPROC) load(userptr, "glIsFramebufferEXT");
    glad_glIsRenderbufferEXT = (PFNGLISRENDERBUFFEREXTPROC) load(userptr, "glIsRenderbufferEXT");
    glad_glRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC) load(userptr, "glRenderbufferStorageEXT");
}
static void glad_gl_load_GL_EXT_gpu_shader4( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_gpu_shader4) return;
    glad_glBindFragDataLocationEXT = (PFNGLBINDFRAGDATALOCATIONEXTPROC) load(userptr, "glBindFragDataLocationEXT");
    glad_glGetFragDataLocationEXT = (PFNGLGETFRAGDATALOCATIONEXTPROC) load(userptr, "glGetFragDataLocationEXT");
    glad_glGetUniformuivEXT = (PFNGLGETUNIFORMUIVEXTPROC) load(userptr, "glGetUniformuivEXT");
    glad_glGetVertexAttribIivEXT = (PFNGLGETVERTEXATTRIBIIVEXTPROC) load(userptr, "glGetVertexAttribIivEXT");
    glad_glGetVertexAttribIuivEXT = (PFNGLGETVERTEXATTRIBIUIVEXTPROC) load(userptr, "glGetVertexAttribIuivEXT");
    glad_glUniform1uiEXT = (PFNGLUNIFORM1UIEXTPROC) load(userptr, "glUniform1uiEXT");
    glad_glUniform1uivEXT = (PFNGLUNIFORM1UIVEXTPROC) load(userptr, "glUniform1uivEXT");
    glad_glUniform2uiEXT = (PFNGLUNIFORM2UIEXTPROC) load(userptr, "glUniform2uiEXT");
    glad_glUniform2uivEXT = (PFNGLUNIFORM2UIVEXTPROC) load(userptr, "glUniform2uivEXT");
    glad_glUniform3uiEXT = (PFNGLUNIFORM3UIEXTPROC) load(userptr, "glUniform3uiEXT");
    glad_glUniform3uivEXT = (PFNGLUNIFORM3UIVEXTPROC) load(userptr, "glUniform3uivEXT");
    glad_glUniform4uiEXT = (PFNGLUNIFORM4UIEXTPROC) load(userptr, "glUniform4uiEXT");
    glad_glUniform4uivEXT = (PFNGLUNIFORM4UIVEXTPROC) load(userptr, "glUniform4uivEXT");
    glad_glVertexAttribI1iEXT = (PFNGLVERTEXATTRIBI1IEXTPROC) load(userptr, "glVertexAttribI1iEXT");
    glad_glVertexAttribI1ivEXT = (PFNGLVERTEXATTRIBI1IVEXTPROC) load(userptr, "glVertexAttribI1ivEXT");
    glad_glVertexAttribI1uiEXT = (PFNGLVERTEXATTRIBI1UIEXTPROC) load(userptr, "glVertexAttribI1uiEXT");
    glad_glVertexAttribI1uivEXT = (PFNGLVERTEXATTRIBI1UIVEXTPROC) load(userptr, "glVertexAttribI1uivEXT");
    glad_glVertexAttribI2iEXT = (PFNGLVERTEXATTRIBI2IEXTPROC) load(userptr, "glVertexAttribI2iEXT");
    glad_glVertexAttribI2ivEXT = (PFNGLVERTEXATTRIBI2IVEXTPROC) load(userptr, "glVertexAttribI2ivEXT");
    glad_glVertexAttribI2uiEXT = (PFNGLVERTEXATTRIBI2UIEXTPROC) load(userptr, "glVertexAttribI2uiEXT");
    glad_glVertexAttribI2uivEXT = (PFNGLVERTEXATTRIBI2UIVEXTPROC) load(userptr, "glVertexAttribI2uivEXT");
    glad_glVertexAttribI3iEXT = (PFNGLVERTEXATTRIBI3IEXTPROC) load(userptr, "glVertexAttribI3iEXT");
    glad_glVertexAttribI3ivEXT = (PFNGLVERTEXATTRIBI3IVEXTPROC) load(userptr, "glVertexAttribI3ivEXT");
    glad_glVertexAttribI3uiEXT = (PFNGLVERTEXATTRIBI3UIEXTPROC) load(userptr, "glVertexAttribI3uiEXT");
    glad_glVertexAttribI3uivEXT = (PFNGLVERTEXATTRIBI3UIVEXTPROC) load(userptr, "glVertexAttribI3uivEXT");
    glad_glVertexAttribI4bvEXT = (PFNGLVERTEXATTRIBI4BVEXTPROC) load(userptr, "glVertexAttribI4bvEXT");
    glad_glVertexAttribI4iEXT = (PFNGLVERTEXATTRIBI4IEXTPROC) load(userptr, "glVertexAttribI4iEXT");
    glad_glVertexAttribI4ivEXT = (PFNGLVERTEXATTRIBI4IVEXTPROC) load(userptr, "glVertexAttribI4ivEXT");
    glad_glVertexAttribI4svEXT = (PFNGLVERTEXATTRIBI4SVEXTPROC) load(userptr, "glVertexAttribI4svEXT");
    glad_glVertexAttribI4ubvEXT = (PFNGLVERTEXATTRIBI4UBVEXTPROC) load(userptr, "glVertexAttribI4ubvEXT");
    glad_glVertexAttribI4uiEXT = (PFNGLVERTEXATTRIBI4UIEXTPROC) load(userptr, "glVertexAttribI4uiEXT");
    glad_glVertexAttribI4uivEXT = (PFNGLVERTEXATTRIBI4UIVEXTPROC) load(userptr, "glVertexAttribI4uivEXT");
    glad_glVertexAttribI4usvEXT = (PFNGLVERTEXATTRIBI4USVEXTPROC) load(userptr, "glVertexAttribI4usvEXT");
    glad_glVertexAttribIPointerEXT = (PFNGLVERTEXATTRIBIPOINTEREXTPROC) load(userptr, "glVertexAttribIPointerEXT");
}
static void glad_gl_load_GL_EXT_multi_draw_arrays( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_multi_draw_arrays) return;
    glad_glMultiDrawArraysEXT = (PFNGLMULTIDRAWARRAYSEXTPROC) load(userptr, "glMultiDrawArraysEXT");
    glad_glMultiDrawElementsEXT = (PFNGLMULTIDRAWELEMENTSEXTPROC) load(userptr, "glMultiDrawElementsEXT");
}
static void glad_gl_load_GL_EXT_point_parameters( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_point_parameters) return;
    glad_glPointParameterfEXT = (PFNGLPOINTPARAMETERFEXTPROC) load(userptr, "glPointParameterfEXT");
    glad_glPointParameterfvEXT = (PFNGLPOINTPARAMETERFVEXTPROC) load(userptr, "glPointParameterfvEXT");
}
static void glad_gl_load_GL_EXT_provoking_vertex( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_provoking_vertex) return;
    glad_glProvokingVertexEXT = (PFNGLPROVOKINGVERTEXEXTPROC) load(userptr, "glProvokingVertexEXT");
}
static void glad_gl_load_GL_EXT_subtexture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_subtexture) return;
    glad_glTexSubImage1DEXT = (PFNGLTEXSUBIMAGE1DEXTPROC) load(userptr, "glTexSubImage1DEXT");
    glad_glTexSubImage2DEXT = (PFNGLTEXSUBIMAGE2DEXTPROC) load(userptr, "glTexSubImage2DEXT");
}
static void glad_gl_load_GL_EXT_texture3D( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_texture3D) return;
    glad_glTexImage3DEXT = (PFNGLTEXIMAGE3DEXTPROC) load(userptr, "glTexImage3DEXT");
    glad_glTexSubImage3DEXT = (PFNGLTEXSUBIMAGE3DEXTPROC) load(userptr, "glTexSubImage3DEXT");
}
static void glad_gl_load_GL_EXT_texture_array( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_texture_array) return;
    glad_glFramebufferTextureLayerEXT = (PFNGLFRAMEBUFFERTEXTURELAYEREXTPROC) load(userptr, "glFramebufferTextureLayerEXT");
}
static void glad_gl_load_GL_EXT_texture_buffer_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_texture_buffer_object) return;
    glad_glTexBufferEXT = (PFNGLTEXBUFFEREXTPROC) load(userptr, "glTexBufferEXT");
}
static void glad_gl_load_GL_EXT_texture_integer( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_texture_integer) return;
    glad_glClearColorIiEXT = (PFNGLCLEARCOLORIIEXTPROC) load(userptr, "glClearColorIiEXT");
    glad_glClearColorIuiEXT = (PFNGLCLEARCOLORIUIEXTPROC) load(userptr, "glClearColorIuiEXT");
    glad_glGetTexParameterIivEXT = (PFNGLGETTEXPARAMETERIIVEXTPROC) load(userptr, "glGetTexParameterIivEXT");
    glad_glGetTexParameterIuivEXT = (PFNGLGETTEXPARAMETERIUIVEXTPROC) load(userptr, "glGetTexParameterIuivEXT");
    glad_glTexParameterIivEXT = (PFNGLTEXPARAMETERIIVEXTPROC) load(userptr, "glTexParameterIivEXT");
    glad_glTexParameterIuivEXT = (PFNGLTEXPARAMETERIUIVEXTPROC) load(userptr, "glTexParameterIuivEXT");
}
static void glad_gl_load_GL_EXT_texture_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_texture_object) return;
    glad_glAreTexturesResidentEXT = (PFNGLARETEXTURESRESIDENTEXTPROC) load(userptr, "glAreTexturesResidentEXT");
    glad_glBindTextureEXT = (PFNGLBINDTEXTUREEXTPROC) load(userptr, "glBindTextureEXT");
    glad_glDeleteTexturesEXT = (PFNGLDELETETEXTURESEXTPROC) load(userptr, "glDeleteTexturesEXT");
    glad_glGenTexturesEXT = (PFNGLGENTEXTURESEXTPROC) load(userptr, "glGenTexturesEXT");
    glad_glIsTextureEXT = (PFNGLISTEXTUREEXTPROC) load(userptr, "glIsTextureEXT");
    glad_glPrioritizeTexturesEXT = (PFNGLPRIORITIZETEXTURESEXTPROC) load(userptr, "glPrioritizeTexturesEXT");
}
static void glad_gl_load_GL_EXT_timer_query( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_timer_query) return;
    glad_glGetQueryObjecti64vEXT = (PFNGLGETQUERYOBJECTI64VEXTPROC) load(userptr, "glGetQueryObjecti64vEXT");
    glad_glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC) load(userptr, "glGetQueryObjectui64vEXT");
}
static void glad_gl_load_GL_EXT_transform_feedback( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_transform_feedback) return;
    glad_glBeginTransformFeedbackEXT = (PFNGLBEGINTRANSFORMFEEDBACKEXTPROC) load(userptr, "glBeginTransformFeedbackEXT");
    glad_glBindBufferBaseEXT = (PFNGLBINDBUFFERBASEEXTPROC) load(userptr, "glBindBufferBaseEXT");
    glad_glBindBufferOffsetEXT = (PFNGLBINDBUFFEROFFSETEXTPROC) load(userptr, "glBindBufferOffsetEXT");
    glad_glBindBufferRangeEXT = (PFNGLBINDBUFFERRANGEEXTPROC) load(userptr, "glBindBufferRangeEXT");
    glad_glEndTransformFeedbackEXT = (PFNGLENDTRANSFORMFEEDBACKEXTPROC) load(userptr, "glEndTransformFeedbackEXT");
    glad_glGetTransformFeedbackVaryingEXT = (PFNGLGETTRANSFORMFEEDBACKVARYINGEXTPROC) load(userptr, "glGetTransformFeedbackVaryingEXT");
    glad_glTransformFeedbackVaryingsEXT = (PFNGLTRANSFORMFEEDBACKVARYINGSEXTPROC) load(userptr, "glTransformFeedbackVaryingsEXT");
}
static void glad_gl_load_GL_EXT_vertex_array( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_vertex_array) return;
    glad_glArrayElementEXT = (PFNGLARRAYELEMENTEXTPROC) load(userptr, "glArrayElementEXT");
    glad_glColorPointerEXT = (PFNGLCOLORPOINTEREXTPROC) load(userptr, "glColorPointerEXT");
    glad_glDrawArraysEXT = (PFNGLDRAWARRAYSEXTPROC) load(userptr, "glDrawArraysEXT");
    glad_glEdgeFlagPointerEXT = (PFNGLEDGEFLAGPOINTEREXTPROC) load(userptr, "glEdgeFlagPointerEXT");
    glad_glGetPointervEXT = (PFNGLGETPOINTERVEXTPROC) load(userptr, "glGetPointervEXT");
    glad_glIndexPointerEXT = (PFNGLINDEXPOINTEREXTPROC) load(userptr, "glIndexPointerEXT");
    glad_glNormalPointerEXT = (PFNGLNORMALPOINTEREXTPROC) load(userptr, "glNormalPointerEXT");
    glad_glTexCoordPointerEXT = (PFNGLTEXCOORDPOINTEREXTPROC) load(userptr, "glTexCoordPointerEXT");
    glad_glVertexPointerEXT = (PFNGLVERTEXPOINTEREXTPROC) load(userptr, "glVertexPointerEXT");
}
static void glad_gl_load_GL_INGR_blend_func_separate( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_INGR_blend_func_separate) return;
    glad_glBlendFuncSeparateINGR = (PFNGLBLENDFUNCSEPARATEINGRPROC) load(userptr, "glBlendFuncSeparateINGR");
}
static void glad_gl_load_GL_NVX_conditional_render( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NVX_conditional_render) return;
    glad_glBeginConditionalRenderNVX = (PFNGLBEGINCONDITIONALRENDERNVXPROC) load(userptr, "glBeginConditionalRenderNVX");
    glad_glEndConditionalRenderNVX = (PFNGLENDCONDITIONALRENDERNVXPROC) load(userptr, "glEndConditionalRenderNVX");
}
static void glad_gl_load_GL_NV_conditional_render( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_conditional_render) return;
    glad_glBeginConditionalRenderNV = (PFNGLBEGINCONDITIONALRENDERNVPROC) load(userptr, "glBeginConditionalRenderNV");
    glad_glEndConditionalRenderNV = (PFNGLENDCONDITIONALRENDERNVPROC) load(userptr, "glEndConditionalRenderNV");
}
static void glad_gl_load_GL_NV_explicit_multisample( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_explicit_multisample) return;
    glad_glGetMultisamplefvNV = (PFNGLGETMULTISAMPLEFVNVPROC) load(userptr, "glGetMultisamplefvNV");
    glad_glSampleMaskIndexedNV = (PFNGLSAMPLEMASKINDEXEDNVPROC) load(userptr, "glSampleMaskIndexedNV");
    glad_glTexRenderbufferNV = (PFNGLTEXRENDERBUFFERNVPROC) load(userptr, "glTexRenderbufferNV");
}
static void glad_gl_load_GL_NV_geometry_program4( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_geometry_program4) return;
    glad_glFramebufferTextureEXT = (PFNGLFRAMEBUFFERTEXTUREEXTPROC) load(userptr, "glFramebufferTextureEXT");
    glad_glFramebufferTextureFaceEXT = (PFNGLFRAMEBUFFERTEXTUREFACEEXTPROC) load(userptr, "glFramebufferTextureFaceEXT");
    glad_glFramebufferTextureLayerEXT = (PFNGLFRAMEBUFFERTEXTURELAYEREXTPROC) load(userptr, "glFramebufferTextureLayerEXT");
    glad_glProgramVertexLimitNV = (PFNGLPROGRAMVERTEXLIMITNVPROC) load(userptr, "glProgramVertexLimitNV");
}
static void glad_gl_load_GL_NV_point_sprite( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_point_sprite) return;
    glad_glPointParameteriNV = (PFNGLPOINTPARAMETERINVPROC) load(userptr, "glPointParameteriNV");
    glad_glPointParameterivNV = (PFNGLPOINTPARAMETERIVNVPROC) load(userptr, "glPointParameterivNV");
}
static void glad_gl_load_GL_NV_transform_feedback( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_transform_feedback) return;
    glad_glActiveVaryingNV = (PFNGLACTIVEVARYINGNVPROC) load(userptr, "glActiveVaryingNV");
    glad_glBeginTransformFeedbackNV = (PFNGLBEGINTRANSFORMFEEDBACKNVPROC) load(userptr, "glBeginTransformFeedbackNV");
    glad_glBindBufferBaseNV = (PFNGLBINDBUFFERBASENVPROC) load(userptr, "glBindBufferBaseNV");
    glad_glBindBufferOffsetNV = (PFNGLBINDBUFFEROFFSETNVPROC) load(userptr, "glBindBufferOffsetNV");
    glad_glBindBufferRangeNV = (PFNGLBINDBUFFERRANGENVPROC) load(userptr, "glBindBufferRangeNV");
    glad_glEndTransformFeedbackNV = (PFNGLENDTRANSFORMFEEDBACKNVPROC) load(userptr, "glEndTransformFeedbackNV");
    glad_glGetActiveVaryingNV = (PFNGLGETACTIVEVARYINGNVPROC) load(userptr, "glGetActiveVaryingNV");
    glad_glGetTransformFeedbackVaryingNV = (PFNGLGETTRANSFORMFEEDBACKVARYINGNVPROC) load(userptr, "glGetTransformFeedbackVaryingNV");
    glad_glGetVaryingLocationNV = (PFNGLGETVARYINGLOCATIONNVPROC) load(userptr, "glGetVaryingLocationNV");
    glad_glTransformFeedbackAttribsNV = (PFNGLTRANSFORMFEEDBACKATTRIBSNVPROC) load(userptr, "glTransformFeedbackAttribsNV");
    glad_glTransformFeedbackStreamAttribsNV = (PFNGLTRANSFORMFEEDBACKSTREAMATTRIBSNVPROC) load(userptr, "glTransformFeedbackStreamAttribsNV");
    glad_glTransformFeedbackVaryingsNV = (PFNGLTRANSFORMFEEDBACKVARYINGSNVPROC) load(userptr, "glTransformFeedbackVaryingsNV");
}
static void glad_gl_load_GL_NV_vertex_program( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_vertex_program) return;
    glad_glAreProgramsResidentNV = (PFNGLAREPROGRAMSRESIDENTNVPROC) load(userptr, "glAreProgramsResidentNV");
    glad_glBindProgramNV = (PFNGLBINDPROGRAMNVPROC) load(userptr, "glBindProgramNV");
    glad_glDeleteProgramsNV = (PFNGLDELETEPROGRAMSNVPROC) load(userptr, "glDeleteProgramsNV");
    glad_glExecuteProgramNV = (PFNGLEXECUTEPROGRAMNVPROC) load(userptr, "glExecuteProgramNV");
    glad_glGenProgramsNV = (PFNGLGENPROGRAMSNVPROC) load(userptr, "glGenProgramsNV");
    glad_glGetProgramParameterdvNV = (PFNGLGETPROGRAMPARAMETERDVNVPROC) load(userptr, "glGetProgramParameterdvNV");
    glad_glGetProgramParameterfvNV = (PFNGLGETPROGRAMPARAMETERFVNVPROC) load(userptr, "glGetProgramParameterfvNV");
    glad_glGetProgramStringNV = (PFNGLGETPROGRAMSTRINGNVPROC) load(userptr, "glGetProgramStringNV");
    glad_glGetProgramivNV = (PFNGLGETPROGRAMIVNVPROC) load(userptr, "glGetProgramivNV");
    glad_glGetTrackMatrixivNV = (PFNGLGETTRACKMATRIXIVNVPROC) load(userptr, "glGetTrackMatrixivNV");
    glad_glGetVertexAttribPointervNV = (PFNGLGETVERTEXATTRIBPOINTERVNVPROC) load(userptr, "glGetVertexAttribPointervNV");
    glad_glGetVertexAttribdvNV = (PFNGLGETVERTEXATTRIBDVNVPROC) load(userptr, "glGetVertexAttribdvNV");
    glad_glGetVertexAttribfvNV = (PFNGLGETVERTEXATTRIBFVNVPROC) load(userptr, "glGetVertexAttribfvNV");
    glad_glGetVertexAttribivNV = (PFNGLGETVERTEXATTRIBIVNVPROC) load(userptr, "glGetVertexAttribivNV");
    glad_glIsProgramNV = (PFNGLISPROGRAMNVPROC) load(userptr, "glIsProgramNV");
    glad_glLoadProgramNV = (PFNGLLOADPROGRAMNVPROC) load(userptr, "glLoadProgramNV");
    glad_glProgramParameter4dNV = (PFNGLPROGRAMPARAMETER4DNVPROC) load(userptr, "glProgramParameter4dNV");
    glad_glProgramParameter4dvNV = (PFNGLPROGRAMPARAMETER4DVNVPROC) load(userptr, "glProgramParameter4dvNV");
    glad_glProgramParameter4fNV = (PFNGLPROGRAMPARAMETER4FNVPROC) load(userptr, "glProgramParameter4fNV");
    glad_glProgramParameter4fvNV = (PFNGLPROGRAMPARAMETER4FVNVPROC) load(userptr, "glProgramParameter4fvNV");
    glad_glProgramParameters4dvNV = (PFNGLPROGRAMPARAMETERS4DVNVPROC) load(userptr, "glProgramParameters4dvNV");
    glad_glProgramParameters4fvNV = (PFNGLPROGRAMPARAMETERS4FVNVPROC) load(userptr, "glProgramParameters4fvNV");
    glad_glRequestResidentProgramsNV = (PFNGLREQUESTRESIDENTPROGRAMSNVPROC) load(userptr, "glRequestResidentProgramsNV");
    glad_glTrackMatrixNV = (PFNGLTRACKMATRIXNVPROC) load(userptr, "glTrackMatrixNV");
    glad_glVertexAttrib1dNV = (PFNGLVERTEXATTRIB1DNVPROC) load(userptr, "glVertexAttrib1dNV");
    glad_glVertexAttrib1dvNV = (PFNGLVERTEXATTRIB1DVNVPROC) load(userptr, "glVertexAttrib1dvNV");
    glad_glVertexAttrib1fNV = (PFNGLVERTEXATTRIB1FNVPROC) load(userptr, "glVertexAttrib1fNV");
    glad_glVertexAttrib1fvNV = (PFNGLVERTEXATTRIB1FVNVPROC) load(userptr, "glVertexAttrib1fvNV");
    glad_glVertexAttrib1sNV = (PFNGLVERTEXATTRIB1SNVPROC) load(userptr, "glVertexAttrib1sNV");
    glad_glVertexAttrib1svNV = (PFNGLVERTEXATTRIB1SVNVPROC) load(userptr, "glVertexAttrib1svNV");
    glad_glVertexAttrib2dNV = (PFNGLVERTEXATTRIB2DNVPROC) load(userptr, "glVertexAttrib2dNV");
    glad_glVertexAttrib2dvNV = (PFNGLVERTEXATTRIB2DVNVPROC) load(userptr, "glVertexAttrib2dvNV");
    glad_glVertexAttrib2fNV = (PFNGLVERTEXATTRIB2FNVPROC) load(userptr, "glVertexAttrib2fNV");
    glad_glVertexAttrib2fvNV = (PFNGLVERTEXATTRIB2FVNVPROC) load(userptr, "glVertexAttrib2fvNV");
    glad_glVertexAttrib2sNV = (PFNGLVERTEXATTRIB2SNVPROC) load(userptr, "glVertexAttrib2sNV");
    glad_glVertexAttrib2svNV = (PFNGLVERTEXATTRIB2SVNVPROC) load(userptr, "glVertexAttrib2svNV");
    glad_glVertexAttrib3dNV = (PFNGLVERTEXATTRIB3DNVPROC) load(userptr, "glVertexAttrib3dNV");
    glad_glVertexAttrib3dvNV = (PFNGLVERTEXATTRIB3DVNVPROC) load(userptr, "glVertexAttrib3dvNV");
    glad_glVertexAttrib3fNV = (PFNGLVERTEXATTRIB3FNVPROC) load(userptr, "glVertexAttrib3fNV");
    glad_glVertexAttrib3fvNV = (PFNGLVERTEXATTRIB3FVNVPROC) load(userptr, "glVertexAttrib3fvNV");
    glad_glVertexAttrib3sNV = (PFNGLVERTEXATTRIB3SNVPROC) load(userptr, "glVertexAttrib3sNV");
    glad_glVertexAttrib3svNV = (PFNGLVERTEXATTRIB3SVNVPROC) load(userptr, "glVertexAttrib3svNV");
    glad_glVertexAttrib4dNV = (PFNGLVERTEXATTRIB4DNVPROC) load(userptr, "glVertexAttrib4dNV");
    glad_glVertexAttrib4dvNV = (PFNGLVERTEXATTRIB4DVNVPROC) load(userptr, "glVertexAttrib4dvNV");
    glad_glVertexAttrib4fNV = (PFNGLVERTEXATTRIB4FNVPROC) load(userptr, "glVertexAttrib4fNV");
    glad_glVertexAttrib4fvNV = (PFNGLVERTEXATTRIB4FVNVPROC) load(userptr, "glVertexAttrib4fvNV");
    glad_glVertexAttrib4sNV = (PFNGLVERTEXATTRIB4SNVPROC) load(userptr, "glVertexAttrib4sNV");
    glad_glVertexAttrib4svNV = (PFNGLVERTEXATTRIB4SVNVPROC) load(userptr, "glVertexAttrib4svNV");
    glad_glVertexAttrib4ubNV = (PFNGLVERTEXATTRIB4UBNVPROC) load(userptr, "glVertexAttrib4ubNV");
    glad_glVertexAttrib4ubvNV = (PFNGLVERTEXATTRIB4UBVNVPROC) load(userptr, "glVertexAttrib4ubvNV");
    glad_glVertexAttribPointerNV = (PFNGLVERTEXATTRIBPOINTERNVPROC) load(userptr, "glVertexAttribPointerNV");
    glad_glVertexAttribs1dvNV = (PFNGLVERTEXATTRIBS1DVNVPROC) load(userptr, "glVertexAttribs1dvNV");
    glad_glVertexAttribs1fvNV = (PFNGLVERTEXATTRIBS1FVNVPROC) load(userptr, "glVertexAttribs1fvNV");
    glad_glVertexAttribs1svNV = (PFNGLVERTEXATTRIBS1SVNVPROC) load(userptr, "glVertexAttribs1svNV");
    glad_glVertexAttribs2dvNV = (PFNGLVERTEXATTRIBS2DVNVPROC) load(userptr, "glVertexAttribs2dvNV");
    glad_glVertexAttribs2fvNV = (PFNGLVERTEXATTRIBS2FVNVPROC) load(userptr, "glVertexAttribs2fvNV");
    glad_glVertexAttribs2svNV = (PFNGLVERTEXATTRIBS2SVNVPROC) load(userptr, "glVertexAttribs2svNV");
    glad_glVertexAttribs3dvNV = (PFNGLVERTEXATTRIBS3DVNVPROC) load(userptr, "glVertexAttribs3dvNV");
    glad_glVertexAttribs3fvNV = (PFNGLVERTEXATTRIBS3FVNVPROC) load(userptr, "glVertexAttribs3fvNV");
    glad_glVertexAttribs3svNV = (PFNGLVERTEXATTRIBS3SVNVPROC) load(userptr, "glVertexAttribs3svNV");
    glad_glVertexAttribs4dvNV = (PFNGLVERTEXATTRIBS4DVNVPROC) load(userptr, "glVertexAttribs4dvNV");
    glad_glVertexAttribs4fvNV = (PFNGLVERTEXATTRIBS4FVNVPROC) load(userptr, "glVertexAttribs4fvNV");
    glad_glVertexAttribs4svNV = (PFNGLVERTEXATTRIBS4SVNVPROC) load(userptr, "glVertexAttribs4svNV");
    glad_glVertexAttribs4ubvNV = (PFNGLVERTEXATTRIBS4UBVNVPROC) load(userptr, "glVertexAttribs4ubvNV");
}
static void glad_gl_load_GL_NV_vertex_program4( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_vertex_program4) return;
    glad_glGetVertexAttribIivEXT = (PFNGLGETVERTEXATTRIBIIVEXTPROC) load(userptr, "glGetVertexAttribIivEXT");
    glad_glGetVertexAttribIuivEXT = (PFNGLGETVERTEXATTRIBIUIVEXTPROC) load(userptr, "glGetVertexAttribIuivEXT");
    glad_glVertexAttribI1iEXT = (PFNGLVERTEXATTRIBI1IEXTPROC) load(userptr, "glVertexAttribI1iEXT");
    glad_glVertexAttribI1ivEXT = (PFNGLVERTEXATTRIBI1IVEXTPROC) load(userptr, "glVertexAttribI1ivEXT");
    glad_glVertexAttribI1uiEXT = (PFNGLVERTEXATTRIBI1UIEXTPROC) load(userptr, "glVertexAttribI1uiEXT");
    glad_glVertexAttribI1uivEXT = (PFNGLVERTEXATTRIBI1UIVEXTPROC) load(userptr, "glVertexAttribI1uivEXT");
    glad_glVertexAttribI2iEXT = (PFNGLVERTEXATTRIBI2IEXTPROC) load(userptr, "glVertexAttribI2iEXT");
    glad_glVertexAttribI2ivEXT = (PFNGLVERTEXATTRIBI2IVEXTPROC) load(userptr, "glVertexAttribI2ivEXT");
    glad_glVertexAttribI2uiEXT = (PFNGLVERTEXATTRIBI2UIEXTPROC) load(userptr, "glVertexAttribI2uiEXT");
    glad_glVertexAttribI2uivEXT = (PFNGLVERTEXATTRIBI2UIVEXTPROC) load(userptr, "glVertexAttribI2uivEXT");
    glad_glVertexAttribI3iEXT = (PFNGLVERTEXATTRIBI3IEXTPROC) load(userptr, "glVertexAttribI3iEXT");
    glad_glVertexAttribI3ivEXT = (PFNGLVERTEXATTRIBI3IVEXTPROC) load(userptr, "glVertexAttribI3ivEXT");
    glad_glVertexAttribI3uiEXT = (PFNGLVERTEXATTRIBI3UIEXTPROC) load(userptr, "glVertexAttribI3uiEXT");
    glad_glVertexAttribI3uivEXT = (PFNGLVERTEXATTRIBI3UIVEXTPROC) load(userptr, "glVertexAttribI3uivEXT");
    glad_glVertexAttribI4bvEXT = (PFNGLVERTEXATTRIBI4BVEXTPROC) load(userptr, "glVertexAttribI4bvEXT");
    glad_glVertexAttribI4iEXT = (PFNGLVERTEXATTRIBI4IEXTPROC) load(userptr, "glVertexAttribI4iEXT");
    glad_glVertexAttribI4ivEXT = (PFNGLVERTEXATTRIBI4IVEXTPROC) load(userptr, "glVertexAttribI4ivEXT");
    glad_glVertexAttribI4svEXT = (PFNGLVERTEXATTRIBI4SVEXTPROC) load(userptr, "glVertexAttribI4svEXT");
    glad_glVertexAttribI4ubvEXT = (PFNGLVERTEXATTRIBI4UBVEXTPROC) load(userptr, "glVertexAttribI4ubvEXT");
    glad_glVertexAttribI4uiEXT = (PFNGLVERTEXATTRIBI4UIEXTPROC) load(userptr, "glVertexAttribI4uiEXT");
    glad_glVertexAttribI4uivEXT = (PFNGLVERTEXATTRIBI4UIVEXTPROC) load(userptr, "glVertexAttribI4uivEXT");
    glad_glVertexAttribI4usvEXT = (PFNGLVERTEXATTRIBI4USVEXTPROC) load(userptr, "glVertexAttribI4usvEXT");
    glad_glVertexAttribIPointerEXT = (PFNGLVERTEXATTRIBIPOINTEREXTPROC) load(userptr, "glVertexAttribIPointerEXT");
}
static void glad_gl_load_GL_SGIS_point_parameters( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_SGIS_point_parameters) return;
    glad_glPointParameterfSGIS = (PFNGLPOINTPARAMETERFSGISPROC) load(userptr, "glPointParameterfSGIS");
    glad_glPointParameterfvSGIS = (PFNGLPOINTPARAMETERFVSGISPROC) load(userptr, "glPointParameterfvSGIS");
}


static void glad_gl_resolve_aliases(void) {
    if (glad_glActiveTexture == NULL && glad_glActiveTextureARB != NULL) glad_glActiveTexture = (PFNGLACTIVETEXTUREPROC)glad_glActiveTextureARB;
    if (glad_glActiveTextureARB == NULL && glad_glActiveTexture != NULL) glad_glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)glad_glActiveTexture;
    if (glad_glAttachObjectARB == NULL && glad_glAttachShader != NULL) glad_glAttachObjectARB = (PFNGLATTACHOBJECTARBPROC)glad_glAttachShader;
    if (glad_glAttachShader == NULL && glad_glAttachObjectARB != NULL) glad_glAttachShader = (PFNGLATTACHSHADERPROC)glad_glAttachObjectARB;
    if (glad_glBeginConditionalRender == NULL && glad_glBeginConditionalRenderNV != NULL) glad_glBeginConditionalRender = (PFNGLBEGINCONDITIONALRENDERPROC)glad_glBeginConditionalRenderNV;
    if (glad_glBeginConditionalRenderNV == NULL && glad_glBeginConditionalRender != NULL) glad_glBeginConditionalRenderNV = (PFNGLBEGINCONDITIONALRENDERNVPROC)glad_glBeginConditionalRender;
    if (glad_glBeginQuery == NULL && glad_glBeginQueryARB != NULL) glad_glBeginQuery = (PFNGLBEGINQUERYPROC)glad_glBeginQueryARB;
    if (glad_glBeginQueryARB == NULL && glad_glBeginQuery != NULL) glad_glBeginQueryARB = (PFNGLBEGINQUERYARBPROC)glad_glBeginQuery;
    if (glad_glBeginTransformFeedback == NULL && glad_glBeginTransformFeedbackEXT != NULL) glad_glBeginTransformFeedback = (PFNGLBEGINTRANSFORMFEEDBACKPROC)glad_glBeginTransformFeedbackEXT;
    if (glad_glBeginTransformFeedback == NULL && glad_glBeginTransformFeedbackNV != NULL) glad_glBeginTransformFeedback = (PFNGLBEGINTRANSFORMFEEDBACKPROC)glad_glBeginTransformFeedbackNV;
    if (glad_glBeginTransformFeedbackEXT == NULL && glad_glBeginTransformFeedback != NULL) glad_glBeginTransformFeedbackEXT = (PFNGLBEGINTRANSFORMFEEDBACKEXTPROC)glad_glBeginTransformFeedback;
    if (glad_glBeginTransformFeedbackEXT == NULL && glad_glBeginTransformFeedbackNV != NULL) glad_glBeginTransformFeedbackEXT = (PFNGLBEGINTRANSFORMFEEDBACKEXTPROC)glad_glBeginTransformFeedbackNV;
    if (glad_glBeginTransformFeedbackNV == NULL && glad_glBeginTransformFeedback != NULL) glad_glBeginTransformFeedbackNV = (PFNGLBEGINTRANSFORMFEEDBACKNVPROC)glad_glBeginTransformFeedback;
    if (glad_glBeginTransformFeedbackNV == NULL && glad_glBeginTransformFeedbackEXT != NULL) glad_glBeginTransformFeedbackNV = (PFNGLBEGINTRANSFORMFEEDBACKNVPROC)glad_glBeginTransformFeedbackEXT;
    if (glad_glBindAttribLocation == NULL && glad_glBindAttribLocationARB != NULL) glad_glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)glad_glBindAttribLocationARB;
    if (glad_glBindAttribLocationARB == NULL && glad_glBindAttribLocation != NULL) glad_glBindAttribLocationARB = (PFNGLBINDATTRIBLOCATIONARBPROC)glad_glBindAttribLocation;
    if (glad_glBindBuffer == NULL && glad_glBindBufferARB != NULL) glad_glBindBuffer = (PFNGLBINDBUFFERPROC)glad_glBindBufferARB;
    if (glad_glBindBufferARB == NULL && glad_glBindBuffer != NULL) glad_glBindBufferARB = (PFNGLBINDBUFFERARBPROC)glad_glBindBuffer;
    if (glad_glBindBufferBase == NULL && glad_glBindBufferBaseEXT != NULL) glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)glad_glBindBufferBaseEXT;
    if (glad_glBindBufferBase == NULL && glad_glBindBufferBaseNV != NULL) glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)glad_glBindBufferBaseNV;
    if (glad_glBindBufferBaseEXT == NULL && glad_glBindBufferBase != NULL) glad_glBindBufferBaseEXT = (PFNGLBINDBUFFERBASEEXTPROC)glad_glBindBufferBase;
    if (glad_glBindBufferBaseEXT == NULL && glad_glBindBufferBaseNV != NULL) glad_glBindBufferBaseEXT = (PFNGLBINDBUFFERBASEEXTPROC)glad_glBindBufferBaseNV;
    if (glad_glBindBufferBaseNV == NULL && glad_glBindBufferBase != NULL) glad_glBindBufferBaseNV = (PFNGLBINDBUFFERBASENVPROC)glad_glBindBufferBase;
    if (glad_glBindBufferBaseNV == NULL && glad_glBindBufferBaseEXT != NULL) glad_glBindBufferBaseNV = (PFNGLBINDBUFFERBASENVPROC)glad_glBindBufferBaseEXT;
    if (glad_glBindBufferOffsetEXT == NULL && glad_glBindBufferOffsetNV != NULL) glad_glBindBufferOffsetEXT = (PFNGLBINDBUFFEROFFSETEXTPROC)glad_glBindBufferOffsetNV;
    if (glad_glBindBufferOffsetNV == NULL && glad_glBindBufferOffsetEXT != NULL) glad_glBindBufferOffsetNV = (PFNGLBINDBUFFEROFFSETNVPROC)glad_glBindBufferOffsetEXT;
    if (glad_glBindBufferRange == NULL && glad_glBindBufferRangeEXT != NULL) glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)glad_glBindBufferRangeEXT;
    if (glad_glBindBufferRange == NULL && glad_glBindBufferRangeNV != NULL) glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)glad_glBindBufferRangeNV;
    if (glad_glBindBufferRangeEXT == NULL && glad_glBindBufferRange != NULL) glad_glBindBufferRangeEXT = (PFNGLBINDBUFFERRANGEEXTPROC)glad_glBindBufferRange;
    if (glad_glBindBufferRangeEXT == NULL && glad_glBindBufferRangeNV != NULL) glad_glBindBufferRangeEXT = (PFNGLBINDBUFFERRANGEEXTPROC)glad_glBindBufferRangeNV;
    if (glad_glBindBufferRangeNV == NULL && glad_glBindBufferRange != NULL) glad_glBindBufferRangeNV = (PFNGLBINDBUFFERRANGENVPROC)glad_glBindBufferRange;
    if (glad_glBindBufferRangeNV == NULL && glad_glBindBufferRangeEXT != NULL) glad_glBindBufferRangeNV = (PFNGLBINDBUFFERRANGENVPROC)glad_glBindBufferRangeEXT;
    if (glad_glBindFragDataLocation == NULL && glad_glBindFragDataLocationEXT != NULL) glad_glBindFragDataLocation = (PFNGLBINDFRAGDATALOCATIONPROC)glad_glBindFragDataLocationEXT;
    if (glad_glBindFragDataLocationEXT == NULL && glad_glBindFragDataLocation != NULL) glad_glBindFragDataLocationEXT = (PFNGLBINDFRAGDATALOCATIONEXTPROC)glad_glBindFragDataLocation;
    if (glad_glBindProgramARB == NULL && glad_glBindProgramNV != NULL) glad_glBindProgramARB = (PFNGLBINDPROGRAMARBPROC)glad_glBindProgramNV;
    if (glad_glBindProgramNV == NULL && glad_glBindProgramARB != NULL) glad_glBindProgramNV = (PFNGLBINDPROGRAMNVPROC)glad_glBindProgramARB;
    if (glad_glBindTexture == NULL && glad_glBindTextureEXT != NULL) glad_glBindTexture = (PFNGLBINDTEXTUREPROC)glad_glBindTextureEXT;
    if (glad_glBindTextureEXT == NULL && glad_glBindTexture != NULL) glad_glBindTextureEXT = (PFNGLBINDTEXTUREEXTPROC)glad_glBindTexture;
    if (glad_glBlendColor == NULL && glad_glBlendColorEXT != NULL) glad_glBlendColor = (PFNGLBLENDCOLORPROC)glad_glBlendColorEXT;
    if (glad_glBlendColorEXT == NULL && glad_glBlendColor != NULL) glad_glBlendColorEXT = (PFNGLBLENDCOLOREXTPROC)glad_glBlendColor;
    if (glad_glBlendEquation == NULL && glad_glBlendEquationEXT != NULL) glad_glBlendEquation = (PFNGLBLENDEQUATIONPROC)glad_glBlendEquationEXT;
    if (glad_glBlendEquationEXT == NULL && glad_glBlendEquation != NULL) glad_glBlendEquationEXT = (PFNGLBLENDEQUATIONEXTPROC)glad_glBlendEquation;
    if (glad_glBlendEquationSeparate == NULL && glad_glBlendEquationSeparateEXT != NULL) glad_glBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC)glad_glBlendEquationSeparateEXT;
    if (glad_glBlendEquationSeparateEXT == NULL && glad_glBlendEquationSeparate != NULL) glad_glBlendEquationSeparateEXT = (PFNGLBLENDEQUATIONSEPARATEEXTPROC)glad_glBlendEquationSeparate;
    if (glad_glBlendFuncSeparate == NULL && glad_glBlendFuncSeparateEXT != NULL) glad_glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)glad_glBlendFuncSeparateEXT;
    if (glad_glBlendFuncSeparate == NULL && glad_glBlendFuncSeparateINGR != NULL) glad_glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)glad_glBlendFuncSeparateINGR;
    if (glad_glBlendFuncSeparateEXT == NULL && glad_glBlendFuncSeparate != NULL) glad_glBlendFuncSeparateEXT = (PFNGLBLENDFUNCSEPARATEEXTPROC)glad_glBlendFuncSeparate;
    if (glad_glBlendFuncSeparateEXT == NULL && glad_glBlendFuncSeparateINGR != NULL) glad_glBlendFuncSeparateEXT = (PFNGLBLENDFUNCSEPARATEEXTPROC)glad_glBlendFuncSeparateINGR;
    if (glad_glBlendFuncSeparateINGR == NULL && glad_glBlendFuncSeparate != NULL) glad_glBlendFuncSeparateINGR = (PFNGLBLENDFUNCSEPARATEINGRPROC)glad_glBlendFuncSeparate;
    if (glad_glBlendFuncSeparateINGR == NULL && glad_glBlendFuncSeparateEXT != NULL) glad_glBlendFuncSeparateINGR = (PFNGLBLENDFUNCSEPARATEINGRPROC)glad_glBlendFuncSeparateEXT;
    if (glad_glBlitFramebuffer == NULL && glad_glBlitFramebufferEXT != NULL) glad_glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)glad_glBlitFramebufferEXT;
    if (glad_glBlitFramebufferEXT == NULL && glad_glBlitFramebuffer != NULL) glad_glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC)glad_glBlitFramebuffer;
    if (glad_glBufferData == NULL && glad_glBufferDataARB != NULL) glad_glBufferData = (PFNGLBUFFERDATAPROC)glad_glBufferDataARB;
    if (glad_glBufferDataARB == NULL && glad_glBufferData != NULL) glad_glBufferDataARB = (PFNGLBUFFERDATAARBPROC)glad_glBufferData;
    if (glad_glBufferSubData == NULL && glad_glBufferSubDataARB != NULL) glad_glBufferSubData = (PFNGLBUFFERSUBDATAPROC)glad_glBufferSubDataARB;
    if (glad_glBufferSubDataARB == NULL && glad_glBufferSubData != NULL) glad_glBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC)glad_glBufferSubData;
    if (glad_glCheckFramebufferStatus == NULL && glad_glCheckFramebufferStatusEXT != NULL) glad_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glad_glCheckFramebufferStatusEXT;
    if (glad_glCheckFramebufferStatusEXT == NULL && glad_glCheckFramebufferStatus != NULL) glad_glCheckFramebufferStatusEXT = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)glad_glCheckFramebufferStatus;
    if (glad_glClampColor == NULL && glad_glClampColorARB != NULL) glad_glClampColor = (PFNGLCLAMPCOLORPROC)glad_glClampColorARB;
    if (glad_glClampColorARB == NULL && glad_glClampColor != NULL) glad_glClampColorARB = (PFNGLCLAMPCOLORARBPROC)glad_glClampColor;
    if (glad_glColorMaski == NULL && glad_glColorMaskIndexedEXT != NULL) glad_glColorMaski = (PFNGLCOLORMASKIPROC)glad_glColorMaskIndexedEXT;
    if (glad_glColorMaskIndexedEXT == NULL && glad_glColorMaski != NULL) glad_glColorMaskIndexedEXT = (PFNGLCOLORMASKINDEXEDEXTPROC)glad_glColorMaski;
    if (glad_glCompileShader == NULL && glad_glCompileShaderARB != NULL) glad_glCompileShader = (PFNGLCOMPILESHADERPROC)glad_glCompileShaderARB;
    if (glad_glCompileShaderARB == NULL && glad_glCompileShader != NULL) glad_glCompileShaderARB = (PFNGLCOMPILESHADERARBPROC)glad_glCompileShader;
    if (glad_glCompressedTexImage1D == NULL && glad_glCompressedTexImage1DARB != NULL) glad_glCompressedTexImage1D = (PFNGLCOMPRESSEDTEXIMAGE1DPROC)glad_glCompressedTexImage1DARB;
    if (glad_glCompressedTexImage1DARB == NULL && glad_glCompressedTexImage1D != NULL) glad_glCompressedTexImage1DARB = (PFNGLCOMPRESSEDTEXIMAGE1DARBPROC)glad_glCompressedTexImage1D;
    if (glad_glCompressedTexImage2D == NULL && glad_glCompressedTexImage2DARB != NULL) glad_glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)glad_glCompressedTexImage2DARB;
    if (glad_glCompressedTexImage2DARB == NULL && glad_glCompressedTexImage2D != NULL) glad_glCompressedTexImage2DARB = (PFNGLCOMPRESSEDTEXIMAGE2DARBPROC)glad_glCompressedTexImage2D;
    if (glad_glCompressedTexImage3D == NULL && glad_glCompressedTexImage3DARB != NULL) glad_glCompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC)glad_glCompressedTexImage3DARB;
    if (glad_glCompressedTexImage3DARB == NULL && glad_glCompressedTexImage3D != NULL) glad_glCompressedTexImage3DARB = (PFNGLCOMPRESSEDTEXIMAGE3DARBPROC)glad_glCompressedTexImage3D;
    if (glad_glCompressedTexSubImage1D == NULL && glad_glCompressedTexSubImage1DARB != NULL) glad_glCompressedTexSubImage1D = (PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)glad_glCompressedTexSubImage1DARB;
    if (glad_glCompressedTexSubImage1DARB == NULL && glad_glCompressedTexSubImage1D != NULL) glad_glCompressedTexSubImage1DARB = (PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC)glad_glCompressedTexSubImage1D;
    if (glad_glCompressedTexSubImage2D == NULL && glad_glCompressedTexSubImage2DARB != NULL) glad_glCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)glad_glCompressedTexSubImage2DARB;
    if (glad_glCompressedTexSubImage2DARB == NULL && glad_glCompressedTexSubImage2D != NULL) glad_glCompressedTexSubImage2DARB = (PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC)glad_glCompressedTexSubImage2D;
    if (glad_glCompressedTexSubImage3D == NULL && glad_glCompressedTexSubImage3DARB != NULL) glad_glCompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)glad_glCompressedTexSubImage3DARB;
    if (glad_glCompressedTexSubImage3DARB == NULL && glad_glCompressedTexSubImage3D != NULL) glad_glCompressedTexSubImage3DARB = (PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC)glad_glCompressedTexSubImage3D;
    if (glad_glCopyTexImage1D == NULL && glad_glCopyTexImage1DEXT != NULL) glad_glCopyTexImage1D = (PFNGLCOPYTEXIMAGE1DPROC)glad_glCopyTexImage1DEXT;
    if (glad_glCopyTexImage1DEXT == NULL && glad_glCopyTexImage1D != NULL) glad_glCopyTexImage1DEXT = (PFNGLCOPYTEXIMAGE1DEXTPROC)glad_glCopyTexImage1D;
    if (glad_glCopyTexImage2D == NULL && glad_glCopyTexImage2DEXT != NULL) glad_glCopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC)glad_glCopyTexImage2DEXT;
    if (glad_glCopyTexImage2DEXT == NULL && glad_glCopyTexImage2D != NULL) glad_glCopyTexImage2DEXT = (PFNGLCOPYTEXIMAGE2DEXTPROC)glad_glCopyTexImage2D;
    if (glad_glCopyTexSubImage1D == NULL && glad_glCopyTexSubImage1DEXT != NULL) glad_glCopyTexSubImage1D = (PFNGLCOPYTEXSUBIMAGE1DPROC)glad_glCopyTexSubImage1DEXT;
    if (glad_glCopyTexSubImage1DEXT == NULL && glad_glCopyTexSubImage1D != NULL) glad_glCopyTexSubImage1DEXT = (PFNGLCOPYTEXSUBIMAGE1DEXTPROC)glad_glCopyTexSubImage1D;
    if (glad_glCopyTexSubImage2D == NULL && glad_glCopyTexSubImage2DEXT != NULL) glad_glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC)glad_glCopyTexSubImage2DEXT;
    if (glad_glCopyTexSubImage2DEXT == NULL && glad_glCopyTexSubImage2D != NULL) glad_glCopyTexSubImage2DEXT = (PFNGLCOPYTEXSUBIMAGE2DEXTPROC)glad_glCopyTexSubImage2D;
    if (glad_glCopyTexSubImage3D == NULL && glad_glCopyTexSubImage3DEXT != NULL) glad_glCopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC)glad_glCopyTexSubImage3DEXT;
    if (glad_glCopyTexSubImage3DEXT == NULL && glad_glCopyTexSubImage3D != NULL) glad_glCopyTexSubImage3DEXT = (PFNGLCOPYTEXSUBIMAGE3DEXTPROC)glad_glCopyTexSubImage3D;
    if (glad_glCreateProgram == NULL && glad_glCreateProgramObjectARB != NULL) glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC)glad_glCreateProgramObjectARB;
    if (glad_glCreateProgramObjectARB == NULL && glad_glCreateProgram != NULL) glad_glCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC)glad_glCreateProgram;
    if (glad_glCreateShader == NULL && glad_glCreateShaderObjectARB != NULL) glad_glCreateShader = (PFNGLCREATESHADERPROC)glad_glCreateShaderObjectARB;
    if (glad_glCreateShaderObjectARB == NULL && glad_glCreateShader != NULL) glad_glCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC)glad_glCreateShader;
    if (glad_glDeleteBuffers == NULL && glad_glDeleteBuffersARB != NULL) glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)glad_glDeleteBuffersARB;
    if (glad_glDeleteBuffersARB == NULL && glad_glDeleteBuffers != NULL) glad_glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)glad_glDeleteBuffers;
    if (glad_glDeleteFramebuffers == NULL && glad_glDeleteFramebuffersEXT != NULL) glad_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)glad_glDeleteFramebuffersEXT;
    if (glad_glDeleteFramebuffersEXT == NULL && glad_glDeleteFramebuffers != NULL) glad_glDeleteFramebuffersEXT = (PFNGLDELETEFRAMEBUFFERSEXTPROC)glad_glDeleteFramebuffers;
    if (glad_glDeleteProgramsARB == NULL && glad_glDeleteProgramsNV != NULL) glad_glDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC)glad_glDeleteProgramsNV;
    if (glad_glDeleteProgramsNV == NULL && glad_glDeleteProgramsARB != NULL) glad_glDeleteProgramsNV = (PFNGLDELETEPROGRAMSNVPROC)glad_glDeleteProgramsARB;
    if (glad_glDeleteQueries == NULL && glad_glDeleteQueriesARB != NULL) glad_glDeleteQueries = (PFNGLDELETEQUERIESPROC)glad_glDeleteQueriesARB;
    if (glad_glDeleteQueriesARB == NULL && glad_glDeleteQueries != NULL) glad_glDeleteQueriesARB = (PFNGLDELETEQUERIESARBPROC)glad_glDeleteQueries;
    if (glad_glDeleteRenderbuffers == NULL && glad_glDeleteRenderbuffersEXT != NULL) glad_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)glad_glDeleteRenderbuffersEXT;
    if (glad_glDeleteRenderbuffersEXT == NULL && glad_glDeleteRenderbuffers != NULL) glad_glDeleteRenderbuffersEXT = (PFNGLDELETERENDERBUFFERSEXTPROC)glad_glDeleteRenderbuffers;
    if (glad_glDeleteVertexArrays == NULL && glad_glDeleteVertexArraysAPPLE != NULL) glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)glad_glDeleteVertexArraysAPPLE;
    if (glad_glDeleteVertexArraysAPPLE == NULL && glad_glDeleteVertexArrays != NULL) glad_glDeleteVertexArraysAPPLE = (PFNGLDELETEVERTEXARRAYSAPPLEPROC)glad_glDeleteVertexArrays;
    if (glad_glDetachObjectARB == NULL && glad_glDetachShader != NULL) glad_glDetachObjectARB = (PFNGLDETACHOBJECTARBPROC)glad_glDetachShader;
    if (glad_glDetachShader == NULL && glad_glDetachObjectARB != NULL) glad_glDetachShader = (PFNGLDETACHSHADERPROC)glad_glDetachObjectARB;
    if (glad_glDisablei == NULL && glad_glDisableIndexedEXT != NULL) glad_glDisablei = (PFNGLDISABLEIPROC)glad_glDisableIndexedEXT;
    if (glad_glDisableIndexedEXT == NULL && glad_glDisablei != NULL) glad_glDisableIndexedEXT = (PFNGLDISABLEINDEXEDEXTPROC)glad_glDisablei;
    if (glad_glDisableVertexAttribArray == NULL && glad_glDisableVertexAttribArrayARB != NULL) glad_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)glad_glDisableVertexAttribArrayARB;
    if (glad_glDisableVertexAttribArrayARB == NULL && glad_glDisableVertexAttribArray != NULL) glad_glDisableVertexAttribArrayARB = (PFNGLDISABLEVERTEXATTRIBARRAYARBPROC)glad_glDisableVertexAttribArray;
    if (glad_glDrawArrays == NULL && glad_glDrawArraysEXT != NULL) glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)glad_glDrawArraysEXT;
    if (glad_glDrawArraysEXT == NULL && glad_glDrawArrays != NULL) glad_glDrawArraysEXT = (PFNGLDRAWARRAYSEXTPROC)glad_glDrawArrays;
    if (glad_glDrawArraysInstanced == NULL && glad_glDrawArraysInstancedARB != NULL) glad_glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)glad_glDrawArraysInstancedARB;
    if (glad_glDrawArraysInstanced == NULL && glad_glDrawArraysInstancedEXT != NULL) glad_glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)glad_glDrawArraysInstancedEXT;
    if (glad_glDrawArraysInstancedARB == NULL && glad_glDrawArraysInstanced != NULL) glad_glDrawArraysInstancedARB = (PFNGLDRAWARRAYSINSTANCEDARBPROC)glad_glDrawArraysInstanced;
    if (glad_glDrawArraysInstancedARB == NULL && glad_glDrawArraysInstancedEXT != NULL) glad_glDrawArraysInstancedARB = (PFNGLDRAWARRAYSINSTANCEDARBPROC)glad_glDrawArraysInstancedEXT;
    if (glad_glDrawArraysInstancedEXT == NULL && glad_glDrawArraysInstanced != NULL) glad_glDrawArraysInstancedEXT = (PFNGLDRAWARRAYSINSTANCEDEXTPROC)glad_glDrawArraysInstanced;
    if (glad_glDrawArraysInstancedEXT == NULL && glad_glDrawArraysInstancedARB != NULL) glad_glDrawArraysInstancedEXT = (PFNGLDRAWARRAYSINSTANCEDEXTPROC)glad_glDrawArraysInstancedARB;
    if (glad_glDrawBuffers == NULL && glad_glDrawBuffersARB != NULL) glad_glDrawBuffers = (PFNGLDRAWBUFFERSPROC)glad_glDrawBuffersARB;
    if (glad_glDrawBuffers == NULL && glad_glDrawBuffersATI != NULL) glad_glDrawBuffers = (PFNGLDRAWBUFFERSPROC)glad_glDrawBuffersATI;
    if (glad_glDrawBuffersARB == NULL && glad_glDrawBuffers != NULL) glad_glDrawBuffersARB = (PFNGLDRAWBUFFERSARBPROC)glad_glDrawBuffers;
    if (glad_glDrawBuffersARB == NULL && glad_glDrawBuffersATI != NULL) glad_glDrawBuffersARB = (PFNGLDRAWBUFFERSARBPROC)glad_glDrawBuffersATI;
    if (glad_glDrawBuffersATI == NULL && glad_glDrawBuffers != NULL) glad_glDrawBuffersATI = (PFNGLDRAWBUFFERSATIPROC)glad_glDrawBuffers;
    if (glad_glDrawBuffersATI == NULL && glad_glDrawBuffersARB != NULL) glad_glDrawBuffersATI = (PFNGLDRAWBUFFERSATIPROC)glad_glDrawBuffersARB;
    if (glad_glDrawElementsInstanced == NULL && glad_glDrawElementsInstancedARB != NULL) glad_glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC)glad_glDrawElementsInstancedARB;
    if (glad_glDrawElementsInstanced == NULL && glad_glDrawElementsInstancedEXT != NULL) glad_glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC)glad_glDrawElementsInstancedEXT;
    if (glad_glDrawElementsInstancedARB == NULL && glad_glDrawElementsInstanced != NULL) glad_glDrawElementsInstancedARB = (PFNGLDRAWELEMENTSINSTANCEDARBPROC)glad_glDrawElementsInstanced;
    if (glad_glDrawElementsInstancedARB == NULL && glad_glDrawElementsInstancedEXT != NULL) glad_glDrawElementsInstancedARB = (PFNGLDRAWELEMENTSINSTANCEDARBPROC)glad_glDrawElementsInstancedEXT;
    if (glad_glDrawElementsInstancedEXT == NULL && glad_glDrawElementsInstanced != NULL) glad_glDrawElementsInstancedEXT = (PFNGLDRAWELEMENTSINSTANCEDEXTPROC)glad_glDrawElementsInstanced;
    if (glad_glDrawElementsInstancedEXT == NULL && glad_glDrawElementsInstancedARB != NULL) glad_glDrawElementsInstancedEXT = (PFNGLDRAWELEMENTSINSTANCEDEXTPROC)glad_glDrawElementsInstancedARB;
    if (glad_glDrawRangeElements == NULL && glad_glDrawRangeElementsEXT != NULL) glad_glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC)glad_glDrawRangeElementsEXT;
    if (glad_glDrawRangeElementsEXT == NULL && glad_glDrawRangeElements != NULL) glad_glDrawRangeElementsEXT = (PFNGLDRAWRANGEELEMENTSEXTPROC)glad_glDrawRangeElements;
    if (glad_glEnablei == NULL && glad_glEnableIndexedEXT != NULL) glad_glEnablei = (PFNGLENABLEIPROC)glad_glEnableIndexedEXT;
    if (glad_glEnableIndexedEXT == NULL && glad_glEnablei != NULL) glad_glEnableIndexedEXT = (PFNGLENABLEINDEXEDEXTPROC)glad_glEnablei;
    if (glad_glEnableVertexAttribArray == NULL && glad_glEnableVertexAttribArrayARB != NULL) glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)glad_glEnableVertexAttribArrayARB;
    if (glad_glEnableVertexAttribArrayARB == NULL && glad_glEnableVertexAttribArray != NULL) glad_glEnableVertexAttribArrayARB = (PFNGLENABLEVERTEXATTRIBARRAYARBPROC)glad_glEnableVertexAttribArray;
    if (glad_glEndConditionalRender == NULL && glad_glEndConditionalRenderNV != NULL) glad_glEndConditionalRender = (PFNGLENDCONDITIONALRENDERPROC)glad_glEndConditionalRenderNV;
    if (glad_glEndConditionalRender == NULL && glad_glEndConditionalRenderNVX != NULL) glad_glEndConditionalRender = (PFNGLENDCONDITIONALRENDERPROC)glad_glEndConditionalRenderNVX;
    if (glad_glEndConditionalRenderNV == NULL && glad_glEndConditionalRender != NULL) glad_glEndConditionalRenderNV = (PFNGLENDCONDITIONALRENDERNVPROC)glad_glEndConditionalRender;
    if (glad_glEndConditionalRenderNV == NULL && glad_glEndConditionalRenderNVX != NULL) glad_glEndConditionalRenderNV = (PFNGLENDCONDITIONALRENDERNVPROC)glad_glEndConditionalRenderNVX;
    if (glad_glEndConditionalRenderNVX == NULL && glad_glEndConditionalRender != NULL) glad_glEndConditionalRenderNVX = (PFNGLENDCONDITIONALRENDERNVXPROC)glad_glEndConditionalRender;
    if (glad_glEndConditionalRenderNVX == NULL && glad_glEndConditionalRenderNV != NULL) glad_glEndConditionalRenderNVX = (PFNGLENDCONDITIONALRENDERNVXPROC)glad_glEndConditionalRenderNV;
    if (glad_glEndQuery == NULL && glad_glEndQueryARB != NULL) glad_glEndQuery = (PFNGLENDQUERYPROC)glad_glEndQueryARB;
    if (glad_glEndQueryARB == NULL && glad_glEndQuery != NULL) glad_glEndQueryARB = (PFNGLENDQUERYARBPROC)glad_glEndQuery;
    if (glad_glEndTransformFeedback == NULL && glad_glEndTransformFeedbackEXT != NULL) glad_glEndTransformFeedback = (PFNGLENDTRANSFORMFEEDBACKPROC)glad_glEndTransformFeedbackEXT;
    if (glad_glEndTransformFeedback == NULL && glad_glEndTransformFeedbackNV != NULL) glad_glEndTransformFeedback = (PFNGLENDTRANSFORMFEEDBACKPROC)glad_glEndTransformFeedbackNV;
    if (glad_glEndTransformFeedbackEXT == NULL && glad_glEndTransformFeedback != NULL) glad_glEndTransformFeedbackEXT = (PFNGLENDTRANSFORMFEEDBACKEXTPROC)glad_glEndTransformFeedback;
    if (glad_glEndTransformFeedbackEXT == NULL && glad_glEndTransformFeedbackNV != NULL) glad_glEndTransformFeedbackEXT = (PFNGLENDTRANSFORMFEEDBACKEXTPROC)glad_glEndTransformFeedbackNV;
    if (glad_glEndTransformFeedbackNV == NULL && glad_glEndTransformFeedback != NULL) glad_glEndTransformFeedbackNV = (PFNGLENDTRANSFORMFEEDBACKNVPROC)glad_glEndTransformFeedback;
    if (glad_glEndTransformFeedbackNV == NULL && glad_glEndTransformFeedbackEXT != NULL) glad_glEndTransformFeedbackNV = (PFNGLENDTRANSFORMFEEDBACKNVPROC)glad_glEndTransformFeedbackEXT;
    if (glad_glFlushMappedBufferRange == NULL && glad_glFlushMappedBufferRangeAPPLE != NULL) glad_glFlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC)glad_glFlushMappedBufferRangeAPPLE;
    if (glad_glFlushMappedBufferRangeAPPLE == NULL && glad_glFlushMappedBufferRange != NULL) glad_glFlushMappedBufferRangeAPPLE = (PFNGLFLUSHMAPPEDBUFFERRANGEAPPLEPROC)glad_glFlushMappedBufferRange;
    if (glad_glFramebufferRenderbuffer == NULL && glad_glFramebufferRenderbufferEXT != NULL) glad_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)glad_glFramebufferRenderbufferEXT;
    if (glad_glFramebufferRenderbufferEXT == NULL && glad_glFramebufferRenderbuffer != NULL) glad_glFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)glad_glFramebufferRenderbuffer;
    if (glad_glFramebufferTexture == NULL && glad_glFramebufferTextureARB != NULL) glad_glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC)glad_glFramebufferTextureARB;
    if (glad_glFramebufferTexture == NULL && glad_glFramebufferTextureEXT != NULL) glad_glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC)glad_glFramebufferTextureEXT;
    if (glad_glFramebufferTexture1D == NULL && glad_glFramebufferTexture1DEXT != NULL) glad_glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC)glad_glFramebufferTexture1DEXT;
    if (glad_glFramebufferTexture1DEXT == NULL && glad_glFramebufferTexture1D != NULL) glad_glFramebufferTexture1DEXT = (PFNGLFRAMEBUFFERTEXTURE1DEXTPROC)glad_glFramebufferTexture1D;
    if (glad_glFramebufferTexture2D == NULL && glad_glFramebufferTexture2DEXT != NULL) glad_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glad_glFramebufferTexture2DEXT;
    if (glad_glFramebufferTexture2DEXT == NULL && glad_glFramebufferTexture2D != NULL) glad_glFramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)glad_glFramebufferTexture2D;
    if (glad_glFramebufferTexture3D == NULL && glad_glFramebufferTexture3DEXT != NULL) glad_glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC)glad_glFramebufferTexture3DEXT;
    if (glad_glFramebufferTexture3DEXT == NULL && glad_glFramebufferTexture3D != NULL) glad_glFramebufferTexture3DEXT = (PFNGLFRAMEBUFFERTEXTURE3DEXTPROC)glad_glFramebufferTexture3D;
    if (glad_glFramebufferTextureARB == NULL && glad_glFramebufferTexture != NULL) glad_glFramebufferTextureARB = (PFNGLFRAMEBUFFERTEXTUREARBPROC)glad_glFramebufferTexture;
    if (glad_glFramebufferTextureARB == NULL && glad_glFramebufferTextureEXT != NULL) glad_glFramebufferTextureARB = (PFNGLFRAMEBUFFERTEXTUREARBPROC)glad_glFramebufferTextureEXT;
    if (glad_glFramebufferTextureEXT == NULL && glad_glFramebufferTexture != NULL) glad_glFramebufferTextureEXT = (PFNGLFRAMEBUFFERTEXTUREEXTPROC)glad_glFramebufferTexture;
    if (glad_glFramebufferTextureEXT == NULL && glad_glFramebufferTextureARB != NULL) glad_glFramebufferTextureEXT = (PFNGLFRAMEBUFFERTEXTUREEXTPROC)glad_glFramebufferTextureARB;
    if (glad_glFramebufferTextureFaceARB == NULL && glad_glFramebufferTextureFaceEXT != NULL) glad_glFramebufferTextureFaceARB = (PFNGLFRAMEBUFFERTEXTUREFACEARBPROC)glad_glFramebufferTextureFaceEXT;
    if (glad_glFramebufferTextureFaceEXT == NULL && glad_glFramebufferTextureFaceARB != NULL) glad_glFramebufferTextureFaceEXT = (PFNGLFRAMEBUFFERTEXTUREFACEEXTPROC)glad_glFramebufferTextureFaceARB;
    if (glad_glFramebufferTextureLayer == NULL && glad_glFramebufferTextureLayerARB != NULL) glad_glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC)glad_glFramebufferTextureLayerARB;
    if (glad_glFramebufferTextureLayer == NULL && glad_glFramebufferTextureLayerEXT != NULL) glad_glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC)glad_glFramebufferTextureLayerEXT;
    if (glad_glFramebufferTextureLayerARB == NULL && glad_glFramebufferTextureLayer != NULL) glad_glFramebufferTextureLayerARB = (PFNGLFRAMEBUFFERTEXTURELAYERARBPROC)glad_glFramebufferTextureLayer;
    if (glad_glFramebufferTextureLayerARB == NULL && glad_glFramebufferTextureLayerEXT != NULL) glad_glFramebufferTextureLayerARB = (PFNGLFRAMEBUFFERTEXTURELAYERARBPROC)glad_glFramebufferTextureLayerEXT;
    if (glad_glFramebufferTextureLayerEXT == NULL && glad_glFramebufferTextureLayer != NULL) glad_glFramebufferTextureLayerEXT = (PFNGLFRAMEBUFFERTEXTURELAYEREXTPROC)glad_glFramebufferTextureLayer;
    if (glad_glFramebufferTextureLayerEXT == NULL && glad_glFramebufferTextureLayerARB != NULL) glad_glFramebufferTextureLayerEXT = (PFNGLFRAMEBUFFERTEXTURELAYEREXTPROC)glad_glFramebufferTextureLayerARB;
    if (glad_glGenBuffers == NULL && glad_glGenBuffersARB != NULL) glad_glGenBuffers = (PFNGLGENBUFFERSPROC)glad_glGenBuffersARB;
    if (glad_glGenBuffersARB == NULL && glad_glGenBuffers != NULL) glad_glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)glad_glGenBuffers;
    if (glad_glGenerateMipmap == NULL && glad_glGenerateMipmapEXT != NULL) glad_glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)glad_glGenerateMipmapEXT;
    if (glad_glGenerateMipmapEXT == NULL && glad_glGenerateMipmap != NULL) glad_glGenerateMipmapEXT = (PFNGLGENERATEMIPMAPEXTPROC)glad_glGenerateMipmap;
    if (glad_glGenFramebuffers == NULL && glad_glGenFramebuffersEXT != NULL) glad_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)glad_glGenFramebuffersEXT;
    if (glad_glGenFramebuffersEXT == NULL && glad_glGenFramebuffers != NULL) glad_glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC)glad_glGenFramebuffers;
    if (glad_glGenProgramsARB == NULL && glad_glGenProgramsNV != NULL) glad_glGenProgramsARB = (PFNGLGENPROGRAMSARBPROC)glad_glGenProgramsNV;
    if (glad_glGenProgramsNV == NULL && glad_glGenProgramsARB != NULL) glad_glGenProgramsNV = (PFNGLGENPROGRAMSNVPROC)glad_glGenProgramsARB;
    if (glad_glGenQueries == NULL && glad_glGenQueriesARB != NULL) glad_glGenQueries = (PFNGLGENQUERIESPROC)glad_glGenQueriesARB;
    if (glad_glGenQueriesARB == NULL && glad_glGenQueries != NULL) glad_glGenQueriesARB = (PFNGLGENQUERIESARBPROC)glad_glGenQueries;
    if (glad_glGenRenderbuffers == NULL && glad_glGenRenderbuffersEXT != NULL) glad_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)glad_glGenRenderbuffersEXT;
    if (glad_glGenRenderbuffersEXT == NULL && glad_glGenRenderbuffers != NULL) glad_glGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC)glad_glGenRenderbuffers;
    if (glad_glGenVertexArrays == NULL && glad_glGenVertexArraysAPPLE != NULL) glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)glad_glGenVertexArraysAPPLE;
    if (glad_glGenVertexArraysAPPLE == NULL && glad_glGenVertexArrays != NULL) glad_glGenVertexArraysAPPLE = (PFNGLGENVERTEXARRAYSAPPLEPROC)glad_glGenVertexArrays;
    if (glad_glGetActiveAttrib == NULL && glad_glGetActiveAttribARB != NULL) glad_glGetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC)glad_glGetActiveAttribARB;
    if (glad_glGetActiveAttribARB == NULL && glad_glGetActiveAttrib != NULL) glad_glGetActiveAttribARB = (PFNGLGETACTIVEATTRIBARBPROC)glad_glGetActiveAttrib;
    if (glad_glGetActiveUniform == NULL && glad_glGetActiveUniformARB != NULL) glad_glGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC)glad_glGetActiveUniformARB;
    if (glad_glGetActiveUniformARB == NULL && glad_glGetActiveUniform != NULL) glad_glGetActiveUniformARB = (PFNGLGETACTIVEUNIFORMARBPROC)glad_glGetActiveUniform;
    if (glad_glGetAttribLocation == NULL && glad_glGetAttribLocationARB != NULL) glad_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)glad_glGetAttribLocationARB;
    if (glad_glGetAttribLocationARB == NULL && glad_glGetAttribLocation != NULL) glad_glGetAttribLocationARB = (PFNGLGETATTRIBLOCATIONARBPROC)glad_glGetAttribLocation;
    if (glad_glGetBooleani_v == NULL && glad_glGetBooleanIndexedvEXT != NULL) glad_glGetBooleani_v = (PFNGLGETBOOLEANI_VPROC)glad_glGetBooleanIndexedvEXT;
    if (glad_glGetBooleanIndexedvEXT == NULL && glad_glGetBooleani_v != NULL) glad_glGetBooleanIndexedvEXT = (PFNGLGETBOOLEANINDEXEDVEXTPROC)glad_glGetBooleani_v;
    if (glad_glGetBufferParameteriv == NULL && glad_glGetBufferParameterivARB != NULL) glad_glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC)glad_glGetBufferParameterivARB;
    if (glad_glGetBufferParameterivARB == NULL && glad_glGetBufferParameteriv != NULL) glad_glGetBufferParameterivARB = (PFNGLGETBUFFERPARAMETERIVARBPROC)glad_glGetBufferParameteriv;
    if (glad_glGetBufferPointerv == NULL && glad_glGetBufferPointervARB != NULL) glad_glGetBufferPointerv = (PFNGLGETBUFFERPOINTERVPROC)glad_glGetBufferPointervARB;
    if (glad_glGetBufferPointervARB == NULL && glad_glGetBufferPointerv != NULL) glad_glGetBufferPointervARB = (PFNGLGETBUFFERPOINTERVARBPROC)glad_glGetBufferPointerv;
    if (glad_glGetBufferSubData == NULL && glad_glGetBufferSubDataARB != NULL) glad_glGetBufferSubData = (PFNGLGETBUFFERSUBDATAPROC)glad_glGetBufferSubDataARB;
    if (glad_glGetBufferSubDataARB == NULL && glad_glGetBufferSubData != NULL) glad_glGetBufferSubDataARB = (PFNGLGETBUFFERSUBDATAARBPROC)glad_glGetBufferSubData;
    if (glad_glGetCompressedTexImage == NULL && glad_glGetCompressedTexImageARB != NULL) glad_glGetCompressedTexImage = (PFNGLGETCOMPRESSEDTEXIMAGEPROC)glad_glGetCompressedTexImageARB;
    if (glad_glGetCompressedTexImageARB == NULL && glad_glGetCompressedTexImage != NULL) glad_glGetCompressedTexImageARB = (PFNGLGETCOMPRESSEDTEXIMAGEARBPROC)glad_glGetCompressedTexImage;
    if (glad_glGetFragDataLocation == NULL && glad_glGetFragDataLocationEXT != NULL) glad_glGetFragDataLocation = (PFNGLGETFRAGDATALOCATIONPROC)glad_glGetFragDataLocationEXT;
    if (glad_glGetFragDataLocationEXT == NULL && glad_glGetFragDataLocation != NULL) glad_glGetFragDataLocationEXT = (PFNGLGETFRAGDATALOCATIONEXTPROC)glad_glGetFragDataLocation;
    if (glad_glGetFramebufferAttachmentParameteriv == NULL && glad_glGetFramebufferAttachmentParameterivEXT != NULL) glad_glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)glad_glGetFramebufferAttachmentParameterivEXT;
    if (glad_glGetFramebufferAttachmentParameterivEXT == NULL && glad_glGetFramebufferAttachmentParameteriv != NULL) glad_glGetFramebufferAttachmentParameterivEXT = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC)glad_glGetFramebufferAttachmentParameteriv;
    if (glad_glGetIntegeri_v == NULL && glad_glGetIntegerIndexedvEXT != NULL) glad_glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC)glad_glGetIntegerIndexedvEXT;
    if (glad_glGetIntegerIndexedvEXT == NULL && glad_glGetIntegeri_v != NULL) glad_glGetIntegerIndexedvEXT = (PFNGLGETINTEGERINDEXEDVEXTPROC)glad_glGetIntegeri_v;
    if (glad_glGetMultisamplefv == NULL && glad_glGetMultisamplefvNV != NULL) glad_glGetMultisamplefv = (PFNGLGETMULTISAMPLEFVPROC)glad_glGetMultisamplefvNV;
    if (glad_glGetMultisamplefvNV == NULL && glad_glGetMultisamplefv != NULL) glad_glGetMultisamplefvNV = (PFNGLGETMULTISAMPLEFVNVPROC)glad_glGetMultisamplefv;
    if (glad_glGetQueryiv == NULL && glad_glGetQueryivARB != NULL) glad_glGetQueryiv = (PFNGLGETQUERYIVPROC)glad_glGetQueryivARB;
    if (glad_glGetQueryivARB == NULL && glad_glGetQueryiv != NULL) glad_glGetQueryivARB = (PFNGLGETQUERYIVARBPROC)glad_glGetQueryiv;
    if (glad_glGetQueryObjecti64v == NULL && glad_glGetQueryObjecti64vEXT != NULL) glad_glGetQueryObjecti64v = (PFNGLGETQUERYOBJECTI64VPROC)glad_glGetQueryObjecti64vEXT;
    if (glad_glGetQueryObjecti64vEXT == NULL && glad_glGetQueryObjecti64v != NULL) glad_glGetQueryObjecti64vEXT = (PFNGLGETQUERYOBJECTI64VEXTPROC)glad_glGetQueryObjecti64v;
    if (glad_glGetQueryObjectiv == NULL && glad_glGetQueryObjectivARB != NULL) glad_glGetQueryObjectiv = (PFNGLGETQUERYOBJECTIVPROC)glad_glGetQueryObjectivARB;
    if (glad_glGetQueryObjectivARB == NULL && glad_glGetQueryObjectiv != NULL) glad_glGetQueryObjectivARB = (PFNGLGETQUERYOBJECTIVARBPROC)glad_glGetQueryObjectiv;
    if (glad_glGetQueryObjectui64v == NULL && glad_glGetQueryObjectui64vEXT != NULL) glad_glGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VPROC)glad_glGetQueryObjectui64vEXT;
    if (glad_glGetQueryObjectui64vEXT == NULL && glad_glGetQueryObjectui64v != NULL) glad_glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)glad_glGetQueryObjectui64v;
    if (glad_glGetQueryObjectuiv == NULL && glad_glGetQueryObjectuivARB != NULL) glad_glGetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC)glad_glGetQueryObjectuivARB;
    if (glad_glGetQueryObjectuivARB == NULL && glad_glGetQueryObjectuiv != NULL) glad_glGetQueryObjectuivARB = (PFNGLGETQUERYOBJECTUIVARBPROC)glad_glGetQueryObjectuiv;
    if (glad_glGetRenderbufferParameteriv == NULL && glad_glGetRenderbufferParameterivEXT != NULL) glad_glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)glad_glGetRenderbufferParameterivEXT;
    if (glad_glGetRenderbufferParameterivEXT == NULL && glad_glGetRenderbufferParameteriv != NULL) glad_glGetRenderbufferParameterivEXT = (PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC)glad_glGetRenderbufferParameteriv;
    if (glad_glGetShaderSource == NULL && glad_glGetShaderSourceARB != NULL) glad_glGetShaderSource = (PFNGLGETSHADERSOURCEPROC)glad_glGetShaderSourceARB;
    if (glad_glGetShaderSourceARB == NULL && glad_glGetShaderSource != NULL) glad_glGetShaderSourceARB = (PFNGLGETSHADERSOURCEARBPROC)glad_glGetShaderSource;
    if (glad_glGetTexParameterIiv == NULL && glad_glGetTexParameterIivEXT != NULL) glad_glGetTexParameterIiv = (PFNGLGETTEXPARAMETERIIVPROC)glad_glGetTexParameterIivEXT;
    if (glad_glGetTexParameterIivEXT == NULL && glad_glGetTexParameterIiv != NULL) glad_glGetTexParameterIivEXT = (PFNGLGETTEXPARAMETERIIVEXTPROC)glad_glGetTexParameterIiv;
    if (glad_glGetTexParameterIuiv == NULL && glad_glGetTexParameterIuivEXT != NULL) glad_glGetTexParameterIuiv = (PFNGLGETTEXPARAMETERIUIVPROC)glad_glGetTexParameterIuivEXT;
    if (glad_glGetTexParameterIuivEXT == NULL && glad_glGetTexParameterIuiv != NULL) glad_glGetTexParameterIuivEXT = (PFNGLGETTEXPARAMETERIUIVEXTPROC)glad_glGetTexParameterIuiv;
    if (glad_glGetTransformFeedbackVarying == NULL && glad_glGetTransformFeedbackVaryingEXT != NULL) glad_glGetTransformFeedbackVarying = (PFNGLGETTRANSFORMFEEDBACKVARYINGPROC)glad_glGetTransformFeedbackVaryingEXT;
    if (glad_glGetTransformFeedbackVaryingEXT == NULL && glad_glGetTransformFeedbackVarying != NULL) glad_glGetTransformFeedbackVaryingEXT = (PFNGLGETTRANSFORMFEEDBACKVARYINGEXTPROC)glad_glGetTransformFeedbackVarying;
    if (glad_glGetUniformfv == NULL && glad_glGetUniformfvARB != NULL) glad_glGetUniformfv = (PFNGLGETUNIFORMFVPROC)glad_glGetUniformfvARB;
    if (glad_glGetUniformfvARB == NULL && glad_glGetUniformfv != NULL) glad_glGetUniformfvARB = (PFNGLGETUNIFORMFVARBPROC)glad_glGetUniformfv;
    if (glad_glGetUniformiv == NULL && glad_glGetUniformivARB != NULL) glad_glGetUniformiv = (PFNGLGETUNIFORMIVPROC)glad_glGetUniformivARB;
    if (glad_glGetUniformivARB == NULL && glad_glGetUniformiv != NULL) glad_glGetUniformivARB = (PFNGLGETUNIFORMIVARBPROC)glad_glGetUniformiv;
    if (glad_glGetUniformLocation == NULL && glad_glGetUniformLocationARB != NULL) glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)glad_glGetUniformLocationARB;
    if (glad_glGetUniformLocationARB == NULL && glad_glGetUniformLocation != NULL) glad_glGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC)glad_glGetUniformLocation;
    if (glad_glGetUniformuiv == NULL && glad_glGetUniformuivEXT != NULL) glad_glGetUniformuiv = (PFNGLGETUNIFORMUIVPROC)glad_glGetUniformuivEXT;
    if (glad_glGetUniformuivEXT == NULL && glad_glGetUniformuiv != NULL) glad_glGetUniformuivEXT = (PFNGLGETUNIFORMUIVEXTPROC)glad_glGetUniformuiv;
    if (glad_glGetVertexAttribdv == NULL && glad_glGetVertexAttribdvARB != NULL) glad_glGetVertexAttribdv = (PFNGLGETVERTEXATTRIBDVPROC)glad_glGetVertexAttribdvARB;
    if (glad_glGetVertexAttribdv == NULL && glad_glGetVertexAttribdvNV != NULL) glad_glGetVertexAttribdv = (PFNGLGETVERTEXATTRIBDVPROC)glad_glGetVertexAttribdvNV;
    if (glad_glGetVertexAttribdvARB == NULL && glad_glGetVertexAttribdv != NULL) glad_glGetVertexAttribdvARB = (PFNGLGETVERTEXATTRIBDVARBPROC)glad_glGetVertexAttribdv;
    if (glad_glGetVertexAttribdvARB == NULL && glad_glGetVertexAttribdvNV != NULL) glad_glGetVertexAttribdvARB = (PFNGLGETVERTEXATTRIBDVARBPROC)glad_glGetVertexAttribdvNV;
    if (glad_glGetVertexAttribdvNV == NULL && glad_glGetVertexAttribdv != NULL) glad_glGetVertexAttribdvNV = (PFNGLGETVERTEXATTRIBDVNVPROC)glad_glGetVertexAttribdv;
    if (glad_glGetVertexAttribdvNV == NULL && glad_glGetVertexAttribdvARB != NULL) glad_glGetVertexAttribdvNV = (PFNGLGETVERTEXATTRIBDVNVPROC)glad_glGetVertexAttribdvARB;
    if (glad_glGetVertexAttribfv == NULL && glad_glGetVertexAttribfvARB != NULL) glad_glGetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC)glad_glGetVertexAttribfvARB;
    if (glad_glGetVertexAttribfv == NULL && glad_glGetVertexAttribfvNV != NULL) glad_glGetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC)glad_glGetVertexAttribfvNV;
    if (glad_glGetVertexAttribfvARB == NULL && glad_glGetVertexAttribfv != NULL) glad_glGetVertexAttribfvARB = (PFNGLGETVERTEXATTRIBFVARBPROC)glad_glGetVertexAttribfv;
    if (glad_glGetVertexAttribfvARB == NULL && glad_glGetVertexAttribfvNV != NULL) glad_glGetVertexAttribfvARB = (PFNGLGETVERTEXATTRIBFVARBPROC)glad_glGetVertexAttribfvNV;
    if (glad_glGetVertexAttribfvNV == NULL && glad_glGetVertexAttribfv != NULL) glad_glGetVertexAttribfvNV = (PFNGLGETVERTEXATTRIBFVNVPROC)glad_glGetVertexAttribfv;
    if (glad_glGetVertexAttribfvNV == NULL && glad_glGetVertexAttribfvARB != NULL) glad_glGetVertexAttribfvNV = (PFNGLGETVERTEXATTRIBFVNVPROC)glad_glGetVertexAttribfvARB;
    if (glad_glGetVertexAttribIiv == NULL && glad_glGetVertexAttribIivEXT != NULL) glad_glGetVertexAttribIiv = (PFNGLGETVERTEXATTRIBIIVPROC)glad_glGetVertexAttribIivEXT;
    if (glad_glGetVertexAttribIivEXT == NULL && glad_glGetVertexAttribIiv != NULL) glad_glGetVertexAttribIivEXT = (PFNGLGETVERTEXATTRIBIIVEXTPROC)glad_glGetVertexAttribIiv;
    if (glad_glGetVertexAttribIuiv == NULL && glad_glGetVertexAttribIuivEXT != NULL) glad_glGetVertexAttribIuiv = (PFNGLGETVERTEXATTRIBIUIVPROC)glad_glGetVertexAttribIuivEXT;
    if (glad_glGetVertexAttribIuivEXT == NULL && glad_glGetVertexAttribIuiv != NULL) glad_glGetVertexAttribIuivEXT = (PFNGLGETVERTEXATTRIBIUIVEXTPROC)glad_glGetVertexAttribIuiv;
    if (glad_glGetVertexAttribiv == NULL && glad_glGetVertexAttribivARB != NULL) glad_glGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)glad_glGetVertexAttribivARB;
    if (glad_glGetVertexAttribiv == NULL && glad_glGetVertexAttribivNV != NULL) glad_glGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)glad_glGetVertexAttribivNV;
    if (glad_glGetVertexAttribivARB == NULL && glad_glGetVertexAttribiv != NULL) glad_glGetVertexAttribivARB = (PFNGLGETVERTEXATTRIBIVARBPROC)glad_glGetVertexAttribiv;
    if (glad_glGetVertexAttribivARB == NULL && glad_glGetVertexAttribivNV != NULL) glad_glGetVertexAttribivARB = (PFNGLGETVERTEXATTRIBIVARBPROC)glad_glGetVertexAttribivNV;
    if (glad_glGetVertexAttribivNV == NULL && glad_glGetVertexAttribiv != NULL) glad_glGetVertexAttribivNV = (PFNGLGETVERTEXATTRIBIVNVPROC)glad_glGetVertexAttribiv;
    if (glad_glGetVertexAttribivNV == NULL && glad_glGetVertexAttribivARB != NULL) glad_glGetVertexAttribivNV = (PFNGLGETVERTEXATTRIBIVNVPROC)glad_glGetVertexAttribivARB;
    if (glad_glGetVertexAttribPointerv == NULL && glad_glGetVertexAttribPointervARB != NULL) glad_glGetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC)glad_glGetVertexAttribPointervARB;
    if (glad_glGetVertexAttribPointerv == NULL && glad_glGetVertexAttribPointervNV != NULL) glad_glGetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC)glad_glGetVertexAttribPointervNV;
    if (glad_glGetVertexAttribPointervARB == NULL && glad_glGetVertexAttribPointerv != NULL) glad_glGetVertexAttribPointervARB = (PFNGLGETVERTEXATTRIBPOINTERVARBPROC)glad_glGetVertexAttribPointerv;
    if (glad_glGetVertexAttribPointervARB == NULL && glad_glGetVertexAttribPointervNV != NULL) glad_glGetVertexAttribPointervARB = (PFNGLGETVERTEXATTRIBPOINTERVARBPROC)glad_glGetVertexAttribPointervNV;
    if (glad_glGetVertexAttribPointervNV == NULL && glad_glGetVertexAttribPointerv != NULL) glad_glGetVertexAttribPointervNV = (PFNGLGETVERTEXATTRIBPOINTERVNVPROC)glad_glGetVertexAttribPointerv;
    if (glad_glGetVertexAttribPointervNV == NULL && glad_glGetVertexAttribPointervARB != NULL) glad_glGetVertexAttribPointervNV = (PFNGLGETVERTEXATTRIBPOINTERVNVPROC)glad_glGetVertexAttribPointervARB;
    if (glad_glIsBuffer == NULL && glad_glIsBufferARB != NULL) glad_glIsBuffer = (PFNGLISBUFFERPROC)glad_glIsBufferARB;
    if (glad_glIsBufferARB == NULL && glad_glIsBuffer != NULL) glad_glIsBufferARB = (PFNGLISBUFFERARBPROC)glad_glIsBuffer;
    if (glad_glIsEnabledi == NULL && glad_glIsEnabledIndexedEXT != NULL) glad_glIsEnabledi = (PFNGLISENABLEDIPROC)glad_glIsEnabledIndexedEXT;
    if (glad_glIsEnabledIndexedEXT == NULL && glad_glIsEnabledi != NULL) glad_glIsEnabledIndexedEXT = (PFNGLISENABLEDINDEXEDEXTPROC)glad_glIsEnabledi;
    if (glad_glIsFramebuffer == NULL && glad_glIsFramebufferEXT != NULL) glad_glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC)glad_glIsFramebufferEXT;
    if (glad_glIsFramebufferEXT == NULL && glad_glIsFramebuffer != NULL) glad_glIsFramebufferEXT = (PFNGLISFRAMEBUFFEREXTPROC)glad_glIsFramebuffer;
    if (glad_glIsProgramARB == NULL && glad_glIsProgramNV != NULL) glad_glIsProgramARB = (PFNGLISPROGRAMARBPROC)glad_glIsProgramNV;
    if (glad_glIsProgramNV == NULL && glad_glIsProgramARB != NULL) glad_glIsProgramNV = (PFNGLISPROGRAMNVPROC)glad_glIsProgramARB;
    if (glad_glIsQuery == NULL && glad_glIsQueryARB != NULL) glad_glIsQuery = (PFNGLISQUERYPROC)glad_glIsQueryARB;
    if (glad_glIsQueryARB == NULL && glad_glIsQuery != NULL) glad_glIsQueryARB = (PFNGLISQUERYARBPROC)glad_glIsQuery;
    if (glad_glIsRenderbuffer == NULL && glad_glIsRenderbufferEXT != NULL) glad_glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC)glad_glIsRenderbufferEXT;
    if (glad_glIsRenderbufferEXT == NULL && glad_glIsRenderbuffer != NULL) glad_glIsRenderbufferEXT = (PFNGLISRENDERBUFFEREXTPROC)glad_glIsRenderbuffer;
    if (glad_glIsVertexArray == NULL && glad_glIsVertexArrayAPPLE != NULL) glad_glIsVertexArray = (PFNGLISVERTEXARRAYPROC)glad_glIsVertexArrayAPPLE;
    if (glad_glIsVertexArrayAPPLE == NULL && glad_glIsVertexArray != NULL) glad_glIsVertexArrayAPPLE = (PFNGLISVERTEXARRAYAPPLEPROC)glad_glIsVertexArray;
    if (glad_glLinkProgram == NULL && glad_glLinkProgramARB != NULL) glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)glad_glLinkProgramARB;
    if (glad_glLinkProgramARB == NULL && glad_glLinkProgram != NULL) glad_glLinkProgramARB = (PFNGLLINKPROGRAMARBPROC)glad_glLinkProgram;
    if (glad_glMapBuffer == NULL && glad_glMapBufferARB != NULL) glad_glMapBuffer = (PFNGLMAPBUFFERPROC)glad_glMapBufferARB;
    if (glad_glMapBufferARB == NULL && glad_glMapBuffer != NULL) glad_glMapBufferARB = (PFNGLMAPBUFFERARBPROC)glad_glMapBuffer;
    if (glad_glMultiDrawArrays == NULL && glad_glMultiDrawArraysEXT != NULL) glad_glMultiDrawArrays = (PFNGLMULTIDRAWARRAYSPROC)glad_glMultiDrawArraysEXT;
    if (glad_glMultiDrawArraysEXT == NULL && glad_glMultiDrawArrays != NULL) glad_glMultiDrawArraysEXT = (PFNGLMULTIDRAWARRAYSEXTPROC)glad_glMultiDrawArrays;
    if (glad_glMultiDrawElements == NULL && glad_glMultiDrawElementsEXT != NULL) glad_glMultiDrawElements = (PFNGLMULTIDRAWELEMENTSPROC)glad_glMultiDrawElementsEXT;
    if (glad_glMultiDrawElementsEXT == NULL && glad_glMultiDrawElements != NULL) glad_glMultiDrawElementsEXT = (PFNGLMULTIDRAWELEMENTSEXTPROC)glad_glMultiDrawElements;
    if (glad_glPointParameterf == NULL && glad_glPointParameterfARB != NULL) glad_glPointParameterf = (PFNGLPOINTPARAMETERFPROC)glad_glPointParameterfARB;
    if (glad_glPointParameterf == NULL && glad_glPointParameterfEXT != NULL) glad_glPointParameterf = (PFNGLPOINTPARAMETERFPROC)glad_glPointParameterfEXT;
    if (glad_glPointParameterf == NULL && glad_glPointParameterfSGIS != NULL) glad_glPointParameterf = (PFNGLPOINTPARAMETERFPROC)glad_glPointParameterfSGIS;
    if (glad_glPointParameterfARB == NULL && glad_glPointParameterf != NULL) glad_glPointParameterfARB = (PFNGLPOINTPARAMETERFARBPROC)glad_glPointParameterf;
    if (glad_glPointParameterfARB == NULL && glad_glPointParameterfEXT != NULL) glad_glPointParameterfARB = (PFNGLPOINTPARAMETERFARBPROC)glad_glPointParameterfEXT;
    if (glad_glPointParameterfARB == NULL && glad_glPointParameterfSGIS != NULL) glad_glPointParameterfARB = (PFNGLPOINTPARAMETERFARBPROC)glad_glPointParameterfSGIS;
    if (glad_glPointParameterfEXT == NULL && glad_glPointParameterf != NULL) glad_glPointParameterfEXT = (PFNGLPOINTPARAMETERFEXTPROC)glad_glPointParameterf;
    if (glad_glPointParameterfEXT == NULL && glad_glPointParameterfARB != NULL) glad_glPointParameterfEXT = (PFNGLPOINTPARAMETERFEXTPROC)glad_glPointParameterfARB;
    if (glad_glPointParameterfEXT == NULL && glad_glPointParameterfSGIS != NULL) glad_glPointParameterfEXT = (PFNGLPOINTPARAMETERFEXTPROC)glad_glPointParameterfSGIS;
    if (glad_glPointParameterfSGIS == NULL && glad_glPointParameterf != NULL) glad_glPointParameterfSGIS = (PFNGLPOINTPARAMETERFSGISPROC)glad_glPointParameterf;
    if (glad_glPointParameterfSGIS == NULL && glad_glPointParameterfARB != NULL) glad_glPointParameterfSGIS = (PFNGLPOINTPARAMETERFSGISPROC)glad_glPointParameterfARB;
    if (glad_glPointParameterfSGIS == NULL && glad_glPointParameterfEXT != NULL) glad_glPointParameterfSGIS = (PFNGLPOINTPARAMETERFSGISPROC)glad_glPointParameterfEXT;
    if (glad_glPointParameterfv == NULL && glad_glPointParameterfvARB != NULL) glad_glPointParameterfv = (PFNGLPOINTPARAMETERFVPROC)glad_glPointParameterfvARB;
    if (glad_glPointParameterfv == NULL && glad_glPointParameterfvEXT != NULL) glad_glPointParameterfv = (PFNGLPOINTPARAMETERFVPROC)glad_glPointParameterfvEXT;
    if (glad_glPointParameterfv == NULL && glad_glPointParameterfvSGIS != NULL) glad_glPointParameterfv = (PFNGLPOINTPARAMETERFVPROC)glad_glPointParameterfvSGIS;
    if (glad_glPointParameterfvARB == NULL && glad_glPointParameterfv != NULL) glad_glPointParameterfvARB = (PFNGLPOINTPARAMETERFVARBPROC)glad_glPointParameterfv;
    if (glad_glPointParameterfvARB == NULL && glad_glPointParameterfvEXT != NULL) glad_glPointParameterfvARB = (PFNGLPOINTPARAMETERFVARBPROC)glad_glPointParameterfvEXT;
    if (glad_glPointParameterfvARB == NULL && glad_glPointParameterfvSGIS != NULL) glad_glPointParameterfvARB = (PFNGLPOINTPARAMETERFVARBPROC)glad_glPointParameterfvSGIS;
    if (glad_glPointParameterfvEXT == NULL && glad_glPointParameterfv != NULL) glad_glPointParameterfvEXT = (PFNGLPOINTPARAMETERFVEXTPROC)glad_glPointParameterfv;
    if (glad_glPointParameterfvEXT == NULL && glad_glPointParameterfvARB != NULL) glad_glPointParameterfvEXT = (PFNGLPOINTPARAMETERFVEXTPROC)glad_glPointParameterfvARB;
    if (glad_glPointParameterfvEXT == NULL && glad_glPointParameterfvSGIS != NULL) glad_glPointParameterfvEXT = (PFNGLPOINTPARAMETERFVEXTPROC)glad_glPointParameterfvSGIS;
    if (glad_glPointParameterfvSGIS == NULL && glad_glPointParameterfv != NULL) glad_glPointParameterfvSGIS = (PFNGLPOINTPARAMETERFVSGISPROC)glad_glPointParameterfv;
    if (glad_glPointParameterfvSGIS == NULL && glad_glPointParameterfvARB != NULL) glad_glPointParameterfvSGIS = (PFNGLPOINTPARAMETERFVSGISPROC)glad_glPointParameterfvARB;
    if (glad_glPointParameterfvSGIS == NULL && glad_glPointParameterfvEXT != NULL) glad_glPointParameterfvSGIS = (PFNGLPOINTPARAMETERFVSGISPROC)glad_glPointParameterfvEXT;
    if (glad_glPointParameteri == NULL && glad_glPointParameteriNV != NULL) glad_glPointParameteri = (PFNGLPOINTPARAMETERIPROC)glad_glPointParameteriNV;
    if (glad_glPointParameteriNV == NULL && glad_glPointParameteri != NULL) glad_glPointParameteriNV = (PFNGLPOINTPARAMETERINVPROC)glad_glPointParameteri;
    if (glad_glPointParameteriv == NULL && glad_glPointParameterivNV != NULL) glad_glPointParameteriv = (PFNGLPOINTPARAMETERIVPROC)glad_glPointParameterivNV;
    if (glad_glPointParameterivNV == NULL && glad_glPointParameteriv != NULL) glad_glPointParameterivNV = (PFNGLPOINTPARAMETERIVNVPROC)glad_glPointParameteriv;
    if (glad_glProvokingVertex == NULL && glad_glProvokingVertexEXT != NULL) glad_glProvokingVertex = (PFNGLPROVOKINGVERTEXPROC)glad_glProvokingVertexEXT;
    if (glad_glProvokingVertexEXT == NULL && glad_glProvokingVertex != NULL) glad_glProvokingVertexEXT = (PFNGLPROVOKINGVERTEXEXTPROC)glad_glProvokingVertex;
    if (glad_glRenderbufferStorage == NULL && glad_glRenderbufferStorageEXT != NULL) glad_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)glad_glRenderbufferStorageEXT;
    if (glad_glRenderbufferStorageEXT == NULL && glad_glRenderbufferStorage != NULL) glad_glRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC)glad_glRenderbufferStorage;
    if (glad_glRenderbufferStorageMultisample == NULL && glad_glRenderbufferStorageMultisampleEXT != NULL) glad_glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)glad_glRenderbufferStorageMultisampleEXT;
    if (glad_glRenderbufferStorageMultisampleEXT == NULL && glad_glRenderbufferStorageMultisample != NULL) glad_glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)glad_glRenderbufferStorageMultisample;
    if (glad_glSampleCoverage == NULL && glad_glSampleCoverageARB != NULL) glad_glSampleCoverage = (PFNGLSAMPLECOVERAGEPROC)glad_glSampleCoverageARB;
    if (glad_glSampleCoverageARB == NULL && glad_glSampleCoverage != NULL) glad_glSampleCoverageARB = (PFNGLSAMPLECOVERAGEARBPROC)glad_glSampleCoverage;
    if (glad_glShaderSource == NULL && glad_glShaderSourceARB != NULL) glad_glShaderSource = (PFNGLSHADERSOURCEPROC)glad_glShaderSourceARB;
    if (glad_glShaderSourceARB == NULL && glad_glShaderSource != NULL) glad_glShaderSourceARB = (PFNGLSHADERSOURCEARBPROC)glad_glShaderSource;
    if (glad_glStencilOpSeparate == NULL && glad_glStencilOpSeparateATI != NULL) glad_glStencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC)glad_glStencilOpSeparateATI;
    if (glad_glStencilOpSeparateATI == NULL && glad_glStencilOpSeparate != NULL) glad_glStencilOpSeparateATI = (PFNGLSTENCILOPSEPARATEATIPROC)glad_glStencilOpSeparate;
    if (glad_glTexBuffer == NULL && glad_glTexBufferARB != NULL) glad_glTexBuffer = (PFNGLTEXBUFFERPROC)glad_glTexBufferARB;
    if (glad_glTexBuffer == NULL && glad_glTexBufferEXT != NULL) glad_glTexBuffer = (PFNGLTEXBUFFERPROC)glad_glTexBufferEXT;
    if (glad_glTexBufferARB == NULL && glad_glTexBuffer != NULL) glad_glTexBufferARB = (PFNGLTEXBUFFERARBPROC)glad_glTexBuffer;
    if (glad_glTexBufferARB == NULL && glad_glTexBufferEXT != NULL) glad_glTexBufferARB = (PFNGLTEXBUFFERARBPROC)glad_glTexBufferEXT;
    if (glad_glTexBufferEXT == NULL && glad_glTexBuffer != NULL) glad_glTexBufferEXT = (PFNGLTEXBUFFEREXTPROC)glad_glTexBuffer;
    if (glad_glTexBufferEXT == NULL && glad_glTexBufferARB != NULL) glad_glTexBufferEXT = (PFNGLTEXBUFFEREXTPROC)glad_glTexBufferARB;
    if (glad_glTexImage3D == NULL && glad_glTexImage3DEXT != NULL) glad_glTexImage3D = (PFNGLTEXIMAGE3DPROC)glad_glTexImage3DEXT;
    if (glad_glTexImage3DEXT == NULL && glad_glTexImage3D != NULL) glad_glTexImage3DEXT = (PFNGLTEXIMAGE3DEXTPROC)glad_glTexImage3D;
    if (glad_glTexParameterIiv == NULL && glad_glTexParameterIivEXT != NULL) glad_glTexParameterIiv = (PFNGLTEXPARAMETERIIVPROC)glad_glTexParameterIivEXT;
    if (glad_glTexParameterIivEXT == NULL && glad_glTexParameterIiv != NULL) glad_glTexParameterIivEXT = (PFNGLTEXPARAMETERIIVEXTPROC)glad_glTexParameterIiv;
    if (glad_glTexParameterIuiv == NULL && glad_glTexParameterIuivEXT != NULL) glad_glTexParameterIuiv = (PFNGLTEXPARAMETERIUIVPROC)glad_glTexParameterIuivEXT;
    if (glad_glTexParameterIuivEXT == NULL && glad_glTexParameterIuiv != NULL) glad_glTexParameterIuivEXT = (PFNGLTEXPARAMETERIUIVEXTPROC)glad_glTexParameterIuiv;
    if (glad_glTexSubImage1D == NULL && glad_glTexSubImage1DEXT != NULL) glad_glTexSubImage1D = (PFNGLTEXSUBIMAGE1DPROC)glad_glTexSubImage1DEXT;
    if (glad_glTexSubImage1DEXT == NULL && glad_glTexSubImage1D != NULL) glad_glTexSubImage1DEXT = (PFNGLTEXSUBIMAGE1DEXTPROC)glad_glTexSubImage1D;
    if (glad_glTexSubImage2D == NULL && glad_glTexSubImage2DEXT != NULL) glad_glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)glad_glTexSubImage2DEXT;
    if (glad_glTexSubImage2DEXT == NULL && glad_glTexSubImage2D != NULL) glad_glTexSubImage2DEXT = (PFNGLTEXSUBIMAGE2DEXTPROC)glad_glTexSubImage2D;
    if (glad_glTexSubImage3D == NULL && glad_glTexSubImage3DEXT != NULL) glad_glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC)glad_glTexSubImage3DEXT;
    if (glad_glTexSubImage3DEXT == NULL && glad_glTexSubImage3D != NULL) glad_glTexSubImage3DEXT = (PFNGLTEXSUBIMAGE3DEXTPROC)glad_glTexSubImage3D;
    if (glad_glTransformFeedbackVaryings == NULL && glad_glTransformFeedbackVaryingsEXT != NULL) glad_glTransformFeedbackVaryings = (PFNGLTRANSFORMFEEDBACKVARYINGSPROC)glad_glTransformFeedbackVaryingsEXT;
    if (glad_glTransformFeedbackVaryingsEXT == NULL && glad_glTransformFeedbackVaryings != NULL) glad_glTransformFeedbackVaryingsEXT = (PFNGLTRANSFORMFEEDBACKVARYINGSEXTPROC)glad_glTransformFeedbackVaryings;
    if (glad_glUniform1f == NULL && glad_glUniform1fARB != NULL) glad_glUniform1f = (PFNGLUNIFORM1FPROC)glad_glUniform1fARB;
    if (glad_glUniform1fARB == NULL && glad_glUniform1f != NULL) glad_glUniform1fARB = (PFNGLUNIFORM1FARBPROC)glad_glUniform1f;
    if (glad_glUniform1fv == NULL && glad_glUniform1fvARB != NULL) glad_glUniform1fv = (PFNGLUNIFORM1FVPROC)glad_glUniform1fvARB;
    if (glad_glUniform1fvARB == NULL && glad_glUniform1fv != NULL) glad_glUniform1fvARB = (PFNGLUNIFORM1FVARBPROC)glad_glUniform1fv;
    if (glad_glUniform1i == NULL && glad_glUniform1iARB != NULL) glad_glUniform1i = (PFNGLUNIFORM1IPROC)glad_glUniform1iARB;
    if (glad_glUniform1iARB == NULL && glad_glUniform1i != NULL) glad_glUniform1iARB = (PFNGLUNIFORM1IARBPROC)glad_glUniform1i;
    if (glad_glUniform1iv == NULL && glad_glUniform1ivARB != NULL) glad_glUniform1iv = (PFNGLUNIFORM1IVPROC)glad_glUniform1ivARB;
    if (glad_glUniform1ivARB == NULL && glad_glUniform1iv != NULL) glad_glUniform1ivARB = (PFNGLUNIFORM1IVARBPROC)glad_glUniform1iv;
    if (glad_glUniform1ui == NULL && glad_glUniform1uiEXT != NULL) glad_glUniform1ui = (PFNGLUNIFORM1UIPROC)glad_glUniform1uiEXT;
    if (glad_glUniform1uiEXT == NULL && glad_glUniform1ui != NULL) glad_glUniform1uiEXT = (PFNGLUNIFORM1UIEXTPROC)glad_glUniform1ui;
    if (glad_glUniform1uiv == NULL && glad_glUniform1uivEXT != NULL) glad_glUniform1uiv = (PFNGLUNIFORM1UIVPROC)glad_glUniform1uivEXT;
    if (glad_glUniform1uivEXT == NULL && glad_glUniform1uiv != NULL) glad_glUniform1uivEXT = (PFNGLUNIFORM1UIVEXTPROC)glad_glUniform1uiv;
    if (glad_glUniform2f == NULL && glad_glUniform2fARB != NULL) glad_glUniform2f = (PFNGLUNIFORM2FPROC)glad_glUniform2fARB;
    if (glad_glUniform2fARB == NULL && glad_glUniform2f != NULL) glad_glUniform2fARB = (PFNGLUNIFORM2FARBPROC)glad_glUniform2f;
    if (glad_glUniform2fv == NULL && glad_glUniform2fvARB != NULL) glad_glUniform2fv = (PFNGLUNIFORM2FVPROC)glad_glUniform2fvARB;
    if (glad_glUniform2fvARB == NULL && glad_glUniform2fv != NULL) glad_glUniform2fvARB = (PFNGLUNIFORM2FVARBPROC)glad_glUniform2fv;
    if (glad_glUniform2i == NULL && glad_glUniform2iARB != NULL) glad_glUniform2i = (PFNGLUNIFORM2IPROC)glad_glUniform2iARB;
    if (glad_glUniform2iARB == NULL && glad_glUniform2i != NULL) glad_glUniform2iARB = (PFNGLUNIFORM2IARBPROC)glad_glUniform2i;
    if (glad_glUniform2iv == NULL && glad_glUniform2ivARB != NULL) glad_glUniform2iv = (PFNGLUNIFORM2IVPROC)glad_glUniform2ivARB;
    if (glad_glUniform2ivARB == NULL && glad_glUniform2iv != NULL) glad_glUniform2ivARB = (PFNGLUNIFORM2IVARBPROC)glad_glUniform2iv;
    if (glad_glUniform2ui == NULL && glad_glUniform2uiEXT != NULL) glad_glUniform2ui = (PFNGLUNIFORM2UIPROC)glad_glUniform2uiEXT;
    if (glad_glUniform2uiEXT == NULL && glad_glUniform2ui != NULL) glad_glUniform2uiEXT = (PFNGLUNIFORM2UIEXTPROC)glad_glUniform2ui;
    if (glad_glUniform2uiv == NULL && glad_glUniform2uivEXT != NULL) glad_glUniform2uiv = (PFNGLUNIFORM2UIVPROC)glad_glUniform2uivEXT;
    if (glad_glUniform2uivEXT == NULL && glad_glUniform2uiv != NULL) glad_glUniform2uivEXT = (PFNGLUNIFORM2UIVEXTPROC)glad_glUniform2uiv;
    if (glad_glUniform3f == NULL && glad_glUniform3fARB != NULL) glad_glUniform3f = (PFNGLUNIFORM3FPROC)glad_glUniform3fARB;
    if (glad_glUniform3fARB == NULL && glad_glUniform3f != NULL) glad_glUniform3fARB = (PFNGLUNIFORM3FARBPROC)glad_glUniform3f;
    if (glad_glUniform3fv == NULL && glad_glUniform3fvARB != NULL) glad_glUniform3fv = (PFNGLUNIFORM3FVPROC)glad_glUniform3fvARB;
    if (glad_glUniform3fvARB == NULL && glad_glUniform3fv != NULL) glad_glUniform3fvARB = (PFNGLUNIFORM3FVARBPROC)glad_glUniform3fv;
    if (glad_glUniform3i == NULL && glad_glUniform3iARB != NULL) glad_glUniform3i = (PFNGLUNIFORM3IPROC)glad_glUniform3iARB;
    if (glad_glUniform3iARB == NULL && glad_glUniform3i != NULL) glad_glUniform3iARB = (PFNGLUNIFORM3IARBPROC)glad_glUniform3i;
    if (glad_glUniform3iv == NULL && glad_glUniform3ivARB != NULL) glad_glUniform3iv = (PFNGLUNIFORM3IVPROC)glad_glUniform3ivARB;
    if (glad_glUniform3ivARB == NULL && glad_glUniform3iv != NULL) glad_glUniform3ivARB = (PFNGLUNIFORM3IVARBPROC)glad_glUniform3iv;
    if (glad_glUniform3ui == NULL && glad_glUniform3uiEXT != NULL) glad_glUniform3ui = (PFNGLUNIFORM3UIPROC)glad_glUniform3uiEXT;
    if (glad_glUniform3uiEXT == NULL && glad_glUniform3ui != NULL) glad_glUniform3uiEXT = (PFNGLUNIFORM3UIEXTPROC)glad_glUniform3ui;
    if (glad_glUniform3uiv == NULL && glad_glUniform3uivEXT != NULL) glad_glUniform3uiv = (PFNGLUNIFORM3UIVPROC)glad_glUniform3uivEXT;
    if (glad_glUniform3uivEXT == NULL && glad_glUniform3uiv != NULL) glad_glUniform3uivEXT = (PFNGLUNIFORM3UIVEXTPROC)glad_glUniform3uiv;
    if (glad_glUniform4f == NULL && glad_glUniform4fARB != NULL) glad_glUniform4f = (PFNGLUNIFORM4FPROC)glad_glUniform4fARB;
    if (glad_glUniform4fARB == NULL && glad_glUniform4f != NULL) glad_glUniform4fARB = (PFNGLUNIFORM4FARBPROC)glad_glUniform4f;
    if (glad_glUniform4fv == NULL && glad_glUniform4fvARB != NULL) glad_glUniform4fv = (PFNGLUNIFORM4FVPROC)glad_glUniform4fvARB;
    if (glad_glUniform4fvARB == NULL && glad_glUniform4fv != NULL) glad_glUniform4fvARB = (PFNGLUNIFORM4FVARBPROC)glad_glUniform4fv;
    if (glad_glUniform4i == NULL && glad_glUniform4iARB != NULL) glad_glUniform4i = (PFNGLUNIFORM4IPROC)glad_glUniform4iARB;
    if (glad_glUniform4iARB == NULL && glad_glUniform4i != NULL) glad_glUniform4iARB = (PFNGLUNIFORM4IARBPROC)glad_glUniform4i;
    if (glad_glUniform4iv == NULL && glad_glUniform4ivARB != NULL) glad_glUniform4iv = (PFNGLUNIFORM4IVPROC)glad_glUniform4ivARB;
    if (glad_glUniform4ivARB == NULL && glad_glUniform4iv != NULL) glad_glUniform4ivARB = (PFNGLUNIFORM4IVARBPROC)glad_glUniform4iv;
    if (glad_glUniform4ui == NULL && glad_glUniform4uiEXT != NULL) glad_glUniform4ui = (PFNGLUNIFORM4UIPROC)glad_glUniform4uiEXT;
    if (glad_glUniform4uiEXT == NULL && glad_glUniform4ui != NULL) glad_glUniform4uiEXT = (PFNGLUNIFORM4UIEXTPROC)glad_glUniform4ui;
    if (glad_glUniform4uiv == NULL && glad_glUniform4uivEXT != NULL) glad_glUniform4uiv = (PFNGLUNIFORM4UIVPROC)glad_glUniform4uivEXT;
    if (glad_glUniform4uivEXT == NULL && glad_glUniform4uiv != NULL) glad_glUniform4uivEXT = (PFNGLUNIFORM4UIVEXTPROC)glad_glUniform4uiv;
    if (glad_glUniformMatrix2fv == NULL && glad_glUniformMatrix2fvARB != NULL) glad_glUniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC)glad_glUniformMatrix2fvARB;
    if (glad_glUniformMatrix2fvARB == NULL && glad_glUniformMatrix2fv != NULL) glad_glUniformMatrix2fvARB = (PFNGLUNIFORMMATRIX2FVARBPROC)glad_glUniformMatrix2fv;
    if (glad_glUniformMatrix3fv == NULL && glad_glUniformMatrix3fvARB != NULL) glad_glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)glad_glUniformMatrix3fvARB;
    if (glad_glUniformMatrix3fvARB == NULL && glad_glUniformMatrix3fv != NULL) glad_glUniformMatrix3fvARB = (PFNGLUNIFORMMATRIX3FVARBPROC)glad_glUniformMatrix3fv;
    if (glad_glUniformMatrix4fv == NULL && glad_glUniformMatrix4fvARB != NULL) glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)glad_glUniformMatrix4fvARB;
    if (glad_glUniformMatrix4fvARB == NULL && glad_glUniformMatrix4fv != NULL) glad_glUniformMatrix4fvARB = (PFNGLUNIFORMMATRIX4FVARBPROC)glad_glUniformMatrix4fv;
    if (glad_glUnmapBuffer == NULL && glad_glUnmapBufferARB != NULL) glad_glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)glad_glUnmapBufferARB;
    if (glad_glUnmapBufferARB == NULL && glad_glUnmapBuffer != NULL) glad_glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)glad_glUnmapBuffer;
    if (glad_glUseProgram == NULL && glad_glUseProgramObjectARB != NULL) glad_glUseProgram = (PFNGLUSEPROGRAMPROC)glad_glUseProgramObjectARB;
    if (glad_glUseProgramObjectARB == NULL && glad_glUseProgram != NULL) glad_glUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC)glad_glUseProgram;
    if (glad_glValidateProgram == NULL && glad_glValidateProgramARB != NULL) glad_glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)glad_glValidateProgramARB;
    if (glad_glValidateProgramARB == NULL && glad_glValidateProgram != NULL) glad_glValidateProgramARB = (PFNGLVALIDATEPROGRAMARBPROC)glad_glValidateProgram;
    if (glad_glVertexAttrib1d == NULL && glad_glVertexAttrib1dARB != NULL) glad_glVertexAttrib1d = (PFNGLVERTEXATTRIB1DPROC)glad_glVertexAttrib1dARB;
    if (glad_glVertexAttrib1d == NULL && glad_glVertexAttrib1dNV != NULL) glad_glVertexAttrib1d = (PFNGLVERTEXATTRIB1DPROC)glad_glVertexAttrib1dNV;
    if (glad_glVertexAttrib1dARB == NULL && glad_glVertexAttrib1d != NULL) glad_glVertexAttrib1dARB = (PFNGLVERTEXATTRIB1DARBPROC)glad_glVertexAttrib1d;
    if (glad_glVertexAttrib1dARB == NULL && glad_glVertexAttrib1dNV != NULL) glad_glVertexAttrib1dARB = (PFNGLVERTEXATTRIB1DARBPROC)glad_glVertexAttrib1dNV;
    if (glad_glVertexAttrib1dNV == NULL && glad_glVertexAttrib1d != NULL) glad_glVertexAttrib1dNV = (PFNGLVERTEXATTRIB1DNVPROC)glad_glVertexAttrib1d;
    if (glad_glVertexAttrib1dNV == NULL && glad_glVertexAttrib1dARB != NULL) glad_glVertexAttrib1dNV = (PFNGLVERTEXATTRIB1DNVPROC)glad_glVertexAttrib1dARB;
    if (glad_glVertexAttrib1dv == NULL && glad_glVertexAttrib1dvARB != NULL) glad_glVertexAttrib1dv = (PFNGLVERTEXATTRIB1DVPROC)glad_glVertexAttrib1dvARB;
    if (glad_glVertexAttrib1dv == NULL && glad_glVertexAttrib1dvNV != NULL) glad_glVertexAttrib1dv = (PFNGLVERTEXATTRIB1DVPROC)glad_glVertexAttrib1dvNV;
    if (glad_glVertexAttrib1dvARB == NULL && glad_glVertexAttrib1dv != NULL) glad_glVertexAttrib1dvARB = (PFNGLVERTEXATTRIB1DVARBPROC)glad_glVertexAttrib1dv;
    if (glad_glVertexAttrib1dvARB == NULL && glad_glVertexAttrib1dvNV != NULL) glad_glVertexAttrib1dvARB = (PFNGLVERTEXATTRIB1DVARBPROC)glad_glVertexAttrib1dvNV;
    if (glad_glVertexAttrib1dvNV == NULL && glad_glVertexAttrib1dv != NULL) glad_glVertexAttrib1dvNV = (PFNGLVERTEXATTRIB1DVNVPROC)glad_glVertexAttrib1dv;
    if (glad_glVertexAttrib1dvNV == NULL && glad_glVertexAttrib1dvARB != NULL) glad_glVertexAttrib1dvNV = (PFNGLVERTEXATTRIB1DVNVPROC)glad_glVertexAttrib1dvARB;
    if (glad_glVertexAttrib1f == NULL && glad_glVertexAttrib1fARB != NULL) glad_glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC)glad_glVertexAttrib1fARB;
    if (glad_glVertexAttrib1f == NULL && glad_glVertexAttrib1fNV != NULL) glad_glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC)glad_glVertexAttrib1fNV;
    if (glad_glVertexAttrib1fARB == NULL && glad_glVertexAttrib1f != NULL) glad_glVertexAttrib1fARB = (PFNGLVERTEXATTRIB1FARBPROC)glad_glVertexAttrib1f;
    if (glad_glVertexAttrib1fARB == NULL && glad_glVertexAttrib1fNV != NULL) glad_glVertexAttrib1fARB = (PFNGLVERTEXATTRIB1FARBPROC)glad_glVertexAttrib1fNV;
    if (glad_glVertexAttrib1fNV == NULL && glad_glVertexAttrib1f != NULL) glad_glVertexAttrib1fNV = (PFNGLVERTEXATTRIB1FNVPROC)glad_glVertexAttrib1f;
    if (glad_glVertexAttrib1fNV == NULL && glad_glVertexAttrib1fARB != NULL) glad_glVertexAttrib1fNV = (PFNGLVERTEXATTRIB1FNVPROC)glad_glVertexAttrib1fARB;
    if (glad_glVertexAttrib1fv == NULL && glad_glVertexAttrib1fvARB != NULL) glad_glVertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC)glad_glVertexAttrib1fvARB;
    if (glad_glVertexAttrib1fv == NULL && glad_glVertexAttrib1fvNV != NULL) glad_glVertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC)glad_glVertexAttrib1fvNV;
    if (glad_glVertexAttrib1fvARB == NULL && glad_glVertexAttrib1fv != NULL) glad_glVertexAttrib1fvARB = (PFNGLVERTEXATTRIB1FVARBPROC)glad_glVertexAttrib1fv;
    if (glad_glVertexAttrib1fvARB == NULL && glad_glVertexAttrib1fvNV != NULL) glad_glVertexAttrib1fvARB = (PFNGLVERTEXATTRIB1FVARBPROC)glad_glVertexAttrib1fvNV;
    if (glad_glVertexAttrib1fvNV == NULL && glad_glVertexAttrib1fv != NULL) glad_glVertexAttrib1fvNV = (PFNGLVERTEXATTRIB1FVNVPROC)glad_glVertexAttrib1fv;
    if (glad_glVertexAttrib1fvNV == NULL && glad_glVertexAttrib1fvARB != NULL) glad_glVertexAttrib1fvNV = (PFNGLVERTEXATTRIB1FVNVPROC)glad_glVertexAttrib1fvARB;
    if (glad_glVertexAttrib1s == NULL && glad_glVertexAttrib1sARB != NULL) glad_glVertexAttrib1s = (PFNGLVERTEXATTRIB1SPROC)glad_glVertexAttrib1sARB;
    if (glad_glVertexAttrib1s == NULL && glad_glVertexAttrib1sNV != NULL) glad_glVertexAttrib1s = (PFNGLVERTEXATTRIB1SPROC)glad_glVertexAttrib1sNV;
    if (glad_glVertexAttrib1sARB == NULL && glad_glVertexAttrib1s != NULL) glad_glVertexAttrib1sARB = (PFNGLVERTEXATTRIB1SARBPROC)glad_glVertexAttrib1s;
    if (glad_glVertexAttrib1sARB == NULL && glad_glVertexAttrib1sNV != NULL) glad_glVertexAttrib1sARB = (PFNGLVERTEXATTRIB1SARBPROC)glad_glVertexAttrib1sNV;
    if (glad_glVertexAttrib1sNV == NULL && glad_glVertexAttrib1s != NULL) glad_glVertexAttrib1sNV = (PFNGLVERTEXATTRIB1SNVPROC)glad_glVertexAttrib1s;
    if (glad_glVertexAttrib1sNV == NULL && glad_glVertexAttrib1sARB != NULL) glad_glVertexAttrib1sNV = (PFNGLVERTEXATTRIB1SNVPROC)glad_glVertexAttrib1sARB;
    if (glad_glVertexAttrib1sv == NULL && glad_glVertexAttrib1svARB != NULL) glad_glVertexAttrib1sv = (PFNGLVERTEXATTRIB1SVPROC)glad_glVertexAttrib1svARB;
    if (glad_glVertexAttrib1sv == NULL && glad_glVertexAttrib1svNV != NULL) glad_glVertexAttrib1sv = (PFNGLVERTEXATTRIB1SVPROC)glad_glVertexAttrib1svNV;
    if (glad_glVertexAttrib1svARB == NULL && glad_glVertexAttrib1sv != NULL) glad_glVertexAttrib1svARB = (PFNGLVERTEXATTRIB1SVARBPROC)glad_glVertexAttrib1sv;
    if (glad_glVertexAttrib1svARB == NULL && glad_glVertexAttrib1svNV != NULL) glad_glVertexAttrib1svARB = (PFNGLVERTEXATTRIB1SVARBPROC)glad_glVertexAttrib1svNV;
    if (glad_glVertexAttrib1svNV == NULL && glad_glVertexAttrib1sv != NULL) glad_glVertexAttrib1svNV = (PFNGLVERTEXATTRIB1SVNVPROC)glad_glVertexAttrib1sv;
    if (glad_glVertexAttrib1svNV == NULL && glad_glVertexAttrib1svARB != NULL) glad_glVertexAttrib1svNV = (PFNGLVERTEXATTRIB1SVNVPROC)glad_glVertexAttrib1svARB;
    if (glad_glVertexAttrib2d == NULL && glad_glVertexAttrib2dARB != NULL) glad_glVertexAttrib2d = (PFNGLVERTEXATTRIB2DPROC)glad_glVertexAttrib2dARB;
    if (glad_glVertexAttrib2d == NULL && glad_glVertexAttrib2dNV != NULL) glad_glVertexAttrib2d = (PFNGLVERTEXATTRIB2DPROC)glad_glVertexAttrib2dNV;
    if (glad_glVertexAttrib2dARB == NULL && glad_glVertexAttrib2d != NULL) glad_glVertexAttrib2dARB = (PFNGLVERTEXATTRIB2DARBPROC)glad_glVertexAttrib2d;
    if (glad_glVertexAttrib2dARB == NULL && glad_glVertexAttrib2dNV != NULL) glad_glVertexAttrib2dARB = (PFNGLVERTEXATTRIB2DARBPROC)glad_glVertexAttrib2dNV;
    if (glad_glVertexAttrib2dNV == NULL && glad_glVertexAttrib2d != NULL) glad_glVertexAttrib2dNV = (PFNGLVERTEXATTRIB2DNVPROC)glad_glVertexAttrib2d;
    if (glad_glVertexAttrib2dNV == NULL && glad_glVertexAttrib2dARB != NULL) glad_glVertexAttrib2dNV = (PFNGLVERTEXATTRIB2DNVPROC)glad_glVertexAttrib2dARB;
    if (glad_glVertexAttrib2dv == NULL && glad_glVertexAttrib2dvARB != NULL) glad_glVertexAttrib2dv = (PFNGLVERTEXATTRIB2DVPROC)glad_glVertexAttrib2dvARB;
    if (glad_glVertexAttrib2dv == NULL && glad_glVertexAttrib2dvNV != NULL) glad_glVertexAttrib2dv = (PFNGLVERTEXATTRIB2DVPROC)glad_glVertexAttrib2dvNV;
    if (glad_glVertexAttrib2dvARB == NULL && glad_glVertexAttrib2dv != NULL) glad_glVertexAttrib2dvARB = (PFNGLVERTEXATTRIB2DVARBPROC)glad_glVertexAttrib2dv;
    if (glad_glVertexAttrib2dvARB == NULL && glad_glVertexAttrib2dvNV != NULL) glad_glVertexAttrib2dvARB = (PFNGLVERTEXATTRIB2DVARBPROC)glad_glVertexAttrib2dvNV;
    if (glad_glVertexAttrib2dvNV == NULL && glad_glVertexAttrib2dv != NULL) glad_glVertexAttrib2dvNV = (PFNGLVERTEXATTRIB2DVNVPROC)glad_glVertexAttrib2dv;
    if (glad_glVertexAttrib2dvNV == NULL && glad_glVertexAttrib2dvARB != NULL) glad_glVertexAttrib2dvNV = (PFNGLVERTEXATTRIB2DVNVPROC)glad_glVertexAttrib2dvARB;
    if (glad_glVertexAttrib2f == NULL && glad_glVertexAttrib2fARB != NULL) glad_glVertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC)glad_glVertexAttrib2fARB;
    if (glad_glVertexAttrib2f == NULL && glad_glVertexAttrib2fNV != NULL) glad_glVertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC)glad_glVertexAttrib2fNV;
    if (glad_glVertexAttrib2fARB == NULL && glad_glVertexAttrib2f != NULL) glad_glVertexAttrib2fARB = (PFNGLVERTEXATTRIB2FARBPROC)glad_glVertexAttrib2f;
    if (glad_glVertexAttrib2fARB == NULL && glad_glVertexAttrib2fNV != NULL) glad_glVertexAttrib2fARB = (PFNGLVERTEXATTRIB2FARBPROC)glad_glVertexAttrib2fNV;
    if (glad_glVertexAttrib2fNV == NULL && glad_glVertexAttrib2f != NULL) glad_glVertexAttrib2fNV = (PFNGLVERTEXATTRIB2FNVPROC)glad_glVertexAttrib2f;
    if (glad_glVertexAttrib2fNV == NULL && glad_glVertexAttrib2fARB != NULL) glad_glVertexAttrib2fNV = (PFNGLVERTEXATTRIB2FNVPROC)glad_glVertexAttrib2fARB;
    if (glad_glVertexAttrib2fv == NULL && glad_glVertexAttrib2fvARB != NULL) glad_glVertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC)glad_glVertexAttrib2fvARB;
    if (glad_glVertexAttrib2fv == NULL && glad_glVertexAttrib2fvNV != NULL) glad_glVertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC)glad_glVertexAttrib2fvNV;
    if (glad_glVertexAttrib2fvARB == NULL && glad_glVertexAttrib2fv != NULL) glad_glVertexAttrib2fvARB = (PFNGLVERTEXATTRIB2FVARBPROC)glad_glVertexAttrib2fv;
    if (glad_glVertexAttrib2fvARB == NULL && glad_glVertexAttrib2fvNV != NULL) glad_glVertexAttrib2fvARB = (PFNGLVERTEXATTRIB2FVARBPROC)glad_glVertexAttrib2fvNV;
    if (glad_glVertexAttrib2fvNV == NULL && glad_glVertexAttrib2fv != NULL) glad_glVertexAttrib2fvNV = (PFNGLVERTEXATTRIB2FVNVPROC)glad_glVertexAttrib2fv;
    if (glad_glVertexAttrib2fvNV == NULL && glad_glVertexAttrib2fvARB != NULL) glad_glVertexAttrib2fvNV = (PFNGLVERTEXATTRIB2FVNVPROC)glad_glVertexAttrib2fvARB;
    if (glad_glVertexAttrib2s == NULL && glad_glVertexAttrib2sARB != NULL) glad_glVertexAttrib2s = (PFNGLVERTEXATTRIB2SPROC)glad_glVertexAttrib2sARB;
    if (glad_glVertexAttrib2s == NULL && glad_glVertexAttrib2sNV != NULL) glad_glVertexAttrib2s = (PFNGLVERTEXATTRIB2SPROC)glad_glVertexAttrib2sNV;
    if (glad_glVertexAttrib2sARB == NULL && glad_glVertexAttrib2s != NULL) glad_glVertexAttrib2sARB = (PFNGLVERTEXATTRIB2SARBPROC)glad_glVertexAttrib2s;
    if (glad_glVertexAttrib2sARB == NULL && glad_glVertexAttrib2sNV != NULL) glad_glVertexAttrib2sARB = (PFNGLVERTEXATTRIB2SARBPROC)glad_glVertexAttrib2sNV;
    if (glad_glVertexAttrib2sNV == NULL && glad_glVertexAttrib2s != NULL) glad_glVertexAttrib2sNV = (PFNGLVERTEXATTRIB2SNVPROC)glad_glVertexAttrib2s;
    if (glad_glVertexAttrib2sNV == NULL && glad_glVertexAttrib2sARB != NULL) glad_glVertexAttrib2sNV = (PFNGLVERTEXATTRIB2SNVPROC)glad_glVertexAttrib2sARB;
    if (glad_glVertexAttrib2sv == NULL && glad_glVertexAttrib2svARB != NULL) glad_glVertexAttrib2sv = (PFNGLVERTEXATTRIB2SVPROC)glad_glVertexAttrib2svARB;
    if (glad_glVertexAttrib2sv == NULL && glad_glVertexAttrib2svNV != NULL) glad_glVertexAttrib2sv = (PFNGLVERTEXATTRIB2SVPROC)glad_glVertexAttrib2svNV;
    if (glad_glVertexAttrib2svARB == NULL && glad_glVertexAttrib2sv != NULL) glad_glVertexAttrib2svARB = (PFNGLVERTEXATTRIB2SVARBPROC)glad_glVertexAttrib2sv;
    if (glad_glVertexAttrib2svARB == NULL && glad_glVertexAttrib2svNV != NULL) glad_glVertexAttrib2svARB = (PFNGLVERTEXATTRIB2SVARBPROC)glad_glVertexAttrib2svNV;
    if (glad_glVertexAttrib2svNV == NULL && glad_glVertexAttrib2sv != NULL) glad_glVertexAttrib2svNV = (PFNGLVERTEXATTRIB2SVNVPROC)glad_glVertexAttrib2sv;
    if (glad_glVertexAttrib2svNV == NULL && glad_glVertexAttrib2svARB != NULL) glad_glVertexAttrib2svNV = (PFNGLVERTEXATTRIB2SVNVPROC)glad_glVertexAttrib2svARB;
    if (glad_glVertexAttrib3d == NULL && glad_glVertexAttrib3dARB != NULL) glad_glVertexAttrib3d = (PFNGLVERTEXATTRIB3DPROC)glad_glVertexAttrib3dARB;
    if (glad_glVertexAttrib3d == NULL && glad_glVertexAttrib3dNV != NULL) glad_glVertexAttrib3d = (PFNGLVERTEXATTRIB3DPROC)glad_glVertexAttrib3dNV;
    if (glad_glVertexAttrib3dARB == NULL && glad_glVertexAttrib3d != NULL) glad_glVertexAttrib3dARB = (PFNGLVERTEXATTRIB3DARBPROC)glad_glVertexAttrib3d;
    if (glad_glVertexAttrib3dARB == NULL && glad_glVertexAttrib3dNV != NULL) glad_glVertexAttrib3dARB = (PFNGLVERTEXATTRIB3DARBPROC)glad_glVertexAttrib3dNV;
    if (glad_glVertexAttrib3dNV == NULL && glad_glVertexAttrib3d != NULL) glad_glVertexAttrib3dNV = (PFNGLVERTEXATTRIB3DNVPROC)glad_glVertexAttrib3d;
    if (glad_glVertexAttrib3dNV == NULL && glad_glVertexAttrib3dARB != NULL) glad_glVertexAttrib3dNV = (PFNGLVERTEXATTRIB3DNVPROC)glad_glVertexAttrib3dARB;
    if (glad_glVertexAttrib3dv == NULL && glad_glVertexAttrib3dvARB != NULL) glad_glVertexAttrib3dv = (PFNGLVERTEXATTRIB3DVPROC)glad_glVertexAttrib3dvARB;
    if (glad_glVertexAttrib3dv == NULL && glad_glVertexAttrib3dvNV != NULL) glad_glVertexAttrib3dv = (PFNGLVERTEXATTRIB3DVPROC)glad_glVertexAttrib3dvNV;
    if (glad_glVertexAttrib3dvARB == NULL && glad_glVertexAttrib3dv != NULL) glad_glVertexAttrib3dvARB = (PFNGLVERTEXATTRIB3DVARBPROC)glad_glVertexAttrib3dv;
    if (glad_glVertexAttrib3dvARB == NULL && glad_glVertexAttrib3dvNV != NULL) glad_glVertexAttrib3dvARB = (PFNGLVERTEXATTRIB3DVARBPROC)glad_glVertexAttrib3dvNV;
    if (glad_glVertexAttrib3dvNV == NULL && glad_glVertexAttrib3dv != NULL) glad_glVertexAttrib3dvNV = (PFNGLVERTEXATTRIB3DVNVPROC)glad_glVertexAttrib3dv;
    if (glad_glVertexAttrib3dvNV == NULL && glad_glVertexAttrib3dvARB != NULL) glad_glVertexAttrib3dvNV = (PFNGLVERTEXATTRIB3DVNVPROC)glad_glVertexAttrib3dvARB;
    if (glad_glVertexAttrib3f == NULL && glad_glVertexAttrib3fARB != NULL) glad_glVertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC)glad_glVertexAttrib3fARB;
    if (glad_glVertexAttrib3f == NULL && glad_glVertexAttrib3fNV != NULL) glad_glVertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC)glad_glVertexAttrib3fNV;
    if (glad_glVertexAttrib3fARB == NULL && glad_glVertexAttrib3f != NULL) glad_glVertexAttrib3fARB = (PFNGLVERTEXATTRIB3FARBPROC)glad_glVertexAttrib3f;
    if (glad_glVertexAttrib3fARB == NULL && glad_glVertexAttrib3fNV != NULL) glad_glVertexAttrib3fARB = (PFNGLVERTEXATTRIB3FARBPROC)glad_glVertexAttrib3fNV;
    if (glad_glVertexAttrib3fNV == NULL && glad_glVertexAttrib3f != NULL) glad_glVertexAttrib3fNV = (PFNGLVERTEXATTRIB3FNVPROC)glad_glVertexAttrib3f;
    if (glad_glVertexAttrib3fNV == NULL && glad_glVertexAttrib3fARB != NULL) glad_glVertexAttrib3fNV = (PFNGLVERTEXATTRIB3FNVPROC)glad_glVertexAttrib3fARB;
    if (glad_glVertexAttrib3fv == NULL && glad_glVertexAttrib3fvARB != NULL) glad_glVertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC)glad_glVertexAttrib3fvARB;
    if (glad_glVertexAttrib3fv == NULL && glad_glVertexAttrib3fvNV != NULL) glad_glVertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC)glad_glVertexAttrib3fvNV;
    if (glad_glVertexAttrib3fvARB == NULL && glad_glVertexAttrib3fv != NULL) glad_glVertexAttrib3fvARB = (PFNGLVERTEXATTRIB3FVARBPROC)glad_glVertexAttrib3fv;
    if (glad_glVertexAttrib3fvARB == NULL && glad_glVertexAttrib3fvNV != NULL) glad_glVertexAttrib3fvARB = (PFNGLVERTEXATTRIB3FVARBPROC)glad_glVertexAttrib3fvNV;
    if (glad_glVertexAttrib3fvNV == NULL && glad_glVertexAttrib3fv != NULL) glad_glVertexAttrib3fvNV = (PFNGLVERTEXATTRIB3FVNVPROC)glad_glVertexAttrib3fv;
    if (glad_glVertexAttrib3fvNV == NULL && glad_glVertexAttrib3fvARB != NULL) glad_glVertexAttrib3fvNV = (PFNGLVERTEXATTRIB3FVNVPROC)glad_glVertexAttrib3fvARB;
    if (glad_glVertexAttrib3s == NULL && glad_glVertexAttrib3sARB != NULL) glad_glVertexAttrib3s = (PFNGLVERTEXATTRIB3SPROC)glad_glVertexAttrib3sARB;
    if (glad_glVertexAttrib3s == NULL && glad_glVertexAttrib3sNV != NULL) glad_glVertexAttrib3s = (PFNGLVERTEXATTRIB3SPROC)glad_glVertexAttrib3sNV;
    if (glad_glVertexAttrib3sARB == NULL && glad_glVertexAttrib3s != NULL) glad_glVertexAttrib3sARB = (PFNGLVERTEXATTRIB3SARBPROC)glad_glVertexAttrib3s;
    if (glad_glVertexAttrib3sARB == NULL && glad_glVertexAttrib3sNV != NULL) glad_glVertexAttrib3sARB = (PFNGLVERTEXATTRIB3SARBPROC)glad_glVertexAttrib3sNV;
    if (glad_glVertexAttrib3sNV == NULL && glad_glVertexAttrib3s != NULL) glad_glVertexAttrib3sNV = (PFNGLVERTEXATTRIB3SNVPROC)glad_glVertexAttrib3s;
    if (glad_glVertexAttrib3sNV == NULL && glad_glVertexAttrib3sARB != NULL) glad_glVertexAttrib3sNV = (PFNGLVERTEXATTRIB3SNVPROC)glad_glVertexAttrib3sARB;
    if (glad_glVertexAttrib3sv == NULL && glad_glVertexAttrib3svARB != NULL) glad_glVertexAttrib3sv = (PFNGLVERTEXATTRIB3SVPROC)glad_glVertexAttrib3svARB;
    if (glad_glVertexAttrib3sv == NULL && glad_glVertexAttrib3svNV != NULL) glad_glVertexAttrib3sv = (PFNGLVERTEXATTRIB3SVPROC)glad_glVertexAttrib3svNV;
    if (glad_glVertexAttrib3svARB == NULL && glad_glVertexAttrib3sv != NULL) glad_glVertexAttrib3svARB = (PFNGLVERTEXATTRIB3SVARBPROC)glad_glVertexAttrib3sv;
    if (glad_glVertexAttrib3svARB == NULL && glad_glVertexAttrib3svNV != NULL) glad_glVertexAttrib3svARB = (PFNGLVERTEXATTRIB3SVARBPROC)glad_glVertexAttrib3svNV;
    if (glad_glVertexAttrib3svNV == NULL && glad_glVertexAttrib3sv != NULL) glad_glVertexAttrib3svNV = (PFNGLVERTEXATTRIB3SVNVPROC)glad_glVertexAttrib3sv;
    if (glad_glVertexAttrib3svNV == NULL && glad_glVertexAttrib3svARB != NULL) glad_glVertexAttrib3svNV = (PFNGLVERTEXATTRIB3SVNVPROC)glad_glVertexAttrib3svARB;
    if (glad_glVertexAttrib4bv == NULL && glad_glVertexAttrib4bvARB != NULL) glad_glVertexAttrib4bv = (PFNGLVERTEXATTRIB4BVPROC)glad_glVertexAttrib4bvARB;
    if (glad_glVertexAttrib4bvARB == NULL && glad_glVertexAttrib4bv != NULL) glad_glVertexAttrib4bvARB = (PFNGLVERTEXATTRIB4BVARBPROC)glad_glVertexAttrib4bv;
    if (glad_glVertexAttrib4d == NULL && glad_glVertexAttrib4dARB != NULL) glad_glVertexAttrib4d = (PFNGLVERTEXATTRIB4DPROC)glad_glVertexAttrib4dARB;
    if (glad_glVertexAttrib4d == NULL && glad_glVertexAttrib4dNV != NULL) glad_glVertexAttrib4d = (PFNGLVERTEXATTRIB4DPROC)glad_glVertexAttrib4dNV;
    if (glad_glVertexAttrib4dARB == NULL && glad_glVertexAttrib4d != NULL) glad_glVertexAttrib4dARB = (PFNGLVERTEXATTRIB4DARBPROC)glad_glVertexAttrib4d;
    if (glad_glVertexAttrib4dARB == NULL && glad_glVertexAttrib4dNV != NULL) glad_glVertexAttrib4dARB = (PFNGLVERTEXATTRIB4DARBPROC)glad_glVertexAttrib4dNV;
    if (glad_glVertexAttrib4dNV == NULL && glad_glVertexAttrib4d != NULL) glad_glVertexAttrib4dNV = (PFNGLVERTEXATTRIB4DNVPROC)glad_glVertexAttrib4d;
    if (glad_glVertexAttrib4dNV == NULL && glad_glVertexAttrib4dARB != NULL) glad_glVertexAttrib4dNV = (PFNGLVERTEXATTRIB4DNVPROC)glad_glVertexAttrib4dARB;
    if (glad_glVertexAttrib4dv == NULL && glad_glVertexAttrib4dvARB != NULL) glad_glVertexAttrib4dv = (PFNGLVERTEXATTRIB4DVPROC)glad_glVertexAttrib4dvARB;
    if (glad_glVertexAttrib4dv == NULL && glad_glVertexAttrib4dvNV != NULL) glad_glVertexAttrib4dv = (PFNGLVERTEXATTRIB4DVPROC)glad_glVertexAttrib4dvNV;
    if (glad_glVertexAttrib4dvARB == NULL && glad_glVertexAttrib4dv != NULL) glad_glVertexAttrib4dvARB = (PFNGLVERTEXATTRIB4DVARBPROC)glad_glVertexAttrib4dv;
    if (glad_glVertexAttrib4dvARB == NULL && glad_glVertexAttrib4dvNV != NULL) glad_glVertexAttrib4dvARB = (PFNGLVERTEXATTRIB4DVARBPROC)glad_glVertexAttrib4dvNV;
    if (glad_glVertexAttrib4dvNV == NULL && glad_glVertexAttrib4dv != NULL) glad_glVertexAttrib4dvNV = (PFNGLVERTEXATTRIB4DVNVPROC)glad_glVertexAttrib4dv;
    if (glad_glVertexAttrib4dvNV == NULL && glad_glVertexAttrib4dvARB != NULL) glad_glVertexAttrib4dvNV = (PFNGLVERTEXATTRIB4DVNVPROC)glad_glVertexAttrib4dvARB;
    if (glad_glVertexAttrib4f == NULL && glad_glVertexAttrib4fARB != NULL) glad_glVertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC)glad_glVertexAttrib4fARB;
    if (glad_glVertexAttrib4f == NULL && glad_glVertexAttrib4fNV != NULL) glad_glVertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC)glad_glVertexAttrib4fNV;
    if (glad_glVertexAttrib4fARB == NULL && glad_glVertexAttrib4f != NULL) glad_glVertexAttrib4fARB = (PFNGLVERTEXATTRIB4FARBPROC)glad_glVertexAttrib4f;
    if (glad_glVertexAttrib4fARB == NULL && glad_glVertexAttrib4fNV != NULL) glad_glVertexAttrib4fARB = (PFNGLVERTEXATTRIB4FARBPROC)glad_glVertexAttrib4fNV;
    if (glad_glVertexAttrib4fNV == NULL && glad_glVertexAttrib4f != NULL) glad_glVertexAttrib4fNV = (PFNGLVERTEXATTRIB4FNVPROC)glad_glVertexAttrib4f;
    if (glad_glVertexAttrib4fNV == NULL && glad_glVertexAttrib4fARB != NULL) glad_glVertexAttrib4fNV = (PFNGLVERTEXATTRIB4FNVPROC)glad_glVertexAttrib4fARB;
    if (glad_glVertexAttrib4fv == NULL && glad_glVertexAttrib4fvARB != NULL) glad_glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC)glad_glVertexAttrib4fvARB;
    if (glad_glVertexAttrib4fv == NULL && glad_glVertexAttrib4fvNV != NULL) glad_glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC)glad_glVertexAttrib4fvNV;
    if (glad_glVertexAttrib4fvARB == NULL && glad_glVertexAttrib4fv != NULL) glad_glVertexAttrib4fvARB = (PFNGLVERTEXATTRIB4FVARBPROC)glad_glVertexAttrib4fv;
    if (glad_glVertexAttrib4fvARB == NULL && glad_glVertexAttrib4fvNV != NULL) glad_glVertexAttrib4fvARB = (PFNGLVERTEXATTRIB4FVARBPROC)glad_glVertexAttrib4fvNV;
    if (glad_glVertexAttrib4fvNV == NULL && glad_glVertexAttrib4fv != NULL) glad_glVertexAttrib4fvNV = (PFNGLVERTEXATTRIB4FVNVPROC)glad_glVertexAttrib4fv;
    if (glad_glVertexAttrib4fvNV == NULL && glad_glVertexAttrib4fvARB != NULL) glad_glVertexAttrib4fvNV = (PFNGLVERTEXATTRIB4FVNVPROC)glad_glVertexAttrib4fvARB;
    if (glad_glVertexAttrib4iv == NULL && glad_glVertexAttrib4ivARB != NULL) glad_glVertexAttrib4iv = (PFNGLVERTEXATTRIB4IVPROC)glad_glVertexAttrib4ivARB;
    if (glad_glVertexAttrib4ivARB == NULL && glad_glVertexAttrib4iv != NULL) glad_glVertexAttrib4ivARB = (PFNGLVERTEXATTRIB4IVARBPROC)glad_glVertexAttrib4iv;
    if (glad_glVertexAttrib4Nbv == NULL && glad_glVertexAttrib4NbvARB != NULL) glad_glVertexAttrib4Nbv = (PFNGLVERTEXATTRIB4NBVPROC)glad_glVertexAttrib4NbvARB;
    if (glad_glVertexAttrib4NbvARB == NULL && glad_glVertexAttrib4Nbv != NULL) glad_glVertexAttrib4NbvARB = (PFNGLVERTEXATTRIB4NBVARBPROC)glad_glVertexAttrib4Nbv;
    if (glad_glVertexAttrib4Niv == NULL && glad_glVertexAttrib4NivARB != NULL) glad_glVertexAttrib4Niv = (PFNGLVERTEXATTRIB4NIVPROC)glad_glVertexAttrib4NivARB;
    if (glad_glVertexAttrib4NivARB == NULL && glad_glVertexAttrib4Niv != NULL) glad_glVertexAttrib4NivARB = (PFNGLVERTEXATTRIB4NIVARBPROC)glad_glVertexAttrib4Niv;
    if (glad_glVertexAttrib4Nsv == NULL && glad_glVertexAttrib4NsvARB != NULL) glad_glVertexAttrib4Nsv = (PFNGLVERTEXATTRIB4NSVPROC)glad_glVertexAttrib4NsvARB;
    if (glad_glVertexAttrib4NsvARB == NULL && glad_glVertexAttrib4Nsv != NULL) glad_glVertexAttrib4NsvARB = (PFNGLVERTEXATTRIB4NSVARBPROC)glad_glVertexAttrib4Nsv;
    if (glad_glVertexAttrib4Nub == NULL && glad_glVertexAttrib4NubARB != NULL) glad_glVertexAttrib4Nub = (PFNGLVERTEXATTRIB4NUBPROC)glad_glVertexAttrib4NubARB;
    if (glad_glVertexAttrib4Nub == NULL && glad_glVertexAttrib4ubNV != NULL) glad_glVertexAttrib4Nub = (PFNGLVERTEXATTRIB4NUBPROC)glad_glVertexAttrib4ubNV;
    if (glad_glVertexAttrib4NubARB == NULL && glad_glVertexAttrib4Nub != NULL) glad_glVertexAttrib4NubARB = (PFNGLVERTEXATTRIB4NUBARBPROC)glad_glVertexAttrib4Nub;
    if (glad_glVertexAttrib4NubARB == NULL && glad_glVertexAttrib4ubNV != NULL) glad_glVertexAttrib4NubARB = (PFNGLVERTEXATTRIB4NUBARBPROC)glad_glVertexAttrib4ubNV;
    if (glad_glVertexAttrib4Nubv == NULL && glad_glVertexAttrib4NubvARB != NULL) glad_glVertexAttrib4Nubv = (PFNGLVERTEXATTRIB4NUBVPROC)glad_glVertexAttrib4NubvARB;
    if (glad_glVertexAttrib4Nubv == NULL && glad_glVertexAttrib4ubvNV != NULL) glad_glVertexAttrib4Nubv = (PFNGLVERTEXATTRIB4NUBVPROC)glad_glVertexAttrib4ubvNV;
    if (glad_glVertexAttrib4NubvARB == NULL && glad_glVertexAttrib4Nubv != NULL) glad_glVertexAttrib4NubvARB = (PFNGLVERTEXATTRIB4NUBVARBPROC)glad_glVertexAttrib4Nubv;
    if (glad_glVertexAttrib4NubvARB == NULL && glad_glVertexAttrib4ubvNV != NULL) glad_glVertexAttrib4NubvARB = (PFNGLVERTEXATTRIB4NUBVARBPROC)glad_glVertexAttrib4ubvNV;
    if (glad_glVertexAttrib4Nuiv == NULL && glad_glVertexAttrib4NuivARB != NULL) glad_glVertexAttrib4Nuiv = (PFNGLVERTEXATTRIB4NUIVPROC)glad_glVertexAttrib4NuivARB;
    if (glad_glVertexAttrib4NuivARB == NULL && glad_glVertexAttrib4Nuiv != NULL) glad_glVertexAttrib4NuivARB = (PFNGLVERTEXATTRIB4NUIVARBPROC)glad_glVertexAttrib4Nuiv;
    if (glad_glVertexAttrib4Nusv == NULL && glad_glVertexAttrib4NusvARB != NULL) glad_glVertexAttrib4Nusv = (PFNGLVERTEXATTRIB4NUSVPROC)glad_glVertexAttrib4NusvARB;
    if (glad_glVertexAttrib4NusvARB == NULL && glad_glVertexAttrib4Nusv != NULL) glad_glVertexAttrib4NusvARB = (PFNGLVERTEXATTRIB4NUSVARBPROC)glad_glVertexAttrib4Nusv;
    if (glad_glVertexAttrib4s == NULL && glad_glVertexAttrib4sARB != NULL) glad_glVertexAttrib4s = (PFNGLVERTEXATTRIB4SPROC)glad_glVertexAttrib4sARB;
    if (glad_glVertexAttrib4s == NULL && glad_glVertexAttrib4sNV != NULL) glad_glVertexAttrib4s = (PFNGLVERTEXATTRIB4SPROC)glad_glVertexAttrib4sNV;
    if (glad_glVertexAttrib4sARB == NULL && glad_glVertexAttrib4s != NULL) glad_glVertexAttrib4sARB = (PFNGLVERTEXATTRIB4SARBPROC)glad_glVertexAttrib4s;
    if (glad_glVertexAttrib4sARB == NULL && glad_glVertexAttrib4sNV != NULL) glad_glVertexAttrib4sARB = (PFNGLVERTEXATTRIB4SARBPROC)glad_glVertexAttrib4sNV;
    if (glad_glVertexAttrib4sNV == NULL && glad_glVertexAttrib4s != NULL) glad_glVertexAttrib4sNV = (PFNGLVERTEXATTRIB4SNVPROC)glad_glVertexAttrib4s;
    if (glad_glVertexAttrib4sNV == NULL && glad_glVertexAttrib4sARB != NULL) glad_glVertexAttrib4sNV = (PFNGLVERTEXATTRIB4SNVPROC)glad_glVertexAttrib4sARB;
    if (glad_glVertexAttrib4sv == NULL && glad_glVertexAttrib4svARB != NULL) glad_glVertexAttrib4sv = (PFNGLVERTEXATTRIB4SVPROC)glad_glVertexAttrib4svARB;
    if (glad_glVertexAttrib4sv == NULL && glad_glVertexAttrib4svNV != NULL) glad_glVertexAttrib4sv = (PFNGLVERTEXATTRIB4SVPROC)glad_glVertexAttrib4svNV;
    if (glad_glVertexAttrib4svARB == NULL && glad_glVertexAttrib4sv != NULL) glad_glVertexAttrib4svARB = (PFNGLVERTEXATTRIB4SVARBPROC)glad_glVertexAttrib4sv;
    if (glad_glVertexAttrib4svARB == NULL && glad_glVertexAttrib4svNV != NULL) glad_glVertexAttrib4svARB = (PFNGLVERTEXATTRIB4SVARBPROC)glad_glVertexAttrib4svNV;
    if (glad_glVertexAttrib4svNV == NULL && glad_glVertexAttrib4sv != NULL) glad_glVertexAttrib4svNV = (PFNGLVERTEXATTRIB4SVNVPROC)glad_glVertexAttrib4sv;
    if (glad_glVertexAttrib4svNV == NULL && glad_glVertexAttrib4svARB != NULL) glad_glVertexAttrib4svNV = (PFNGLVERTEXATTRIB4SVNVPROC)glad_glVertexAttrib4svARB;
    if (glad_glVertexAttrib4ubNV == NULL && glad_glVertexAttrib4Nub != NULL) glad_glVertexAttrib4ubNV = (PFNGLVERTEXATTRIB4UBNVPROC)glad_glVertexAttrib4Nub;
    if (glad_glVertexAttrib4ubNV == NULL && glad_glVertexAttrib4NubARB != NULL) glad_glVertexAttrib4ubNV = (PFNGLVERTEXATTRIB4UBNVPROC)glad_glVertexAttrib4NubARB;
    if (glad_glVertexAttrib4ubv == NULL && glad_glVertexAttrib4ubvARB != NULL) glad_glVertexAttrib4ubv = (PFNGLVERTEXATTRIB4UBVPROC)glad_glVertexAttrib4ubvARB;
    if (glad_glVertexAttrib4ubvARB == NULL && glad_glVertexAttrib4ubv != NULL) glad_glVertexAttrib4ubvARB = (PFNGLVERTEXATTRIB4UBVARBPROC)glad_glVertexAttrib4ubv;
    if (glad_glVertexAttrib4ubvNV == NULL && glad_glVertexAttrib4Nubv != NULL) glad_glVertexAttrib4ubvNV = (PFNGLVERTEXATTRIB4UBVNVPROC)glad_glVertexAttrib4Nubv;
    if (glad_glVertexAttrib4ubvNV == NULL && glad_glVertexAttrib4NubvARB != NULL) glad_glVertexAttrib4ubvNV = (PFNGLVERTEXATTRIB4UBVNVPROC)glad_glVertexAttrib4NubvARB;
    if (glad_glVertexAttrib4uiv == NULL && glad_glVertexAttrib4uivARB != NULL) glad_glVertexAttrib4uiv = (PFNGLVERTEXATTRIB4UIVPROC)glad_glVertexAttrib4uivARB;
    if (glad_glVertexAttrib4uivARB == NULL && glad_glVertexAttrib4uiv != NULL) glad_glVertexAttrib4uivARB = (PFNGLVERTEXATTRIB4UIVARBPROC)glad_glVertexAttrib4uiv;
    if (glad_glVertexAttrib4usv == NULL && glad_glVertexAttrib4usvARB != NULL) glad_glVertexAttrib4usv = (PFNGLVERTEXATTRIB4USVPROC)glad_glVertexAttrib4usvARB;
    if (glad_glVertexAttrib4usvARB == NULL && glad_glVertexAttrib4usv != NULL) glad_glVertexAttrib4usvARB = (PFNGLVERTEXATTRIB4USVARBPROC)glad_glVertexAttrib4usv;
    if (glad_glVertexAttribDivisor == NULL && glad_glVertexAttribDivisorARB != NULL) glad_glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)glad_glVertexAttribDivisorARB;
    if (glad_glVertexAttribDivisorARB == NULL && glad_glVertexAttribDivisor != NULL) glad_glVertexAttribDivisorARB = (PFNGLVERTEXATTRIBDIVISORARBPROC)glad_glVertexAttribDivisor;
    if (glad_glVertexAttribI1i == NULL && glad_glVertexAttribI1iEXT != NULL) glad_glVertexAttribI1i = (PFNGLVERTEXATTRIBI1IPROC)glad_glVertexAttribI1iEXT;
    if (glad_glVertexAttribI1iEXT == NULL && glad_glVertexAttribI1i != NULL) glad_glVertexAttribI1iEXT = (PFNGLVERTEXATTRIBI1IEXTPROC)glad_glVertexAttribI1i;
    if (glad_glVertexAttribI1iv == NULL && glad_glVertexAttribI1ivEXT != NULL) glad_glVertexAttribI1iv = (PFNGLVERTEXATTRIBI1IVPROC)glad_glVertexAttribI1ivEXT;
    if (glad_glVertexAttribI1ivEXT == NULL && glad_glVertexAttribI1iv != NULL) glad_glVertexAttribI1ivEXT = (PFNGLVERTEXATTRIBI1IVEXTPROC)glad_glVertexAttribI1iv;
    if (glad_glVertexAttribI1ui == NULL && glad_glVertexAttribI1uiEXT != NULL) glad_glVertexAttribI1ui = (PFNGLVERTEXATTRIBI1UIPROC)glad_glVertexAttribI1uiEXT;
    if (glad_glVertexAttribI1uiEXT == NULL && glad_glVertexAttribI1ui != NULL) glad_glVertexAttribI1uiEXT = (PFNGLVERTEXATTRIBI1UIEXTPROC)glad_glVertexAttribI1ui;
    if (glad_glVertexAttribI1uiv == NULL && glad_glVertexAttribI1uivEXT != NULL) glad_glVertexAttribI1uiv = (PFNGLVERTEXATTRIBI1UIVPROC)glad_glVertexAttribI1uivEXT;
    if (glad_glVertexAttribI1uivEXT == NULL && glad_glVertexAttribI1uiv != NULL) glad_glVertexAttribI1uivEXT = (PFNGLVERTEXATTRIBI1UIVEXTPROC)glad_glVertexAttribI1uiv;
    if (glad_glVertexAttribI2i == NULL && glad_glVertexAttribI2iEXT != NULL) glad_glVertexAttribI2i = (PFNGLVERTEXATTRIBI2IPROC)glad_glVertexAttribI2iEXT;
    if (glad_glVertexAttribI2iEXT == NULL && glad_glVertexAttribI2i != NULL) glad_glVertexAttribI2iEXT = (PFNGLVERTEXATTRIBI2IEXTPROC)glad_glVertexAttribI2i;
    if (glad_glVertexAttribI2iv == NULL && glad_glVertexAttribI2ivEXT != NULL) glad_glVertexAttribI2iv = (PFNGLVERTEXATTRIBI2IVPROC)glad_glVertexAttribI2ivEXT;
    if (glad_glVertexAttribI2ivEXT == NULL && glad_glVertexAttribI2iv != NULL) glad_glVertexAttribI2ivEXT = (PFNGLVERTEXATTRIBI2IVEXTPROC)glad_glVertexAttribI2iv;
    if (glad_glVertexAttribI2ui == NULL && glad_glVertexAttribI2uiEXT != NULL) glad_glVertexAttribI2ui = (PFNGLVERTEXATTRIBI2UIPROC)glad_glVertexAttribI2uiEXT;
    if (glad_glVertexAttribI2uiEXT == NULL && glad_glVertexAttribI2ui != NULL) glad_glVertexAttribI2uiEXT = (PFNGLVERTEXATTRIBI2UIEXTPROC)glad_glVertexAttribI2ui;
    if (glad_glVertexAttribI2uiv == NULL && glad_glVertexAttribI2uivEXT != NULL) glad_glVertexAttribI2uiv = (PFNGLVERTEXATTRIBI2UIVPROC)glad_glVertexAttribI2uivEXT;
    if (glad_glVertexAttribI2uivEXT == NULL && glad_glVertexAttribI2uiv != NULL) glad_glVertexAttribI2uivEXT = (PFNGLVERTEXATTRIBI2UIVEXTPROC)glad_glVertexAttribI2uiv;
    if (glad_glVertexAttribI3i == NULL && glad_glVertexAttribI3iEXT != NULL) glad_glVertexAttribI3i = (PFNGLVERTEXATTRIBI3IPROC)glad_glVertexAttribI3iEXT;
    if (glad_glVertexAttribI3iEXT == NULL && glad_glVertexAttribI3i != NULL) glad_glVertexAttribI3iEXT = (PFNGLVERTEXATTRIBI3IEXTPROC)glad_glVertexAttribI3i;
    if (glad_glVertexAttribI3iv == NULL && glad_glVertexAttribI3ivEXT != NULL) glad_glVertexAttribI3iv = (PFNGLVERTEXATTRIBI3IVPROC)glad_glVertexAttribI3ivEXT;
    if (glad_glVertexAttribI3ivEXT == NULL && glad_glVertexAttribI3iv != NULL) glad_glVertexAttribI3ivEXT = (PFNGLVERTEXATTRIBI3IVEXTPROC)glad_glVertexAttribI3iv;
    if (glad_glVertexAttribI3ui == NULL && glad_glVertexAttribI3uiEXT != NULL) glad_glVertexAttribI3ui = (PFNGLVERTEXATTRIBI3UIPROC)glad_glVertexAttribI3uiEXT;
    if (glad_glVertexAttribI3uiEXT == NULL && glad_glVertexAttribI3ui != NULL) glad_glVertexAttribI3uiEXT = (PFNGLVERTEXATTRIBI3UIEXTPROC)glad_glVertexAttribI3ui;
    if (glad_glVertexAttribI3uiv == NULL && glad_glVertexAttribI3uivEXT != NULL) glad_glVertexAttribI3uiv = (PFNGLVERTEXATTRIBI3UIVPROC)glad_glVertexAttribI3uivEXT;
    if (glad_glVertexAttribI3uivEXT == NULL && glad_glVertexAttribI3uiv != NULL) glad_glVertexAttribI3uivEXT = (PFNGLVERTEXATTRIBI3UIVEXTPROC)glad_glVertexAttribI3uiv;
    if (glad_glVertexAttribI4bv == NULL && glad_glVertexAttribI4bvEXT != NULL) glad_glVertexAttribI4bv = (PFNGLVERTEXATTRIBI4BVPROC)glad_glVertexAttribI4bvEXT;
    if (glad_glVertexAttribI4bvEXT == NULL && glad_glVertexAttribI4bv != NULL) glad_glVertexAttribI4bvEXT = (PFNGLVERTEXATTRIBI4BVEXTPROC)glad_glVertexAttribI4bv;
    if (glad_glVertexAttribI4i == NULL && glad_glVertexAttribI4iEXT != NULL) glad_glVertexAttribI4i = (PFNGLVERTEXATTRIBI4IPROC)glad_glVertexAttribI4iEXT;
    if (glad_glVertexAttribI4iEXT == NULL && glad_glVertexAttribI4i != NULL) glad_glVertexAttribI4iEXT = (PFNGLVERTEXATTRIBI4IEXTPROC)glad_glVertexAttribI4i;
    if (glad_glVertexAttribI4iv == NULL && glad_glVertexAttribI4ivEXT != NULL) glad_glVertexAttribI4iv = (PFNGLVERTEXATTRIBI4IVPROC)glad_glVertexAttribI4ivEXT;
    if (glad_glVertexAttribI4ivEXT == NULL && glad_glVertexAttribI4iv != NULL) glad_glVertexAttribI4ivEXT = (PFNGLVERTEXATTRIBI4IVEXTPROC)glad_glVertexAttribI4iv;
    if (glad_glVertexAttribI4sv == NULL && glad_glVertexAttribI4svEXT != NULL) glad_glVertexAttribI4sv = (PFNGLVERTEXATTRIBI4SVPROC)glad_glVertexAttribI4svEXT;
    if (glad_glVertexAttribI4svEXT == NULL && glad_glVertexAttribI4sv != NULL) glad_glVertexAttribI4svEXT = (PFNGLVERTEXATTRIBI4SVEXTPROC)glad_glVertexAttribI4sv;
    if (glad_glVertexAttribI4ubv == NULL && glad_glVertexAttribI4ubvEXT != NULL) glad_glVertexAttribI4ubv = (PFNGLVERTEXATTRIBI4UBVPROC)glad_glVertexAttribI4ubvEXT;
    if (glad_glVertexAttribI4ubvEXT == NULL && glad_glVertexAttribI4ubv != NULL) glad_glVertexAttribI4ubvEXT = (PFNGLVERTEXATTRIBI4UBVEXTPROC)glad_glVertexAttribI4ubv;
    if (glad_glVertexAttribI4ui == NULL && glad_glVertexAttribI4uiEXT != NULL) glad_glVertexAttribI4ui = (PFNGLVERTEXATTRIBI4UIPROC)glad_glVertexAttribI4uiEXT;
    if (glad_glVertexAttribI4uiEXT == NULL && glad_glVertexAttribI4ui != NULL) glad_glVertexAttribI4uiEXT = (PFNGLVERTEXATTRIBI4UIEXTPROC)glad_glVertexAttribI4ui;
    if (glad_glVertexAttribI4uiv == NULL && glad_glVertexAttribI4uivEXT != NULL) glad_glVertexAttribI4uiv = (PFNGLVERTEXATTRIBI4UIVPROC)glad_glVertexAttribI4uivEXT;
    if (glad_glVertexAttribI4uivEXT == NULL && glad_glVertexAttribI4uiv != NULL) glad_glVertexAttribI4uivEXT = (PFNGLVERTEXATTRIBI4UIVEXTPROC)glad_glVertexAttribI4uiv;
    if (glad_glVertexAttribI4usv == NULL && glad_glVertexAttribI4usvEXT != NULL) glad_glVertexAttribI4usv = (PFNGLVERTEXATTRIBI4USVPROC)glad_glVertexAttribI4usvEXT;
    if (glad_glVertexAttribI4usvEXT == NULL && glad_glVertexAttribI4usv != NULL) glad_glVertexAttribI4usvEXT = (PFNGLVERTEXATTRIBI4USVEXTPROC)glad_glVertexAttribI4usv;
    if (glad_glVertexAttribIPointer == NULL && glad_glVertexAttribIPointerEXT != NULL) glad_glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC)glad_glVertexAttribIPointerEXT;
    if (glad_glVertexAttribIPointerEXT == NULL && glad_glVertexAttribIPointer != NULL) glad_glVertexAttribIPointerEXT = (PFNGLVERTEXATTRIBIPOINTEREXTPROC)glad_glVertexAttribIPointer;
    if (glad_glVertexAttribPointer == NULL && glad_glVertexAttribPointerARB != NULL) glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)glad_glVertexAttribPointerARB;
    if (glad_glVertexAttribPointerARB == NULL && glad_glVertexAttribPointer != NULL) glad_glVertexAttribPointerARB = (PFNGLVERTEXATTRIBPOINTERARBPROC)glad_glVertexAttribPointer;
}

static void glad_gl_free_extensions(char **exts_i) {
    if (exts_i != NULL) {
        unsigned int index;
        for(index = 0; exts_i[index]; index++) {
            free((void *) (exts_i[index]));
        }
        free((void *)exts_i);
        exts_i = NULL;
    }
}
static int glad_gl_get_extensions( const char **out_exts, char ***out_exts_i) {
#if defined(GL_ES_VERSION_3_0) || defined(GL_VERSION_3_0)
    if (glad_glGetStringi != NULL && glad_glGetIntegerv != NULL) {
        unsigned int index = 0;
        unsigned int num_exts_i = 0;
        char **exts_i = NULL;
        glad_glGetIntegerv(GL_NUM_EXTENSIONS, (int*) &num_exts_i);
        exts_i = (char **) malloc((num_exts_i + 1) * (sizeof *exts_i));
        if (exts_i == NULL) {
            return 0;
        }
        for(index = 0; index < num_exts_i; index++) {
            const char *gl_str_tmp = (const char*) glad_glGetStringi(GL_EXTENSIONS, index);
            size_t len = strlen(gl_str_tmp) + 1;

            char *local_str = (char*) malloc(len * sizeof(char));
            if(local_str == NULL) {
                exts_i[index] = NULL;
                glad_gl_free_extensions(exts_i);
                return 0;
            }

            memcpy(local_str, gl_str_tmp, len * sizeof(char));
            exts_i[index] = local_str;
        }
        exts_i[index] = NULL;

        *out_exts_i = exts_i;

        return 1;
    }
#else
    GLAD_UNUSED(out_exts_i);
#endif
    if (glad_glGetString == NULL) {
        return 0;
    }
    *out_exts = (const char *)glad_glGetString(GL_EXTENSIONS);
    return 1;
}
static int glad_gl_has_extension(const char *exts, char **exts_i, const char *ext) {
    if(exts_i) {
        unsigned int index;
        for(index = 0; exts_i[index]; index++) {
            const char *e = exts_i[index];
            if(strcmp(e, ext) == 0) {
                return 1;
            }
        }
    } else {
        const char *extensions;
        const char *loc;
        const char *terminator;
        extensions = exts;
        if(extensions == NULL || ext == NULL) {
            return 0;
        }
        while(1) {
            loc = strstr(extensions, ext);
            if(loc == NULL) {
                return 0;
            }
            terminator = loc + strlen(ext);
            if((loc == extensions || *(loc - 1) == ' ') &&
                (*terminator == ' ' || *terminator == '\0')) {
                return 1;
            }
            extensions = terminator;
        }
    }
    return 0;
}

static GLADapiproc glad_gl_get_proc_from_userptr(void *userptr, const char* name) {
    return (GLAD_GNUC_EXTENSION (GLADapiproc (*)(const char *name)) userptr)(name);
}

static int glad_gl_find_extensions_gl(void) {
    const char *exts = NULL;
    char **exts_i = NULL;
    if (!glad_gl_get_extensions(&exts, &exts_i)) return 0;

    GLAD_GL_APPLE_flush_buffer_range = glad_gl_has_extension(exts, exts_i, "GL_APPLE_flush_buffer_range");
    GLAD_GL_APPLE_vertex_array_object = glad_gl_has_extension(exts, exts_i, "GL_APPLE_vertex_array_object");
    GLAD_GL_ARB_blend_func_extended = glad_gl_has_extension(exts, exts_i, "GL_ARB_blend_func_extended");
    GLAD_GL_ARB_color_buffer_float = glad_gl_has_extension(exts, exts_i, "GL_ARB_color_buffer_float");
    GLAD_GL_ARB_copy_buffer = glad_gl_has_extension(exts, exts_i, "GL_ARB_copy_buffer");
    GLAD_GL_ARB_draw_buffers = glad_gl_has_extension(exts, exts_i, "GL_ARB_draw_buffers");
    GLAD_GL_ARB_draw_elements_base_vertex = glad_gl_has_extension(exts, exts_i, "GL_ARB_draw_elements_base_vertex");
    GLAD_GL_ARB_draw_instanced = glad_gl_has_extension(exts, exts_i, "GL_ARB_draw_instanced");
    GLAD_GL_ARB_framebuffer_object = glad_gl_has_extension(exts, exts_i, "GL_ARB_framebuffer_object");
    GLAD_GL_ARB_geometry_shader4 = glad_gl_has_extension(exts, exts_i, "GL_ARB_geometry_shader4");
    GLAD_GL_ARB_imaging = glad_gl_has_extension(exts, exts_i, "GL_ARB_imaging");
    GLAD_GL_ARB_instanced_arrays = glad_gl_has_extension(exts, exts_i, "GL_ARB_instanced_arrays");
    GLAD_GL_ARB_map_buffer_range = glad_gl_has_extension(exts, exts_i, "GL_ARB_map_buffer_range");
    GLAD_GL_ARB_multisample = glad_gl_has_extension(exts, exts_i, "GL_ARB_multisample");
    GLAD_GL_ARB_multitexture = glad_gl_has_extension(exts, exts_i, "GL_ARB_multitexture");
    GLAD_GL_ARB_occlusion_query = glad_gl_has_extension(exts, exts_i, "GL_ARB_occlusion_query");
    GLAD_GL_ARB_point_parameters = glad_gl_has_extension(exts, exts_i, "GL_ARB_point_parameters");
    GLAD_GL_ARB_provoking_vertex = glad_gl_has_extension(exts, exts_i, "GL_ARB_provoking_vertex");
    GLAD_GL_ARB_sampler_objects = glad_gl_has_extension(exts, exts_i, "GL_ARB_sampler_objects");
    GLAD_GL_ARB_shader_objects = glad_gl_has_extension(exts, exts_i, "GL_ARB_shader_objects");
    GLAD_GL_ARB_sync = glad_gl_has_extension(exts, exts_i, "GL_ARB_sync");
    GLAD_GL_ARB_texture_buffer_object = glad_gl_has_extension(exts, exts_i, "GL_ARB_texture_buffer_object");
    GLAD_GL_ARB_texture_compression = glad_gl_has_extension(exts, exts_i, "GL_ARB_texture_compression");
    GLAD_GL_ARB_texture_multisample = glad_gl_has_extension(exts, exts_i, "GL_ARB_texture_multisample");
    GLAD_GL_ARB_timer_query = glad_gl_has_extension(exts, exts_i, "GL_ARB_timer_query");
    GLAD_GL_ARB_uniform_buffer_object = glad_gl_has_extension(exts, exts_i, "GL_ARB_uniform_buffer_object");
    GLAD_GL_ARB_vertex_array_object = glad_gl_has_extension(exts, exts_i, "GL_ARB_vertex_array_object");
    GLAD_GL_ARB_vertex_buffer_object = glad_gl_has_extension(exts, exts_i, "GL_ARB_vertex_buffer_object");
    GLAD_GL_ARB_vertex_program = glad_gl_has_extension(exts, exts_i, "GL_ARB_vertex_program");
    GLAD_GL_ARB_vertex_shader = glad_gl_has_extension(exts, exts_i, "GL_ARB_vertex_shader");
    GLAD_GL_ARB_vertex_type_2_10_10_10_rev = glad_gl_has_extension(exts, exts_i, "GL_ARB_vertex_type_2_10_10_10_rev");
    GLAD_GL_ATI_draw_buffers = glad_gl_has_extension(exts, exts_i, "GL_ATI_draw_buffers");
    GLAD_GL_ATI_separate_stencil = glad_gl_has_extension(exts, exts_i, "GL_ATI_separate_stencil");
    GLAD_GL_EXT_blend_color = glad_gl_has_extension(exts, exts_i, "GL_EXT_blend_color");
    GLAD_GL_EXT_blend_equation_separate = glad_gl_has_extension(exts, exts_i, "GL_EXT_blend_equation_separate");
    GLAD_GL_EXT_blend_func_separate = glad_gl_has_extension(exts, exts_i, "GL_EXT_blend_func_separate");
    GLAD_GL_EXT_blend_minmax = glad_gl_has_extension(exts, exts_i, "GL_EXT_blend_minmax");
    GLAD_GL_EXT_copy_texture = glad_gl_has_extension(exts, exts_i, "GL_EXT_copy_texture");
    GLAD_GL_EXT_direct_state_access = glad_gl_has_extension(exts, exts_i, "GL_EXT_direct_state_access");
    GLAD_GL_EXT_draw_buffers2 = glad_gl_has_extension(exts, exts_i, "GL_EXT_draw_buffers2");
    GLAD_GL_EXT_draw_instanced = glad_gl_has_extension(exts, exts_i, "GL_EXT_draw_instanced");
    GLAD_GL_EXT_draw_range_elements = glad_gl_has_extension(exts, exts_i, "GL_EXT_draw_range_elements");
    GLAD_GL_EXT_framebuffer_blit = glad_gl_has_extension(exts, exts_i, "GL_EXT_framebuffer_blit");
    GLAD_GL_EXT_framebuffer_multisample = glad_gl_has_extension(exts, exts_i, "GL_EXT_framebuffer_multisample");
    GLAD_GL_EXT_framebuffer_object = glad_gl_has_extension(exts, exts_i, "GL_EXT_framebuffer_object");
    GLAD_GL_EXT_gpu_shader4 = glad_gl_has_extension(exts, exts_i, "GL_EXT_gpu_shader4");
    GLAD_GL_EXT_multi_draw_arrays = glad_gl_has_extension(exts, exts_i, "GL_EXT_multi_draw_arrays");
    GLAD_GL_EXT_point_parameters = glad_gl_has_extension(exts, exts_i, "GL_EXT_point_parameters");
    GLAD_GL_EXT_provoking_vertex = glad_gl_has_extension(exts, exts_i, "GL_EXT_provoking_vertex");
    GLAD_GL_EXT_subtexture = glad_gl_has_extension(exts, exts_i, "GL_EXT_subtexture");
    GLAD_GL_EXT_texture3D = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture3D");
    GLAD_GL_EXT_texture_array = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_array");
    GLAD_GL_EXT_texture_buffer_object = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_buffer_object");
    GLAD_GL_EXT_texture_integer = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_integer");
    GLAD_GL_EXT_texture_object = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_object");
    GLAD_GL_EXT_timer_query = glad_gl_has_extension(exts, exts_i, "GL_EXT_timer_query");
    GLAD_GL_EXT_transform_feedback = glad_gl_has_extension(exts, exts_i, "GL_EXT_transform_feedback");
    GLAD_GL_EXT_vertex_array = glad_gl_has_extension(exts, exts_i, "GL_EXT_vertex_array");
    GLAD_GL_INGR_blend_func_separate = glad_gl_has_extension(exts, exts_i, "GL_INGR_blend_func_separate");
    GLAD_GL_NVX_conditional_render = glad_gl_has_extension(exts, exts_i, "GL_NVX_conditional_render");
    GLAD_GL_NV_conditional_render = glad_gl_has_extension(exts, exts_i, "GL_NV_conditional_render");
    GLAD_GL_NV_explicit_multisample = glad_gl_has_extension(exts, exts_i, "GL_NV_explicit_multisample");
    GLAD_GL_NV_geometry_program4 = glad_gl_has_extension(exts, exts_i, "GL_NV_geometry_program4");
    GLAD_GL_NV_point_sprite = glad_gl_has_extension(exts, exts_i, "GL_NV_point_sprite");
    GLAD_GL_NV_transform_feedback = glad_gl_has_extension(exts, exts_i, "GL_NV_transform_feedback");
    GLAD_GL_NV_vertex_program = glad_gl_has_extension(exts, exts_i, "GL_NV_vertex_program");
    GLAD_GL_NV_vertex_program4 = glad_gl_has_extension(exts, exts_i, "GL_NV_vertex_program4");
    GLAD_GL_SGIS_point_parameters = glad_gl_has_extension(exts, exts_i, "GL_SGIS_point_parameters");

    glad_gl_free_extensions(exts_i);

    return 1;
}

static int glad_gl_find_core_gl(void) {
    int i;
    const char* version;
    const char* prefixes[] = {
        "OpenGL ES-CM ",
        "OpenGL ES-CL ",
        "OpenGL ES ",
        "OpenGL SC ",
        NULL
    };
    int major = 0;
    int minor = 0;
    version = (const char*) glad_glGetString(GL_VERSION);
    if (!version) return 0;
    for (i = 0;  prefixes[i];  i++) {
        const size_t length = strlen(prefixes[i]);
        if (strncmp(version, prefixes[i], length) == 0) {
            version += length;
            break;
        }
    }

    GLAD_IMPL_UTIL_SSCANF(version, "%d.%d", &major, &minor);

    GLAD_GL_VERSION_1_0 = (major == 1 && minor >= 0) || major > 1;
    GLAD_GL_VERSION_1_1 = (major == 1 && minor >= 1) || major > 1;
    GLAD_GL_VERSION_1_2 = (major == 1 && minor >= 2) || major > 1;
    GLAD_GL_VERSION_1_3 = (major == 1 && minor >= 3) || major > 1;
    GLAD_GL_VERSION_1_4 = (major == 1 && minor >= 4) || major > 1;
    GLAD_GL_VERSION_1_5 = (major == 1 && minor >= 5) || major > 1;
    GLAD_GL_VERSION_2_0 = (major == 2 && minor >= 0) || major > 2;
    GLAD_GL_VERSION_2_1 = (major == 2 && minor >= 1) || major > 2;
    GLAD_GL_VERSION_3_0 = (major == 3 && minor >= 0) || major > 3;
    GLAD_GL_VERSION_3_1 = (major == 3 && minor >= 1) || major > 3;
    GLAD_GL_VERSION_3_2 = (major == 3 && minor >= 2) || major > 3;
    GLAD_GL_VERSION_3_3 = (major == 3 && minor >= 3) || major > 3;

    return GLAD_MAKE_VERSION(major, minor);
}

int gladLoadGLUserPtr( GLADuserptrloadfunc load, void *userptr) {
    int version;

    glad_glGetString = (PFNGLGETSTRINGPROC) load(userptr, "glGetString");
    if(glad_glGetString == NULL) return 0;
    version = glad_gl_find_core_gl();

    glad_gl_load_GL_VERSION_1_0(load, userptr);
    glad_gl_load_GL_VERSION_1_1(load, userptr);
    glad_gl_load_GL_VERSION_1_2(load, userptr);
    glad_gl_load_GL_VERSION_1_3(load, userptr);
    glad_gl_load_GL_VERSION_1_4(load, userptr);
    glad_gl_load_GL_VERSION_1_5(load, userptr);
    glad_gl_load_GL_VERSION_2_0(load, userptr);
    glad_gl_load_GL_VERSION_2_1(load, userptr);
    glad_gl_load_GL_VERSION_3_0(load, userptr);
    glad_gl_load_GL_VERSION_3_1(load, userptr);
    glad_gl_load_GL_VERSION_3_2(load, userptr);
    glad_gl_load_GL_VERSION_3_3(load, userptr);

    if (!glad_gl_find_extensions_gl()) return 0;
    glad_gl_load_GL_APPLE_flush_buffer_range(load, userptr);
    glad_gl_load_GL_APPLE_vertex_array_object(load, userptr);
    glad_gl_load_GL_ARB_blend_func_extended(load, userptr);
    glad_gl_load_GL_ARB_color_buffer_float(load, userptr);
    glad_gl_load_GL_ARB_copy_buffer(load, userptr);
    glad_gl_load_GL_ARB_draw_buffers(load, userptr);
    glad_gl_load_GL_ARB_draw_elements_base_vertex(load, userptr);
    glad_gl_load_GL_ARB_draw_instanced(load, userptr);
    glad_gl_load_GL_ARB_framebuffer_object(load, userptr);
    glad_gl_load_GL_ARB_geometry_shader4(load, userptr);
    glad_gl_load_GL_ARB_imaging(load, userptr);
    glad_gl_load_GL_ARB_instanced_arrays(load, userptr);
    glad_gl_load_GL_ARB_map_buffer_range(load, userptr);
    glad_gl_load_GL_ARB_multisample(load, userptr);
    glad_gl_load_GL_ARB_multitexture(load, userptr);
    glad_gl_load_GL_ARB_occlusion_query(load, userptr);
    glad_gl_load_GL_ARB_point_parameters(load, userptr);
    glad_gl_load_GL_ARB_provoking_vertex(load, userptr);
    glad_gl_load_GL_ARB_sampler_objects(load, userptr);
    glad_gl_load_GL_ARB_shader_objects(load, userptr);
    glad_gl_load_GL_ARB_sync(load, userptr);
    glad_gl_load_GL_ARB_texture_buffer_object(load, userptr);
    glad_gl_load_GL_ARB_texture_compression(load, userptr);
    glad_gl_load_GL_ARB_texture_multisample(load, userptr);
    glad_gl_load_GL_ARB_timer_query(load, userptr);
    glad_gl_load_GL_ARB_uniform_buffer_object(load, userptr);
    glad_gl_load_GL_ARB_vertex_array_object(load, userptr);
    glad_gl_load_GL_ARB_vertex_buffer_object(load, userptr);
    glad_gl_load_GL_ARB_vertex_program(load, userptr);
    glad_gl_load_GL_ARB_vertex_shader(load, userptr);
    glad_gl_load_GL_ARB_vertex_type_2_10_10_10_rev(load, userptr);
    glad_gl_load_GL_ATI_draw_buffers(load, userptr);
    glad_gl_load_GL_ATI_separate_stencil(load, userptr);
    glad_gl_load_GL_EXT_blend_color(load, userptr);
    glad_gl_load_GL_EXT_blend_equation_separate(load, userptr);
    glad_gl_load_GL_EXT_blend_func_separate(load, userptr);
    glad_gl_load_GL_EXT_blend_minmax(load, userptr);
    glad_gl_load_GL_EXT_copy_texture(load, userptr);
    glad_gl_load_GL_EXT_direct_state_access(load, userptr);
    glad_gl_load_GL_EXT_draw_buffers2(load, userptr);
    glad_gl_load_GL_EXT_draw_instanced(load, userptr);
    glad_gl_load_GL_EXT_draw_range_elements(load, userptr);
    glad_gl_load_GL_EXT_framebuffer_blit(load, userptr);
    glad_gl_load_GL_EXT_framebuffer_multisample(load, userptr);
    glad_gl_load_GL_EXT_framebuffer_object(load, userptr);
    glad_gl_load_GL_EXT_gpu_shader4(load, userptr);
    glad_gl_load_GL_EXT_multi_draw_arrays(load, userptr);
    glad_gl_load_GL_EXT_point_parameters(load, userptr);
    glad_gl_load_GL_EXT_provoking_vertex(load, userptr);
    glad_gl_load_GL_EXT_subtexture(load, userptr);
    glad_gl_load_GL_EXT_texture3D(load, userptr);
    glad_gl_load_GL_EXT_texture_array(load, userptr);
    glad_gl_load_GL_EXT_texture_buffer_object(load, userptr);
    glad_gl_load_GL_EXT_texture_integer(load, userptr);
    glad_gl_load_GL_EXT_texture_object(load, userptr);
    glad_gl_load_GL_EXT_timer_query(load, userptr);
    glad_gl_load_GL_EXT_transform_feedback(load, userptr);
    glad_gl_load_GL_EXT_vertex_array(load, userptr);
    glad_gl_load_GL_INGR_blend_func_separate(load, userptr);
    glad_gl_load_GL_NVX_conditional_render(load, userptr);
    glad_gl_load_GL_NV_conditional_render(load, userptr);
    glad_gl_load_GL_NV_explicit_multisample(load, userptr);
    glad_gl_load_GL_NV_geometry_program4(load, userptr);
    glad_gl_load_GL_NV_point_sprite(load, userptr);
    glad_gl_load_GL_NV_transform_feedback(load, userptr);
    glad_gl_load_GL_NV_vertex_program(load, userptr);
    glad_gl_load_GL_NV_vertex_program4(load, userptr);
    glad_gl_load_GL_SGIS_point_parameters(load, userptr);


    glad_gl_resolve_aliases();

    return version;
}


int gladLoadGL( GLADloadfunc load) {
    return gladLoadGLUserPtr( glad_gl_get_proc_from_userptr, GLAD_GNUC_EXTENSION (void*) load);
}



 
void gladInstallGLDebug(void) {
    glad_debug_glActiveTexture = glad_debug_impl_glActiveTexture;
    glad_debug_glActiveTextureARB = glad_debug_impl_glActiveTextureARB;
    glad_debug_glActiveVaryingNV = glad_debug_impl_glActiveVaryingNV;
    glad_debug_glAreProgramsResidentNV = glad_debug_impl_glAreProgramsResidentNV;
    glad_debug_glAreTexturesResidentEXT = glad_debug_impl_glAreTexturesResidentEXT;
    glad_debug_glArrayElementEXT = glad_debug_impl_glArrayElementEXT;
    glad_debug_glAttachObjectARB = glad_debug_impl_glAttachObjectARB;
    glad_debug_glAttachShader = glad_debug_impl_glAttachShader;
    glad_debug_glBeginConditionalRender = glad_debug_impl_glBeginConditionalRender;
    glad_debug_glBeginConditionalRenderNV = glad_debug_impl_glBeginConditionalRenderNV;
    glad_debug_glBeginConditionalRenderNVX = glad_debug_impl_glBeginConditionalRenderNVX;
    glad_debug_glBeginQuery = glad_debug_impl_glBeginQuery;
    glad_debug_glBeginQueryARB = glad_debug_impl_glBeginQueryARB;
    glad_debug_glBeginTransformFeedback = glad_debug_impl_glBeginTransformFeedback;
    glad_debug_glBeginTransformFeedbackEXT = glad_debug_impl_glBeginTransformFeedbackEXT;
    glad_debug_glBeginTransformFeedbackNV = glad_debug_impl_glBeginTransformFeedbackNV;
    glad_debug_glBindAttribLocation = glad_debug_impl_glBindAttribLocation;
    glad_debug_glBindAttribLocationARB = glad_debug_impl_glBindAttribLocationARB;
    glad_debug_glBindBuffer = glad_debug_impl_glBindBuffer;
    glad_debug_glBindBufferARB = glad_debug_impl_glBindBufferARB;
    glad_debug_glBindBufferBase = glad_debug_impl_glBindBufferBase;
    glad_debug_glBindBufferBaseEXT = glad_debug_impl_glBindBufferBaseEXT;
    glad_debug_glBindBufferBaseNV = glad_debug_impl_glBindBufferBaseNV;
    glad_debug_glBindBufferOffsetEXT = glad_debug_impl_glBindBufferOffsetEXT;
    glad_debug_glBindBufferOffsetNV = glad_debug_impl_glBindBufferOffsetNV;
    glad_debug_glBindBufferRange = glad_debug_impl_glBindBufferRange;
    glad_debug_glBindBufferRangeEXT = glad_debug_impl_glBindBufferRangeEXT;
    glad_debug_glBindBufferRangeNV = glad_debug_impl_glBindBufferRangeNV;
    glad_debug_glBindFragDataLocation = glad_debug_impl_glBindFragDataLocation;
    glad_debug_glBindFragDataLocationEXT = glad_debug_impl_glBindFragDataLocationEXT;
    glad_debug_glBindFragDataLocationIndexed = glad_debug_impl_glBindFragDataLocationIndexed;
    glad_debug_glBindFramebuffer = glad_debug_impl_glBindFramebuffer;
    glad_debug_glBindFramebufferEXT = glad_debug_impl_glBindFramebufferEXT;
    glad_debug_glBindMultiTextureEXT = glad_debug_impl_glBindMultiTextureEXT;
    glad_debug_glBindProgramARB = glad_debug_impl_glBindProgramARB;
    glad_debug_glBindProgramNV = glad_debug_impl_glBindProgramNV;
    glad_debug_glBindRenderbuffer = glad_debug_impl_glBindRenderbuffer;
    glad_debug_glBindRenderbufferEXT = glad_debug_impl_glBindRenderbufferEXT;
    glad_debug_glBindSampler = glad_debug_impl_glBindSampler;
    glad_debug_glBindTexture = glad_debug_impl_glBindTexture;
    glad_debug_glBindTextureEXT = glad_debug_impl_glBindTextureEXT;
    glad_debug_glBindVertexArray = glad_debug_impl_glBindVertexArray;
    glad_debug_glBindVertexArrayAPPLE = glad_debug_impl_glBindVertexArrayAPPLE;
    glad_debug_glBlendColor = glad_debug_impl_glBlendColor;
    glad_debug_glBlendColorEXT = glad_debug_impl_glBlendColorEXT;
    glad_debug_glBlendEquation = glad_debug_impl_glBlendEquation;
    glad_debug_glBlendEquationEXT = glad_debug_impl_glBlendEquationEXT;
    glad_debug_glBlendEquationSeparate = glad_debug_impl_glBlendEquationSeparate;
    glad_debug_glBlendEquationSeparateEXT = glad_debug_impl_glBlendEquationSeparateEXT;
    glad_debug_glBlendFunc = glad_debug_impl_glBlendFunc;
    glad_debug_glBlendFuncSeparate = glad_debug_impl_glBlendFuncSeparate;
    glad_debug_glBlendFuncSeparateEXT = glad_debug_impl_glBlendFuncSeparateEXT;
    glad_debug_glBlendFuncSeparateINGR = glad_debug_impl_glBlendFuncSeparateINGR;
    glad_debug_glBlitFramebuffer = glad_debug_impl_glBlitFramebuffer;
    glad_debug_glBlitFramebufferEXT = glad_debug_impl_glBlitFramebufferEXT;
    glad_debug_glBufferData = glad_debug_impl_glBufferData;
    glad_debug_glBufferDataARB = glad_debug_impl_glBufferDataARB;
    glad_debug_glBufferParameteriAPPLE = glad_debug_impl_glBufferParameteriAPPLE;
    glad_debug_glBufferSubData = glad_debug_impl_glBufferSubData;
    glad_debug_glBufferSubDataARB = glad_debug_impl_glBufferSubDataARB;
    glad_debug_glCheckFramebufferStatus = glad_debug_impl_glCheckFramebufferStatus;
    glad_debug_glCheckFramebufferStatusEXT = glad_debug_impl_glCheckFramebufferStatusEXT;
    glad_debug_glCheckNamedFramebufferStatusEXT = glad_debug_impl_glCheckNamedFramebufferStatusEXT;
    glad_debug_glClampColor = glad_debug_impl_glClampColor;
    glad_debug_glClampColorARB = glad_debug_impl_glClampColorARB;
    glad_debug_glClear = glad_debug_impl_glClear;
    glad_debug_glClearBufferfi = glad_debug_impl_glClearBufferfi;
    glad_debug_glClearBufferfv = glad_debug_impl_glClearBufferfv;
    glad_debug_glClearBufferiv = glad_debug_impl_glClearBufferiv;
    glad_debug_glClearBufferuiv = glad_debug_impl_glClearBufferuiv;
    glad_debug_glClearColor = glad_debug_impl_glClearColor;
    glad_debug_glClearColorIiEXT = glad_debug_impl_glClearColorIiEXT;
    glad_debug_glClearColorIuiEXT = glad_debug_impl_glClearColorIuiEXT;
    glad_debug_glClearDepth = glad_debug_impl_glClearDepth;
    glad_debug_glClearNamedBufferDataEXT = glad_debug_impl_glClearNamedBufferDataEXT;
    glad_debug_glClearNamedBufferSubDataEXT = glad_debug_impl_glClearNamedBufferSubDataEXT;
    glad_debug_glClearStencil = glad_debug_impl_glClearStencil;
    glad_debug_glClientActiveTextureARB = glad_debug_impl_glClientActiveTextureARB;
    glad_debug_glClientAttribDefaultEXT = glad_debug_impl_glClientAttribDefaultEXT;
    glad_debug_glClientWaitSync = glad_debug_impl_glClientWaitSync;
    glad_debug_glColorMask = glad_debug_impl_glColorMask;
    glad_debug_glColorMaskIndexedEXT = glad_debug_impl_glColorMaskIndexedEXT;
    glad_debug_glColorMaski = glad_debug_impl_glColorMaski;
    glad_debug_glColorPointerEXT = glad_debug_impl_glColorPointerEXT;
    glad_debug_glCompileShader = glad_debug_impl_glCompileShader;
    glad_debug_glCompileShaderARB = glad_debug_impl_glCompileShaderARB;
    glad_debug_glCompressedMultiTexImage1DEXT = glad_debug_impl_glCompressedMultiTexImage1DEXT;
    glad_debug_glCompressedMultiTexImage2DEXT = glad_debug_impl_glCompressedMultiTexImage2DEXT;
    glad_debug_glCompressedMultiTexImage3DEXT = glad_debug_impl_glCompressedMultiTexImage3DEXT;
    glad_debug_glCompressedMultiTexSubImage1DEXT = glad_debug_impl_glCompressedMultiTexSubImage1DEXT;
    glad_debug_glCompressedMultiTexSubImage2DEXT = glad_debug_impl_glCompressedMultiTexSubImage2DEXT;
    glad_debug_glCompressedMultiTexSubImage3DEXT = glad_debug_impl_glCompressedMultiTexSubImage3DEXT;
    glad_debug_glCompressedTexImage1D = glad_debug_impl_glCompressedTexImage1D;
    glad_debug_glCompressedTexImage1DARB = glad_debug_impl_glCompressedTexImage1DARB;
    glad_debug_glCompressedTexImage2D = glad_debug_impl_glCompressedTexImage2D;
    glad_debug_glCompressedTexImage2DARB = glad_debug_impl_glCompressedTexImage2DARB;
    glad_debug_glCompressedTexImage3D = glad_debug_impl_glCompressedTexImage3D;
    glad_debug_glCompressedTexImage3DARB = glad_debug_impl_glCompressedTexImage3DARB;
    glad_debug_glCompressedTexSubImage1D = glad_debug_impl_glCompressedTexSubImage1D;
    glad_debug_glCompressedTexSubImage1DARB = glad_debug_impl_glCompressedTexSubImage1DARB;
    glad_debug_glCompressedTexSubImage2D = glad_debug_impl_glCompressedTexSubImage2D;
    glad_debug_glCompressedTexSubImage2DARB = glad_debug_impl_glCompressedTexSubImage2DARB;
    glad_debug_glCompressedTexSubImage3D = glad_debug_impl_glCompressedTexSubImage3D;
    glad_debug_glCompressedTexSubImage3DARB = glad_debug_impl_glCompressedTexSubImage3DARB;
    glad_debug_glCompressedTextureImage1DEXT = glad_debug_impl_glCompressedTextureImage1DEXT;
    glad_debug_glCompressedTextureImage2DEXT = glad_debug_impl_glCompressedTextureImage2DEXT;
    glad_debug_glCompressedTextureImage3DEXT = glad_debug_impl_glCompressedTextureImage3DEXT;
    glad_debug_glCompressedTextureSubImage1DEXT = glad_debug_impl_glCompressedTextureSubImage1DEXT;
    glad_debug_glCompressedTextureSubImage2DEXT = glad_debug_impl_glCompressedTextureSubImage2DEXT;
    glad_debug_glCompressedTextureSubImage3DEXT = glad_debug_impl_glCompressedTextureSubImage3DEXT;
    glad_debug_glCopyBufferSubData = glad_debug_impl_glCopyBufferSubData;
    glad_debug_glCopyMultiTexImage1DEXT = glad_debug_impl_glCopyMultiTexImage1DEXT;
    glad_debug_glCopyMultiTexImage2DEXT = glad_debug_impl_glCopyMultiTexImage2DEXT;
    glad_debug_glCopyMultiTexSubImage1DEXT = glad_debug_impl_glCopyMultiTexSubImage1DEXT;
    glad_debug_glCopyMultiTexSubImage2DEXT = glad_debug_impl_glCopyMultiTexSubImage2DEXT;
    glad_debug_glCopyMultiTexSubImage3DEXT = glad_debug_impl_glCopyMultiTexSubImage3DEXT;
    glad_debug_glCopyTexImage1D = glad_debug_impl_glCopyTexImage1D;
    glad_debug_glCopyTexImage1DEXT = glad_debug_impl_glCopyTexImage1DEXT;
    glad_debug_glCopyTexImage2D = glad_debug_impl_glCopyTexImage2D;
    glad_debug_glCopyTexImage2DEXT = glad_debug_impl_glCopyTexImage2DEXT;
    glad_debug_glCopyTexSubImage1D = glad_debug_impl_glCopyTexSubImage1D;
    glad_debug_glCopyTexSubImage1DEXT = glad_debug_impl_glCopyTexSubImage1DEXT;
    glad_debug_glCopyTexSubImage2D = glad_debug_impl_glCopyTexSubImage2D;
    glad_debug_glCopyTexSubImage2DEXT = glad_debug_impl_glCopyTexSubImage2DEXT;
    glad_debug_glCopyTexSubImage3D = glad_debug_impl_glCopyTexSubImage3D;
    glad_debug_glCopyTexSubImage3DEXT = glad_debug_impl_glCopyTexSubImage3DEXT;
    glad_debug_glCopyTextureImage1DEXT = glad_debug_impl_glCopyTextureImage1DEXT;
    glad_debug_glCopyTextureImage2DEXT = glad_debug_impl_glCopyTextureImage2DEXT;
    glad_debug_glCopyTextureSubImage1DEXT = glad_debug_impl_glCopyTextureSubImage1DEXT;
    glad_debug_glCopyTextureSubImage2DEXT = glad_debug_impl_glCopyTextureSubImage2DEXT;
    glad_debug_glCopyTextureSubImage3DEXT = glad_debug_impl_glCopyTextureSubImage3DEXT;
    glad_debug_glCreateProgram = glad_debug_impl_glCreateProgram;
    glad_debug_glCreateProgramObjectARB = glad_debug_impl_glCreateProgramObjectARB;
    glad_debug_glCreateShader = glad_debug_impl_glCreateShader;
    glad_debug_glCreateShaderObjectARB = glad_debug_impl_glCreateShaderObjectARB;
    glad_debug_glCullFace = glad_debug_impl_glCullFace;
    glad_debug_glDeleteBuffers = glad_debug_impl_glDeleteBuffers;
    glad_debug_glDeleteBuffersARB = glad_debug_impl_glDeleteBuffersARB;
    glad_debug_glDeleteFramebuffers = glad_debug_impl_glDeleteFramebuffers;
    glad_debug_glDeleteFramebuffersEXT = glad_debug_impl_glDeleteFramebuffersEXT;
    glad_debug_glDeleteObjectARB = glad_debug_impl_glDeleteObjectARB;
    glad_debug_glDeleteProgram = glad_debug_impl_glDeleteProgram;
    glad_debug_glDeleteProgramsARB = glad_debug_impl_glDeleteProgramsARB;
    glad_debug_glDeleteProgramsNV = glad_debug_impl_glDeleteProgramsNV;
    glad_debug_glDeleteQueries = glad_debug_impl_glDeleteQueries;
    glad_debug_glDeleteQueriesARB = glad_debug_impl_glDeleteQueriesARB;
    glad_debug_glDeleteRenderbuffers = glad_debug_impl_glDeleteRenderbuffers;
    glad_debug_glDeleteRenderbuffersEXT = glad_debug_impl_glDeleteRenderbuffersEXT;
    glad_debug_glDeleteSamplers = glad_debug_impl_glDeleteSamplers;
    glad_debug_glDeleteShader = glad_debug_impl_glDeleteShader;
    glad_debug_glDeleteSync = glad_debug_impl_glDeleteSync;
    glad_debug_glDeleteTextures = glad_debug_impl_glDeleteTextures;
    glad_debug_glDeleteTexturesEXT = glad_debug_impl_glDeleteTexturesEXT;
    glad_debug_glDeleteVertexArrays = glad_debug_impl_glDeleteVertexArrays;
    glad_debug_glDeleteVertexArraysAPPLE = glad_debug_impl_glDeleteVertexArraysAPPLE;
    glad_debug_glDepthFunc = glad_debug_impl_glDepthFunc;
    glad_debug_glDepthMask = glad_debug_impl_glDepthMask;
    glad_debug_glDepthRange = glad_debug_impl_glDepthRange;
    glad_debug_glDetachObjectARB = glad_debug_impl_glDetachObjectARB;
    glad_debug_glDetachShader = glad_debug_impl_glDetachShader;
    glad_debug_glDisable = glad_debug_impl_glDisable;
    glad_debug_glDisableClientStateIndexedEXT = glad_debug_impl_glDisableClientStateIndexedEXT;
    glad_debug_glDisableClientStateiEXT = glad_debug_impl_glDisableClientStateiEXT;
    glad_debug_glDisableIndexedEXT = glad_debug_impl_glDisableIndexedEXT;
    glad_debug_glDisableVertexArrayAttribEXT = glad_debug_impl_glDisableVertexArrayAttribEXT;
    glad_debug_glDisableVertexArrayEXT = glad_debug_impl_glDisableVertexArrayEXT;
    glad_debug_glDisableVertexAttribArray = glad_debug_impl_glDisableVertexAttribArray;
    glad_debug_glDisableVertexAttribArrayARB = glad_debug_impl_glDisableVertexAttribArrayARB;
    glad_debug_glDisablei = glad_debug_impl_glDisablei;
    glad_debug_glDrawArrays = glad_debug_impl_glDrawArrays;
    glad_debug_glDrawArraysEXT = glad_debug_impl_glDrawArraysEXT;
    glad_debug_glDrawArraysInstanced = glad_debug_impl_glDrawArraysInstanced;
    glad_debug_glDrawArraysInstancedARB = glad_debug_impl_glDrawArraysInstancedARB;
    glad_debug_glDrawArraysInstancedEXT = glad_debug_impl_glDrawArraysInstancedEXT;
    glad_debug_glDrawBuffer = glad_debug_impl_glDrawBuffer;
    glad_debug_glDrawBuffers = glad_debug_impl_glDrawBuffers;
    glad_debug_glDrawBuffersARB = glad_debug_impl_glDrawBuffersARB;
    glad_debug_glDrawBuffersATI = glad_debug_impl_glDrawBuffersATI;
    glad_debug_glDrawElements = glad_debug_impl_glDrawElements;
    glad_debug_glDrawElementsBaseVertex = glad_debug_impl_glDrawElementsBaseVertex;
    glad_debug_glDrawElementsInstanced = glad_debug_impl_glDrawElementsInstanced;
    glad_debug_glDrawElementsInstancedARB = glad_debug_impl_glDrawElementsInstancedARB;
    glad_debug_glDrawElementsInstancedBaseVertex = glad_debug_impl_glDrawElementsInstancedBaseVertex;
    glad_debug_glDrawElementsInstancedEXT = glad_debug_impl_glDrawElementsInstancedEXT;
    glad_debug_glDrawRangeElements = glad_debug_impl_glDrawRangeElements;
    glad_debug_glDrawRangeElementsBaseVertex = glad_debug_impl_glDrawRangeElementsBaseVertex;
    glad_debug_glDrawRangeElementsEXT = glad_debug_impl_glDrawRangeElementsEXT;
    glad_debug_glEdgeFlagPointerEXT = glad_debug_impl_glEdgeFlagPointerEXT;
    glad_debug_glEnable = glad_debug_impl_glEnable;
    glad_debug_glEnableClientStateIndexedEXT = glad_debug_impl_glEnableClientStateIndexedEXT;
    glad_debug_glEnableClientStateiEXT = glad_debug_impl_glEnableClientStateiEXT;
    glad_debug_glEnableIndexedEXT = glad_debug_impl_glEnableIndexedEXT;
    glad_debug_glEnableVertexArrayAttribEXT = glad_debug_impl_glEnableVertexArrayAttribEXT;
    glad_debug_glEnableVertexArrayEXT = glad_debug_impl_glEnableVertexArrayEXT;
    glad_debug_glEnableVertexAttribArray = glad_debug_impl_glEnableVertexAttribArray;
    glad_debug_glEnableVertexAttribArrayARB = glad_debug_impl_glEnableVertexAttribArrayARB;
    glad_debug_glEnablei = glad_debug_impl_glEnablei;
    glad_debug_glEndConditionalRender = glad_debug_impl_glEndConditionalRender;
    glad_debug_glEndConditionalRenderNV = glad_debug_impl_glEndConditionalRenderNV;
    glad_debug_glEndConditionalRenderNVX = glad_debug_impl_glEndConditionalRenderNVX;
    glad_debug_glEndQuery = glad_debug_impl_glEndQuery;
    glad_debug_glEndQueryARB = glad_debug_impl_glEndQueryARB;
    glad_debug_glEndTransformFeedback = glad_debug_impl_glEndTransformFeedback;
    glad_debug_glEndTransformFeedbackEXT = glad_debug_impl_glEndTransformFeedbackEXT;
    glad_debug_glEndTransformFeedbackNV = glad_debug_impl_glEndTransformFeedbackNV;
    glad_debug_glExecuteProgramNV = glad_debug_impl_glExecuteProgramNV;
    glad_debug_glFenceSync = glad_debug_impl_glFenceSync;
    glad_debug_glFinish = glad_debug_impl_glFinish;
    glad_debug_glFlush = glad_debug_impl_glFlush;
    glad_debug_glFlushMappedBufferRange = glad_debug_impl_glFlushMappedBufferRange;
    glad_debug_glFlushMappedBufferRangeAPPLE = glad_debug_impl_glFlushMappedBufferRangeAPPLE;
    glad_debug_glFlushMappedNamedBufferRangeEXT = glad_debug_impl_glFlushMappedNamedBufferRangeEXT;
    glad_debug_glFramebufferDrawBufferEXT = glad_debug_impl_glFramebufferDrawBufferEXT;
    glad_debug_glFramebufferDrawBuffersEXT = glad_debug_impl_glFramebufferDrawBuffersEXT;
    glad_debug_glFramebufferReadBufferEXT = glad_debug_impl_glFramebufferReadBufferEXT;
    glad_debug_glFramebufferRenderbuffer = glad_debug_impl_glFramebufferRenderbuffer;
    glad_debug_glFramebufferRenderbufferEXT = glad_debug_impl_glFramebufferRenderbufferEXT;
    glad_debug_glFramebufferTexture = glad_debug_impl_glFramebufferTexture;
    glad_debug_glFramebufferTexture1D = glad_debug_impl_glFramebufferTexture1D;
    glad_debug_glFramebufferTexture1DEXT = glad_debug_impl_glFramebufferTexture1DEXT;
    glad_debug_glFramebufferTexture2D = glad_debug_impl_glFramebufferTexture2D;
    glad_debug_glFramebufferTexture2DEXT = glad_debug_impl_glFramebufferTexture2DEXT;
    glad_debug_glFramebufferTexture3D = glad_debug_impl_glFramebufferTexture3D;
    glad_debug_glFramebufferTexture3DEXT = glad_debug_impl_glFramebufferTexture3DEXT;
    glad_debug_glFramebufferTextureARB = glad_debug_impl_glFramebufferTextureARB;
    glad_debug_glFramebufferTextureEXT = glad_debug_impl_glFramebufferTextureEXT;
    glad_debug_glFramebufferTextureFaceARB = glad_debug_impl_glFramebufferTextureFaceARB;
    glad_debug_glFramebufferTextureFaceEXT = glad_debug_impl_glFramebufferTextureFaceEXT;
    glad_debug_glFramebufferTextureLayer = glad_debug_impl_glFramebufferTextureLayer;
    glad_debug_glFramebufferTextureLayerARB = glad_debug_impl_glFramebufferTextureLayerARB;
    glad_debug_glFramebufferTextureLayerEXT = glad_debug_impl_glFramebufferTextureLayerEXT;
    glad_debug_glFrontFace = glad_debug_impl_glFrontFace;
    glad_debug_glGenBuffers = glad_debug_impl_glGenBuffers;
    glad_debug_glGenBuffersARB = glad_debug_impl_glGenBuffersARB;
    glad_debug_glGenFramebuffers = glad_debug_impl_glGenFramebuffers;
    glad_debug_glGenFramebuffersEXT = glad_debug_impl_glGenFramebuffersEXT;
    glad_debug_glGenProgramsARB = glad_debug_impl_glGenProgramsARB;
    glad_debug_glGenProgramsNV = glad_debug_impl_glGenProgramsNV;
    glad_debug_glGenQueries = glad_debug_impl_glGenQueries;
    glad_debug_glGenQueriesARB = glad_debug_impl_glGenQueriesARB;
    glad_debug_glGenRenderbuffers = glad_debug_impl_glGenRenderbuffers;
    glad_debug_glGenRenderbuffersEXT = glad_debug_impl_glGenRenderbuffersEXT;
    glad_debug_glGenSamplers = glad_debug_impl_glGenSamplers;
    glad_debug_glGenTextures = glad_debug_impl_glGenTextures;
    glad_debug_glGenTexturesEXT = glad_debug_impl_glGenTexturesEXT;
    glad_debug_glGenVertexArrays = glad_debug_impl_glGenVertexArrays;
    glad_debug_glGenVertexArraysAPPLE = glad_debug_impl_glGenVertexArraysAPPLE;
    glad_debug_glGenerateMipmap = glad_debug_impl_glGenerateMipmap;
    glad_debug_glGenerateMipmapEXT = glad_debug_impl_glGenerateMipmapEXT;
    glad_debug_glGenerateMultiTexMipmapEXT = glad_debug_impl_glGenerateMultiTexMipmapEXT;
    glad_debug_glGenerateTextureMipmapEXT = glad_debug_impl_glGenerateTextureMipmapEXT;
    glad_debug_glGetActiveAttrib = glad_debug_impl_glGetActiveAttrib;
    glad_debug_glGetActiveAttribARB = glad_debug_impl_glGetActiveAttribARB;
    glad_debug_glGetActiveUniform = glad_debug_impl_glGetActiveUniform;
    glad_debug_glGetActiveUniformARB = glad_debug_impl_glGetActiveUniformARB;
    glad_debug_glGetActiveUniformBlockName = glad_debug_impl_glGetActiveUniformBlockName;
    glad_debug_glGetActiveUniformBlockiv = glad_debug_impl_glGetActiveUniformBlockiv;
    glad_debug_glGetActiveUniformName = glad_debug_impl_glGetActiveUniformName;
    glad_debug_glGetActiveUniformsiv = glad_debug_impl_glGetActiveUniformsiv;
    glad_debug_glGetActiveVaryingNV = glad_debug_impl_glGetActiveVaryingNV;
    glad_debug_glGetAttachedObjectsARB = glad_debug_impl_glGetAttachedObjectsARB;
    glad_debug_glGetAttachedShaders = glad_debug_impl_glGetAttachedShaders;
    glad_debug_glGetAttribLocation = glad_debug_impl_glGetAttribLocation;
    glad_debug_glGetAttribLocationARB = glad_debug_impl_glGetAttribLocationARB;
    glad_debug_glGetBooleanIndexedvEXT = glad_debug_impl_glGetBooleanIndexedvEXT;
    glad_debug_glGetBooleani_v = glad_debug_impl_glGetBooleani_v;
    glad_debug_glGetBooleanv = glad_debug_impl_glGetBooleanv;
    glad_debug_glGetBufferParameteri64v = glad_debug_impl_glGetBufferParameteri64v;
    glad_debug_glGetBufferParameteriv = glad_debug_impl_glGetBufferParameteriv;
    glad_debug_glGetBufferParameterivARB = glad_debug_impl_glGetBufferParameterivARB;
    glad_debug_glGetBufferPointerv = glad_debug_impl_glGetBufferPointerv;
    glad_debug_glGetBufferPointervARB = glad_debug_impl_glGetBufferPointervARB;
    glad_debug_glGetBufferSubData = glad_debug_impl_glGetBufferSubData;
    glad_debug_glGetBufferSubDataARB = glad_debug_impl_glGetBufferSubDataARB;
    glad_debug_glGetCompressedMultiTexImageEXT = glad_debug_impl_glGetCompressedMultiTexImageEXT;
    glad_debug_glGetCompressedTexImage = glad_debug_impl_glGetCompressedTexImage;
    glad_debug_glGetCompressedTexImageARB = glad_debug_impl_glGetCompressedTexImageARB;
    glad_debug_glGetCompressedTextureImageEXT = glad_debug_impl_glGetCompressedTextureImageEXT;
    glad_debug_glGetDoubleIndexedvEXT = glad_debug_impl_glGetDoubleIndexedvEXT;
    glad_debug_glGetDoublei_vEXT = glad_debug_impl_glGetDoublei_vEXT;
    glad_debug_glGetDoublev = glad_debug_impl_glGetDoublev;
    glad_debug_glGetError = glad_debug_impl_glGetError;
    glad_debug_glGetFloatIndexedvEXT = glad_debug_impl_glGetFloatIndexedvEXT;
    glad_debug_glGetFloati_vEXT = glad_debug_impl_glGetFloati_vEXT;
    glad_debug_glGetFloatv = glad_debug_impl_glGetFloatv;
    glad_debug_glGetFragDataIndex = glad_debug_impl_glGetFragDataIndex;
    glad_debug_glGetFragDataLocation = glad_debug_impl_glGetFragDataLocation;
    glad_debug_glGetFragDataLocationEXT = glad_debug_impl_glGetFragDataLocationEXT;
    glad_debug_glGetFramebufferAttachmentParameteriv = glad_debug_impl_glGetFramebufferAttachmentParameteriv;
    glad_debug_glGetFramebufferAttachmentParameterivEXT = glad_debug_impl_glGetFramebufferAttachmentParameterivEXT;
    glad_debug_glGetFramebufferParameterivEXT = glad_debug_impl_glGetFramebufferParameterivEXT;
    glad_debug_glGetHandleARB = glad_debug_impl_glGetHandleARB;
    glad_debug_glGetInfoLogARB = glad_debug_impl_glGetInfoLogARB;
    glad_debug_glGetInteger64i_v = glad_debug_impl_glGetInteger64i_v;
    glad_debug_glGetInteger64v = glad_debug_impl_glGetInteger64v;
    glad_debug_glGetIntegerIndexedvEXT = glad_debug_impl_glGetIntegerIndexedvEXT;
    glad_debug_glGetIntegeri_v = glad_debug_impl_glGetIntegeri_v;
    glad_debug_glGetIntegerv = glad_debug_impl_glGetIntegerv;
    glad_debug_glGetMultiTexEnvfvEXT = glad_debug_impl_glGetMultiTexEnvfvEXT;
    glad_debug_glGetMultiTexEnvivEXT = glad_debug_impl_glGetMultiTexEnvivEXT;
    glad_debug_glGetMultiTexGendvEXT = glad_debug_impl_glGetMultiTexGendvEXT;
    glad_debug_glGetMultiTexGenfvEXT = glad_debug_impl_glGetMultiTexGenfvEXT;
    glad_debug_glGetMultiTexGenivEXT = glad_debug_impl_glGetMultiTexGenivEXT;
    glad_debug_glGetMultiTexImageEXT = glad_debug_impl_glGetMultiTexImageEXT;
    glad_debug_glGetMultiTexLevelParameterfvEXT = glad_debug_impl_glGetMultiTexLevelParameterfvEXT;
    glad_debug_glGetMultiTexLevelParameterivEXT = glad_debug_impl_glGetMultiTexLevelParameterivEXT;
    glad_debug_glGetMultiTexParameterIivEXT = glad_debug_impl_glGetMultiTexParameterIivEXT;
    glad_debug_glGetMultiTexParameterIuivEXT = glad_debug_impl_glGetMultiTexParameterIuivEXT;
    glad_debug_glGetMultiTexParameterfvEXT = glad_debug_impl_glGetMultiTexParameterfvEXT;
    glad_debug_glGetMultiTexParameterivEXT = glad_debug_impl_glGetMultiTexParameterivEXT;
    glad_debug_glGetMultisamplefv = glad_debug_impl_glGetMultisamplefv;
    glad_debug_glGetMultisamplefvNV = glad_debug_impl_glGetMultisamplefvNV;
    glad_debug_glGetNamedBufferParameterivEXT = glad_debug_impl_glGetNamedBufferParameterivEXT;
    glad_debug_glGetNamedBufferPointervEXT = glad_debug_impl_glGetNamedBufferPointervEXT;
    glad_debug_glGetNamedBufferSubDataEXT = glad_debug_impl_glGetNamedBufferSubDataEXT;
    glad_debug_glGetNamedFramebufferAttachmentParameterivEXT = glad_debug_impl_glGetNamedFramebufferAttachmentParameterivEXT;
    glad_debug_glGetNamedFramebufferParameterivEXT = glad_debug_impl_glGetNamedFramebufferParameterivEXT;
    glad_debug_glGetNamedProgramLocalParameterIivEXT = glad_debug_impl_glGetNamedProgramLocalParameterIivEXT;
    glad_debug_glGetNamedProgramLocalParameterIuivEXT = glad_debug_impl_glGetNamedProgramLocalParameterIuivEXT;
    glad_debug_glGetNamedProgramLocalParameterdvEXT = glad_debug_impl_glGetNamedProgramLocalParameterdvEXT;
    glad_debug_glGetNamedProgramLocalParameterfvEXT = glad_debug_impl_glGetNamedProgramLocalParameterfvEXT;
    glad_debug_glGetNamedProgramStringEXT = glad_debug_impl_glGetNamedProgramStringEXT;
    glad_debug_glGetNamedProgramivEXT = glad_debug_impl_glGetNamedProgramivEXT;
    glad_debug_glGetNamedRenderbufferParameterivEXT = glad_debug_impl_glGetNamedRenderbufferParameterivEXT;
    glad_debug_glGetObjectParameterfvARB = glad_debug_impl_glGetObjectParameterfvARB;
    glad_debug_glGetObjectParameterivARB = glad_debug_impl_glGetObjectParameterivARB;
    glad_debug_glGetPointerIndexedvEXT = glad_debug_impl_glGetPointerIndexedvEXT;
    glad_debug_glGetPointeri_vEXT = glad_debug_impl_glGetPointeri_vEXT;
    glad_debug_glGetPointervEXT = glad_debug_impl_glGetPointervEXT;
    glad_debug_glGetProgramEnvParameterdvARB = glad_debug_impl_glGetProgramEnvParameterdvARB;
    glad_debug_glGetProgramEnvParameterfvARB = glad_debug_impl_glGetProgramEnvParameterfvARB;
    glad_debug_glGetProgramInfoLog = glad_debug_impl_glGetProgramInfoLog;
    glad_debug_glGetProgramLocalParameterdvARB = glad_debug_impl_glGetProgramLocalParameterdvARB;
    glad_debug_glGetProgramLocalParameterfvARB = glad_debug_impl_glGetProgramLocalParameterfvARB;
    glad_debug_glGetProgramParameterdvNV = glad_debug_impl_glGetProgramParameterdvNV;
    glad_debug_glGetProgramParameterfvNV = glad_debug_impl_glGetProgramParameterfvNV;
    glad_debug_glGetProgramStringARB = glad_debug_impl_glGetProgramStringARB;
    glad_debug_glGetProgramStringNV = glad_debug_impl_glGetProgramStringNV;
    glad_debug_glGetProgramiv = glad_debug_impl_glGetProgramiv;
    glad_debug_glGetProgramivARB = glad_debug_impl_glGetProgramivARB;
    glad_debug_glGetProgramivNV = glad_debug_impl_glGetProgramivNV;
    glad_debug_glGetQueryObjecti64v = glad_debug_impl_glGetQueryObjecti64v;
    glad_debug_glGetQueryObjecti64vEXT = glad_debug_impl_glGetQueryObjecti64vEXT;
    glad_debug_glGetQueryObjectiv = glad_debug_impl_glGetQueryObjectiv;
    glad_debug_glGetQueryObjectivARB = glad_debug_impl_glGetQueryObjectivARB;
    glad_debug_glGetQueryObjectui64v = glad_debug_impl_glGetQueryObjectui64v;
    glad_debug_glGetQueryObjectui64vEXT = glad_debug_impl_glGetQueryObjectui64vEXT;
    glad_debug_glGetQueryObjectuiv = glad_debug_impl_glGetQueryObjectuiv;
    glad_debug_glGetQueryObjectuivARB = glad_debug_impl_glGetQueryObjectuivARB;
    glad_debug_glGetQueryiv = glad_debug_impl_glGetQueryiv;
    glad_debug_glGetQueryivARB = glad_debug_impl_glGetQueryivARB;
    glad_debug_glGetRenderbufferParameteriv = glad_debug_impl_glGetRenderbufferParameteriv;
    glad_debug_glGetRenderbufferParameterivEXT = glad_debug_impl_glGetRenderbufferParameterivEXT;
    glad_debug_glGetSamplerParameterIiv = glad_debug_impl_glGetSamplerParameterIiv;
    glad_debug_glGetSamplerParameterIuiv = glad_debug_impl_glGetSamplerParameterIuiv;
    glad_debug_glGetSamplerParameterfv = glad_debug_impl_glGetSamplerParameterfv;
    glad_debug_glGetSamplerParameteriv = glad_debug_impl_glGetSamplerParameteriv;
    glad_debug_glGetShaderInfoLog = glad_debug_impl_glGetShaderInfoLog;
    glad_debug_glGetShaderSource = glad_debug_impl_glGetShaderSource;
    glad_debug_glGetShaderSourceARB = glad_debug_impl_glGetShaderSourceARB;
    glad_debug_glGetShaderiv = glad_debug_impl_glGetShaderiv;
    glad_debug_glGetString = glad_debug_impl_glGetString;
    glad_debug_glGetStringi = glad_debug_impl_glGetStringi;
    glad_debug_glGetSynciv = glad_debug_impl_glGetSynciv;
    glad_debug_glGetTexImage = glad_debug_impl_glGetTexImage;
    glad_debug_glGetTexLevelParameterfv = glad_debug_impl_glGetTexLevelParameterfv;
    glad_debug_glGetTexLevelParameteriv = glad_debug_impl_glGetTexLevelParameteriv;
    glad_debug_glGetTexParameterIiv = glad_debug_impl_glGetTexParameterIiv;
    glad_debug_glGetTexParameterIivEXT = glad_debug_impl_glGetTexParameterIivEXT;
    glad_debug_glGetTexParameterIuiv = glad_debug_impl_glGetTexParameterIuiv;
    glad_debug_glGetTexParameterIuivEXT = glad_debug_impl_glGetTexParameterIuivEXT;
    glad_debug_glGetTexParameterfv = glad_debug_impl_glGetTexParameterfv;
    glad_debug_glGetTexParameteriv = glad_debug_impl_glGetTexParameteriv;
    glad_debug_glGetTextureImageEXT = glad_debug_impl_glGetTextureImageEXT;
    glad_debug_glGetTextureLevelParameterfvEXT = glad_debug_impl_glGetTextureLevelParameterfvEXT;
    glad_debug_glGetTextureLevelParameterivEXT = glad_debug_impl_glGetTextureLevelParameterivEXT;
    glad_debug_glGetTextureParameterIivEXT = glad_debug_impl_glGetTextureParameterIivEXT;
    glad_debug_glGetTextureParameterIuivEXT = glad_debug_impl_glGetTextureParameterIuivEXT;
    glad_debug_glGetTextureParameterfvEXT = glad_debug_impl_glGetTextureParameterfvEXT;
    glad_debug_glGetTextureParameterivEXT = glad_debug_impl_glGetTextureParameterivEXT;
    glad_debug_glGetTrackMatrixivNV = glad_debug_impl_glGetTrackMatrixivNV;
    glad_debug_glGetTransformFeedbackVarying = glad_debug_impl_glGetTransformFeedbackVarying;
    glad_debug_glGetTransformFeedbackVaryingEXT = glad_debug_impl_glGetTransformFeedbackVaryingEXT;
    glad_debug_glGetTransformFeedbackVaryingNV = glad_debug_impl_glGetTransformFeedbackVaryingNV;
    glad_debug_glGetUniformBlockIndex = glad_debug_impl_glGetUniformBlockIndex;
    glad_debug_glGetUniformIndices = glad_debug_impl_glGetUniformIndices;
    glad_debug_glGetUniformLocation = glad_debug_impl_glGetUniformLocation;
    glad_debug_glGetUniformLocationARB = glad_debug_impl_glGetUniformLocationARB;
    glad_debug_glGetUniformfv = glad_debug_impl_glGetUniformfv;
    glad_debug_glGetUniformfvARB = glad_debug_impl_glGetUniformfvARB;
    glad_debug_glGetUniformiv = glad_debug_impl_glGetUniformiv;
    glad_debug_glGetUniformivARB = glad_debug_impl_glGetUniformivARB;
    glad_debug_glGetUniformuiv = glad_debug_impl_glGetUniformuiv;
    glad_debug_glGetUniformuivEXT = glad_debug_impl_glGetUniformuivEXT;
    glad_debug_glGetVaryingLocationNV = glad_debug_impl_glGetVaryingLocationNV;
    glad_debug_glGetVertexArrayIntegeri_vEXT = glad_debug_impl_glGetVertexArrayIntegeri_vEXT;
    glad_debug_glGetVertexArrayIntegervEXT = glad_debug_impl_glGetVertexArrayIntegervEXT;
    glad_debug_glGetVertexArrayPointeri_vEXT = glad_debug_impl_glGetVertexArrayPointeri_vEXT;
    glad_debug_glGetVertexArrayPointervEXT = glad_debug_impl_glGetVertexArrayPointervEXT;
    glad_debug_glGetVertexAttribIiv = glad_debug_impl_glGetVertexAttribIiv;
    glad_debug_glGetVertexAttribIivEXT = glad_debug_impl_glGetVertexAttribIivEXT;
    glad_debug_glGetVertexAttribIuiv = glad_debug_impl_glGetVertexAttribIuiv;
    glad_debug_glGetVertexAttribIuivEXT = glad_debug_impl_glGetVertexAttribIuivEXT;
    glad_debug_glGetVertexAttribPointerv = glad_debug_impl_glGetVertexAttribPointerv;
    glad_debug_glGetVertexAttribPointervARB = glad_debug_impl_glGetVertexAttribPointervARB;
    glad_debug_glGetVertexAttribPointervNV = glad_debug_impl_glGetVertexAttribPointervNV;
    glad_debug_glGetVertexAttribdv = glad_debug_impl_glGetVertexAttribdv;
    glad_debug_glGetVertexAttribdvARB = glad_debug_impl_glGetVertexAttribdvARB;
    glad_debug_glGetVertexAttribdvNV = glad_debug_impl_glGetVertexAttribdvNV;
    glad_debug_glGetVertexAttribfv = glad_debug_impl_glGetVertexAttribfv;
    glad_debug_glGetVertexAttribfvARB = glad_debug_impl_glGetVertexAttribfvARB;
    glad_debug_glGetVertexAttribfvNV = glad_debug_impl_glGetVertexAttribfvNV;
    glad_debug_glGetVertexAttribiv = glad_debug_impl_glGetVertexAttribiv;
    glad_debug_glGetVertexAttribivARB = glad_debug_impl_glGetVertexAttribivARB;
    glad_debug_glGetVertexAttribivNV = glad_debug_impl_glGetVertexAttribivNV;
    glad_debug_glHint = glad_debug_impl_glHint;
    glad_debug_glIndexPointerEXT = glad_debug_impl_glIndexPointerEXT;
    glad_debug_glIsBuffer = glad_debug_impl_glIsBuffer;
    glad_debug_glIsBufferARB = glad_debug_impl_glIsBufferARB;
    glad_debug_glIsEnabled = glad_debug_impl_glIsEnabled;
    glad_debug_glIsEnabledIndexedEXT = glad_debug_impl_glIsEnabledIndexedEXT;
    glad_debug_glIsEnabledi = glad_debug_impl_glIsEnabledi;
    glad_debug_glIsFramebuffer = glad_debug_impl_glIsFramebuffer;
    glad_debug_glIsFramebufferEXT = glad_debug_impl_glIsFramebufferEXT;
    glad_debug_glIsProgram = glad_debug_impl_glIsProgram;
    glad_debug_glIsProgramARB = glad_debug_impl_glIsProgramARB;
    glad_debug_glIsProgramNV = glad_debug_impl_glIsProgramNV;
    glad_debug_glIsQuery = glad_debug_impl_glIsQuery;
    glad_debug_glIsQueryARB = glad_debug_impl_glIsQueryARB;
    glad_debug_glIsRenderbuffer = glad_debug_impl_glIsRenderbuffer;
    glad_debug_glIsRenderbufferEXT = glad_debug_impl_glIsRenderbufferEXT;
    glad_debug_glIsSampler = glad_debug_impl_glIsSampler;
    glad_debug_glIsShader = glad_debug_impl_glIsShader;
    glad_debug_glIsSync = glad_debug_impl_glIsSync;
    glad_debug_glIsTexture = glad_debug_impl_glIsTexture;
    glad_debug_glIsTextureEXT = glad_debug_impl_glIsTextureEXT;
    glad_debug_glIsVertexArray = glad_debug_impl_glIsVertexArray;
    glad_debug_glIsVertexArrayAPPLE = glad_debug_impl_glIsVertexArrayAPPLE;
    glad_debug_glLineWidth = glad_debug_impl_glLineWidth;
    glad_debug_glLinkProgram = glad_debug_impl_glLinkProgram;
    glad_debug_glLinkProgramARB = glad_debug_impl_glLinkProgramARB;
    glad_debug_glLoadProgramNV = glad_debug_impl_glLoadProgramNV;
    glad_debug_glLogicOp = glad_debug_impl_glLogicOp;
    glad_debug_glMapBuffer = glad_debug_impl_glMapBuffer;
    glad_debug_glMapBufferARB = glad_debug_impl_glMapBufferARB;
    glad_debug_glMapBufferRange = glad_debug_impl_glMapBufferRange;
    glad_debug_glMapNamedBufferEXT = glad_debug_impl_glMapNamedBufferEXT;
    glad_debug_glMapNamedBufferRangeEXT = glad_debug_impl_glMapNamedBufferRangeEXT;
    glad_debug_glMatrixFrustumEXT = glad_debug_impl_glMatrixFrustumEXT;
    glad_debug_glMatrixLoadIdentityEXT = glad_debug_impl_glMatrixLoadIdentityEXT;
    glad_debug_glMatrixLoadTransposedEXT = glad_debug_impl_glMatrixLoadTransposedEXT;
    glad_debug_glMatrixLoadTransposefEXT = glad_debug_impl_glMatrixLoadTransposefEXT;
    glad_debug_glMatrixLoaddEXT = glad_debug_impl_glMatrixLoaddEXT;
    glad_debug_glMatrixLoadfEXT = glad_debug_impl_glMatrixLoadfEXT;
    glad_debug_glMatrixMultTransposedEXT = glad_debug_impl_glMatrixMultTransposedEXT;
    glad_debug_glMatrixMultTransposefEXT = glad_debug_impl_glMatrixMultTransposefEXT;
    glad_debug_glMatrixMultdEXT = glad_debug_impl_glMatrixMultdEXT;
    glad_debug_glMatrixMultfEXT = glad_debug_impl_glMatrixMultfEXT;
    glad_debug_glMatrixOrthoEXT = glad_debug_impl_glMatrixOrthoEXT;
    glad_debug_glMatrixPopEXT = glad_debug_impl_glMatrixPopEXT;
    glad_debug_glMatrixPushEXT = glad_debug_impl_glMatrixPushEXT;
    glad_debug_glMatrixRotatedEXT = glad_debug_impl_glMatrixRotatedEXT;
    glad_debug_glMatrixRotatefEXT = glad_debug_impl_glMatrixRotatefEXT;
    glad_debug_glMatrixScaledEXT = glad_debug_impl_glMatrixScaledEXT;
    glad_debug_glMatrixScalefEXT = glad_debug_impl_glMatrixScalefEXT;
    glad_debug_glMatrixTranslatedEXT = glad_debug_impl_glMatrixTranslatedEXT;
    glad_debug_glMatrixTranslatefEXT = glad_debug_impl_glMatrixTranslatefEXT;
    glad_debug_glMultiDrawArrays = glad_debug_impl_glMultiDrawArrays;
    glad_debug_glMultiDrawArraysEXT = glad_debug_impl_glMultiDrawArraysEXT;
    glad_debug_glMultiDrawElements = glad_debug_impl_glMultiDrawElements;
    glad_debug_glMultiDrawElementsBaseVertex = glad_debug_impl_glMultiDrawElementsBaseVertex;
    glad_debug_glMultiDrawElementsEXT = glad_debug_impl_glMultiDrawElementsEXT;
    glad_debug_glMultiTexBufferEXT = glad_debug_impl_glMultiTexBufferEXT;
    glad_debug_glMultiTexCoord1dARB = glad_debug_impl_glMultiTexCoord1dARB;
    glad_debug_glMultiTexCoord1dvARB = glad_debug_impl_glMultiTexCoord1dvARB;
    glad_debug_glMultiTexCoord1fARB = glad_debug_impl_glMultiTexCoord1fARB;
    glad_debug_glMultiTexCoord1fvARB = glad_debug_impl_glMultiTexCoord1fvARB;
    glad_debug_glMultiTexCoord1iARB = glad_debug_impl_glMultiTexCoord1iARB;
    glad_debug_glMultiTexCoord1ivARB = glad_debug_impl_glMultiTexCoord1ivARB;
    glad_debug_glMultiTexCoord1sARB = glad_debug_impl_glMultiTexCoord1sARB;
    glad_debug_glMultiTexCoord1svARB = glad_debug_impl_glMultiTexCoord1svARB;
    glad_debug_glMultiTexCoord2dARB = glad_debug_impl_glMultiTexCoord2dARB;
    glad_debug_glMultiTexCoord2dvARB = glad_debug_impl_glMultiTexCoord2dvARB;
    glad_debug_glMultiTexCoord2fARB = glad_debug_impl_glMultiTexCoord2fARB;
    glad_debug_glMultiTexCoord2fvARB = glad_debug_impl_glMultiTexCoord2fvARB;
    glad_debug_glMultiTexCoord2iARB = glad_debug_impl_glMultiTexCoord2iARB;
    glad_debug_glMultiTexCoord2ivARB = glad_debug_impl_glMultiTexCoord2ivARB;
    glad_debug_glMultiTexCoord2sARB = glad_debug_impl_glMultiTexCoord2sARB;
    glad_debug_glMultiTexCoord2svARB = glad_debug_impl_glMultiTexCoord2svARB;
    glad_debug_glMultiTexCoord3dARB = glad_debug_impl_glMultiTexCoord3dARB;
    glad_debug_glMultiTexCoord3dvARB = glad_debug_impl_glMultiTexCoord3dvARB;
    glad_debug_glMultiTexCoord3fARB = glad_debug_impl_glMultiTexCoord3fARB;
    glad_debug_glMultiTexCoord3fvARB = glad_debug_impl_glMultiTexCoord3fvARB;
    glad_debug_glMultiTexCoord3iARB = glad_debug_impl_glMultiTexCoord3iARB;
    glad_debug_glMultiTexCoord3ivARB = glad_debug_impl_glMultiTexCoord3ivARB;
    glad_debug_glMultiTexCoord3sARB = glad_debug_impl_glMultiTexCoord3sARB;
    glad_debug_glMultiTexCoord3svARB = glad_debug_impl_glMultiTexCoord3svARB;
    glad_debug_glMultiTexCoord4dARB = glad_debug_impl_glMultiTexCoord4dARB;
    glad_debug_glMultiTexCoord4dvARB = glad_debug_impl_glMultiTexCoord4dvARB;
    glad_debug_glMultiTexCoord4fARB = glad_debug_impl_glMultiTexCoord4fARB;
    glad_debug_glMultiTexCoord4fvARB = glad_debug_impl_glMultiTexCoord4fvARB;
    glad_debug_glMultiTexCoord4iARB = glad_debug_impl_glMultiTexCoord4iARB;
    glad_debug_glMultiTexCoord4ivARB = glad_debug_impl_glMultiTexCoord4ivARB;
    glad_debug_glMultiTexCoord4sARB = glad_debug_impl_glMultiTexCoord4sARB;
    glad_debug_glMultiTexCoord4svARB = glad_debug_impl_glMultiTexCoord4svARB;
    glad_debug_glMultiTexCoordPointerEXT = glad_debug_impl_glMultiTexCoordPointerEXT;
    glad_debug_glMultiTexEnvfEXT = glad_debug_impl_glMultiTexEnvfEXT;
    glad_debug_glMultiTexEnvfvEXT = glad_debug_impl_glMultiTexEnvfvEXT;
    glad_debug_glMultiTexEnviEXT = glad_debug_impl_glMultiTexEnviEXT;
    glad_debug_glMultiTexEnvivEXT = glad_debug_impl_glMultiTexEnvivEXT;
    glad_debug_glMultiTexGendEXT = glad_debug_impl_glMultiTexGendEXT;
    glad_debug_glMultiTexGendvEXT = glad_debug_impl_glMultiTexGendvEXT;
    glad_debug_glMultiTexGenfEXT = glad_debug_impl_glMultiTexGenfEXT;
    glad_debug_glMultiTexGenfvEXT = glad_debug_impl_glMultiTexGenfvEXT;
    glad_debug_glMultiTexGeniEXT = glad_debug_impl_glMultiTexGeniEXT;
    glad_debug_glMultiTexGenivEXT = glad_debug_impl_glMultiTexGenivEXT;
    glad_debug_glMultiTexImage1DEXT = glad_debug_impl_glMultiTexImage1DEXT;
    glad_debug_glMultiTexImage2DEXT = glad_debug_impl_glMultiTexImage2DEXT;
    glad_debug_glMultiTexImage3DEXT = glad_debug_impl_glMultiTexImage3DEXT;
    glad_debug_glMultiTexParameterIivEXT = glad_debug_impl_glMultiTexParameterIivEXT;
    glad_debug_glMultiTexParameterIuivEXT = glad_debug_impl_glMultiTexParameterIuivEXT;
    glad_debug_glMultiTexParameterfEXT = glad_debug_impl_glMultiTexParameterfEXT;
    glad_debug_glMultiTexParameterfvEXT = glad_debug_impl_glMultiTexParameterfvEXT;
    glad_debug_glMultiTexParameteriEXT = glad_debug_impl_glMultiTexParameteriEXT;
    glad_debug_glMultiTexParameterivEXT = glad_debug_impl_glMultiTexParameterivEXT;
    glad_debug_glMultiTexRenderbufferEXT = glad_debug_impl_glMultiTexRenderbufferEXT;
    glad_debug_glMultiTexSubImage1DEXT = glad_debug_impl_glMultiTexSubImage1DEXT;
    glad_debug_glMultiTexSubImage2DEXT = glad_debug_impl_glMultiTexSubImage2DEXT;
    glad_debug_glMultiTexSubImage3DEXT = glad_debug_impl_glMultiTexSubImage3DEXT;
    glad_debug_glNamedBufferDataEXT = glad_debug_impl_glNamedBufferDataEXT;
    glad_debug_glNamedBufferStorageEXT = glad_debug_impl_glNamedBufferStorageEXT;
    glad_debug_glNamedBufferSubDataEXT = glad_debug_impl_glNamedBufferSubDataEXT;
    glad_debug_glNamedCopyBufferSubDataEXT = glad_debug_impl_glNamedCopyBufferSubDataEXT;
    glad_debug_glNamedFramebufferParameteriEXT = glad_debug_impl_glNamedFramebufferParameteriEXT;
    glad_debug_glNamedFramebufferRenderbufferEXT = glad_debug_impl_glNamedFramebufferRenderbufferEXT;
    glad_debug_glNamedFramebufferTexture1DEXT = glad_debug_impl_glNamedFramebufferTexture1DEXT;
    glad_debug_glNamedFramebufferTexture2DEXT = glad_debug_impl_glNamedFramebufferTexture2DEXT;
    glad_debug_glNamedFramebufferTexture3DEXT = glad_debug_impl_glNamedFramebufferTexture3DEXT;
    glad_debug_glNamedFramebufferTextureEXT = glad_debug_impl_glNamedFramebufferTextureEXT;
    glad_debug_glNamedFramebufferTextureFaceEXT = glad_debug_impl_glNamedFramebufferTextureFaceEXT;
    glad_debug_glNamedFramebufferTextureLayerEXT = glad_debug_impl_glNamedFramebufferTextureLayerEXT;
    glad_debug_glNamedProgramLocalParameter4dEXT = glad_debug_impl_glNamedProgramLocalParameter4dEXT;
    glad_debug_glNamedProgramLocalParameter4dvEXT = glad_debug_impl_glNamedProgramLocalParameter4dvEXT;
    glad_debug_glNamedProgramLocalParameter4fEXT = glad_debug_impl_glNamedProgramLocalParameter4fEXT;
    glad_debug_glNamedProgramLocalParameter4fvEXT = glad_debug_impl_glNamedProgramLocalParameter4fvEXT;
    glad_debug_glNamedProgramLocalParameterI4iEXT = glad_debug_impl_glNamedProgramLocalParameterI4iEXT;
    glad_debug_glNamedProgramLocalParameterI4ivEXT = glad_debug_impl_glNamedProgramLocalParameterI4ivEXT;
    glad_debug_glNamedProgramLocalParameterI4uiEXT = glad_debug_impl_glNamedProgramLocalParameterI4uiEXT;
    glad_debug_glNamedProgramLocalParameterI4uivEXT = glad_debug_impl_glNamedProgramLocalParameterI4uivEXT;
    glad_debug_glNamedProgramLocalParameters4fvEXT = glad_debug_impl_glNamedProgramLocalParameters4fvEXT;
    glad_debug_glNamedProgramLocalParametersI4ivEXT = glad_debug_impl_glNamedProgramLocalParametersI4ivEXT;
    glad_debug_glNamedProgramLocalParametersI4uivEXT = glad_debug_impl_glNamedProgramLocalParametersI4uivEXT;
    glad_debug_glNamedProgramStringEXT = glad_debug_impl_glNamedProgramStringEXT;
    glad_debug_glNamedRenderbufferStorageEXT = glad_debug_impl_glNamedRenderbufferStorageEXT;
    glad_debug_glNamedRenderbufferStorageMultisampleCoverageEXT = glad_debug_impl_glNamedRenderbufferStorageMultisampleCoverageEXT;
    glad_debug_glNamedRenderbufferStorageMultisampleEXT = glad_debug_impl_glNamedRenderbufferStorageMultisampleEXT;
    glad_debug_glNormalPointerEXT = glad_debug_impl_glNormalPointerEXT;
    glad_debug_glPixelStoref = glad_debug_impl_glPixelStoref;
    glad_debug_glPixelStorei = glad_debug_impl_glPixelStorei;
    glad_debug_glPointParameterf = glad_debug_impl_glPointParameterf;
    glad_debug_glPointParameterfARB = glad_debug_impl_glPointParameterfARB;
    glad_debug_glPointParameterfEXT = glad_debug_impl_glPointParameterfEXT;
    glad_debug_glPointParameterfSGIS = glad_debug_impl_glPointParameterfSGIS;
    glad_debug_glPointParameterfv = glad_debug_impl_glPointParameterfv;
    glad_debug_glPointParameterfvARB = glad_debug_impl_glPointParameterfvARB;
    glad_debug_glPointParameterfvEXT = glad_debug_impl_glPointParameterfvEXT;
    glad_debug_glPointParameterfvSGIS = glad_debug_impl_glPointParameterfvSGIS;
    glad_debug_glPointParameteri = glad_debug_impl_glPointParameteri;
    glad_debug_glPointParameteriNV = glad_debug_impl_glPointParameteriNV;
    glad_debug_glPointParameteriv = glad_debug_impl_glPointParameteriv;
    glad_debug_glPointParameterivNV = glad_debug_impl_glPointParameterivNV;
    glad_debug_glPointSize = glad_debug_impl_glPointSize;
    glad_debug_glPolygonMode = glad_debug_impl_glPolygonMode;
    glad_debug_glPolygonOffset = glad_debug_impl_glPolygonOffset;
    glad_debug_glPrimitiveRestartIndex = glad_debug_impl_glPrimitiveRestartIndex;
    glad_debug_glPrioritizeTexturesEXT = glad_debug_impl_glPrioritizeTexturesEXT;
    glad_debug_glProgramEnvParameter4dARB = glad_debug_impl_glProgramEnvParameter4dARB;
    glad_debug_glProgramEnvParameter4dvARB = glad_debug_impl_glProgramEnvParameter4dvARB;
    glad_debug_glProgramEnvParameter4fARB = glad_debug_impl_glProgramEnvParameter4fARB;
    glad_debug_glProgramEnvParameter4fvARB = glad_debug_impl_glProgramEnvParameter4fvARB;
    glad_debug_glProgramLocalParameter4dARB = glad_debug_impl_glProgramLocalParameter4dARB;
    glad_debug_glProgramLocalParameter4dvARB = glad_debug_impl_glProgramLocalParameter4dvARB;
    glad_debug_glProgramLocalParameter4fARB = glad_debug_impl_glProgramLocalParameter4fARB;
    glad_debug_glProgramLocalParameter4fvARB = glad_debug_impl_glProgramLocalParameter4fvARB;
    glad_debug_glProgramParameter4dNV = glad_debug_impl_glProgramParameter4dNV;
    glad_debug_glProgramParameter4dvNV = glad_debug_impl_glProgramParameter4dvNV;
    glad_debug_glProgramParameter4fNV = glad_debug_impl_glProgramParameter4fNV;
    glad_debug_glProgramParameter4fvNV = glad_debug_impl_glProgramParameter4fvNV;
    glad_debug_glProgramParameteriARB = glad_debug_impl_glProgramParameteriARB;
    glad_debug_glProgramParameters4dvNV = glad_debug_impl_glProgramParameters4dvNV;
    glad_debug_glProgramParameters4fvNV = glad_debug_impl_glProgramParameters4fvNV;
    glad_debug_glProgramStringARB = glad_debug_impl_glProgramStringARB;
    glad_debug_glProgramUniform1dEXT = glad_debug_impl_glProgramUniform1dEXT;
    glad_debug_glProgramUniform1dvEXT = glad_debug_impl_glProgramUniform1dvEXT;
    glad_debug_glProgramUniform1fEXT = glad_debug_impl_glProgramUniform1fEXT;
    glad_debug_glProgramUniform1fvEXT = glad_debug_impl_glProgramUniform1fvEXT;
    glad_debug_glProgramUniform1iEXT = glad_debug_impl_glProgramUniform1iEXT;
    glad_debug_glProgramUniform1ivEXT = glad_debug_impl_glProgramUniform1ivEXT;
    glad_debug_glProgramUniform1uiEXT = glad_debug_impl_glProgramUniform1uiEXT;
    glad_debug_glProgramUniform1uivEXT = glad_debug_impl_glProgramUniform1uivEXT;
    glad_debug_glProgramUniform2dEXT = glad_debug_impl_glProgramUniform2dEXT;
    glad_debug_glProgramUniform2dvEXT = glad_debug_impl_glProgramUniform2dvEXT;
    glad_debug_glProgramUniform2fEXT = glad_debug_impl_glProgramUniform2fEXT;
    glad_debug_glProgramUniform2fvEXT = glad_debug_impl_glProgramUniform2fvEXT;
    glad_debug_glProgramUniform2iEXT = glad_debug_impl_glProgramUniform2iEXT;
    glad_debug_glProgramUniform2ivEXT = glad_debug_impl_glProgramUniform2ivEXT;
    glad_debug_glProgramUniform2uiEXT = glad_debug_impl_glProgramUniform2uiEXT;
    glad_debug_glProgramUniform2uivEXT = glad_debug_impl_glProgramUniform2uivEXT;
    glad_debug_glProgramUniform3dEXT = glad_debug_impl_glProgramUniform3dEXT;
    glad_debug_glProgramUniform3dvEXT = glad_debug_impl_glProgramUniform3dvEXT;
    glad_debug_glProgramUniform3fEXT = glad_debug_impl_glProgramUniform3fEXT;
    glad_debug_glProgramUniform3fvEXT = glad_debug_impl_glProgramUniform3fvEXT;
    glad_debug_glProgramUniform3iEXT = glad_debug_impl_glProgramUniform3iEXT;
    glad_debug_glProgramUniform3ivEXT = glad_debug_impl_glProgramUniform3ivEXT;
    glad_debug_glProgramUniform3uiEXT = glad_debug_impl_glProgramUniform3uiEXT;
    glad_debug_glProgramUniform3uivEXT = glad_debug_impl_glProgramUniform3uivEXT;
    glad_debug_glProgramUniform4dEXT = glad_debug_impl_glProgramUniform4dEXT;
    glad_debug_glProgramUniform4dvEXT = glad_debug_impl_glProgramUniform4dvEXT;
    glad_debug_glProgramUniform4fEXT = glad_debug_impl_glProgramUniform4fEXT;
    glad_debug_glProgramUniform4fvEXT = glad_debug_impl_glProgramUniform4fvEXT;
    glad_debug_glProgramUniform4iEXT = glad_debug_impl_glProgramUniform4iEXT;
    glad_debug_glProgramUniform4ivEXT = glad_debug_impl_glProgramUniform4ivEXT;
    glad_debug_glProgramUniform4uiEXT = glad_debug_impl_glProgramUniform4uiEXT;
    glad_debug_glProgramUniform4uivEXT = glad_debug_impl_glProgramUniform4uivEXT;
    glad_debug_glProgramUniformMatrix2dvEXT = glad_debug_impl_glProgramUniformMatrix2dvEXT;
    glad_debug_glProgramUniformMatrix2fvEXT = glad_debug_impl_glProgramUniformMatrix2fvEXT;
    glad_debug_glProgramUniformMatrix2x3dvEXT = glad_debug_impl_glProgramUniformMatrix2x3dvEXT;
    glad_debug_glProgramUniformMatrix2x3fvEXT = glad_debug_impl_glProgramUniformMatrix2x3fvEXT;
    glad_debug_glProgramUniformMatrix2x4dvEXT = glad_debug_impl_glProgramUniformMatrix2x4dvEXT;
    glad_debug_glProgramUniformMatrix2x4fvEXT = glad_debug_impl_glProgramUniformMatrix2x4fvEXT;
    glad_debug_glProgramUniformMatrix3dvEXT = glad_debug_impl_glProgramUniformMatrix3dvEXT;
    glad_debug_glProgramUniformMatrix3fvEXT = glad_debug_impl_glProgramUniformMatrix3fvEXT;
    glad_debug_glProgramUniformMatrix3x2dvEXT = glad_debug_impl_glProgramUniformMatrix3x2dvEXT;
    glad_debug_glProgramUniformMatrix3x2fvEXT = glad_debug_impl_glProgramUniformMatrix3x2fvEXT;
    glad_debug_glProgramUniformMatrix3x4dvEXT = glad_debug_impl_glProgramUniformMatrix3x4dvEXT;
    glad_debug_glProgramUniformMatrix3x4fvEXT = glad_debug_impl_glProgramUniformMatrix3x4fvEXT;
    glad_debug_glProgramUniformMatrix4dvEXT = glad_debug_impl_glProgramUniformMatrix4dvEXT;
    glad_debug_glProgramUniformMatrix4fvEXT = glad_debug_impl_glProgramUniformMatrix4fvEXT;
    glad_debug_glProgramUniformMatrix4x2dvEXT = glad_debug_impl_glProgramUniformMatrix4x2dvEXT;
    glad_debug_glProgramUniformMatrix4x2fvEXT = glad_debug_impl_glProgramUniformMatrix4x2fvEXT;
    glad_debug_glProgramUniformMatrix4x3dvEXT = glad_debug_impl_glProgramUniformMatrix4x3dvEXT;
    glad_debug_glProgramUniformMatrix4x3fvEXT = glad_debug_impl_glProgramUniformMatrix4x3fvEXT;
    glad_debug_glProgramVertexLimitNV = glad_debug_impl_glProgramVertexLimitNV;
    glad_debug_glProvokingVertex = glad_debug_impl_glProvokingVertex;
    glad_debug_glProvokingVertexEXT = glad_debug_impl_glProvokingVertexEXT;
    glad_debug_glPushClientAttribDefaultEXT = glad_debug_impl_glPushClientAttribDefaultEXT;
    glad_debug_glQueryCounter = glad_debug_impl_glQueryCounter;
    glad_debug_glReadBuffer = glad_debug_impl_glReadBuffer;
    glad_debug_glReadPixels = glad_debug_impl_glReadPixels;
    glad_debug_glRenderbufferStorage = glad_debug_impl_glRenderbufferStorage;
    glad_debug_glRenderbufferStorageEXT = glad_debug_impl_glRenderbufferStorageEXT;
    glad_debug_glRenderbufferStorageMultisample = glad_debug_impl_glRenderbufferStorageMultisample;
    glad_debug_glRenderbufferStorageMultisampleEXT = glad_debug_impl_glRenderbufferStorageMultisampleEXT;
    glad_debug_glRequestResidentProgramsNV = glad_debug_impl_glRequestResidentProgramsNV;
    glad_debug_glSampleCoverage = glad_debug_impl_glSampleCoverage;
    glad_debug_glSampleCoverageARB = glad_debug_impl_glSampleCoverageARB;
    glad_debug_glSampleMaskIndexedNV = glad_debug_impl_glSampleMaskIndexedNV;
    glad_debug_glSampleMaski = glad_debug_impl_glSampleMaski;
    glad_debug_glSamplerParameterIiv = glad_debug_impl_glSamplerParameterIiv;
    glad_debug_glSamplerParameterIuiv = glad_debug_impl_glSamplerParameterIuiv;
    glad_debug_glSamplerParameterf = glad_debug_impl_glSamplerParameterf;
    glad_debug_glSamplerParameterfv = glad_debug_impl_glSamplerParameterfv;
    glad_debug_glSamplerParameteri = glad_debug_impl_glSamplerParameteri;
    glad_debug_glSamplerParameteriv = glad_debug_impl_glSamplerParameteriv;
    glad_debug_glScissor = glad_debug_impl_glScissor;
    glad_debug_glShaderSource = glad_debug_impl_glShaderSource;
    glad_debug_glShaderSourceARB = glad_debug_impl_glShaderSourceARB;
    glad_debug_glStencilFunc = glad_debug_impl_glStencilFunc;
    glad_debug_glStencilFuncSeparate = glad_debug_impl_glStencilFuncSeparate;
    glad_debug_glStencilFuncSeparateATI = glad_debug_impl_glStencilFuncSeparateATI;
    glad_debug_glStencilMask = glad_debug_impl_glStencilMask;
    glad_debug_glStencilMaskSeparate = glad_debug_impl_glStencilMaskSeparate;
    glad_debug_glStencilOp = glad_debug_impl_glStencilOp;
    glad_debug_glStencilOpSeparate = glad_debug_impl_glStencilOpSeparate;
    glad_debug_glStencilOpSeparateATI = glad_debug_impl_glStencilOpSeparateATI;
    glad_debug_glTexBuffer = glad_debug_impl_glTexBuffer;
    glad_debug_glTexBufferARB = glad_debug_impl_glTexBufferARB;
    glad_debug_glTexBufferEXT = glad_debug_impl_glTexBufferEXT;
    glad_debug_glTexCoordPointerEXT = glad_debug_impl_glTexCoordPointerEXT;
    glad_debug_glTexImage1D = glad_debug_impl_glTexImage1D;
    glad_debug_glTexImage2D = glad_debug_impl_glTexImage2D;
    glad_debug_glTexImage2DMultisample = glad_debug_impl_glTexImage2DMultisample;
    glad_debug_glTexImage3D = glad_debug_impl_glTexImage3D;
    glad_debug_glTexImage3DEXT = glad_debug_impl_glTexImage3DEXT;
    glad_debug_glTexImage3DMultisample = glad_debug_impl_glTexImage3DMultisample;
    glad_debug_glTexParameterIiv = glad_debug_impl_glTexParameterIiv;
    glad_debug_glTexParameterIivEXT = glad_debug_impl_glTexParameterIivEXT;
    glad_debug_glTexParameterIuiv = glad_debug_impl_glTexParameterIuiv;
    glad_debug_glTexParameterIuivEXT = glad_debug_impl_glTexParameterIuivEXT;
    glad_debug_glTexParameterf = glad_debug_impl_glTexParameterf;
    glad_debug_glTexParameterfv = glad_debug_impl_glTexParameterfv;
    glad_debug_glTexParameteri = glad_debug_impl_glTexParameteri;
    glad_debug_glTexParameteriv = glad_debug_impl_glTexParameteriv;
    glad_debug_glTexRenderbufferNV = glad_debug_impl_glTexRenderbufferNV;
    glad_debug_glTexSubImage1D = glad_debug_impl_glTexSubImage1D;
    glad_debug_glTexSubImage1DEXT = glad_debug_impl_glTexSubImage1DEXT;
    glad_debug_glTexSubImage2D = glad_debug_impl_glTexSubImage2D;
    glad_debug_glTexSubImage2DEXT = glad_debug_impl_glTexSubImage2DEXT;
    glad_debug_glTexSubImage3D = glad_debug_impl_glTexSubImage3D;
    glad_debug_glTexSubImage3DEXT = glad_debug_impl_glTexSubImage3DEXT;
    glad_debug_glTextureBufferEXT = glad_debug_impl_glTextureBufferEXT;
    glad_debug_glTextureBufferRangeEXT = glad_debug_impl_glTextureBufferRangeEXT;
    glad_debug_glTextureImage1DEXT = glad_debug_impl_glTextureImage1DEXT;
    glad_debug_glTextureImage2DEXT = glad_debug_impl_glTextureImage2DEXT;
    glad_debug_glTextureImage3DEXT = glad_debug_impl_glTextureImage3DEXT;
    glad_debug_glTexturePageCommitmentEXT = glad_debug_impl_glTexturePageCommitmentEXT;
    glad_debug_glTextureParameterIivEXT = glad_debug_impl_glTextureParameterIivEXT;
    glad_debug_glTextureParameterIuivEXT = glad_debug_impl_glTextureParameterIuivEXT;
    glad_debug_glTextureParameterfEXT = glad_debug_impl_glTextureParameterfEXT;
    glad_debug_glTextureParameterfvEXT = glad_debug_impl_glTextureParameterfvEXT;
    glad_debug_glTextureParameteriEXT = glad_debug_impl_glTextureParameteriEXT;
    glad_debug_glTextureParameterivEXT = glad_debug_impl_glTextureParameterivEXT;
    glad_debug_glTextureRenderbufferEXT = glad_debug_impl_glTextureRenderbufferEXT;
    glad_debug_glTextureStorage1DEXT = glad_debug_impl_glTextureStorage1DEXT;
    glad_debug_glTextureStorage2DEXT = glad_debug_impl_glTextureStorage2DEXT;
    glad_debug_glTextureStorage2DMultisampleEXT = glad_debug_impl_glTextureStorage2DMultisampleEXT;
    glad_debug_glTextureStorage3DEXT = glad_debug_impl_glTextureStorage3DEXT;
    glad_debug_glTextureStorage3DMultisampleEXT = glad_debug_impl_glTextureStorage3DMultisampleEXT;
    glad_debug_glTextureSubImage1DEXT = glad_debug_impl_glTextureSubImage1DEXT;
    glad_debug_glTextureSubImage2DEXT = glad_debug_impl_glTextureSubImage2DEXT;
    glad_debug_glTextureSubImage3DEXT = glad_debug_impl_glTextureSubImage3DEXT;
    glad_debug_glTrackMatrixNV = glad_debug_impl_glTrackMatrixNV;
    glad_debug_glTransformFeedbackAttribsNV = glad_debug_impl_glTransformFeedbackAttribsNV;
    glad_debug_glTransformFeedbackStreamAttribsNV = glad_debug_impl_glTransformFeedbackStreamAttribsNV;
    glad_debug_glTransformFeedbackVaryings = glad_debug_impl_glTransformFeedbackVaryings;
    glad_debug_glTransformFeedbackVaryingsEXT = glad_debug_impl_glTransformFeedbackVaryingsEXT;
    glad_debug_glTransformFeedbackVaryingsNV = glad_debug_impl_glTransformFeedbackVaryingsNV;
    glad_debug_glUniform1f = glad_debug_impl_glUniform1f;
    glad_debug_glUniform1fARB = glad_debug_impl_glUniform1fARB;
    glad_debug_glUniform1fv = glad_debug_impl_glUniform1fv;
    glad_debug_glUniform1fvARB = glad_debug_impl_glUniform1fvARB;
    glad_debug_glUniform1i = glad_debug_impl_glUniform1i;
    glad_debug_glUniform1iARB = glad_debug_impl_glUniform1iARB;
    glad_debug_glUniform1iv = glad_debug_impl_glUniform1iv;
    glad_debug_glUniform1ivARB = glad_debug_impl_glUniform1ivARB;
    glad_debug_glUniform1ui = glad_debug_impl_glUniform1ui;
    glad_debug_glUniform1uiEXT = glad_debug_impl_glUniform1uiEXT;
    glad_debug_glUniform1uiv = glad_debug_impl_glUniform1uiv;
    glad_debug_glUniform1uivEXT = glad_debug_impl_glUniform1uivEXT;
    glad_debug_glUniform2f = glad_debug_impl_glUniform2f;
    glad_debug_glUniform2fARB = glad_debug_impl_glUniform2fARB;
    glad_debug_glUniform2fv = glad_debug_impl_glUniform2fv;
    glad_debug_glUniform2fvARB = glad_debug_impl_glUniform2fvARB;
    glad_debug_glUniform2i = glad_debug_impl_glUniform2i;
    glad_debug_glUniform2iARB = glad_debug_impl_glUniform2iARB;
    glad_debug_glUniform2iv = glad_debug_impl_glUniform2iv;
    glad_debug_glUniform2ivARB = glad_debug_impl_glUniform2ivARB;
    glad_debug_glUniform2ui = glad_debug_impl_glUniform2ui;
    glad_debug_glUniform2uiEXT = glad_debug_impl_glUniform2uiEXT;
    glad_debug_glUniform2uiv = glad_debug_impl_glUniform2uiv;
    glad_debug_glUniform2uivEXT = glad_debug_impl_glUniform2uivEXT;
    glad_debug_glUniform3f = glad_debug_impl_glUniform3f;
    glad_debug_glUniform3fARB = glad_debug_impl_glUniform3fARB;
    glad_debug_glUniform3fv = glad_debug_impl_glUniform3fv;
    glad_debug_glUniform3fvARB = glad_debug_impl_glUniform3fvARB;
    glad_debug_glUniform3i = glad_debug_impl_glUniform3i;
    glad_debug_glUniform3iARB = glad_debug_impl_glUniform3iARB;
    glad_debug_glUniform3iv = glad_debug_impl_glUniform3iv;
    glad_debug_glUniform3ivARB = glad_debug_impl_glUniform3ivARB;
    glad_debug_glUniform3ui = glad_debug_impl_glUniform3ui;
    glad_debug_glUniform3uiEXT = glad_debug_impl_glUniform3uiEXT;
    glad_debug_glUniform3uiv = glad_debug_impl_glUniform3uiv;
    glad_debug_glUniform3uivEXT = glad_debug_impl_glUniform3uivEXT;
    glad_debug_glUniform4f = glad_debug_impl_glUniform4f;
    glad_debug_glUniform4fARB = glad_debug_impl_glUniform4fARB;
    glad_debug_glUniform4fv = glad_debug_impl_glUniform4fv;
    glad_debug_glUniform4fvARB = glad_debug_impl_glUniform4fvARB;
    glad_debug_glUniform4i = glad_debug_impl_glUniform4i;
    glad_debug_glUniform4iARB = glad_debug_impl_glUniform4iARB;
    glad_debug_glUniform4iv = glad_debug_impl_glUniform4iv;
    glad_debug_glUniform4ivARB = glad_debug_impl_glUniform4ivARB;
    glad_debug_glUniform4ui = glad_debug_impl_glUniform4ui;
    glad_debug_glUniform4uiEXT = glad_debug_impl_glUniform4uiEXT;
    glad_debug_glUniform4uiv = glad_debug_impl_glUniform4uiv;
    glad_debug_glUniform4uivEXT = glad_debug_impl_glUniform4uivEXT;
    glad_debug_glUniformBlockBinding = glad_debug_impl_glUniformBlockBinding;
    glad_debug_glUniformMatrix2fv = glad_debug_impl_glUniformMatrix2fv;
    glad_debug_glUniformMatrix2fvARB = glad_debug_impl_glUniformMatrix2fvARB;
    glad_debug_glUniformMatrix2x3fv = glad_debug_impl_glUniformMatrix2x3fv;
    glad_debug_glUniformMatrix2x4fv = glad_debug_impl_glUniformMatrix2x4fv;
    glad_debug_glUniformMatrix3fv = glad_debug_impl_glUniformMatrix3fv;
    glad_debug_glUniformMatrix3fvARB = glad_debug_impl_glUniformMatrix3fvARB;
    glad_debug_glUniformMatrix3x2fv = glad_debug_impl_glUniformMatrix3x2fv;
    glad_debug_glUniformMatrix3x4fv = glad_debug_impl_glUniformMatrix3x4fv;
    glad_debug_glUniformMatrix4fv = glad_debug_impl_glUniformMatrix4fv;
    glad_debug_glUniformMatrix4fvARB = glad_debug_impl_glUniformMatrix4fvARB;
    glad_debug_glUniformMatrix4x2fv = glad_debug_impl_glUniformMatrix4x2fv;
    glad_debug_glUniformMatrix4x3fv = glad_debug_impl_glUniformMatrix4x3fv;
    glad_debug_glUnmapBuffer = glad_debug_impl_glUnmapBuffer;
    glad_debug_glUnmapBufferARB = glad_debug_impl_glUnmapBufferARB;
    glad_debug_glUnmapNamedBufferEXT = glad_debug_impl_glUnmapNamedBufferEXT;
    glad_debug_glUseProgram = glad_debug_impl_glUseProgram;
    glad_debug_glUseProgramObjectARB = glad_debug_impl_glUseProgramObjectARB;
    glad_debug_glValidateProgram = glad_debug_impl_glValidateProgram;
    glad_debug_glValidateProgramARB = glad_debug_impl_glValidateProgramARB;
    glad_debug_glVertexArrayBindVertexBufferEXT = glad_debug_impl_glVertexArrayBindVertexBufferEXT;
    glad_debug_glVertexArrayColorOffsetEXT = glad_debug_impl_glVertexArrayColorOffsetEXT;
    glad_debug_glVertexArrayEdgeFlagOffsetEXT = glad_debug_impl_glVertexArrayEdgeFlagOffsetEXT;
    glad_debug_glVertexArrayFogCoordOffsetEXT = glad_debug_impl_glVertexArrayFogCoordOffsetEXT;
    glad_debug_glVertexArrayIndexOffsetEXT = glad_debug_impl_glVertexArrayIndexOffsetEXT;
    glad_debug_glVertexArrayMultiTexCoordOffsetEXT = glad_debug_impl_glVertexArrayMultiTexCoordOffsetEXT;
    glad_debug_glVertexArrayNormalOffsetEXT = glad_debug_impl_glVertexArrayNormalOffsetEXT;
    glad_debug_glVertexArraySecondaryColorOffsetEXT = glad_debug_impl_glVertexArraySecondaryColorOffsetEXT;
    glad_debug_glVertexArrayTexCoordOffsetEXT = glad_debug_impl_glVertexArrayTexCoordOffsetEXT;
    glad_debug_glVertexArrayVertexAttribBindingEXT = glad_debug_impl_glVertexArrayVertexAttribBindingEXT;
    glad_debug_glVertexArrayVertexAttribDivisorEXT = glad_debug_impl_glVertexArrayVertexAttribDivisorEXT;
    glad_debug_glVertexArrayVertexAttribFormatEXT = glad_debug_impl_glVertexArrayVertexAttribFormatEXT;
    glad_debug_glVertexArrayVertexAttribIFormatEXT = glad_debug_impl_glVertexArrayVertexAttribIFormatEXT;
    glad_debug_glVertexArrayVertexAttribIOffsetEXT = glad_debug_impl_glVertexArrayVertexAttribIOffsetEXT;
    glad_debug_glVertexArrayVertexAttribLFormatEXT = glad_debug_impl_glVertexArrayVertexAttribLFormatEXT;
    glad_debug_glVertexArrayVertexAttribLOffsetEXT = glad_debug_impl_glVertexArrayVertexAttribLOffsetEXT;
    glad_debug_glVertexArrayVertexAttribOffsetEXT = glad_debug_impl_glVertexArrayVertexAttribOffsetEXT;
    glad_debug_glVertexArrayVertexBindingDivisorEXT = glad_debug_impl_glVertexArrayVertexBindingDivisorEXT;
    glad_debug_glVertexArrayVertexOffsetEXT = glad_debug_impl_glVertexArrayVertexOffsetEXT;
    glad_debug_glVertexAttrib1d = glad_debug_impl_glVertexAttrib1d;
    glad_debug_glVertexAttrib1dARB = glad_debug_impl_glVertexAttrib1dARB;
    glad_debug_glVertexAttrib1dNV = glad_debug_impl_glVertexAttrib1dNV;
    glad_debug_glVertexAttrib1dv = glad_debug_impl_glVertexAttrib1dv;
    glad_debug_glVertexAttrib1dvARB = glad_debug_impl_glVertexAttrib1dvARB;
    glad_debug_glVertexAttrib1dvNV = glad_debug_impl_glVertexAttrib1dvNV;
    glad_debug_glVertexAttrib1f = glad_debug_impl_glVertexAttrib1f;
    glad_debug_glVertexAttrib1fARB = glad_debug_impl_glVertexAttrib1fARB;
    glad_debug_glVertexAttrib1fNV = glad_debug_impl_glVertexAttrib1fNV;
    glad_debug_glVertexAttrib1fv = glad_debug_impl_glVertexAttrib1fv;
    glad_debug_glVertexAttrib1fvARB = glad_debug_impl_glVertexAttrib1fvARB;
    glad_debug_glVertexAttrib1fvNV = glad_debug_impl_glVertexAttrib1fvNV;
    glad_debug_glVertexAttrib1s = glad_debug_impl_glVertexAttrib1s;
    glad_debug_glVertexAttrib1sARB = glad_debug_impl_glVertexAttrib1sARB;
    glad_debug_glVertexAttrib1sNV = glad_debug_impl_glVertexAttrib1sNV;
    glad_debug_glVertexAttrib1sv = glad_debug_impl_glVertexAttrib1sv;
    glad_debug_glVertexAttrib1svARB = glad_debug_impl_glVertexAttrib1svARB;
    glad_debug_glVertexAttrib1svNV = glad_debug_impl_glVertexAttrib1svNV;
    glad_debug_glVertexAttrib2d = glad_debug_impl_glVertexAttrib2d;
    glad_debug_glVertexAttrib2dARB = glad_debug_impl_glVertexAttrib2dARB;
    glad_debug_glVertexAttrib2dNV = glad_debug_impl_glVertexAttrib2dNV;
    glad_debug_glVertexAttrib2dv = glad_debug_impl_glVertexAttrib2dv;
    glad_debug_glVertexAttrib2dvARB = glad_debug_impl_glVertexAttrib2dvARB;
    glad_debug_glVertexAttrib2dvNV = glad_debug_impl_glVertexAttrib2dvNV;
    glad_debug_glVertexAttrib2f = glad_debug_impl_glVertexAttrib2f;
    glad_debug_glVertexAttrib2fARB = glad_debug_impl_glVertexAttrib2fARB;
    glad_debug_glVertexAttrib2fNV = glad_debug_impl_glVertexAttrib2fNV;
    glad_debug_glVertexAttrib2fv = glad_debug_impl_glVertexAttrib2fv;
    glad_debug_glVertexAttrib2fvARB = glad_debug_impl_glVertexAttrib2fvARB;
    glad_debug_glVertexAttrib2fvNV = glad_debug_impl_glVertexAttrib2fvNV;
    glad_debug_glVertexAttrib2s = glad_debug_impl_glVertexAttrib2s;
    glad_debug_glVertexAttrib2sARB = glad_debug_impl_glVertexAttrib2sARB;
    glad_debug_glVertexAttrib2sNV = glad_debug_impl_glVertexAttrib2sNV;
    glad_debug_glVertexAttrib2sv = glad_debug_impl_glVertexAttrib2sv;
    glad_debug_glVertexAttrib2svARB = glad_debug_impl_glVertexAttrib2svARB;
    glad_debug_glVertexAttrib2svNV = glad_debug_impl_glVertexAttrib2svNV;
    glad_debug_glVertexAttrib3d = glad_debug_impl_glVertexAttrib3d;
    glad_debug_glVertexAttrib3dARB = glad_debug_impl_glVertexAttrib3dARB;
    glad_debug_glVertexAttrib3dNV = glad_debug_impl_glVertexAttrib3dNV;
    glad_debug_glVertexAttrib3dv = glad_debug_impl_glVertexAttrib3dv;
    glad_debug_glVertexAttrib3dvARB = glad_debug_impl_glVertexAttrib3dvARB;
    glad_debug_glVertexAttrib3dvNV = glad_debug_impl_glVertexAttrib3dvNV;
    glad_debug_glVertexAttrib3f = glad_debug_impl_glVertexAttrib3f;
    glad_debug_glVertexAttrib3fARB = glad_debug_impl_glVertexAttrib3fARB;
    glad_debug_glVertexAttrib3fNV = glad_debug_impl_glVertexAttrib3fNV;
    glad_debug_glVertexAttrib3fv = glad_debug_impl_glVertexAttrib3fv;
    glad_debug_glVertexAttrib3fvARB = glad_debug_impl_glVertexAttrib3fvARB;
    glad_debug_glVertexAttrib3fvNV = glad_debug_impl_glVertexAttrib3fvNV;
    glad_debug_glVertexAttrib3s = glad_debug_impl_glVertexAttrib3s;
    glad_debug_glVertexAttrib3sARB = glad_debug_impl_glVertexAttrib3sARB;
    glad_debug_glVertexAttrib3sNV = glad_debug_impl_glVertexAttrib3sNV;
    glad_debug_glVertexAttrib3sv = glad_debug_impl_glVertexAttrib3sv;
    glad_debug_glVertexAttrib3svARB = glad_debug_impl_glVertexAttrib3svARB;
    glad_debug_glVertexAttrib3svNV = glad_debug_impl_glVertexAttrib3svNV;
    glad_debug_glVertexAttrib4Nbv = glad_debug_impl_glVertexAttrib4Nbv;
    glad_debug_glVertexAttrib4NbvARB = glad_debug_impl_glVertexAttrib4NbvARB;
    glad_debug_glVertexAttrib4Niv = glad_debug_impl_glVertexAttrib4Niv;
    glad_debug_glVertexAttrib4NivARB = glad_debug_impl_glVertexAttrib4NivARB;
    glad_debug_glVertexAttrib4Nsv = glad_debug_impl_glVertexAttrib4Nsv;
    glad_debug_glVertexAttrib4NsvARB = glad_debug_impl_glVertexAttrib4NsvARB;
    glad_debug_glVertexAttrib4Nub = glad_debug_impl_glVertexAttrib4Nub;
    glad_debug_glVertexAttrib4NubARB = glad_debug_impl_glVertexAttrib4NubARB;
    glad_debug_glVertexAttrib4Nubv = glad_debug_impl_glVertexAttrib4Nubv;
    glad_debug_glVertexAttrib4NubvARB = glad_debug_impl_glVertexAttrib4NubvARB;
    glad_debug_glVertexAttrib4Nuiv = glad_debug_impl_glVertexAttrib4Nuiv;
    glad_debug_glVertexAttrib4NuivARB = glad_debug_impl_glVertexAttrib4NuivARB;
    glad_debug_glVertexAttrib4Nusv = glad_debug_impl_glVertexAttrib4Nusv;
    glad_debug_glVertexAttrib4NusvARB = glad_debug_impl_glVertexAttrib4NusvARB;
    glad_debug_glVertexAttrib4bv = glad_debug_impl_glVertexAttrib4bv;
    glad_debug_glVertexAttrib4bvARB = glad_debug_impl_glVertexAttrib4bvARB;
    glad_debug_glVertexAttrib4d = glad_debug_impl_glVertexAttrib4d;
    glad_debug_glVertexAttrib4dARB = glad_debug_impl_glVertexAttrib4dARB;
    glad_debug_glVertexAttrib4dNV = glad_debug_impl_glVertexAttrib4dNV;
    glad_debug_glVertexAttrib4dv = glad_debug_impl_glVertexAttrib4dv;
    glad_debug_glVertexAttrib4dvARB = glad_debug_impl_glVertexAttrib4dvARB;
    glad_debug_glVertexAttrib4dvNV = glad_debug_impl_glVertexAttrib4dvNV;
    glad_debug_glVertexAttrib4f = glad_debug_impl_glVertexAttrib4f;
    glad_debug_glVertexAttrib4fARB = glad_debug_impl_glVertexAttrib4fARB;
    glad_debug_glVertexAttrib4fNV = glad_debug_impl_glVertexAttrib4fNV;
    glad_debug_glVertexAttrib4fv = glad_debug_impl_glVertexAttrib4fv;
    glad_debug_glVertexAttrib4fvARB = glad_debug_impl_glVertexAttrib4fvARB;
    glad_debug_glVertexAttrib4fvNV = glad_debug_impl_glVertexAttrib4fvNV;
    glad_debug_glVertexAttrib4iv = glad_debug_impl_glVertexAttrib4iv;
    glad_debug_glVertexAttrib4ivARB = glad_debug_impl_glVertexAttrib4ivARB;
    glad_debug_glVertexAttrib4s = glad_debug_impl_glVertexAttrib4s;
    glad_debug_glVertexAttrib4sARB = glad_debug_impl_glVertexAttrib4sARB;
    glad_debug_glVertexAttrib4sNV = glad_debug_impl_glVertexAttrib4sNV;
    glad_debug_glVertexAttrib4sv = glad_debug_impl_glVertexAttrib4sv;
    glad_debug_glVertexAttrib4svARB = glad_debug_impl_glVertexAttrib4svARB;
    glad_debug_glVertexAttrib4svNV = glad_debug_impl_glVertexAttrib4svNV;
    glad_debug_glVertexAttrib4ubNV = glad_debug_impl_glVertexAttrib4ubNV;
    glad_debug_glVertexAttrib4ubv = glad_debug_impl_glVertexAttrib4ubv;
    glad_debug_glVertexAttrib4ubvARB = glad_debug_impl_glVertexAttrib4ubvARB;
    glad_debug_glVertexAttrib4ubvNV = glad_debug_impl_glVertexAttrib4ubvNV;
    glad_debug_glVertexAttrib4uiv = glad_debug_impl_glVertexAttrib4uiv;
    glad_debug_glVertexAttrib4uivARB = glad_debug_impl_glVertexAttrib4uivARB;
    glad_debug_glVertexAttrib4usv = glad_debug_impl_glVertexAttrib4usv;
    glad_debug_glVertexAttrib4usvARB = glad_debug_impl_glVertexAttrib4usvARB;
    glad_debug_glVertexAttribDivisor = glad_debug_impl_glVertexAttribDivisor;
    glad_debug_glVertexAttribDivisorARB = glad_debug_impl_glVertexAttribDivisorARB;
    glad_debug_glVertexAttribI1i = glad_debug_impl_glVertexAttribI1i;
    glad_debug_glVertexAttribI1iEXT = glad_debug_impl_glVertexAttribI1iEXT;
    glad_debug_glVertexAttribI1iv = glad_debug_impl_glVertexAttribI1iv;
    glad_debug_glVertexAttribI1ivEXT = glad_debug_impl_glVertexAttribI1ivEXT;
    glad_debug_glVertexAttribI1ui = glad_debug_impl_glVertexAttribI1ui;
    glad_debug_glVertexAttribI1uiEXT = glad_debug_impl_glVertexAttribI1uiEXT;
    glad_debug_glVertexAttribI1uiv = glad_debug_impl_glVertexAttribI1uiv;
    glad_debug_glVertexAttribI1uivEXT = glad_debug_impl_glVertexAttribI1uivEXT;
    glad_debug_glVertexAttribI2i = glad_debug_impl_glVertexAttribI2i;
    glad_debug_glVertexAttribI2iEXT = glad_debug_impl_glVertexAttribI2iEXT;
    glad_debug_glVertexAttribI2iv = glad_debug_impl_glVertexAttribI2iv;
    glad_debug_glVertexAttribI2ivEXT = glad_debug_impl_glVertexAttribI2ivEXT;
    glad_debug_glVertexAttribI2ui = glad_debug_impl_glVertexAttribI2ui;
    glad_debug_glVertexAttribI2uiEXT = glad_debug_impl_glVertexAttribI2uiEXT;
    glad_debug_glVertexAttribI2uiv = glad_debug_impl_glVertexAttribI2uiv;
    glad_debug_glVertexAttribI2uivEXT = glad_debug_impl_glVertexAttribI2uivEXT;
    glad_debug_glVertexAttribI3i = glad_debug_impl_glVertexAttribI3i;
    glad_debug_glVertexAttribI3iEXT = glad_debug_impl_glVertexAttribI3iEXT;
    glad_debug_glVertexAttribI3iv = glad_debug_impl_glVertexAttribI3iv;
    glad_debug_glVertexAttribI3ivEXT = glad_debug_impl_glVertexAttribI3ivEXT;
    glad_debug_glVertexAttribI3ui = glad_debug_impl_glVertexAttribI3ui;
    glad_debug_glVertexAttribI3uiEXT = glad_debug_impl_glVertexAttribI3uiEXT;
    glad_debug_glVertexAttribI3uiv = glad_debug_impl_glVertexAttribI3uiv;
    glad_debug_glVertexAttribI3uivEXT = glad_debug_impl_glVertexAttribI3uivEXT;
    glad_debug_glVertexAttribI4bv = glad_debug_impl_glVertexAttribI4bv;
    glad_debug_glVertexAttribI4bvEXT = glad_debug_impl_glVertexAttribI4bvEXT;
    glad_debug_glVertexAttribI4i = glad_debug_impl_glVertexAttribI4i;
    glad_debug_glVertexAttribI4iEXT = glad_debug_impl_glVertexAttribI4iEXT;
    glad_debug_glVertexAttribI4iv = glad_debug_impl_glVertexAttribI4iv;
    glad_debug_glVertexAttribI4ivEXT = glad_debug_impl_glVertexAttribI4ivEXT;
    glad_debug_glVertexAttribI4sv = glad_debug_impl_glVertexAttribI4sv;
    glad_debug_glVertexAttribI4svEXT = glad_debug_impl_glVertexAttribI4svEXT;
    glad_debug_glVertexAttribI4ubv = glad_debug_impl_glVertexAttribI4ubv;
    glad_debug_glVertexAttribI4ubvEXT = glad_debug_impl_glVertexAttribI4ubvEXT;
    glad_debug_glVertexAttribI4ui = glad_debug_impl_glVertexAttribI4ui;
    glad_debug_glVertexAttribI4uiEXT = glad_debug_impl_glVertexAttribI4uiEXT;
    glad_debug_glVertexAttribI4uiv = glad_debug_impl_glVertexAttribI4uiv;
    glad_debug_glVertexAttribI4uivEXT = glad_debug_impl_glVertexAttribI4uivEXT;
    glad_debug_glVertexAttribI4usv = glad_debug_impl_glVertexAttribI4usv;
    glad_debug_glVertexAttribI4usvEXT = glad_debug_impl_glVertexAttribI4usvEXT;
    glad_debug_glVertexAttribIPointer = glad_debug_impl_glVertexAttribIPointer;
    glad_debug_glVertexAttribIPointerEXT = glad_debug_impl_glVertexAttribIPointerEXT;
    glad_debug_glVertexAttribP1ui = glad_debug_impl_glVertexAttribP1ui;
    glad_debug_glVertexAttribP1uiv = glad_debug_impl_glVertexAttribP1uiv;
    glad_debug_glVertexAttribP2ui = glad_debug_impl_glVertexAttribP2ui;
    glad_debug_glVertexAttribP2uiv = glad_debug_impl_glVertexAttribP2uiv;
    glad_debug_glVertexAttribP3ui = glad_debug_impl_glVertexAttribP3ui;
    glad_debug_glVertexAttribP3uiv = glad_debug_impl_glVertexAttribP3uiv;
    glad_debug_glVertexAttribP4ui = glad_debug_impl_glVertexAttribP4ui;
    glad_debug_glVertexAttribP4uiv = glad_debug_impl_glVertexAttribP4uiv;
    glad_debug_glVertexAttribPointer = glad_debug_impl_glVertexAttribPointer;
    glad_debug_glVertexAttribPointerARB = glad_debug_impl_glVertexAttribPointerARB;
    glad_debug_glVertexAttribPointerNV = glad_debug_impl_glVertexAttribPointerNV;
    glad_debug_glVertexAttribs1dvNV = glad_debug_impl_glVertexAttribs1dvNV;
    glad_debug_glVertexAttribs1fvNV = glad_debug_impl_glVertexAttribs1fvNV;
    glad_debug_glVertexAttribs1svNV = glad_debug_impl_glVertexAttribs1svNV;
    glad_debug_glVertexAttribs2dvNV = glad_debug_impl_glVertexAttribs2dvNV;
    glad_debug_glVertexAttribs2fvNV = glad_debug_impl_glVertexAttribs2fvNV;
    glad_debug_glVertexAttribs2svNV = glad_debug_impl_glVertexAttribs2svNV;
    glad_debug_glVertexAttribs3dvNV = glad_debug_impl_glVertexAttribs3dvNV;
    glad_debug_glVertexAttribs3fvNV = glad_debug_impl_glVertexAttribs3fvNV;
    glad_debug_glVertexAttribs3svNV = glad_debug_impl_glVertexAttribs3svNV;
    glad_debug_glVertexAttribs4dvNV = glad_debug_impl_glVertexAttribs4dvNV;
    glad_debug_glVertexAttribs4fvNV = glad_debug_impl_glVertexAttribs4fvNV;
    glad_debug_glVertexAttribs4svNV = glad_debug_impl_glVertexAttribs4svNV;
    glad_debug_glVertexAttribs4ubvNV = glad_debug_impl_glVertexAttribs4ubvNV;
    glad_debug_glVertexPointerEXT = glad_debug_impl_glVertexPointerEXT;
    glad_debug_glViewport = glad_debug_impl_glViewport;
    glad_debug_glWaitSync = glad_debug_impl_glWaitSync;
}

void gladUninstallGLDebug(void) {
    glad_debug_glActiveTexture = glad_glActiveTexture;
    glad_debug_glActiveTextureARB = glad_glActiveTextureARB;
    glad_debug_glActiveVaryingNV = glad_glActiveVaryingNV;
    glad_debug_glAreProgramsResidentNV = glad_glAreProgramsResidentNV;
    glad_debug_glAreTexturesResidentEXT = glad_glAreTexturesResidentEXT;
    glad_debug_glArrayElementEXT = glad_glArrayElementEXT;
    glad_debug_glAttachObjectARB = glad_glAttachObjectARB;
    glad_debug_glAttachShader = glad_glAttachShader;
    glad_debug_glBeginConditionalRender = glad_glBeginConditionalRender;
    glad_debug_glBeginConditionalRenderNV = glad_glBeginConditionalRenderNV;
    glad_debug_glBeginConditionalRenderNVX = glad_glBeginConditionalRenderNVX;
    glad_debug_glBeginQuery = glad_glBeginQuery;
    glad_debug_glBeginQueryARB = glad_glBeginQueryARB;
    glad_debug_glBeginTransformFeedback = glad_glBeginTransformFeedback;
    glad_debug_glBeginTransformFeedbackEXT = glad_glBeginTransformFeedbackEXT;
    glad_debug_glBeginTransformFeedbackNV = glad_glBeginTransformFeedbackNV;
    glad_debug_glBindAttribLocation = glad_glBindAttribLocation;
    glad_debug_glBindAttribLocationARB = glad_glBindAttribLocationARB;
    glad_debug_glBindBuffer = glad_glBindBuffer;
    glad_debug_glBindBufferARB = glad_glBindBufferARB;
    glad_debug_glBindBufferBase = glad_glBindBufferBase;
    glad_debug_glBindBufferBaseEXT = glad_glBindBufferBaseEXT;
    glad_debug_glBindBufferBaseNV = glad_glBindBufferBaseNV;
    glad_debug_glBindBufferOffsetEXT = glad_glBindBufferOffsetEXT;
    glad_debug_glBindBufferOffsetNV = glad_glBindBufferOffsetNV;
    glad_debug_glBindBufferRange = glad_glBindBufferRange;
    glad_debug_glBindBufferRangeEXT = glad_glBindBufferRangeEXT;
    glad_debug_glBindBufferRangeNV = glad_glBindBufferRangeNV;
    glad_debug_glBindFragDataLocation = glad_glBindFragDataLocation;
    glad_debug_glBindFragDataLocationEXT = glad_glBindFragDataLocationEXT;
    glad_debug_glBindFragDataLocationIndexed = glad_glBindFragDataLocationIndexed;
    glad_debug_glBindFramebuffer = glad_glBindFramebuffer;
    glad_debug_glBindFramebufferEXT = glad_glBindFramebufferEXT;
    glad_debug_glBindMultiTextureEXT = glad_glBindMultiTextureEXT;
    glad_debug_glBindProgramARB = glad_glBindProgramARB;
    glad_debug_glBindProgramNV = glad_glBindProgramNV;
    glad_debug_glBindRenderbuffer = glad_glBindRenderbuffer;
    glad_debug_glBindRenderbufferEXT = glad_glBindRenderbufferEXT;
    glad_debug_glBindSampler = glad_glBindSampler;
    glad_debug_glBindTexture = glad_glBindTexture;
    glad_debug_glBindTextureEXT = glad_glBindTextureEXT;
    glad_debug_glBindVertexArray = glad_glBindVertexArray;
    glad_debug_glBindVertexArrayAPPLE = glad_glBindVertexArrayAPPLE;
    glad_debug_glBlendColor = glad_glBlendColor;
    glad_debug_glBlendColorEXT = glad_glBlendColorEXT;
    glad_debug_glBlendEquation = glad_glBlendEquation;
    glad_debug_glBlendEquationEXT = glad_glBlendEquationEXT;
    glad_debug_glBlendEquationSeparate = glad_glBlendEquationSeparate;
    glad_debug_glBlendEquationSeparateEXT = glad_glBlendEquationSeparateEXT;
    glad_debug_glBlendFunc = glad_glBlendFunc;
    glad_debug_glBlendFuncSeparate = glad_glBlendFuncSeparate;
    glad_debug_glBlendFuncSeparateEXT = glad_glBlendFuncSeparateEXT;
    glad_debug_glBlendFuncSeparateINGR = glad_glBlendFuncSeparateINGR;
    glad_debug_glBlitFramebuffer = glad_glBlitFramebuffer;
    glad_debug_glBlitFramebufferEXT = glad_glBlitFramebufferEXT;
    glad_debug_glBufferData = glad_glBufferData;
    glad_debug_glBufferDataARB = glad_glBufferDataARB;
    glad_debug_glBufferParameteriAPPLE = glad_glBufferParameteriAPPLE;
    glad_debug_glBufferSubData = glad_glBufferSubData;
    glad_debug_glBufferSubDataARB = glad_glBufferSubDataARB;
    glad_debug_glCheckFramebufferStatus = glad_glCheckFramebufferStatus;
    glad_debug_glCheckFramebufferStatusEXT = glad_glCheckFramebufferStatusEXT;
    glad_debug_glCheckNamedFramebufferStatusEXT = glad_glCheckNamedFramebufferStatusEXT;
    glad_debug_glClampColor = glad_glClampColor;
    glad_debug_glClampColorARB = glad_glClampColorARB;
    glad_debug_glClear = glad_glClear;
    glad_debug_glClearBufferfi = glad_glClearBufferfi;
    glad_debug_glClearBufferfv = glad_glClearBufferfv;
    glad_debug_glClearBufferiv = glad_glClearBufferiv;
    glad_debug_glClearBufferuiv = glad_glClearBufferuiv;
    glad_debug_glClearColor = glad_glClearColor;
    glad_debug_glClearColorIiEXT = glad_glClearColorIiEXT;
    glad_debug_glClearColorIuiEXT = glad_glClearColorIuiEXT;
    glad_debug_glClearDepth = glad_glClearDepth;
    glad_debug_glClearNamedBufferDataEXT = glad_glClearNamedBufferDataEXT;
    glad_debug_glClearNamedBufferSubDataEXT = glad_glClearNamedBufferSubDataEXT;
    glad_debug_glClearStencil = glad_glClearStencil;
    glad_debug_glClientActiveTextureARB = glad_glClientActiveTextureARB;
    glad_debug_glClientAttribDefaultEXT = glad_glClientAttribDefaultEXT;
    glad_debug_glClientWaitSync = glad_glClientWaitSync;
    glad_debug_glColorMask = glad_glColorMask;
    glad_debug_glColorMaskIndexedEXT = glad_glColorMaskIndexedEXT;
    glad_debug_glColorMaski = glad_glColorMaski;
    glad_debug_glColorPointerEXT = glad_glColorPointerEXT;
    glad_debug_glCompileShader = glad_glCompileShader;
    glad_debug_glCompileShaderARB = glad_glCompileShaderARB;
    glad_debug_glCompressedMultiTexImage1DEXT = glad_glCompressedMultiTexImage1DEXT;
    glad_debug_glCompressedMultiTexImage2DEXT = glad_glCompressedMultiTexImage2DEXT;
    glad_debug_glCompressedMultiTexImage3DEXT = glad_glCompressedMultiTexImage3DEXT;
    glad_debug_glCompressedMultiTexSubImage1DEXT = glad_glCompressedMultiTexSubImage1DEXT;
    glad_debug_glCompressedMultiTexSubImage2DEXT = glad_glCompressedMultiTexSubImage2DEXT;
    glad_debug_glCompressedMultiTexSubImage3DEXT = glad_glCompressedMultiTexSubImage3DEXT;
    glad_debug_glCompressedTexImage1D = glad_glCompressedTexImage1D;
    glad_debug_glCompressedTexImage1DARB = glad_glCompressedTexImage1DARB;
    glad_debug_glCompressedTexImage2D = glad_glCompressedTexImage2D;
    glad_debug_glCompressedTexImage2DARB = glad_glCompressedTexImage2DARB;
    glad_debug_glCompressedTexImage3D = glad_glCompressedTexImage3D;
    glad_debug_glCompressedTexImage3DARB = glad_glCompressedTexImage3DARB;
    glad_debug_glCompressedTexSubImage1D = glad_glCompressedTexSubImage1D;
    glad_debug_glCompressedTexSubImage1DARB = glad_glCompressedTexSubImage1DARB;
    glad_debug_glCompressedTexSubImage2D = glad_glCompressedTexSubImage2D;
    glad_debug_glCompressedTexSubImage2DARB = glad_glCompressedTexSubImage2DARB;
    glad_debug_glCompressedTexSubImage3D = glad_glCompressedTexSubImage3D;
    glad_debug_glCompressedTexSubImage3DARB = glad_glCompressedTexSubImage3DARB;
    glad_debug_glCompressedTextureImage1DEXT = glad_glCompressedTextureImage1DEXT;
    glad_debug_glCompressedTextureImage2DEXT = glad_glCompressedTextureImage2DEXT;
    glad_debug_glCompressedTextureImage3DEXT = glad_glCompressedTextureImage3DEXT;
    glad_debug_glCompressedTextureSubImage1DEXT = glad_glCompressedTextureSubImage1DEXT;
    glad_debug_glCompressedTextureSubImage2DEXT = glad_glCompressedTextureSubImage2DEXT;
    glad_debug_glCompressedTextureSubImage3DEXT = glad_glCompressedTextureSubImage3DEXT;
    glad_debug_glCopyBufferSubData = glad_glCopyBufferSubData;
    glad_debug_glCopyMultiTexImage1DEXT = glad_glCopyMultiTexImage1DEXT;
    glad_debug_glCopyMultiTexImage2DEXT = glad_glCopyMultiTexImage2DEXT;
    glad_debug_glCopyMultiTexSubImage1DEXT = glad_glCopyMultiTexSubImage1DEXT;
    glad_debug_glCopyMultiTexSubImage2DEXT = glad_glCopyMultiTexSubImage2DEXT;
    glad_debug_glCopyMultiTexSubImage3DEXT = glad_glCopyMultiTexSubImage3DEXT;
    glad_debug_glCopyTexImage1D = glad_glCopyTexImage1D;
    glad_debug_glCopyTexImage1DEXT = glad_glCopyTexImage1DEXT;
    glad_debug_glCopyTexImage2D = glad_glCopyTexImage2D;
    glad_debug_glCopyTexImage2DEXT = glad_glCopyTexImage2DEXT;
    glad_debug_glCopyTexSubImage1D = glad_glCopyTexSubImage1D;
    glad_debug_glCopyTexSubImage1DEXT = glad_glCopyTexSubImage1DEXT;
    glad_debug_glCopyTexSubImage2D = glad_glCopyTexSubImage2D;
    glad_debug_glCopyTexSubImage2DEXT = glad_glCopyTexSubImage2DEXT;
    glad_debug_glCopyTexSubImage3D = glad_glCopyTexSubImage3D;
    glad_debug_glCopyTexSubImage3DEXT = glad_glCopyTexSubImage3DEXT;
    glad_debug_glCopyTextureImage1DEXT = glad_glCopyTextureImage1DEXT;
    glad_debug_glCopyTextureImage2DEXT = glad_glCopyTextureImage2DEXT;
    glad_debug_glCopyTextureSubImage1DEXT = glad_glCopyTextureSubImage1DEXT;
    glad_debug_glCopyTextureSubImage2DEXT = glad_glCopyTextureSubImage2DEXT;
    glad_debug_glCopyTextureSubImage3DEXT = glad_glCopyTextureSubImage3DEXT;
    glad_debug_glCreateProgram = glad_glCreateProgram;
    glad_debug_glCreateProgramObjectARB = glad_glCreateProgramObjectARB;
    glad_debug_glCreateShader = glad_glCreateShader;
    glad_debug_glCreateShaderObjectARB = glad_glCreateShaderObjectARB;
    glad_debug_glCullFace = glad_glCullFace;
    glad_debug_glDeleteBuffers = glad_glDeleteBuffers;
    glad_debug_glDeleteBuffersARB = glad_glDeleteBuffersARB;
    glad_debug_glDeleteFramebuffers = glad_glDeleteFramebuffers;
    glad_debug_glDeleteFramebuffersEXT = glad_glDeleteFramebuffersEXT;
    glad_debug_glDeleteObjectARB = glad_glDeleteObjectARB;
    glad_debug_glDeleteProgram = glad_glDeleteProgram;
    glad_debug_glDeleteProgramsARB = glad_glDeleteProgramsARB;
    glad_debug_glDeleteProgramsNV = glad_glDeleteProgramsNV;
    glad_debug_glDeleteQueries = glad_glDeleteQueries;
    glad_debug_glDeleteQueriesARB = glad_glDeleteQueriesARB;
    glad_debug_glDeleteRenderbuffers = glad_glDeleteRenderbuffers;
    glad_debug_glDeleteRenderbuffersEXT = glad_glDeleteRenderbuffersEXT;
    glad_debug_glDeleteSamplers = glad_glDeleteSamplers;
    glad_debug_glDeleteShader = glad_glDeleteShader;
    glad_debug_glDeleteSync = glad_glDeleteSync;
    glad_debug_glDeleteTextures = glad_glDeleteTextures;
    glad_debug_glDeleteTexturesEXT = glad_glDeleteTexturesEXT;
    glad_debug_glDeleteVertexArrays = glad_glDeleteVertexArrays;
    glad_debug_glDeleteVertexArraysAPPLE = glad_glDeleteVertexArraysAPPLE;
    glad_debug_glDepthFunc = glad_glDepthFunc;
    glad_debug_glDepthMask = glad_glDepthMask;
    glad_debug_glDepthRange = glad_glDepthRange;
    glad_debug_glDetachObjectARB = glad_glDetachObjectARB;
    glad_debug_glDetachShader = glad_glDetachShader;
    glad_debug_glDisable = glad_glDisable;
    glad_debug_glDisableClientStateIndexedEXT = glad_glDisableClientStateIndexedEXT;
    glad_debug_glDisableClientStateiEXT = glad_glDisableClientStateiEXT;
    glad_debug_glDisableIndexedEXT = glad_glDisableIndexedEXT;
    glad_debug_glDisableVertexArrayAttribEXT = glad_glDisableVertexArrayAttribEXT;
    glad_debug_glDisableVertexArrayEXT = glad_glDisableVertexArrayEXT;
    glad_debug_glDisableVertexAttribArray = glad_glDisableVertexAttribArray;
    glad_debug_glDisableVertexAttribArrayARB = glad_glDisableVertexAttribArrayARB;
    glad_debug_glDisablei = glad_glDisablei;
    glad_debug_glDrawArrays = glad_glDrawArrays;
    glad_debug_glDrawArraysEXT = glad_glDrawArraysEXT;
    glad_debug_glDrawArraysInstanced = glad_glDrawArraysInstanced;
    glad_debug_glDrawArraysInstancedARB = glad_glDrawArraysInstancedARB;
    glad_debug_glDrawArraysInstancedEXT = glad_glDrawArraysInstancedEXT;
    glad_debug_glDrawBuffer = glad_glDrawBuffer;
    glad_debug_glDrawBuffers = glad_glDrawBuffers;
    glad_debug_glDrawBuffersARB = glad_glDrawBuffersARB;
    glad_debug_glDrawBuffersATI = glad_glDrawBuffersATI;
    glad_debug_glDrawElements = glad_glDrawElements;
    glad_debug_glDrawElementsBaseVertex = glad_glDrawElementsBaseVertex;
    glad_debug_glDrawElementsInstanced = glad_glDrawElementsInstanced;
    glad_debug_glDrawElementsInstancedARB = glad_glDrawElementsInstancedARB;
    glad_debug_glDrawElementsInstancedBaseVertex = glad_glDrawElementsInstancedBaseVertex;
    glad_debug_glDrawElementsInstancedEXT = glad_glDrawElementsInstancedEXT;
    glad_debug_glDrawRangeElements = glad_glDrawRangeElements;
    glad_debug_glDrawRangeElementsBaseVertex = glad_glDrawRangeElementsBaseVertex;
    glad_debug_glDrawRangeElementsEXT = glad_glDrawRangeElementsEXT;
    glad_debug_glEdgeFlagPointerEXT = glad_glEdgeFlagPointerEXT;
    glad_debug_glEnable = glad_glEnable;
    glad_debug_glEnableClientStateIndexedEXT = glad_glEnableClientStateIndexedEXT;
    glad_debug_glEnableClientStateiEXT = glad_glEnableClientStateiEXT;
    glad_debug_glEnableIndexedEXT = glad_glEnableIndexedEXT;
    glad_debug_glEnableVertexArrayAttribEXT = glad_glEnableVertexArrayAttribEXT;
    glad_debug_glEnableVertexArrayEXT = glad_glEnableVertexArrayEXT;
    glad_debug_glEnableVertexAttribArray = glad_glEnableVertexAttribArray;
    glad_debug_glEnableVertexAttribArrayARB = glad_glEnableVertexAttribArrayARB;
    glad_debug_glEnablei = glad_glEnablei;
    glad_debug_glEndConditionalRender = glad_glEndConditionalRender;
    glad_debug_glEndConditionalRenderNV = glad_glEndConditionalRenderNV;
    glad_debug_glEndConditionalRenderNVX = glad_glEndConditionalRenderNVX;
    glad_debug_glEndQuery = glad_glEndQuery;
    glad_debug_glEndQueryARB = glad_glEndQueryARB;
    glad_debug_glEndTransformFeedback = glad_glEndTransformFeedback;
    glad_debug_glEndTransformFeedbackEXT = glad_glEndTransformFeedbackEXT;
    glad_debug_glEndTransformFeedbackNV = glad_glEndTransformFeedbackNV;
    glad_debug_glExecuteProgramNV = glad_glExecuteProgramNV;
    glad_debug_glFenceSync = glad_glFenceSync;
    glad_debug_glFinish = glad_glFinish;
    glad_debug_glFlush = glad_glFlush;
    glad_debug_glFlushMappedBufferRange = glad_glFlushMappedBufferRange;
    glad_debug_glFlushMappedBufferRangeAPPLE = glad_glFlushMappedBufferRangeAPPLE;
    glad_debug_glFlushMappedNamedBufferRangeEXT = glad_glFlushMappedNamedBufferRangeEXT;
    glad_debug_glFramebufferDrawBufferEXT = glad_glFramebufferDrawBufferEXT;
    glad_debug_glFramebufferDrawBuffersEXT = glad_glFramebufferDrawBuffersEXT;
    glad_debug_glFramebufferReadBufferEXT = glad_glFramebufferReadBufferEXT;
    glad_debug_glFramebufferRenderbuffer = glad_glFramebufferRenderbuffer;
    glad_debug_glFramebufferRenderbufferEXT = glad_glFramebufferRenderbufferEXT;
    glad_debug_glFramebufferTexture = glad_glFramebufferTexture;
    glad_debug_glFramebufferTexture1D = glad_glFramebufferTexture1D;
    glad_debug_glFramebufferTexture1DEXT = glad_glFramebufferTexture1DEXT;
    glad_debug_glFramebufferTexture2D = glad_glFramebufferTexture2D;
    glad_debug_glFramebufferTexture2DEXT = glad_glFramebufferTexture2DEXT;
    glad_debug_glFramebufferTexture3D = glad_glFramebufferTexture3D;
    glad_debug_glFramebufferTexture3DEXT = glad_glFramebufferTexture3DEXT;
    glad_debug_glFramebufferTextureARB = glad_glFramebufferTextureARB;
    glad_debug_glFramebufferTextureEXT = glad_glFramebufferTextureEXT;
    glad_debug_glFramebufferTextureFaceARB = glad_glFramebufferTextureFaceARB;
    glad_debug_glFramebufferTextureFaceEXT = glad_glFramebufferTextureFaceEXT;
    glad_debug_glFramebufferTextureLayer = glad_glFramebufferTextureLayer;
    glad_debug_glFramebufferTextureLayerARB = glad_glFramebufferTextureLayerARB;
    glad_debug_glFramebufferTextureLayerEXT = glad_glFramebufferTextureLayerEXT;
    glad_debug_glFrontFace = glad_glFrontFace;
    glad_debug_glGenBuffers = glad_glGenBuffers;
    glad_debug_glGenBuffersARB = glad_glGenBuffersARB;
    glad_debug_glGenFramebuffers = glad_glGenFramebuffers;
    glad_debug_glGenFramebuffersEXT = glad_glGenFramebuffersEXT;
    glad_debug_glGenProgramsARB = glad_glGenProgramsARB;
    glad_debug_glGenProgramsNV = glad_glGenProgramsNV;
    glad_debug_glGenQueries = glad_glGenQueries;
    glad_debug_glGenQueriesARB = glad_glGenQueriesARB;
    glad_debug_glGenRenderbuffers = glad_glGenRenderbuffers;
    glad_debug_glGenRenderbuffersEXT = glad_glGenRenderbuffersEXT;
    glad_debug_glGenSamplers = glad_glGenSamplers;
    glad_debug_glGenTextures = glad_glGenTextures;
    glad_debug_glGenTexturesEXT = glad_glGenTexturesEXT;
    glad_debug_glGenVertexArrays = glad_glGenVertexArrays;
    glad_debug_glGenVertexArraysAPPLE = glad_glGenVertexArraysAPPLE;
    glad_debug_glGenerateMipmap = glad_glGenerateMipmap;
    glad_debug_glGenerateMipmapEXT = glad_glGenerateMipmapEXT;
    glad_debug_glGenerateMultiTexMipmapEXT = glad_glGenerateMultiTexMipmapEXT;
    glad_debug_glGenerateTextureMipmapEXT = glad_glGenerateTextureMipmapEXT;
    glad_debug_glGetActiveAttrib = glad_glGetActiveAttrib;
    glad_debug_glGetActiveAttribARB = glad_glGetActiveAttribARB;
    glad_debug_glGetActiveUniform = glad_glGetActiveUniform;
    glad_debug_glGetActiveUniformARB = glad_glGetActiveUniformARB;
    glad_debug_glGetActiveUniformBlockName = glad_glGetActiveUniformBlockName;
    glad_debug_glGetActiveUniformBlockiv = glad_glGetActiveUniformBlockiv;
    glad_debug_glGetActiveUniformName = glad_glGetActiveUniformName;
    glad_debug_glGetActiveUniformsiv = glad_glGetActiveUniformsiv;
    glad_debug_glGetActiveVaryingNV = glad_glGetActiveVaryingNV;
    glad_debug_glGetAttachedObjectsARB = glad_glGetAttachedObjectsARB;
    glad_debug_glGetAttachedShaders = glad_glGetAttachedShaders;
    glad_debug_glGetAttribLocation = glad_glGetAttribLocation;
    glad_debug_glGetAttribLocationARB = glad_glGetAttribLocationARB;
    glad_debug_glGetBooleanIndexedvEXT = glad_glGetBooleanIndexedvEXT;
    glad_debug_glGetBooleani_v = glad_glGetBooleani_v;
    glad_debug_glGetBooleanv = glad_glGetBooleanv;
    glad_debug_glGetBufferParameteri64v = glad_glGetBufferParameteri64v;
    glad_debug_glGetBufferParameteriv = glad_glGetBufferParameteriv;
    glad_debug_glGetBufferParameterivARB = glad_glGetBufferParameterivARB;
    glad_debug_glGetBufferPointerv = glad_glGetBufferPointerv;
    glad_debug_glGetBufferPointervARB = glad_glGetBufferPointervARB;
    glad_debug_glGetBufferSubData = glad_glGetBufferSubData;
    glad_debug_glGetBufferSubDataARB = glad_glGetBufferSubDataARB;
    glad_debug_glGetCompressedMultiTexImageEXT = glad_glGetCompressedMultiTexImageEXT;
    glad_debug_glGetCompressedTexImage = glad_glGetCompressedTexImage;
    glad_debug_glGetCompressedTexImageARB = glad_glGetCompressedTexImageARB;
    glad_debug_glGetCompressedTextureImageEXT = glad_glGetCompressedTextureImageEXT;
    glad_debug_glGetDoubleIndexedvEXT = glad_glGetDoubleIndexedvEXT;
    glad_debug_glGetDoublei_vEXT = glad_glGetDoublei_vEXT;
    glad_debug_glGetDoublev = glad_glGetDoublev;
    glad_debug_glGetError = glad_glGetError;
    glad_debug_glGetFloatIndexedvEXT = glad_glGetFloatIndexedvEXT;
    glad_debug_glGetFloati_vEXT = glad_glGetFloati_vEXT;
    glad_debug_glGetFloatv = glad_glGetFloatv;
    glad_debug_glGetFragDataIndex = glad_glGetFragDataIndex;
    glad_debug_glGetFragDataLocation = glad_glGetFragDataLocation;
    glad_debug_glGetFragDataLocationEXT = glad_glGetFragDataLocationEXT;
    glad_debug_glGetFramebufferAttachmentParameteriv = glad_glGetFramebufferAttachmentParameteriv;
    glad_debug_glGetFramebufferAttachmentParameterivEXT = glad_glGetFramebufferAttachmentParameterivEXT;
    glad_debug_glGetFramebufferParameterivEXT = glad_glGetFramebufferParameterivEXT;
    glad_debug_glGetHandleARB = glad_glGetHandleARB;
    glad_debug_glGetInfoLogARB = glad_glGetInfoLogARB;
    glad_debug_glGetInteger64i_v = glad_glGetInteger64i_v;
    glad_debug_glGetInteger64v = glad_glGetInteger64v;
    glad_debug_glGetIntegerIndexedvEXT = glad_glGetIntegerIndexedvEXT;
    glad_debug_glGetIntegeri_v = glad_glGetIntegeri_v;
    glad_debug_glGetIntegerv = glad_glGetIntegerv;
    glad_debug_glGetMultiTexEnvfvEXT = glad_glGetMultiTexEnvfvEXT;
    glad_debug_glGetMultiTexEnvivEXT = glad_glGetMultiTexEnvivEXT;
    glad_debug_glGetMultiTexGendvEXT = glad_glGetMultiTexGendvEXT;
    glad_debug_glGetMultiTexGenfvEXT = glad_glGetMultiTexGenfvEXT;
    glad_debug_glGetMultiTexGenivEXT = glad_glGetMultiTexGenivEXT;
    glad_debug_glGetMultiTexImageEXT = glad_glGetMultiTexImageEXT;
    glad_debug_glGetMultiTexLevelParameterfvEXT = glad_glGetMultiTexLevelParameterfvEXT;
    glad_debug_glGetMultiTexLevelParameterivEXT = glad_glGetMultiTexLevelParameterivEXT;
    glad_debug_glGetMultiTexParameterIivEXT = glad_glGetMultiTexParameterIivEXT;
    glad_debug_glGetMultiTexParameterIuivEXT = glad_glGetMultiTexParameterIuivEXT;
    glad_debug_glGetMultiTexParameterfvEXT = glad_glGetMultiTexParameterfvEXT;
    glad_debug_glGetMultiTexParameterivEXT = glad_glGetMultiTexParameterivEXT;
    glad_debug_glGetMultisamplefv = glad_glGetMultisamplefv;
    glad_debug_glGetMultisamplefvNV = glad_glGetMultisamplefvNV;
    glad_debug_glGetNamedBufferParameterivEXT = glad_glGetNamedBufferParameterivEXT;
    glad_debug_glGetNamedBufferPointervEXT = glad_glGetNamedBufferPointervEXT;
    glad_debug_glGetNamedBufferSubDataEXT = glad_glGetNamedBufferSubDataEXT;
    glad_debug_glGetNamedFramebufferAttachmentParameterivEXT = glad_glGetNamedFramebufferAttachmentParameterivEXT;
    glad_debug_glGetNamedFramebufferParameterivEXT = glad_glGetNamedFramebufferParameterivEXT;
    glad_debug_glGetNamedProgramLocalParameterIivEXT = glad_glGetNamedProgramLocalParameterIivEXT;
    glad_debug_glGetNamedProgramLocalParameterIuivEXT = glad_glGetNamedProgramLocalParameterIuivEXT;
    glad_debug_glGetNamedProgramLocalParameterdvEXT = glad_glGetNamedProgramLocalParameterdvEXT;
    glad_debug_glGetNamedProgramLocalParameterfvEXT = glad_glGetNamedProgramLocalParameterfvEXT;
    glad_debug_glGetNamedProgramStringEXT = glad_glGetNamedProgramStringEXT;
    glad_debug_glGetNamedProgramivEXT = glad_glGetNamedProgramivEXT;
    glad_debug_glGetNamedRenderbufferParameterivEXT = glad_glGetNamedRenderbufferParameterivEXT;
    glad_debug_glGetObjectParameterfvARB = glad_glGetObjectParameterfvARB;
    glad_debug_glGetObjectParameterivARB = glad_glGetObjectParameterivARB;
    glad_debug_glGetPointerIndexedvEXT = glad_glGetPointerIndexedvEXT;
    glad_debug_glGetPointeri_vEXT = glad_glGetPointeri_vEXT;
    glad_debug_glGetPointervEXT = glad_glGetPointervEXT;
    glad_debug_glGetProgramEnvParameterdvARB = glad_glGetProgramEnvParameterdvARB;
    glad_debug_glGetProgramEnvParameterfvARB = glad_glGetProgramEnvParameterfvARB;
    glad_debug_glGetProgramInfoLog = glad_glGetProgramInfoLog;
    glad_debug_glGetProgramLocalParameterdvARB = glad_glGetProgramLocalParameterdvARB;
    glad_debug_glGetProgramLocalParameterfvARB = glad_glGetProgramLocalParameterfvARB;
    glad_debug_glGetProgramParameterdvNV = glad_glGetProgramParameterdvNV;
    glad_debug_glGetProgramParameterfvNV = glad_glGetProgramParameterfvNV;
    glad_debug_glGetProgramStringARB = glad_glGetProgramStringARB;
    glad_debug_glGetProgramStringNV = glad_glGetProgramStringNV;
    glad_debug_glGetProgramiv = glad_glGetProgramiv;
    glad_debug_glGetProgramivARB = glad_glGetProgramivARB;
    glad_debug_glGetProgramivNV = glad_glGetProgramivNV;
    glad_debug_glGetQueryObjecti64v = glad_glGetQueryObjecti64v;
    glad_debug_glGetQueryObjecti64vEXT = glad_glGetQueryObjecti64vEXT;
    glad_debug_glGetQueryObjectiv = glad_glGetQueryObjectiv;
    glad_debug_glGetQueryObjectivARB = glad_glGetQueryObjectivARB;
    glad_debug_glGetQueryObjectui64v = glad_glGetQueryObjectui64v;
    glad_debug_glGetQueryObjectui64vEXT = glad_glGetQueryObjectui64vEXT;
    glad_debug_glGetQueryObjectuiv = glad_glGetQueryObjectuiv;
    glad_debug_glGetQueryObjectuivARB = glad_glGetQueryObjectuivARB;
    glad_debug_glGetQueryiv = glad_glGetQueryiv;
    glad_debug_glGetQueryivARB = glad_glGetQueryivARB;
    glad_debug_glGetRenderbufferParameteriv = glad_glGetRenderbufferParameteriv;
    glad_debug_glGetRenderbufferParameterivEXT = glad_glGetRenderbufferParameterivEXT;
    glad_debug_glGetSamplerParameterIiv = glad_glGetSamplerParameterIiv;
    glad_debug_glGetSamplerParameterIuiv = glad_glGetSamplerParameterIuiv;
    glad_debug_glGetSamplerParameterfv = glad_glGetSamplerParameterfv;
    glad_debug_glGetSamplerParameteriv = glad_glGetSamplerParameteriv;
    glad_debug_glGetShaderInfoLog = glad_glGetShaderInfoLog;
    glad_debug_glGetShaderSource = glad_glGetShaderSource;
    glad_debug_glGetShaderSourceARB = glad_glGetShaderSourceARB;
    glad_debug_glGetShaderiv = glad_glGetShaderiv;
    glad_debug_glGetString = glad_glGetString;
    glad_debug_glGetStringi = glad_glGetStringi;
    glad_debug_glGetSynciv = glad_glGetSynciv;
    glad_debug_glGetTexImage = glad_glGetTexImage;
    glad_debug_glGetTexLevelParameterfv = glad_glGetTexLevelParameterfv;
    glad_debug_glGetTexLevelParameteriv = glad_glGetTexLevelParameteriv;
    glad_debug_glGetTexParameterIiv = glad_glGetTexParameterIiv;
    glad_debug_glGetTexParameterIivEXT = glad_glGetTexParameterIivEXT;
    glad_debug_glGetTexParameterIuiv = glad_glGetTexParameterIuiv;
    glad_debug_glGetTexParameterIuivEXT = glad_glGetTexParameterIuivEXT;
    glad_debug_glGetTexParameterfv = glad_glGetTexParameterfv;
    glad_debug_glGetTexParameteriv = glad_glGetTexParameteriv;
    glad_debug_glGetTextureImageEXT = glad_glGetTextureImageEXT;
    glad_debug_glGetTextureLevelParameterfvEXT = glad_glGetTextureLevelParameterfvEXT;
    glad_debug_glGetTextureLevelParameterivEXT = glad_glGetTextureLevelParameterivEXT;
    glad_debug_glGetTextureParameterIivEXT = glad_glGetTextureParameterIivEXT;
    glad_debug_glGetTextureParameterIuivEXT = glad_glGetTextureParameterIuivEXT;
    glad_debug_glGetTextureParameterfvEXT = glad_glGetTextureParameterfvEXT;
    glad_debug_glGetTextureParameterivEXT = glad_glGetTextureParameterivEXT;
    glad_debug_glGetTrackMatrixivNV = glad_glGetTrackMatrixivNV;
    glad_debug_glGetTransformFeedbackVarying = glad_glGetTransformFeedbackVarying;
    glad_debug_glGetTransformFeedbackVaryingEXT = glad_glGetTransformFeedbackVaryingEXT;
    glad_debug_glGetTransformFeedbackVaryingNV = glad_glGetTransformFeedbackVaryingNV;
    glad_debug_glGetUniformBlockIndex = glad_glGetUniformBlockIndex;
    glad_debug_glGetUniformIndices = glad_glGetUniformIndices;
    glad_debug_glGetUniformLocation = glad_glGetUniformLocation;
    glad_debug_glGetUniformLocationARB = glad_glGetUniformLocationARB;
    glad_debug_glGetUniformfv = glad_glGetUniformfv;
    glad_debug_glGetUniformfvARB = glad_glGetUniformfvARB;
    glad_debug_glGetUniformiv = glad_glGetUniformiv;
    glad_debug_glGetUniformivARB = glad_glGetUniformivARB;
    glad_debug_glGetUniformuiv = glad_glGetUniformuiv;
    glad_debug_glGetUniformuivEXT = glad_glGetUniformuivEXT;
    glad_debug_glGetVaryingLocationNV = glad_glGetVaryingLocationNV;
    glad_debug_glGetVertexArrayIntegeri_vEXT = glad_glGetVertexArrayIntegeri_vEXT;
    glad_debug_glGetVertexArrayIntegervEXT = glad_glGetVertexArrayIntegervEXT;
    glad_debug_glGetVertexArrayPointeri_vEXT = glad_glGetVertexArrayPointeri_vEXT;
    glad_debug_glGetVertexArrayPointervEXT = glad_glGetVertexArrayPointervEXT;
    glad_debug_glGetVertexAttribIiv = glad_glGetVertexAttribIiv;
    glad_debug_glGetVertexAttribIivEXT = glad_glGetVertexAttribIivEXT;
    glad_debug_glGetVertexAttribIuiv = glad_glGetVertexAttribIuiv;
    glad_debug_glGetVertexAttribIuivEXT = glad_glGetVertexAttribIuivEXT;
    glad_debug_glGetVertexAttribPointerv = glad_glGetVertexAttribPointerv;
    glad_debug_glGetVertexAttribPointervARB = glad_glGetVertexAttribPointervARB;
    glad_debug_glGetVertexAttribPointervNV = glad_glGetVertexAttribPointervNV;
    glad_debug_glGetVertexAttribdv = glad_glGetVertexAttribdv;
    glad_debug_glGetVertexAttribdvARB = glad_glGetVertexAttribdvARB;
    glad_debug_glGetVertexAttribdvNV = glad_glGetVertexAttribdvNV;
    glad_debug_glGetVertexAttribfv = glad_glGetVertexAttribfv;
    glad_debug_glGetVertexAttribfvARB = glad_glGetVertexAttribfvARB;
    glad_debug_glGetVertexAttribfvNV = glad_glGetVertexAttribfvNV;
    glad_debug_glGetVertexAttribiv = glad_glGetVertexAttribiv;
    glad_debug_glGetVertexAttribivARB = glad_glGetVertexAttribivARB;
    glad_debug_glGetVertexAttribivNV = glad_glGetVertexAttribivNV;
    glad_debug_glHint = glad_glHint;
    glad_debug_glIndexPointerEXT = glad_glIndexPointerEXT;
    glad_debug_glIsBuffer = glad_glIsBuffer;
    glad_debug_glIsBufferARB = glad_glIsBufferARB;
    glad_debug_glIsEnabled = glad_glIsEnabled;
    glad_debug_glIsEnabledIndexedEXT = glad_glIsEnabledIndexedEXT;
    glad_debug_glIsEnabledi = glad_glIsEnabledi;
    glad_debug_glIsFramebuffer = glad_glIsFramebuffer;
    glad_debug_glIsFramebufferEXT = glad_glIsFramebufferEXT;
    glad_debug_glIsProgram = glad_glIsProgram;
    glad_debug_glIsProgramARB = glad_glIsProgramARB;
    glad_debug_glIsProgramNV = glad_glIsProgramNV;
    glad_debug_glIsQuery = glad_glIsQuery;
    glad_debug_glIsQueryARB = glad_glIsQueryARB;
    glad_debug_glIsRenderbuffer = glad_glIsRenderbuffer;
    glad_debug_glIsRenderbufferEXT = glad_glIsRenderbufferEXT;
    glad_debug_glIsSampler = glad_glIsSampler;
    glad_debug_glIsShader = glad_glIsShader;
    glad_debug_glIsSync = glad_glIsSync;
    glad_debug_glIsTexture = glad_glIsTexture;
    glad_debug_glIsTextureEXT = glad_glIsTextureEXT;
    glad_debug_glIsVertexArray = glad_glIsVertexArray;
    glad_debug_glIsVertexArrayAPPLE = glad_glIsVertexArrayAPPLE;
    glad_debug_glLineWidth = glad_glLineWidth;
    glad_debug_glLinkProgram = glad_glLinkProgram;
    glad_debug_glLinkProgramARB = glad_glLinkProgramARB;
    glad_debug_glLoadProgramNV = glad_glLoadProgramNV;
    glad_debug_glLogicOp = glad_glLogicOp;
    glad_debug_glMapBuffer = glad_glMapBuffer;
    glad_debug_glMapBufferARB = glad_glMapBufferARB;
    glad_debug_glMapBufferRange = glad_glMapBufferRange;
    glad_debug_glMapNamedBufferEXT = glad_glMapNamedBufferEXT;
    glad_debug_glMapNamedBufferRangeEXT = glad_glMapNamedBufferRangeEXT;
    glad_debug_glMatrixFrustumEXT = glad_glMatrixFrustumEXT;
    glad_debug_glMatrixLoadIdentityEXT = glad_glMatrixLoadIdentityEXT;
    glad_debug_glMatrixLoadTransposedEXT = glad_glMatrixLoadTransposedEXT;
    glad_debug_glMatrixLoadTransposefEXT = glad_glMatrixLoadTransposefEXT;
    glad_debug_glMatrixLoaddEXT = glad_glMatrixLoaddEXT;
    glad_debug_glMatrixLoadfEXT = glad_glMatrixLoadfEXT;
    glad_debug_glMatrixMultTransposedEXT = glad_glMatrixMultTransposedEXT;
    glad_debug_glMatrixMultTransposefEXT = glad_glMatrixMultTransposefEXT;
    glad_debug_glMatrixMultdEXT = glad_glMatrixMultdEXT;
    glad_debug_glMatrixMultfEXT = glad_glMatrixMultfEXT;
    glad_debug_glMatrixOrthoEXT = glad_glMatrixOrthoEXT;
    glad_debug_glMatrixPopEXT = glad_glMatrixPopEXT;
    glad_debug_glMatrixPushEXT = glad_glMatrixPushEXT;
    glad_debug_glMatrixRotatedEXT = glad_glMatrixRotatedEXT;
    glad_debug_glMatrixRotatefEXT = glad_glMatrixRotatefEXT;
    glad_debug_glMatrixScaledEXT = glad_glMatrixScaledEXT;
    glad_debug_glMatrixScalefEXT = glad_glMatrixScalefEXT;
    glad_debug_glMatrixTranslatedEXT = glad_glMatrixTranslatedEXT;
    glad_debug_glMatrixTranslatefEXT = glad_glMatrixTranslatefEXT;
    glad_debug_glMultiDrawArrays = glad_glMultiDrawArrays;
    glad_debug_glMultiDrawArraysEXT = glad_glMultiDrawArraysEXT;
    glad_debug_glMultiDrawElements = glad_glMultiDrawElements;
    glad_debug_glMultiDrawElementsBaseVertex = glad_glMultiDrawElementsBaseVertex;
    glad_debug_glMultiDrawElementsEXT = glad_glMultiDrawElementsEXT;
    glad_debug_glMultiTexBufferEXT = glad_glMultiTexBufferEXT;
    glad_debug_glMultiTexCoord1dARB = glad_glMultiTexCoord1dARB;
    glad_debug_glMultiTexCoord1dvARB = glad_glMultiTexCoord1dvARB;
    glad_debug_glMultiTexCoord1fARB = glad_glMultiTexCoord1fARB;
    glad_debug_glMultiTexCoord1fvARB = glad_glMultiTexCoord1fvARB;
    glad_debug_glMultiTexCoord1iARB = glad_glMultiTexCoord1iARB;
    glad_debug_glMultiTexCoord1ivARB = glad_glMultiTexCoord1ivARB;
    glad_debug_glMultiTexCoord1sARB = glad_glMultiTexCoord1sARB;
    glad_debug_glMultiTexCoord1svARB = glad_glMultiTexCoord1svARB;
    glad_debug_glMultiTexCoord2dARB = glad_glMultiTexCoord2dARB;
    glad_debug_glMultiTexCoord2dvARB = glad_glMultiTexCoord2dvARB;
    glad_debug_glMultiTexCoord2fARB = glad_glMultiTexCoord2fARB;
    glad_debug_glMultiTexCoord2fvARB = glad_glMultiTexCoord2fvARB;
    glad_debug_glMultiTexCoord2iARB = glad_glMultiTexCoord2iARB;
    glad_debug_glMultiTexCoord2ivARB = glad_glMultiTexCoord2ivARB;
    glad_debug_glMultiTexCoord2sARB = glad_glMultiTexCoord2sARB;
    glad_debug_glMultiTexCoord2svARB = glad_glMultiTexCoord2svARB;
    glad_debug_glMultiTexCoord3dARB = glad_glMultiTexCoord3dARB;
    glad_debug_glMultiTexCoord3dvARB = glad_glMultiTexCoord3dvARB;
    glad_debug_glMultiTexCoord3fARB = glad_glMultiTexCoord3fARB;
    glad_debug_glMultiTexCoord3fvARB = glad_glMultiTexCoord3fvARB;
    glad_debug_glMultiTexCoord3iARB = glad_glMultiTexCoord3iARB;
    glad_debug_glMultiTexCoord3ivARB = glad_glMultiTexCoord3ivARB;
    glad_debug_glMultiTexCoord3sARB = glad_glMultiTexCoord3sARB;
    glad_debug_glMultiTexCoord3svARB = glad_glMultiTexCoord3svARB;
    glad_debug_glMultiTexCoord4dARB = glad_glMultiTexCoord4dARB;
    glad_debug_glMultiTexCoord4dvARB = glad_glMultiTexCoord4dvARB;
    glad_debug_glMultiTexCoord4fARB = glad_glMultiTexCoord4fARB;
    glad_debug_glMultiTexCoord4fvARB = glad_glMultiTexCoord4fvARB;
    glad_debug_glMultiTexCoord4iARB = glad_glMultiTexCoord4iARB;
    glad_debug_glMultiTexCoord4ivARB = glad_glMultiTexCoord4ivARB;
    glad_debug_glMultiTexCoord4sARB = glad_glMultiTexCoord4sARB;
    glad_debug_glMultiTexCoord4svARB = glad_glMultiTexCoord4svARB;
    glad_debug_glMultiTexCoordPointerEXT = glad_glMultiTexCoordPointerEXT;
    glad_debug_glMultiTexEnvfEXT = glad_glMultiTexEnvfEXT;
    glad_debug_glMultiTexEnvfvEXT = glad_glMultiTexEnvfvEXT;
    glad_debug_glMultiTexEnviEXT = glad_glMultiTexEnviEXT;
    glad_debug_glMultiTexEnvivEXT = glad_glMultiTexEnvivEXT;
    glad_debug_glMultiTexGendEXT = glad_glMultiTexGendEXT;
    glad_debug_glMultiTexGendvEXT = glad_glMultiTexGendvEXT;
    glad_debug_glMultiTexGenfEXT = glad_glMultiTexGenfEXT;
    glad_debug_glMultiTexGenfvEXT = glad_glMultiTexGenfvEXT;
    glad_debug_glMultiTexGeniEXT = glad_glMultiTexGeniEXT;
    glad_debug_glMultiTexGenivEXT = glad_glMultiTexGenivEXT;
    glad_debug_glMultiTexImage1DEXT = glad_glMultiTexImage1DEXT;
    glad_debug_glMultiTexImage2DEXT = glad_glMultiTexImage2DEXT;
    glad_debug_glMultiTexImage3DEXT = glad_glMultiTexImage3DEXT;
    glad_debug_glMultiTexParameterIivEXT = glad_glMultiTexParameterIivEXT;
    glad_debug_glMultiTexParameterIuivEXT = glad_glMultiTexParameterIuivEXT;
    glad_debug_glMultiTexParameterfEXT = glad_glMultiTexParameterfEXT;
    glad_debug_glMultiTexParameterfvEXT = glad_glMultiTexParameterfvEXT;
    glad_debug_glMultiTexParameteriEXT = glad_glMultiTexParameteriEXT;
    glad_debug_glMultiTexParameterivEXT = glad_glMultiTexParameterivEXT;
    glad_debug_glMultiTexRenderbufferEXT = glad_glMultiTexRenderbufferEXT;
    glad_debug_glMultiTexSubImage1DEXT = glad_glMultiTexSubImage1DEXT;
    glad_debug_glMultiTexSubImage2DEXT = glad_glMultiTexSubImage2DEXT;
    glad_debug_glMultiTexSubImage3DEXT = glad_glMultiTexSubImage3DEXT;
    glad_debug_glNamedBufferDataEXT = glad_glNamedBufferDataEXT;
    glad_debug_glNamedBufferStorageEXT = glad_glNamedBufferStorageEXT;
    glad_debug_glNamedBufferSubDataEXT = glad_glNamedBufferSubDataEXT;
    glad_debug_glNamedCopyBufferSubDataEXT = glad_glNamedCopyBufferSubDataEXT;
    glad_debug_glNamedFramebufferParameteriEXT = glad_glNamedFramebufferParameteriEXT;
    glad_debug_glNamedFramebufferRenderbufferEXT = glad_glNamedFramebufferRenderbufferEXT;
    glad_debug_glNamedFramebufferTexture1DEXT = glad_glNamedFramebufferTexture1DEXT;
    glad_debug_glNamedFramebufferTexture2DEXT = glad_glNamedFramebufferTexture2DEXT;
    glad_debug_glNamedFramebufferTexture3DEXT = glad_glNamedFramebufferTexture3DEXT;
    glad_debug_glNamedFramebufferTextureEXT = glad_glNamedFramebufferTextureEXT;
    glad_debug_glNamedFramebufferTextureFaceEXT = glad_glNamedFramebufferTextureFaceEXT;
    glad_debug_glNamedFramebufferTextureLayerEXT = glad_glNamedFramebufferTextureLayerEXT;
    glad_debug_glNamedProgramLocalParameter4dEXT = glad_glNamedProgramLocalParameter4dEXT;
    glad_debug_glNamedProgramLocalParameter4dvEXT = glad_glNamedProgramLocalParameter4dvEXT;
    glad_debug_glNamedProgramLocalParameter4fEXT = glad_glNamedProgramLocalParameter4fEXT;
    glad_debug_glNamedProgramLocalParameter4fvEXT = glad_glNamedProgramLocalParameter4fvEXT;
    glad_debug_glNamedProgramLocalParameterI4iEXT = glad_glNamedProgramLocalParameterI4iEXT;
    glad_debug_glNamedProgramLocalParameterI4ivEXT = glad_glNamedProgramLocalParameterI4ivEXT;
    glad_debug_glNamedProgramLocalParameterI4uiEXT = glad_glNamedProgramLocalParameterI4uiEXT;
    glad_debug_glNamedProgramLocalParameterI4uivEXT = glad_glNamedProgramLocalParameterI4uivEXT;
    glad_debug_glNamedProgramLocalParameters4fvEXT = glad_glNamedProgramLocalParameters4fvEXT;
    glad_debug_glNamedProgramLocalParametersI4ivEXT = glad_glNamedProgramLocalParametersI4ivEXT;
    glad_debug_glNamedProgramLocalParametersI4uivEXT = glad_glNamedProgramLocalParametersI4uivEXT;
    glad_debug_glNamedProgramStringEXT = glad_glNamedProgramStringEXT;
    glad_debug_glNamedRenderbufferStorageEXT = glad_glNamedRenderbufferStorageEXT;
    glad_debug_glNamedRenderbufferStorageMultisampleCoverageEXT = glad_glNamedRenderbufferStorageMultisampleCoverageEXT;
    glad_debug_glNamedRenderbufferStorageMultisampleEXT = glad_glNamedRenderbufferStorageMultisampleEXT;
    glad_debug_glNormalPointerEXT = glad_glNormalPointerEXT;
    glad_debug_glPixelStoref = glad_glPixelStoref;
    glad_debug_glPixelStorei = glad_glPixelStorei;
    glad_debug_glPointParameterf = glad_glPointParameterf;
    glad_debug_glPointParameterfARB = glad_glPointParameterfARB;
    glad_debug_glPointParameterfEXT = glad_glPointParameterfEXT;
    glad_debug_glPointParameterfSGIS = glad_glPointParameterfSGIS;
    glad_debug_glPointParameterfv = glad_glPointParameterfv;
    glad_debug_glPointParameterfvARB = glad_glPointParameterfvARB;
    glad_debug_glPointParameterfvEXT = glad_glPointParameterfvEXT;
    glad_debug_glPointParameterfvSGIS = glad_glPointParameterfvSGIS;
    glad_debug_glPointParameteri = glad_glPointParameteri;
    glad_debug_glPointParameteriNV = glad_glPointParameteriNV;
    glad_debug_glPointParameteriv = glad_glPointParameteriv;
    glad_debug_glPointParameterivNV = glad_glPointParameterivNV;
    glad_debug_glPointSize = glad_glPointSize;
    glad_debug_glPolygonMode = glad_glPolygonMode;
    glad_debug_glPolygonOffset = glad_glPolygonOffset;
    glad_debug_glPrimitiveRestartIndex = glad_glPrimitiveRestartIndex;
    glad_debug_glPrioritizeTexturesEXT = glad_glPrioritizeTexturesEXT;
    glad_debug_glProgramEnvParameter4dARB = glad_glProgramEnvParameter4dARB;
    glad_debug_glProgramEnvParameter4dvARB = glad_glProgramEnvParameter4dvARB;
    glad_debug_glProgramEnvParameter4fARB = glad_glProgramEnvParameter4fARB;
    glad_debug_glProgramEnvParameter4fvARB = glad_glProgramEnvParameter4fvARB;
    glad_debug_glProgramLocalParameter4dARB = glad_glProgramLocalParameter4dARB;
    glad_debug_glProgramLocalParameter4dvARB = glad_glProgramLocalParameter4dvARB;
    glad_debug_glProgramLocalParameter4fARB = glad_glProgramLocalParameter4fARB;
    glad_debug_glProgramLocalParameter4fvARB = glad_glProgramLocalParameter4fvARB;
    glad_debug_glProgramParameter4dNV = glad_glProgramParameter4dNV;
    glad_debug_glProgramParameter4dvNV = glad_glProgramParameter4dvNV;
    glad_debug_glProgramParameter4fNV = glad_glProgramParameter4fNV;
    glad_debug_glProgramParameter4fvNV = glad_glProgramParameter4fvNV;
    glad_debug_glProgramParameteriARB = glad_glProgramParameteriARB;
    glad_debug_glProgramParameters4dvNV = glad_glProgramParameters4dvNV;
    glad_debug_glProgramParameters4fvNV = glad_glProgramParameters4fvNV;
    glad_debug_glProgramStringARB = glad_glProgramStringARB;
    glad_debug_glProgramUniform1dEXT = glad_glProgramUniform1dEXT;
    glad_debug_glProgramUniform1dvEXT = glad_glProgramUniform1dvEXT;
    glad_debug_glProgramUniform1fEXT = glad_glProgramUniform1fEXT;
    glad_debug_glProgramUniform1fvEXT = glad_glProgramUniform1fvEXT;
    glad_debug_glProgramUniform1iEXT = glad_glProgramUniform1iEXT;
    glad_debug_glProgramUniform1ivEXT = glad_glProgramUniform1ivEXT;
    glad_debug_glProgramUniform1uiEXT = glad_glProgramUniform1uiEXT;
    glad_debug_glProgramUniform1uivEXT = glad_glProgramUniform1uivEXT;
    glad_debug_glProgramUniform2dEXT = glad_glProgramUniform2dEXT;
    glad_debug_glProgramUniform2dvEXT = glad_glProgramUniform2dvEXT;
    glad_debug_glProgramUniform2fEXT = glad_glProgramUniform2fEXT;
    glad_debug_glProgramUniform2fvEXT = glad_glProgramUniform2fvEXT;
    glad_debug_glProgramUniform2iEXT = glad_glProgramUniform2iEXT;
    glad_debug_glProgramUniform2ivEXT = glad_glProgramUniform2ivEXT;
    glad_debug_glProgramUniform2uiEXT = glad_glProgramUniform2uiEXT;
    glad_debug_glProgramUniform2uivEXT = glad_glProgramUniform2uivEXT;
    glad_debug_glProgramUniform3dEXT = glad_glProgramUniform3dEXT;
    glad_debug_glProgramUniform3dvEXT = glad_glProgramUniform3dvEXT;
    glad_debug_glProgramUniform3fEXT = glad_glProgramUniform3fEXT;
    glad_debug_glProgramUniform3fvEXT = glad_glProgramUniform3fvEXT;
    glad_debug_glProgramUniform3iEXT = glad_glProgramUniform3iEXT;
    glad_debug_glProgramUniform3ivEXT = glad_glProgramUniform3ivEXT;
    glad_debug_glProgramUniform3uiEXT = glad_glProgramUniform3uiEXT;
    glad_debug_glProgramUniform3uivEXT = glad_glProgramUniform3uivEXT;
    glad_debug_glProgramUniform4dEXT = glad_glProgramUniform4dEXT;
    glad_debug_glProgramUniform4dvEXT = glad_glProgramUniform4dvEXT;
    glad_debug_glProgramUniform4fEXT = glad_glProgramUniform4fEXT;
    glad_debug_glProgramUniform4fvEXT = glad_glProgramUniform4fvEXT;
    glad_debug_glProgramUniform4iEXT = glad_glProgramUniform4iEXT;
    glad_debug_glProgramUniform4ivEXT = glad_glProgramUniform4ivEXT;
    glad_debug_glProgramUniform4uiEXT = glad_glProgramUniform4uiEXT;
    glad_debug_glProgramUniform4uivEXT = glad_glProgramUniform4uivEXT;
    glad_debug_glProgramUniformMatrix2dvEXT = glad_glProgramUniformMatrix2dvEXT;
    glad_debug_glProgramUniformMatrix2fvEXT = glad_glProgramUniformMatrix2fvEXT;
    glad_debug_glProgramUniformMatrix2x3dvEXT = glad_glProgramUniformMatrix2x3dvEXT;
    glad_debug_glProgramUniformMatrix2x3fvEXT = glad_glProgramUniformMatrix2x3fvEXT;
    glad_debug_glProgramUniformMatrix2x4dvEXT = glad_glProgramUniformMatrix2x4dvEXT;
    glad_debug_glProgramUniformMatrix2x4fvEXT = glad_glProgramUniformMatrix2x4fvEXT;
    glad_debug_glProgramUniformMatrix3dvEXT = glad_glProgramUniformMatrix3dvEXT;
    glad_debug_glProgramUniformMatrix3fvEXT = glad_glProgramUniformMatrix3fvEXT;
    glad_debug_glProgramUniformMatrix3x2dvEXT = glad_glProgramUniformMatrix3x2dvEXT;
    glad_debug_glProgramUniformMatrix3x2fvEXT = glad_glProgramUniformMatrix3x2fvEXT;
    glad_debug_glProgramUniformMatrix3x4dvEXT = glad_glProgramUniformMatrix3x4dvEXT;
    glad_debug_glProgramUniformMatrix3x4fvEXT = glad_glProgramUniformMatrix3x4fvEXT;
    glad_debug_glProgramUniformMatrix4dvEXT = glad_glProgramUniformMatrix4dvEXT;
    glad_debug_glProgramUniformMatrix4fvEXT = glad_glProgramUniformMatrix4fvEXT;
    glad_debug_glProgramUniformMatrix4x2dvEXT = glad_glProgramUniformMatrix4x2dvEXT;
    glad_debug_glProgramUniformMatrix4x2fvEXT = glad_glProgramUniformMatrix4x2fvEXT;
    glad_debug_glProgramUniformMatrix4x3dvEXT = glad_glProgramUniformMatrix4x3dvEXT;
    glad_debug_glProgramUniformMatrix4x3fvEXT = glad_glProgramUniformMatrix4x3fvEXT;
    glad_debug_glProgramVertexLimitNV = glad_glProgramVertexLimitNV;
    glad_debug_glProvokingVertex = glad_glProvokingVertex;
    glad_debug_glProvokingVertexEXT = glad_glProvokingVertexEXT;
    glad_debug_glPushClientAttribDefaultEXT = glad_glPushClientAttribDefaultEXT;
    glad_debug_glQueryCounter = glad_glQueryCounter;
    glad_debug_glReadBuffer = glad_glReadBuffer;
    glad_debug_glReadPixels = glad_glReadPixels;
    glad_debug_glRenderbufferStorage = glad_glRenderbufferStorage;
    glad_debug_glRenderbufferStorageEXT = glad_glRenderbufferStorageEXT;
    glad_debug_glRenderbufferStorageMultisample = glad_glRenderbufferStorageMultisample;
    glad_debug_glRenderbufferStorageMultisampleEXT = glad_glRenderbufferStorageMultisampleEXT;
    glad_debug_glRequestResidentProgramsNV = glad_glRequestResidentProgramsNV;
    glad_debug_glSampleCoverage = glad_glSampleCoverage;
    glad_debug_glSampleCoverageARB = glad_glSampleCoverageARB;
    glad_debug_glSampleMaskIndexedNV = glad_glSampleMaskIndexedNV;
    glad_debug_glSampleMaski = glad_glSampleMaski;
    glad_debug_glSamplerParameterIiv = glad_glSamplerParameterIiv;
    glad_debug_glSamplerParameterIuiv = glad_glSamplerParameterIuiv;
    glad_debug_glSamplerParameterf = glad_glSamplerParameterf;
    glad_debug_glSamplerParameterfv = glad_glSamplerParameterfv;
    glad_debug_glSamplerParameteri = glad_glSamplerParameteri;
    glad_debug_glSamplerParameteriv = glad_glSamplerParameteriv;
    glad_debug_glScissor = glad_glScissor;
    glad_debug_glShaderSource = glad_glShaderSource;
    glad_debug_glShaderSourceARB = glad_glShaderSourceARB;
    glad_debug_glStencilFunc = glad_glStencilFunc;
    glad_debug_glStencilFuncSeparate = glad_glStencilFuncSeparate;
    glad_debug_glStencilFuncSeparateATI = glad_glStencilFuncSeparateATI;
    glad_debug_glStencilMask = glad_glStencilMask;
    glad_debug_glStencilMaskSeparate = glad_glStencilMaskSeparate;
    glad_debug_glStencilOp = glad_glStencilOp;
    glad_debug_glStencilOpSeparate = glad_glStencilOpSeparate;
    glad_debug_glStencilOpSeparateATI = glad_glStencilOpSeparateATI;
    glad_debug_glTexBuffer = glad_glTexBuffer;
    glad_debug_glTexBufferARB = glad_glTexBufferARB;
    glad_debug_glTexBufferEXT = glad_glTexBufferEXT;
    glad_debug_glTexCoordPointerEXT = glad_glTexCoordPointerEXT;
    glad_debug_glTexImage1D = glad_glTexImage1D;
    glad_debug_glTexImage2D = glad_glTexImage2D;
    glad_debug_glTexImage2DMultisample = glad_glTexImage2DMultisample;
    glad_debug_glTexImage3D = glad_glTexImage3D;
    glad_debug_glTexImage3DEXT = glad_glTexImage3DEXT;
    glad_debug_glTexImage3DMultisample = glad_glTexImage3DMultisample;
    glad_debug_glTexParameterIiv = glad_glTexParameterIiv;
    glad_debug_glTexParameterIivEXT = glad_glTexParameterIivEXT;
    glad_debug_glTexParameterIuiv = glad_glTexParameterIuiv;
    glad_debug_glTexParameterIuivEXT = glad_glTexParameterIuivEXT;
    glad_debug_glTexParameterf = glad_glTexParameterf;
    glad_debug_glTexParameterfv = glad_glTexParameterfv;
    glad_debug_glTexParameteri = glad_glTexParameteri;
    glad_debug_glTexParameteriv = glad_glTexParameteriv;
    glad_debug_glTexRenderbufferNV = glad_glTexRenderbufferNV;
    glad_debug_glTexSubImage1D = glad_glTexSubImage1D;
    glad_debug_glTexSubImage1DEXT = glad_glTexSubImage1DEXT;
    glad_debug_glTexSubImage2D = glad_glTexSubImage2D;
    glad_debug_glTexSubImage2DEXT = glad_glTexSubImage2DEXT;
    glad_debug_glTexSubImage3D = glad_glTexSubImage3D;
    glad_debug_glTexSubImage3DEXT = glad_glTexSubImage3DEXT;
    glad_debug_glTextureBufferEXT = glad_glTextureBufferEXT;
    glad_debug_glTextureBufferRangeEXT = glad_glTextureBufferRangeEXT;
    glad_debug_glTextureImage1DEXT = glad_glTextureImage1DEXT;
    glad_debug_glTextureImage2DEXT = glad_glTextureImage2DEXT;
    glad_debug_glTextureImage3DEXT = glad_glTextureImage3DEXT;
    glad_debug_glTexturePageCommitmentEXT = glad_glTexturePageCommitmentEXT;
    glad_debug_glTextureParameterIivEXT = glad_glTextureParameterIivEXT;
    glad_debug_glTextureParameterIuivEXT = glad_glTextureParameterIuivEXT;
    glad_debug_glTextureParameterfEXT = glad_glTextureParameterfEXT;
    glad_debug_glTextureParameterfvEXT = glad_glTextureParameterfvEXT;
    glad_debug_glTextureParameteriEXT = glad_glTextureParameteriEXT;
    glad_debug_glTextureParameterivEXT = glad_glTextureParameterivEXT;
    glad_debug_glTextureRenderbufferEXT = glad_glTextureRenderbufferEXT;
    glad_debug_glTextureStorage1DEXT = glad_glTextureStorage1DEXT;
    glad_debug_glTextureStorage2DEXT = glad_glTextureStorage2DEXT;
    glad_debug_glTextureStorage2DMultisampleEXT = glad_glTextureStorage2DMultisampleEXT;
    glad_debug_glTextureStorage3DEXT = glad_glTextureStorage3DEXT;
    glad_debug_glTextureStorage3DMultisampleEXT = glad_glTextureStorage3DMultisampleEXT;
    glad_debug_glTextureSubImage1DEXT = glad_glTextureSubImage1DEXT;
    glad_debug_glTextureSubImage2DEXT = glad_glTextureSubImage2DEXT;
    glad_debug_glTextureSubImage3DEXT = glad_glTextureSubImage3DEXT;
    glad_debug_glTrackMatrixNV = glad_glTrackMatrixNV;
    glad_debug_glTransformFeedbackAttribsNV = glad_glTransformFeedbackAttribsNV;
    glad_debug_glTransformFeedbackStreamAttribsNV = glad_glTransformFeedbackStreamAttribsNV;
    glad_debug_glTransformFeedbackVaryings = glad_glTransformFeedbackVaryings;
    glad_debug_glTransformFeedbackVaryingsEXT = glad_glTransformFeedbackVaryingsEXT;
    glad_debug_glTransformFeedbackVaryingsNV = glad_glTransformFeedbackVaryingsNV;
    glad_debug_glUniform1f = glad_glUniform1f;
    glad_debug_glUniform1fARB = glad_glUniform1fARB;
    glad_debug_glUniform1fv = glad_glUniform1fv;
    glad_debug_glUniform1fvARB = glad_glUniform1fvARB;
    glad_debug_glUniform1i = glad_glUniform1i;
    glad_debug_glUniform1iARB = glad_glUniform1iARB;
    glad_debug_glUniform1iv = glad_glUniform1iv;
    glad_debug_glUniform1ivARB = glad_glUniform1ivARB;
    glad_debug_glUniform1ui = glad_glUniform1ui;
    glad_debug_glUniform1uiEXT = glad_glUniform1uiEXT;
    glad_debug_glUniform1uiv = glad_glUniform1uiv;
    glad_debug_glUniform1uivEXT = glad_glUniform1uivEXT;
    glad_debug_glUniform2f = glad_glUniform2f;
    glad_debug_glUniform2fARB = glad_glUniform2fARB;
    glad_debug_glUniform2fv = glad_glUniform2fv;
    glad_debug_glUniform2fvARB = glad_glUniform2fvARB;
    glad_debug_glUniform2i = glad_glUniform2i;
    glad_debug_glUniform2iARB = glad_glUniform2iARB;
    glad_debug_glUniform2iv = glad_glUniform2iv;
    glad_debug_glUniform2ivARB = glad_glUniform2ivARB;
    glad_debug_glUniform2ui = glad_glUniform2ui;
    glad_debug_glUniform2uiEXT = glad_glUniform2uiEXT;
    glad_debug_glUniform2uiv = glad_glUniform2uiv;
    glad_debug_glUniform2uivEXT = glad_glUniform2uivEXT;
    glad_debug_glUniform3f = glad_glUniform3f;
    glad_debug_glUniform3fARB = glad_glUniform3fARB;
    glad_debug_glUniform3fv = glad_glUniform3fv;
    glad_debug_glUniform3fvARB = glad_glUniform3fvARB;
    glad_debug_glUniform3i = glad_glUniform3i;
    glad_debug_glUniform3iARB = glad_glUniform3iARB;
    glad_debug_glUniform3iv = glad_glUniform3iv;
    glad_debug_glUniform3ivARB = glad_glUniform3ivARB;
    glad_debug_glUniform3ui = glad_glUniform3ui;
    glad_debug_glUniform3uiEXT = glad_glUniform3uiEXT;
    glad_debug_glUniform3uiv = glad_glUniform3uiv;
    glad_debug_glUniform3uivEXT = glad_glUniform3uivEXT;
    glad_debug_glUniform4f = glad_glUniform4f;
    glad_debug_glUniform4fARB = glad_glUniform4fARB;
    glad_debug_glUniform4fv = glad_glUniform4fv;
    glad_debug_glUniform4fvARB = glad_glUniform4fvARB;
    glad_debug_glUniform4i = glad_glUniform4i;
    glad_debug_glUniform4iARB = glad_glUniform4iARB;
    glad_debug_glUniform4iv = glad_glUniform4iv;
    glad_debug_glUniform4ivARB = glad_glUniform4ivARB;
    glad_debug_glUniform4ui = glad_glUniform4ui;
    glad_debug_glUniform4uiEXT = glad_glUniform4uiEXT;
    glad_debug_glUniform4uiv = glad_glUniform4uiv;
    glad_debug_glUniform4uivEXT = glad_glUniform4uivEXT;
    glad_debug_glUniformBlockBinding = glad_glUniformBlockBinding;
    glad_debug_glUniformMatrix2fv = glad_glUniformMatrix2fv;
    glad_debug_glUniformMatrix2fvARB = glad_glUniformMatrix2fvARB;
    glad_debug_glUniformMatrix2x3fv = glad_glUniformMatrix2x3fv;
    glad_debug_glUniformMatrix2x4fv = glad_glUniformMatrix2x4fv;
    glad_debug_glUniformMatrix3fv = glad_glUniformMatrix3fv;
    glad_debug_glUniformMatrix3fvARB = glad_glUniformMatrix3fvARB;
    glad_debug_glUniformMatrix3x2fv = glad_glUniformMatrix3x2fv;
    glad_debug_glUniformMatrix3x4fv = glad_glUniformMatrix3x4fv;
    glad_debug_glUniformMatrix4fv = glad_glUniformMatrix4fv;
    glad_debug_glUniformMatrix4fvARB = glad_glUniformMatrix4fvARB;
    glad_debug_glUniformMatrix4x2fv = glad_glUniformMatrix4x2fv;
    glad_debug_glUniformMatrix4x3fv = glad_glUniformMatrix4x3fv;
    glad_debug_glUnmapBuffer = glad_glUnmapBuffer;
    glad_debug_glUnmapBufferARB = glad_glUnmapBufferARB;
    glad_debug_glUnmapNamedBufferEXT = glad_glUnmapNamedBufferEXT;
    glad_debug_glUseProgram = glad_glUseProgram;
    glad_debug_glUseProgramObjectARB = glad_glUseProgramObjectARB;
    glad_debug_glValidateProgram = glad_glValidateProgram;
    glad_debug_glValidateProgramARB = glad_glValidateProgramARB;
    glad_debug_glVertexArrayBindVertexBufferEXT = glad_glVertexArrayBindVertexBufferEXT;
    glad_debug_glVertexArrayColorOffsetEXT = glad_glVertexArrayColorOffsetEXT;
    glad_debug_glVertexArrayEdgeFlagOffsetEXT = glad_glVertexArrayEdgeFlagOffsetEXT;
    glad_debug_glVertexArrayFogCoordOffsetEXT = glad_glVertexArrayFogCoordOffsetEXT;
    glad_debug_glVertexArrayIndexOffsetEXT = glad_glVertexArrayIndexOffsetEXT;
    glad_debug_glVertexArrayMultiTexCoordOffsetEXT = glad_glVertexArrayMultiTexCoordOffsetEXT;
    glad_debug_glVertexArrayNormalOffsetEXT = glad_glVertexArrayNormalOffsetEXT;
    glad_debug_glVertexArraySecondaryColorOffsetEXT = glad_glVertexArraySecondaryColorOffsetEXT;
    glad_debug_glVertexArrayTexCoordOffsetEXT = glad_glVertexArrayTexCoordOffsetEXT;
    glad_debug_glVertexArrayVertexAttribBindingEXT = glad_glVertexArrayVertexAttribBindingEXT;
    glad_debug_glVertexArrayVertexAttribDivisorEXT = glad_glVertexArrayVertexAttribDivisorEXT;
    glad_debug_glVertexArrayVertexAttribFormatEXT = glad_glVertexArrayVertexAttribFormatEXT;
    glad_debug_glVertexArrayVertexAttribIFormatEXT = glad_glVertexArrayVertexAttribIFormatEXT;
    glad_debug_glVertexArrayVertexAttribIOffsetEXT = glad_glVertexArrayVertexAttribIOffsetEXT;
    glad_debug_glVertexArrayVertexAttribLFormatEXT = glad_glVertexArrayVertexAttribLFormatEXT;
    glad_debug_glVertexArrayVertexAttribLOffsetEXT = glad_glVertexArrayVertexAttribLOffsetEXT;
    glad_debug_glVertexArrayVertexAttribOffsetEXT = glad_glVertexArrayVertexAttribOffsetEXT;
    glad_debug_glVertexArrayVertexBindingDivisorEXT = glad_glVertexArrayVertexBindingDivisorEXT;
    glad_debug_glVertexArrayVertexOffsetEXT = glad_glVertexArrayVertexOffsetEXT;
    glad_debug_glVertexAttrib1d = glad_glVertexAttrib1d;
    glad_debug_glVertexAttrib1dARB = glad_glVertexAttrib1dARB;
    glad_debug_glVertexAttrib1dNV = glad_glVertexAttrib1dNV;
    glad_debug_glVertexAttrib1dv = glad_glVertexAttrib1dv;
    glad_debug_glVertexAttrib1dvARB = glad_glVertexAttrib1dvARB;
    glad_debug_glVertexAttrib1dvNV = glad_glVertexAttrib1dvNV;
    glad_debug_glVertexAttrib1f = glad_glVertexAttrib1f;
    glad_debug_glVertexAttrib1fARB = glad_glVertexAttrib1fARB;
    glad_debug_glVertexAttrib1fNV = glad_glVertexAttrib1fNV;
    glad_debug_glVertexAttrib1fv = glad_glVertexAttrib1fv;
    glad_debug_glVertexAttrib1fvARB = glad_glVertexAttrib1fvARB;
    glad_debug_glVertexAttrib1fvNV = glad_glVertexAttrib1fvNV;
    glad_debug_glVertexAttrib1s = glad_glVertexAttrib1s;
    glad_debug_glVertexAttrib1sARB = glad_glVertexAttrib1sARB;
    glad_debug_glVertexAttrib1sNV = glad_glVertexAttrib1sNV;
    glad_debug_glVertexAttrib1sv = glad_glVertexAttrib1sv;
    glad_debug_glVertexAttrib1svARB = glad_glVertexAttrib1svARB;
    glad_debug_glVertexAttrib1svNV = glad_glVertexAttrib1svNV;
    glad_debug_glVertexAttrib2d = glad_glVertexAttrib2d;
    glad_debug_glVertexAttrib2dARB = glad_glVertexAttrib2dARB;
    glad_debug_glVertexAttrib2dNV = glad_glVertexAttrib2dNV;
    glad_debug_glVertexAttrib2dv = glad_glVertexAttrib2dv;
    glad_debug_glVertexAttrib2dvARB = glad_glVertexAttrib2dvARB;
    glad_debug_glVertexAttrib2dvNV = glad_glVertexAttrib2dvNV;
    glad_debug_glVertexAttrib2f = glad_glVertexAttrib2f;
    glad_debug_glVertexAttrib2fARB = glad_glVertexAttrib2fARB;
    glad_debug_glVertexAttrib2fNV = glad_glVertexAttrib2fNV;
    glad_debug_glVertexAttrib2fv = glad_glVertexAttrib2fv;
    glad_debug_glVertexAttrib2fvARB = glad_glVertexAttrib2fvARB;
    glad_debug_glVertexAttrib2fvNV = glad_glVertexAttrib2fvNV;
    glad_debug_glVertexAttrib2s = glad_glVertexAttrib2s;
    glad_debug_glVertexAttrib2sARB = glad_glVertexAttrib2sARB;
    glad_debug_glVertexAttrib2sNV = glad_glVertexAttrib2sNV;
    glad_debug_glVertexAttrib2sv = glad_glVertexAttrib2sv;
    glad_debug_glVertexAttrib2svARB = glad_glVertexAttrib2svARB;
    glad_debug_glVertexAttrib2svNV = glad_glVertexAttrib2svNV;
    glad_debug_glVertexAttrib3d = glad_glVertexAttrib3d;
    glad_debug_glVertexAttrib3dARB = glad_glVertexAttrib3dARB;
    glad_debug_glVertexAttrib3dNV = glad_glVertexAttrib3dNV;
    glad_debug_glVertexAttrib3dv = glad_glVertexAttrib3dv;
    glad_debug_glVertexAttrib3dvARB = glad_glVertexAttrib3dvARB;
    glad_debug_glVertexAttrib3dvNV = glad_glVertexAttrib3dvNV;
    glad_debug_glVertexAttrib3f = glad_glVertexAttrib3f;
    glad_debug_glVertexAttrib3fARB = glad_glVertexAttrib3fARB;
    glad_debug_glVertexAttrib3fNV = glad_glVertexAttrib3fNV;
    glad_debug_glVertexAttrib3fv = glad_glVertexAttrib3fv;
    glad_debug_glVertexAttrib3fvARB = glad_glVertexAttrib3fvARB;
    glad_debug_glVertexAttrib3fvNV = glad_glVertexAttrib3fvNV;
    glad_debug_glVertexAttrib3s = glad_glVertexAttrib3s;
    glad_debug_glVertexAttrib3sARB = glad_glVertexAttrib3sARB;
    glad_debug_glVertexAttrib3sNV = glad_glVertexAttrib3sNV;
    glad_debug_glVertexAttrib3sv = glad_glVertexAttrib3sv;
    glad_debug_glVertexAttrib3svARB = glad_glVertexAttrib3svARB;
    glad_debug_glVertexAttrib3svNV = glad_glVertexAttrib3svNV;
    glad_debug_glVertexAttrib4Nbv = glad_glVertexAttrib4Nbv;
    glad_debug_glVertexAttrib4NbvARB = glad_glVertexAttrib4NbvARB;
    glad_debug_glVertexAttrib4Niv = glad_glVertexAttrib4Niv;
    glad_debug_glVertexAttrib4NivARB = glad_glVertexAttrib4NivARB;
    glad_debug_glVertexAttrib4Nsv = glad_glVertexAttrib4Nsv;
    glad_debug_glVertexAttrib4NsvARB = glad_glVertexAttrib4NsvARB;
    glad_debug_glVertexAttrib4Nub = glad_glVertexAttrib4Nub;
    glad_debug_glVertexAttrib4NubARB = glad_glVertexAttrib4NubARB;
    glad_debug_glVertexAttrib4Nubv = glad_glVertexAttrib4Nubv;
    glad_debug_glVertexAttrib4NubvARB = glad_glVertexAttrib4NubvARB;
    glad_debug_glVertexAttrib4Nuiv = glad_glVertexAttrib4Nuiv;
    glad_debug_glVertexAttrib4NuivARB = glad_glVertexAttrib4NuivARB;
    glad_debug_glVertexAttrib4Nusv = glad_glVertexAttrib4Nusv;
    glad_debug_glVertexAttrib4NusvARB = glad_glVertexAttrib4NusvARB;
    glad_debug_glVertexAttrib4bv = glad_glVertexAttrib4bv;
    glad_debug_glVertexAttrib4bvARB = glad_glVertexAttrib4bvARB;
    glad_debug_glVertexAttrib4d = glad_glVertexAttrib4d;
    glad_debug_glVertexAttrib4dARB = glad_glVertexAttrib4dARB;
    glad_debug_glVertexAttrib4dNV = glad_glVertexAttrib4dNV;
    glad_debug_glVertexAttrib4dv = glad_glVertexAttrib4dv;
    glad_debug_glVertexAttrib4dvARB = glad_glVertexAttrib4dvARB;
    glad_debug_glVertexAttrib4dvNV = glad_glVertexAttrib4dvNV;
    glad_debug_glVertexAttrib4f = glad_glVertexAttrib4f;
    glad_debug_glVertexAttrib4fARB = glad_glVertexAttrib4fARB;
    glad_debug_glVertexAttrib4fNV = glad_glVertexAttrib4fNV;
    glad_debug_glVertexAttrib4fv = glad_glVertexAttrib4fv;
    glad_debug_glVertexAttrib4fvARB = glad_glVertexAttrib4fvARB;
    glad_debug_glVertexAttrib4fvNV = glad_glVertexAttrib4fvNV;
    glad_debug_glVertexAttrib4iv = glad_glVertexAttrib4iv;
    glad_debug_glVertexAttrib4ivARB = glad_glVertexAttrib4ivARB;
    glad_debug_glVertexAttrib4s = glad_glVertexAttrib4s;
    glad_debug_glVertexAttrib4sARB = glad_glVertexAttrib4sARB;
    glad_debug_glVertexAttrib4sNV = glad_glVertexAttrib4sNV;
    glad_debug_glVertexAttrib4sv = glad_glVertexAttrib4sv;
    glad_debug_glVertexAttrib4svARB = glad_glVertexAttrib4svARB;
    glad_debug_glVertexAttrib4svNV = glad_glVertexAttrib4svNV;
    glad_debug_glVertexAttrib4ubNV = glad_glVertexAttrib4ubNV;
    glad_debug_glVertexAttrib4ubv = glad_glVertexAttrib4ubv;
    glad_debug_glVertexAttrib4ubvARB = glad_glVertexAttrib4ubvARB;
    glad_debug_glVertexAttrib4ubvNV = glad_glVertexAttrib4ubvNV;
    glad_debug_glVertexAttrib4uiv = glad_glVertexAttrib4uiv;
    glad_debug_glVertexAttrib4uivARB = glad_glVertexAttrib4uivARB;
    glad_debug_glVertexAttrib4usv = glad_glVertexAttrib4usv;
    glad_debug_glVertexAttrib4usvARB = glad_glVertexAttrib4usvARB;
    glad_debug_glVertexAttribDivisor = glad_glVertexAttribDivisor;
    glad_debug_glVertexAttribDivisorARB = glad_glVertexAttribDivisorARB;
    glad_debug_glVertexAttribI1i = glad_glVertexAttribI1i;
    glad_debug_glVertexAttribI1iEXT = glad_glVertexAttribI1iEXT;
    glad_debug_glVertexAttribI1iv = glad_glVertexAttribI1iv;
    glad_debug_glVertexAttribI1ivEXT = glad_glVertexAttribI1ivEXT;
    glad_debug_glVertexAttribI1ui = glad_glVertexAttribI1ui;
    glad_debug_glVertexAttribI1uiEXT = glad_glVertexAttribI1uiEXT;
    glad_debug_glVertexAttribI1uiv = glad_glVertexAttribI1uiv;
    glad_debug_glVertexAttribI1uivEXT = glad_glVertexAttribI1uivEXT;
    glad_debug_glVertexAttribI2i = glad_glVertexAttribI2i;
    glad_debug_glVertexAttribI2iEXT = glad_glVertexAttribI2iEXT;
    glad_debug_glVertexAttribI2iv = glad_glVertexAttribI2iv;
    glad_debug_glVertexAttribI2ivEXT = glad_glVertexAttribI2ivEXT;
    glad_debug_glVertexAttribI2ui = glad_glVertexAttribI2ui;
    glad_debug_glVertexAttribI2uiEXT = glad_glVertexAttribI2uiEXT;
    glad_debug_glVertexAttribI2uiv = glad_glVertexAttribI2uiv;
    glad_debug_glVertexAttribI2uivEXT = glad_glVertexAttribI2uivEXT;
    glad_debug_glVertexAttribI3i = glad_glVertexAttribI3i;
    glad_debug_glVertexAttribI3iEXT = glad_glVertexAttribI3iEXT;
    glad_debug_glVertexAttribI3iv = glad_glVertexAttribI3iv;
    glad_debug_glVertexAttribI3ivEXT = glad_glVertexAttribI3ivEXT;
    glad_debug_glVertexAttribI3ui = glad_glVertexAttribI3ui;
    glad_debug_glVertexAttribI3uiEXT = glad_glVertexAttribI3uiEXT;
    glad_debug_glVertexAttribI3uiv = glad_glVertexAttribI3uiv;
    glad_debug_glVertexAttribI3uivEXT = glad_glVertexAttribI3uivEXT;
    glad_debug_glVertexAttribI4bv = glad_glVertexAttribI4bv;
    glad_debug_glVertexAttribI4bvEXT = glad_glVertexAttribI4bvEXT;
    glad_debug_glVertexAttribI4i = glad_glVertexAttribI4i;
    glad_debug_glVertexAttribI4iEXT = glad_glVertexAttribI4iEXT;
    glad_debug_glVertexAttribI4iv = glad_glVertexAttribI4iv;
    glad_debug_glVertexAttribI4ivEXT = glad_glVertexAttribI4ivEXT;
    glad_debug_glVertexAttribI4sv = glad_glVertexAttribI4sv;
    glad_debug_glVertexAttribI4svEXT = glad_glVertexAttribI4svEXT;
    glad_debug_glVertexAttribI4ubv = glad_glVertexAttribI4ubv;
    glad_debug_glVertexAttribI4ubvEXT = glad_glVertexAttribI4ubvEXT;
    glad_debug_glVertexAttribI4ui = glad_glVertexAttribI4ui;
    glad_debug_glVertexAttribI4uiEXT = glad_glVertexAttribI4uiEXT;
    glad_debug_glVertexAttribI4uiv = glad_glVertexAttribI4uiv;
    glad_debug_glVertexAttribI4uivEXT = glad_glVertexAttribI4uivEXT;
    glad_debug_glVertexAttribI4usv = glad_glVertexAttribI4usv;
    glad_debug_glVertexAttribI4usvEXT = glad_glVertexAttribI4usvEXT;
    glad_debug_glVertexAttribIPointer = glad_glVertexAttribIPointer;
    glad_debug_glVertexAttribIPointerEXT = glad_glVertexAttribIPointerEXT;
    glad_debug_glVertexAttribP1ui = glad_glVertexAttribP1ui;
    glad_debug_glVertexAttribP1uiv = glad_glVertexAttribP1uiv;
    glad_debug_glVertexAttribP2ui = glad_glVertexAttribP2ui;
    glad_debug_glVertexAttribP2uiv = glad_glVertexAttribP2uiv;
    glad_debug_glVertexAttribP3ui = glad_glVertexAttribP3ui;
    glad_debug_glVertexAttribP3uiv = glad_glVertexAttribP3uiv;
    glad_debug_glVertexAttribP4ui = glad_glVertexAttribP4ui;
    glad_debug_glVertexAttribP4uiv = glad_glVertexAttribP4uiv;
    glad_debug_glVertexAttribPointer = glad_glVertexAttribPointer;
    glad_debug_glVertexAttribPointerARB = glad_glVertexAttribPointerARB;
    glad_debug_glVertexAttribPointerNV = glad_glVertexAttribPointerNV;
    glad_debug_glVertexAttribs1dvNV = glad_glVertexAttribs1dvNV;
    glad_debug_glVertexAttribs1fvNV = glad_glVertexAttribs1fvNV;
    glad_debug_glVertexAttribs1svNV = glad_glVertexAttribs1svNV;
    glad_debug_glVertexAttribs2dvNV = glad_glVertexAttribs2dvNV;
    glad_debug_glVertexAttribs2fvNV = glad_glVertexAttribs2fvNV;
    glad_debug_glVertexAttribs2svNV = glad_glVertexAttribs2svNV;
    glad_debug_glVertexAttribs3dvNV = glad_glVertexAttribs3dvNV;
    glad_debug_glVertexAttribs3fvNV = glad_glVertexAttribs3fvNV;
    glad_debug_glVertexAttribs3svNV = glad_glVertexAttribs3svNV;
    glad_debug_glVertexAttribs4dvNV = glad_glVertexAttribs4dvNV;
    glad_debug_glVertexAttribs4fvNV = glad_glVertexAttribs4fvNV;
    glad_debug_glVertexAttribs4svNV = glad_glVertexAttribs4svNV;
    glad_debug_glVertexAttribs4ubvNV = glad_glVertexAttribs4ubvNV;
    glad_debug_glVertexPointerEXT = glad_glVertexPointerEXT;
    glad_debug_glViewport = glad_glViewport;
    glad_debug_glWaitSync = glad_glWaitSync;
}


#ifdef __cplusplus
}
#endif
