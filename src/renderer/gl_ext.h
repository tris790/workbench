#ifndef GL_EXT_H
#define GL_EXT_H

#ifdef _WIN32
#include <GL/gl.h>
#include <stddef.h>
#include <windows.h>

/* These types are sometimes missing on Windows GL headers */
typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;

/* ===== WGL Extensions ===== */

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB 0x2093
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

#define WGL_DRAW_TO_WINDOW_ARB 0x2001
#define WGL_SUPPORT_OPENGL_ARB 0x2010
#define WGL_DOUBLE_BUFFER_ARB 0x2011
#define WGL_PIXEL_TYPE_ARB 0x2013
#define WGL_TYPE_RGBA_ARB 0x202B
#define WGL_COLOR_BITS_ARB 0x2014
#define WGL_DEPTH_BITS_ARB 0x2016
#define WGL_STENCIL_BITS_ARB 0x2023

typedef HGLRC(WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC,
                                                         HGLRC hShareContext,
                                                         const int *attribList);
typedef BOOL(WINAPI *PFNWGLCHOOSEPIXELFORMATARBPROC)(
    HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList,
    UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
typedef BOOL(WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int interval);

/* ===== OpenGL Extensions ===== */

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_MULTISAMPLE 0x809D

#define GL_RED 0x1903
#define GL_R8 0x8227
#define GL_RG 0x8227
#define GL_RG8 0x822B
#define GL_CLAMP_TO_EDGE 0x812F

/* Function pointer types */
typedef void(APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void(APIENTRY *PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void(APIENTRY *PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size,
                                            const void *data, GLenum usage);
typedef void(APIENTRY *PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset,
                                               GLsizeiptr size,
                                               const void *data);
typedef GLuint(APIENTRY *PFNGLCREATESHADERPROC)(GLenum type);
typedef void(APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count,
                                              const char *const *string,
                                              const GLint *length);
typedef void(APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void(APIENTRY *PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname,
                                             GLint *params);
typedef void(APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint shader,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  char *infoLog);
typedef GLuint(APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void(APIENTRY *PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void(APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void(APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void(APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname,
                                              GLint *params);
typedef void(APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   char *infoLog);
typedef void(APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
typedef void(APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void(APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size,
                                                     GLenum type,
                                                     GLboolean normalized,
                                                     GLsizei stride,
                                                     const void *pointer);
typedef void(APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef GLint(APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint program,
                                                     const char *name);
typedef void(APIENTRY *PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value);
typedef void(APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void(APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei n,
                                               const GLuint *buffers);
typedef void(APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n,
                                                    const GLuint *arrays);
typedef void(APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void(APIENTRY *PFNGLDELETESHADERPROC)(GLuint shader);
typedef void(APIENTRY *PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void(APIENTRY *PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void(APIENTRY *PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0,
                                           GLfloat v1);
typedef void(APIENTRY *PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0,
                                           GLfloat v1, GLfloat v2);
typedef void(APIENTRY *PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0,
                                           GLfloat v1, GLfloat v2, GLfloat v3);
typedef void(APIENTRY *PFNGLBLENDFUNCSEPARATEPROC)(GLenum sfactorRGB,
                                                   GLenum dfactorRGB,
                                                   GLenum sfactorAlpha,
                                                   GLenum dfactorAlpha);

/* Function declarations */
#ifdef GL_IMPLEMENTATION
#define GL_EXTERN
#else
#define GL_EXTERN extern
#endif

GL_EXTERN PFNGLGENBUFFERSPROC glGenBuffers;
GL_EXTERN PFNGLBINDBUFFERPROC glBindBuffer;
GL_EXTERN PFNGLBUFFERDATAPROC glBufferData;
GL_EXTERN PFNGLBUFFERSUBDATAPROC glBufferSubData;
GL_EXTERN PFNGLCREATESHADERPROC glCreateShader;
GL_EXTERN PFNGLSHADERSOURCEPROC glShaderSource;
GL_EXTERN PFNGLCOMPILESHADERPROC glCompileShader;
GL_EXTERN PFNGLGETSHADERIVPROC glGetShaderiv;
GL_EXTERN PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
GL_EXTERN PFNGLCREATEPROGRAMPROC glCreateProgram;
GL_EXTERN PFNGLATTACHSHADERPROC glAttachShader;
GL_EXTERN PFNGLLINKPROGRAMPROC glLinkProgram;
GL_EXTERN PFNGLUSEPROGRAMPROC glUseProgram;
GL_EXTERN PFNGLGETPROGRAMIVPROC glGetProgramiv;
GL_EXTERN PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
GL_EXTERN PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
GL_EXTERN PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
GL_EXTERN PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
GL_EXTERN PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
GL_EXTERN PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
GL_EXTERN PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
GL_EXTERN PFNGLACTIVETEXTUREPROC glActiveTexture;
GL_EXTERN PFNGLDELETEBUFFERSPROC glDeleteBuffers;
GL_EXTERN PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
GL_EXTERN PFNGLDELETEPROGRAMPROC glDeleteProgram;
GL_EXTERN PFNGLDELETESHADERPROC glDeleteShader;
GL_EXTERN PFNGLUNIFORM1IPROC glUniform1i;
GL_EXTERN PFNGLUNIFORM1FPROC glUniform1f;
GL_EXTERN PFNGLUNIFORM2FPROC glUniform2f;
GL_EXTERN PFNGLUNIFORM3FPROC glUniform3f;
GL_EXTERN PFNGLUNIFORM4FPROC glUniform4f;
GL_EXTERN PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;

GL_EXTERN PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
GL_EXTERN PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
GL_EXTERN PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

#endif /* _WIN32 */

#endif /* GL_EXT_H */
