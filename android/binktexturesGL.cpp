// Copyright Epic Games, Inc. All Rights Reserved.
#include "bink.h"

#if defined(__RADMAC__)

  #define TARGET_GL_CORE
  #define TEXFORMAT GL_RED
  #define LOAD_DYNAMICALLY

#elif defined(__RADIPHONE__)

  #include <OpenGLES/ES2/gl.h>
  #include <OpenGLES/ES2/glext.h>
  #include <alloca.h>
  #include <string.h>

  #define TARGET_GL_ES
  #define TEXFORMAT GL_LUMINANCE

  #define glGenVertexArrays glGenVertexArraysOES
  #define glBindVertexArray glBindVertexArrayOES
  #define glDeleteVertexArrays(c,p)

#elif defined(__RADANDROID__)

  #include <alloca.h>
  #include <string.h>

  #define TARGET_GL_ES
  #define glGenVertexArrays(c, p)
  #define glBindVertexArray(c)
  #define glDeleteVertexArrays(c,p)

  #define TEXFORMAT GL_LUMINANCE

  #include <android/log.h>
  #define rdbgprint(...) ((void)__android_log_print(ANDROID_LOG_INFO, "bink-gl-textures", __VA_ARGS__))

  #define LOAD_DYNAMICALLY

#elif defined(__RADLINUX__)

  #define TARGET_GL_CORE
  #define TEXFORMAT GL_RED
  #define LOAD_DYNAMICALLY

#elif defined(__RADNT__)

  #define TARGET_GL_CORE
  #define TEXFORMAT GL_RED
  #define LOAD_DYNAMICALLY

#elif defined(__RADEMSCRIPTEN__)

  #include <GLES2/gl2.h>
  #include <GLES2/gl2ext.h>

  #include <alloca.h>
  #include <string.h>

  #define TARGET_GL_ES
  #define glGenVertexArrays(c, p)
  #define glBindVertexArray(c)
  #define glDeleteVertexArrays(c,p)

  #define TEXFORMAT GL_LUMINANCE

#endif

#ifdef nofprintf
#undef rdbgprint
#define rdbgprint(...)
#else
#ifndef rdbgprint
#include <stdio.h>
#define rdbgprint(...) fprintf(stderr,__VA_ARGS__)
#endif
#endif

#define BINKGLFUNCTIONS

#if defined __RADANDROID__ && defined BUILDING_FOR_UNREAL_ONLY
#define HAS_VERTEXATTRIBBINDING 1
#else
#define HAS_VERTEXATTRIBBINDING 0
#endif

#include "binktextures.h"

#ifdef LOAD_DYNAMICALLY

typedef void voidfunc(void);

#ifdef __RADNT__

#define WIN32_LEAN_AND_MEAN
#include <windows.h> 
#include <malloc.h> 

static void * StartGLLoad( void )
{
  return (void *)LoadLibraryA("opengl32.dll");
}

typedef voidfunc* WINAPI wglGetProcAddressTYPE( LPCSTR lpszProc );
static wglGetProcAddressTYPE * wglGetProcAddressptr;

static voidfunc * GetGLProcAddress( void * handle, char const * proc )
{
  voidfunc * res = 0;

  if ( wglGetProcAddressptr == 0 )
  {
    wglGetProcAddressptr = (wglGetProcAddressTYPE*)GetProcAddress( (HMODULE)handle, "wglGetProcAddress" );
  }

  if ( wglGetProcAddressptr )
    res = (voidfunc*)wglGetProcAddressptr( proc );
  
  if ( !res )
      res = (voidfunc*)GetProcAddress( (HMODULE)handle, proc );

  return res;
}

static void EndGLLoad( void * handle )
{
  FreeLibrary( (HMODULE)handle );
}

#elif defined( __RADLINUX__ )

#include <stddef.h>
#include <stdlib.h>
#include <dlfcn.h>

#define APIENTRY

static void * StartGLLoad( void )
{
  return dlopen( "libGL.so.1", RTLD_LAZY | RTLD_GLOBAL );
}

typedef voidfunc* xglGetProcAddressTYPE( char const * lpszProc );
static xglGetProcAddressTYPE * xglGetProcAddressptr;

static voidfunc * GetGLProcAddress( void * handle, char const * proc )
{
  voidfunc * res = 0;

  if ( xglGetProcAddressptr == 0 )
    xglGetProcAddressptr = (xglGetProcAddressTYPE*)dlsym( handle, "glXGetProcAddress" );
  if ( xglGetProcAddressptr == 0 )
    xglGetProcAddressptr = (xglGetProcAddressTYPE*)dlsym( handle, "eglGetProcAddress" );

  if ( xglGetProcAddressptr )
    res = xglGetProcAddressptr( proc );
    
  if ( !res )
      res = (voidfunc*)dlsym( handle, proc );

  return res;
}

static void EndGLLoad( void * handle )
{
  dlclose( handle );
}

#elif defined( __RADANDROID__ )

#include <stddef.h>
#include <stdlib.h>
#include <dlfcn.h>

#define APIENTRY

static void * StartGLLoad( void )
{
  return dlopen( "libGLESv2.so", RTLD_LAZY | RTLD_GLOBAL );
}

static voidfunc * GetGLProcAddress( void * handle, char const * proc )
{
  voidfunc * res = 0;

  res = (voidfunc*)dlsym( handle, proc );

  return res;
}

static void EndGLLoad( void * handle )
{
  dlclose( handle );
}

#elif defined( __RADMAC__ )

#include <stddef.h>
#include <dlfcn.h>
#include <stdlib.h>

#define APIENTRY

static void * StartGLLoad( void )
{
  return dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_LAZY);
}

static voidfunc * GetGLProcAddress (void * handle, char const *name)
{
  voidfunc * res = 0;

  res = (voidfunc*) dlsym( handle, name);
  if ( res == 0 )
  {
    char aname[128];
    char * a;
    char const *n;

    // try apple extension version
    a = aname;
    n = name;
    while(*n) *a++=*n++; // copies name into aname
    n = "APPLE";
    while(*n) *a++=*n++; // copies apple into aname;
    *a=0;
    res = (voidfunc*) dlsym( handle, aname);
  }

  return res;
}

static void EndGLLoad( void * handle )
{
  dlclose( handle );
}

#else

#error Platform not supported.

#endif

typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;
typedef unsigned int GLbitfield;
typedef char GLchar;
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef int GLint;
typedef unsigned int GLuint;
typedef void GLvoid;
typedef int GLsizei;
typedef short GLshort;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;

#define GL_NO_ERROR                       0

#define GL_TRUE                           1
#define GL_FALSE                          0

#define GL_EXTENSIONS                     0x1F03
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_LUMINANCE                      0x1909

#define GL_BLEND                          0x0BE2
#define GL_ONE                            1
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_SRC_ALPHA                      0x0302
#define GL_TRIANGLES                      0x0003
#define GL_TRIANGLE_STRIP                 0x0005

#define GL_TEXTURE_2D                     0x0DE1
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_LINEAR                         0x2601

#define GL_MAJOR_VERSION                  0x821B
#define GL_MINOR_VERSION                  0x821C
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_STATIC_DRAW                    0x88E4
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_TEXTURE2                       0x84C2
#define GL_TEXTURE3                       0x84C3
#define GL_TEXTURE4                       0x84C4
#define GL_TEXTURE_MAX_LEVEL              0x813D
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_SHADING_LANGUAGE_VERSION       0x8B8C

#define GL_UNSIGNED_SHORT                 0x1403
#define GL_FLOAT                          0x1406
#define GL_UNSIGNED_BYTE                  0x1401

#define GL_RED                            0x1903
#define GL_CULL_FACE                      0x0B44

#define GL_DEPTH_TEST                     0x0B71
#define GL_STENCIL_TEST                   0x0B90
#define GL_SCISSOR_TEST                   0x0C11
#define GL_ALPHA_TEST                     0x0BC0

#define BINKGL_LIST \
   /*  ret, name, params */ \
    GLE(GLenum,    GetError,                void) \
    GLE(void,      LinkProgram,             GLuint program) \
    GLE(void,      GetProgramiv,            GLuint program, GLenum pname, GLint *params) \
    GLE(GLuint,    CreateShader,            GLenum type) \
    GLE(void,      GetIntegerv,             GLenum pname, GLint *params) \
    GLE(void,      ShaderSource,            GLuint shader, GLsizei count, const GLchar* const *string, const GLint *length) \
    GLE(void,      CompileShader,           GLuint shader) \
    GLE(void,      GetShaderiv,             GLuint shader, GLenum pname, GLint *params) \
    GLE(void,      GetShaderInfoLog,        GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) \
    GLE(void,      DeleteShader,            GLuint shader) \
    GLE(GLuint,    CreateProgram,           void) \
    GLE(void,      AttachShader,            GLuint program, GLuint shader) \
    GLE(void,      DetachShader,            GLuint program, GLuint shader) \
    GLE(void,      UseProgram,              GLuint program) \
    GLE(void,      DeleteProgram,           GLuint program) \
    GLE(void,      GenVertexArrays,         GLsizei n, GLuint *arrays) \
    GLE(void,      BindVertexArray,         GLuint array) \
    GLE(void,      BufferData,              GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) \
    GLE(void,      GenBuffers,              GLsizei n, GLuint *buffers) \
    GLE(void,      BindBuffer,              GLenum target, GLuint buffer) \
    GLE(void,      DeleteBuffers,           GLsizei n, const GLuint *buffers) \
    GLE(void,      TexParameteri,           GLenum target, GLenum pname, GLint param) \
    GLE(void,      ActiveTexture,           GLenum texture) \
    GLE(void,      Disable,                 GLenum cap) \
    GLE(void,      ColorMask,               GLboolean r, GLboolean g, GLboolean b, GLboolean a ) \
    GLE(void,      Enable,                  GLenum cap) \
    GLE(void,      BlendFunc,               GLenum sfactor, GLenum dfactor) \
    GLE(void,      GenTextures,             GLsizei n, GLuint *textures) \
    GLE(void,      DeleteTextures,          GLsizei n, const GLuint *textures) \
    GLE(void,      BindAttribLocation,      GLuint program, GLuint index, const GLchar *name) \
    GLE(GLint,     GetUniformLocation,      GLuint program, const GLchar *name) \
    GLE(void,      BindTexture,             GLenum target, GLuint texture) \
    GLE(void,      TexImage2D,              GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) \
    GLE(void,      TexSubImage2D,           GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) \
    GLE(void,      PixelStorei,             GLenum pname, GLint param) \
    GLE(void,      Uniform4f,               GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) \
    GLE(void,      Uniform4fv,              GLint location, GLsizei count, const GLfloat *value) \
    GLE(void,      DrawElements,            GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) \
    GLE(void,      DrawArrays,              GLenum mode, GLint first, GLsizei count) \
    GLE(GLubyte*,  GetString,               GLenum name) \
    GLE(void,      DeleteVertexArrays,      GLsizei n, const GLuint *arrays) \
    GLE(void,      EnableVertexAttribArray, GLuint index) \
    GLE(void,      DisableVertexAttribArray,GLuint index) \
    GLE(void,      VertexAttribPointer,     GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer) \
    GLE(void,      VertexAttribFormat,      GLuint index, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) \
    GLE(void,      VertexAttribBinding,     GLuint attribindex, GLuint bindingindex) \
    GLE(void,      BindVertexBuffer,        GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) \
    GLE(void,      Uniform1i,               GLint location, GLint v0) \
/* end */

#define GLE(ret, name, ...) typedef ret APIENTRY name##proc(__VA_ARGS__); static name##proc * gl##name;
BINKGL_LIST
#undef GLE

static void *GLhandle;

static void load_extensions( void )
{
  if ( GLhandle == 0 )
  {
    GLhandle = StartGLLoad();
#define GLE(ret, name, ...) gl##name = (name##proc *) GetGLProcAddress(GLhandle, "gl" #name);
    BINKGL_LIST
#undef GLE
  }
}

#endif

#ifndef GL_VERSION
#define GL_VERSION 0x1F02
#endif

#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif

#ifndef GL_TEXTURE_MAX_LEVEL              
#define GL_TEXTURE_MAX_LEVEL 0x813D
#endif

//-----------------------------------------------------------------------------
static S32 CheckGL(S32 Line)
{
  S32 err = glGetError();
  if (err == 0)
    return 0;
  rdbgprint("GL Error: %d @ %d\n", err, Line);
  return 1;
}


//-----------------------------------------------------------------------------
static S32 Shader_Link(U32 i_Shader)
{
  S32 Success = 0;

  glLinkProgram(i_Shader);
  if (CheckGL(__LINE__))
    return 0;

  glGetProgramiv(i_Shader, GL_LINK_STATUS, &Success);
  if (Success == 0)
    return 0;

  return 1;
}


//-----------------------------------------------------------------------------
static S32 Shader_Compile(S32 i_Target, char const* i_Source, U32* o_Shader)
{
  S32 Success;
  char const * ar[2];

  if (i_Source == 0)
    return 0;
  
  // Clear existing errors.
  CheckGL(__LINE__); 

  *o_Shader = glCreateShader(i_Target);
  if (CheckGL(__LINE__))
    return 0;

  // try compiling first with a 130 version, since this is required on 4.3.
  //   (I want to find the person that decided this and have them killed.)
  ar[0] ="#version 130\n";
  ar[1] = i_Source;
  glShaderSource(*o_Shader, 2, ar, 0);

  glCompileShader(*o_Shader);
  if ( glGetError() != 0 )
    goto try_other_ver;

  // did it compile?
  Success = 0;
  glGetShaderiv(*o_Shader, GL_COMPILE_STATUS, &Success);

  // if the compile failed, then try with a newer version and some fixups. GL, sigh.
  if ( !Success )
  {
   try_other_ver:
    ar[0] = (i_Target == GL_VERTEX_SHADER) ?
      "#version 150\n#define texture2D texture\n#define attribute in\n#define varying out\n" :
      "#version 150\n#define texture2D texture\n#define varying in\n#define gl_FragColor MyFragColor\nout vec4 MyFragColor;\n";
    ar[1] = i_Source;
    glShaderSource(*o_Shader, 2, ar, 0);

    glCompileShader(*o_Shader);
    if ( glGetError() != 0 )
      goto try_no_ver;

    // did it compile?
    Success = 0;
    glGetShaderiv(*o_Shader, GL_COMPILE_STATUS, &Success);

    // if the compile failed, then try without a version number
    if ( !Success )
    {
     try_no_ver: 
      // if the compile failed, try without a version number
      glShaderSource(*o_Shader, 1, (const char**)&i_Source, 0);
      glCompileShader(*o_Shader);

      if (CheckGL(__LINE__))
        return 0;

      // did this compile work? 
      Success = 0;
      glGetShaderiv(*o_Shader, GL_COMPILE_STATUS, &Success);

      if (Success == 0)
      {
        char Log[1024];
        glGetShaderInfoLog(*o_Shader, 1024, 0, Log);
        rdbgprint("%s\n", Log);
        rdbgprint("%s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
        return 0;
      }
    }
  }

  return 1;
}


//-----------------------------------------------------------------------------

#ifdef TARGET_GL_ES

static S32 hasext( char const * which )
{
#ifdef TARGET_GL_CORE
  // GL Core deprecates glGetString(GL_EXTENSIONS)
  GLint i, num_exts = 0;

  glGetIntegerv( GL_NUM_EXTENSIONS, &num_exts );
  for ( i = 0 ; i < num_exts ; i++ )
    if ( strcmp( which, (char const *) glGetStringi( GL_EXTENSIONS, i ) ) == 0 )
      return 1;

  return 0;
#else
  GLubyte const * start = glGetString( GL_EXTENSIONS );
  GLubyte const * current = start;

  for (;;)
  {
    GLubyte const * terminator;
    GLubyte const * where = (GLubyte *) strstr( (const char *) current, which );
    if (!where)
      break;

    terminator = where + strlen(which);
    if ( where == start || *(where - 1) == ' ' )
      if ( *terminator == ' ' || *terminator == '\0' )
        return 1;

    current = terminator;
  }

  return 0;
#endif
}

#endif

//-----------------------------------------------------------------------------
// Flags
static S32 s_HasUnpackRowLength = 1;
static S32 s_HasVertexAttribBinding = 0;
static S32 s_ES30Support = 1;
static S32 s_ES31Support = 1;
static S32 s_TextureMaxLevel = 1;

typedef struct BINKSHADERSGL
{
  BINKSHADERS pub;

  // GL Objects...
  GLuint VertexBuffer;
  GLuint VAO;
  GLuint vert_shader;

  // Attributes/uniforms/programs/etc...
  struct {
      GLuint Program;
      GLuint frag_shader;
      S32 yscaleNPAIndex;
      S32 crcNPAIndex;
      S32 cbcNPAIndex;
      S32 adjNPAIndex;
      S32 AlphaNPAIndex;
      S32 coordXYIndex;
      S32 coordUVIndex;
      S32 uscaleIndex;
      S32 ctcpIndex;
      S32 hdrIndex;
  } gl[3];

} BINKSHADERSGL; 


typedef struct BINKTEXTURESGL
{
  BINKTEXTURES pub;

  // OpenGL
  S32 video_width, video_height;
  S32 dirty;

  BINKSHADERSGL * shaders;
  HBINK bink;
  // this is the Bink info on the textures
  BINKFRAMEBUFFERS bink_buffers;

  F32 Ax,Ay, Bx,By, Cx,Cy;
  F32 alpha;
  S32 draw_type;
  S32 draw_flags;
  F32 u0,v0,u1,v1;
  F32 uscale[4];
  S32 texture_width[4];

  GLuint DummyAlpha;
  GLuint Ytexture_draw;
  GLuint cRtexture_draw;
  GLuint cBtexture_draw;
  GLuint Atexture_draw;
  GLuint Htexture_draw;

  // unused for now
  S32 tonemap;
  F32 exposure;
  F32 out_luma;

} BINKTEXTURESGL;

#if defined( BINKTEXTURESINDIRECTBINKCALLS )
  RADDEFFUNC void indirectBinkGetFrameBuffersInfo( HBINK bink, BINKFRAMEBUFFERS * fbset );
  #define BinkGetFrameBuffersInfo indirectBinkGetFrameBuffersInfo
  RADDEFFUNC void indirectBinkRegisterFrameBuffers( HBINK bink, BINKFRAMEBUFFERS * fbset );
  #define BinkRegisterFrameBuffers indirectBinkRegisterFrameBuffers
  RADDEFFUNC S32 indirectBinkAllocateFrameBuffers( HBINK bp, BINKFRAMEBUFFERS * set, U32 minimum_alignment );
  #define BinkAllocateFrameBuffers indirectBinkAllocateFrameBuffers
  RADDEFFUNC void * indirectBinkUtilMalloc(U64 bytes);
  #define BinkUtilMalloc indirectBinkUtilMalloc
  RADDEFFUNC void indirectBinkUtilFree(void * ptr);
  #define BinkUtilFree indirectBinkUtilFree
#endif

static BINKTEXTURES * Create_textures( BINKSHADERS * pshaders, HBINK bink, void * user_ptr );
static void Free_shaders( BINKSHADERS * pshaders );
static void Free_textures( BINKTEXTURES * ptextures );
static void Start_texture_update( BINKTEXTURES * ptextures );
static void Finish_texture_update( BINKTEXTURES * ptextures );
static void Draw_textures( BINKTEXTURES * ptextures,
                           BINKSHADERS * pshaders, 
                           void * graphics_context );
static void Set_draw_position( BINKTEXTURES * ptextures,
                               F32 x0, F32 y0, F32 x1, F32 y1 );
static void Set_draw_corners( BINKTEXTURES * ptextures,
                               F32 Ax, F32 Ay, F32 Bx, F32 By, F32 Cx, F32 Cy );
static void Set_source_rect( BINKTEXTURES * ptextures,
                             F32 u0, F32 v0, F32 u1, F32 v1 );
static void Set_alpha_settings( BINKTEXTURES * ptextures,
                                F32 alpha_value, S32 draw_type );
static void Set_hdr_settings( BINKTEXTURES * ptextures,
                                S32 tonemap, F32 exposure, S32 out_nits  );

// avoid crt if possible (might need -fno-builtin)
static void ourmemset(void * dest, char val, int bytes )
{
  unsigned char *d=(unsigned char*)dest;
  while(bytes)
  {
    *d++=val;
    --bytes;
  }
}

static S32 Shader_BuildOurShaders( BINKSHADERSGL * shaders )
{

#if defined(TARGET_GL_CORE) 
  #define SHADER_HEADER "#define mediump\n"
#elif defined(TARGET_GL_ES)
  #define SHADER_HEADER
#endif

  char const* VertexShader =
      SHADER_HEADER
      "attribute vec2 a_position;\n"
      "varying vec2 v_texcoords;\n"
      "uniform vec4 xy_xform[2];\n"
      "uniform vec4 uv_xform;\n"
      "void main()\n"
      "{\n"
      "   gl_Position.xy = a_position.x * xy_xform[0].xz + a_position.y * xy_xform[0].yw + xy_xform[1].xy;\n"
      "   gl_Position.z = 0.0;\n"
      "   gl_Position.w = 1.0;\n"
      "   v_texcoords = a_position * uv_xform.xy + uv_xform.zw;\n"
      "}\n";

  char const* FragmentShader =
      SHADER_HEADER
      "uniform sampler2D YTex;\n"
      "uniform sampler2D cRTex;\n"
      "uniform sampler2D cBTex;\n"
      "uniform sampler2D ATex;\n"
      "varying mediump vec2 v_texcoords;\n"
      "uniform mediump vec4 crcNPA;\n"
      "uniform mediump vec4 cbcNPA;\n"
      "uniform mediump vec4 adjNPA;\n"
      "uniform mediump vec4 yscaleNPA;\n"
      "uniform mediump vec4 AlphaNPA;\n"
      "uniform mediump vec4 uscale;\n"
      "void main()\n"
      "{\n"
      "   mediump float Y = texture2D(YTex, v_texcoords * vec2(uscale.x,1)).x;\n"
      "   mediump float cR = texture2D(cRTex, v_texcoords * vec2(uscale.y,1)).x;\n"
      "   mediump float cB = texture2D(cBTex, v_texcoords * vec2(uscale.z,1)).x;\n"
      "   mediump float A = texture2D(ATex, v_texcoords * vec2(uscale.w,1)).x;\n"
      "   mediump vec4 p;\n"
      "   p = yscaleNPA * Y;\n"
      "   p += crcNPA * cR;\n"
      "   p += cbcNPA * cB;\n"
      "   p += adjNPA;\n"
      "   p.w = A;\n"
      "   p *= AlphaNPA;\n"
      "   gl_FragColor = p;\n"
      "}\n";

  char const* FragmentShaderSRGB =
      SHADER_HEADER
      "uniform sampler2D YTex;\n"
      "uniform sampler2D cRTex;\n"
      "uniform sampler2D cBTex;\n"
      "uniform sampler2D ATex;\n"
      "varying mediump vec2 v_texcoords;\n"
      "uniform mediump vec4 crcNPA;\n"
      "uniform mediump vec4 cbcNPA;\n"
      "uniform mediump vec4 adjNPA;\n"
      "uniform mediump vec4 yscaleNPA;\n"
      "uniform mediump vec4 AlphaNPA;\n"
      "uniform mediump vec4 uscale;\n"
      "void main()\n"
      "{\n"
      "   mediump float Y = texture2D(YTex, v_texcoords * vec2(uscale.x,1)).x;\n"
      "   mediump float cR = texture2D(cRTex, v_texcoords * vec2(uscale.y,1)).x;\n"
      "   mediump float cB = texture2D(cBTex, v_texcoords * vec2(uscale.z,1)).x;\n"
      "   mediump float A = texture2D(ATex, v_texcoords * vec2(uscale.w,1)).x;\n"
      "   mediump vec4 p;\n"
      "   p = yscaleNPA * Y;\n"
      "   p += crcNPA * cR;\n"
      "   p += cbcNPA * cB;\n"
      "   p += adjNPA;\n"
      "   p.xyz = p.xyz * (p.xyz * (p.xyz * 0.305306011 + 0.682171111) + 0.012522878);\n"
      "   p.w = A;\n"
      "   p *= AlphaNPA;\n"
      "   gl_FragColor = p;\n"
      "}\n";

  char const* FragmentICtCpShader =
      SHADER_HEADER
      "uniform sampler2D I0Tex;\n"
      "uniform sampler2D CtTex;\n"
      "uniform sampler2D CpTex;\n"
      "uniform sampler2D ATex;\n"
      "uniform sampler2D I1Tex;\n"
      "varying mediump vec2 v_texcoords;\n"
      "uniform mediump vec4 AlphaNPA;\n"
      "uniform mediump vec4 uscale;\n"
      "uniform mediump vec4 ctcp;\n"
      "uniform mediump vec4 hdr;\n"
      "void main()\n"
      "{\n"
      "   mediump float I0 = texture2D(I0Tex, v_texcoords * vec2(uscale.x,1)).x;\n"
      "   mediump float I1 = texture2D(I1Tex, v_texcoords * vec2(uscale.x,1)).x;\n"
      "   mediump float Ct = texture2D(CtTex, v_texcoords * vec2(uscale.y,1)).x;\n"
      "   mediump float Cp = texture2D(CpTex, v_texcoords * vec2(uscale.z,1)).x;\n"
      "   mediump float A = texture2D(ATex, v_texcoords * vec2(uscale.w,1)).x;\n"
      "   mediump vec4 p;\n"
      "   mediump vec3 LMS;\n"
      "   mediump vec3 num;\n"
      "   mediump vec3 denom;\n"
      "   Ct = Ct * ctcp.x + ctcp.z;\n"
      "   Cp = Cp * ctcp.y + ctcp.w;\n"
      "   LMS = (I0 + I1) * hdr.xxx;\n"
      "   LMS += Ct * vec3(0.00860903703793281, -0.00860903703793281, 0.56003133571067909);\n"
      "   LMS += Cp * vec3(0.11102962500302593, -0.11102962500302593, -0.32062717498731880);\n"
      "   LMS = max(LMS, 0.0);\n"
          // SMPTE-2084 decode
      "   LMS = pow(LMS, vec3(1.0/(2523.0/4096.0*128.0)));\n"
      "   num = max(LMS - (3424.0/4096.0), 0.0);\n"
      "   denom = (2413.0/4096.0*32.0) - (2392.0/4096.0*32.0) * LMS;\n"
      "   LMS = pow(abs(num / denom), vec3(1.0/(2610.0/16384.0)));\n"
          // Convert from LMS to RGB
      "   p.xyz = LMS.x * (vec3(3.4366066943330793, -0.7913295555989289, -0.0259498996905927) * 125.0);\n"
      "   p.xyz += LMS.y * (vec3(-2.5064521186562705, 1.9836004517922909, -0.0989137147117265) * 125.0);\n"
      "   p.xyz += LMS.z * (vec3(0.0698454243231915, -0.1922708961933620, 1.1248636144023192) * 125.0);\n"
      "   p.xyz *= hdr.y;\n" // exposure
      "   p.w = A;\n"
      "   p *= AlphaNPA;\n"
      "   gl_FragColor = p;\n"
      "}\n";

  if (Shader_Compile(GL_VERTEX_SHADER, VertexShader, &shaders->vert_shader) == 0)
  {
    rdbgprint("Couldn't compile vertex shader.\n");
    return 0;
  }
  if (Shader_Compile(GL_FRAGMENT_SHADER, FragmentShader, &shaders->gl[0].frag_shader) == 0)
  {
    glDeleteShader(shaders->vert_shader);
    rdbgprint("Couldn't compile fragment shader.\n");
    return 0;
  }
  if (Shader_Compile(GL_FRAGMENT_SHADER, FragmentICtCpShader, &shaders->gl[1].frag_shader) == 0)
  {
    glDeleteShader(shaders->gl[0].frag_shader);
    glDeleteShader(shaders->vert_shader);
    rdbgprint("Couldn't compile fragment shader.\n");
    return 0;
  }
  if (Shader_Compile(GL_FRAGMENT_SHADER, FragmentShaderSRGB, &shaders->gl[2].frag_shader) == 0)
  {
    glDeleteShader(shaders->gl[0].frag_shader);
    glDeleteShader(shaders->gl[1].frag_shader);
    glDeleteShader(shaders->vert_shader);
    rdbgprint("Couldn't compile fragment shader.\n");
    return 0;
  }

  // Creation done in separate pass so that error conditions are easier
  for(int i = 0; i < 3; ++i) 
    shaders->gl[i].Program = glCreateProgram();

  for(int i = 0; i < 3; ++i) 
  {
    glAttachShader(shaders->gl[i].Program, shaders->vert_shader);
    glAttachShader(shaders->gl[i].Program, shaders->gl[i].frag_shader);

    glBindAttribLocation(shaders->gl[i].Program, 0, "a_position");

    if (Shader_Link(shaders->gl[i].Program) == 0)
    {
      glDeleteShader(shaders->vert_shader);
      for(int j = 0; j < 3; ++j) {
          glDeleteShader(shaders->gl[j].frag_shader);
          glDeleteProgram(shaders->gl[j].Program);
      }
      rdbgprint("Couldn't link program.\n");
      return 0;
    }

    glDetachShader(shaders->gl[i].Program, shaders->vert_shader);
    glDetachShader(shaders->gl[i].Program, shaders->gl[i].frag_shader);

    shaders->gl[i].coordXYIndex = glGetUniformLocation(shaders->gl[i].Program, "xy_xform");
    shaders->gl[i].coordUVIndex = glGetUniformLocation(shaders->gl[i].Program, "uv_xform");
    shaders->gl[i].uscaleIndex = glGetUniformLocation(shaders->gl[i].Program, "uscale");
    shaders->gl[i].AlphaNPAIndex = glGetUniformLocation(shaders->gl[i].Program, "AlphaNPA");
  }

  glUseProgram(shaders->gl[0].Program);
  glUniform1i(glGetUniformLocation(shaders->gl[0].Program, "YTex"), 0);
  glUniform1i(glGetUniformLocation(shaders->gl[0].Program, "cRTex"), 1);
  glUniform1i(glGetUniformLocation(shaders->gl[0].Program, "cBTex"), 2);
  glUniform1i(glGetUniformLocation(shaders->gl[0].Program, "ATex"), 3);

  glUseProgram(shaders->gl[1].Program);
  glUniform1i(glGetUniformLocation(shaders->gl[1].Program, "I0Tex"), 0);
  glUniform1i(glGetUniformLocation(shaders->gl[1].Program, "CtTex"), 1);
  glUniform1i(glGetUniformLocation(shaders->gl[1].Program, "CpTex"), 2);
  glUniform1i(glGetUniformLocation(shaders->gl[1].Program, "ATex"), 3);
  glUniform1i(glGetUniformLocation(shaders->gl[1].Program, "I1Tex"), 4);

  glUseProgram(shaders->gl[2].Program);
  glUniform1i(glGetUniformLocation(shaders->gl[2].Program, "YTex"), 0);
  glUniform1i(glGetUniformLocation(shaders->gl[2].Program, "cRTex"), 1);
  glUniform1i(glGetUniformLocation(shaders->gl[2].Program, "cBTex"), 2);
  glUniform1i(glGetUniformLocation(shaders->gl[2].Program, "ATex"), 3);

  shaders->gl[0].yscaleNPAIndex = glGetUniformLocation(shaders->gl[0].Program, "yscaleNPA");
  shaders->gl[0].crcNPAIndex = glGetUniformLocation(shaders->gl[0].Program, "crcNPA");
  shaders->gl[0].cbcNPAIndex = glGetUniformLocation(shaders->gl[0].Program, "cbcNPA");
  shaders->gl[0].adjNPAIndex = glGetUniformLocation(shaders->gl[0].Program, "adjNPA");

  shaders->gl[1].ctcpIndex = glGetUniformLocation(shaders->gl[1].Program, "ctcp");
  shaders->gl[1].hdrIndex = glGetUniformLocation(shaders->gl[1].Program, "hdr");

  shaders->gl[2].yscaleNPAIndex = glGetUniformLocation(shaders->gl[2].Program, "yscaleNPA");
  shaders->gl[2].crcNPAIndex = glGetUniformLocation(shaders->gl[2].Program, "crcNPA");
  shaders->gl[2].cbcNPAIndex = glGetUniformLocation(shaders->gl[2].Program, "cbcNPA");
  shaders->gl[2].adjNPAIndex = glGetUniformLocation(shaders->gl[2].Program, "adjNPA");

  glGenBuffers(1, &shaders->VertexBuffer);

  // GL core requires a VAO to be bound.
  glGenVertexArrays(1, &shaders->VAO);
  glBindVertexArray(shaders->VAO);

  static GLfloat const Vertices[] = {0,0, 1,0, 0,1, 1,1};
  glBindBuffer(GL_ARRAY_BUFFER, shaders->VertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);

#if HAS_VERTEXATTRIBBINDING
  if (s_HasVertexAttribBinding)
  {
    glEnableVertexAttribArray(0);
    glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(0, 0);
    glBindVertexBuffer(0, shaders->VertexBuffer, 0, 8);
  }
#endif

  glBindVertexArray(0);

  return CheckGL( __LINE__ ) == 0;
}

//-----------------------------------------------------------------------------
RADDEFFUNC BINKSHADERS * Create_Bink_shaders( void * dummy_device )
{
  BINKSHADERSGL * shaders;

#ifdef LOAD_DYNAMICALLY
  load_extensions();
#endif

#ifdef TARGET_GL_ES
  s_HasUnpackRowLength = hasext( "GL_EXT_unpack_subimage" );

  const char *version = (const char*)glGetString(GL_VERSION);
  const char *gles3_at = strstr(version, "OpenGL ES 3.");
  s_ES30Support = gles3_at ? 1 : 0;
  s_ES31Support = s_ES30Support && gles3_at[12] >= '1' ? 1 : 0;
  s_TextureMaxLevel = s_ES31Support;
  s_HasVertexAttribBinding = s_ES31Support;
#endif

  shaders = (BINKSHADERSGL*) BinkUtilMalloc( sizeof( *shaders ) );
  if ( shaders == 0 )
    return 0;

  ourmemset( shaders, 0, sizeof( *shaders ) );

  if ( !Shader_BuildOurShaders( shaders ) )
  {
    BinkUtilFree( shaders );
    return 0;
  }

  shaders->pub.Create_textures = Create_textures;
  shaders->pub.Free_shaders = Free_shaders;

  return &shaders->pub;
}

//-----------------------------------------------------------------------------
static void Free_shaders( BINKSHADERS * pshaders )
{
  BINKSHADERSGL * shaders = (BINKSHADERSGL*)pshaders;

  glDeleteBuffers(1, &shaders->VertexBuffer);
  glDeleteVertexArrays(1, &shaders->VAO);
  glDeleteShader(shaders->vert_shader);

  for(int i = 0; i < 3; ++i)
  {
    glDeleteProgram(shaders->gl[i].Program);
    glDeleteShader(shaders->gl[i].frag_shader);
    shaders->gl[i].Program = 0;
    shaders->gl[i].frag_shader = 0;
  }

  shaders->VertexBuffer = 0;
  shaders->VAO = 0;
  shaders->vert_shader = 0;

  BinkUtilFree( shaders );

#ifdef LOAD_DYNAMICALLY
  EndGLLoad( GLhandle );
  GLhandle = 0;
#endif  
}

//-----------------------------------------------------------------------------
static void CreateTexture( GLuint* texture, GLsizei width, GLsizei height, GLubyte clear_value )
{
  glGenTextures( 1, texture );
  glBindTexture( GL_TEXTURE_2D, *texture );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
  if(s_TextureMaxLevel) glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );

  // make sure the texture is created here.
  glTexImage2D( GL_TEXTURE_2D, 0, TEXFORMAT, width, height, 0, TEXFORMAT, GL_UNSIGNED_BYTE, NULL );

  // clear
  U8 * init = (U8 *) alloca( width );
  ourmemset( init, clear_value, width );
  for ( GLsizei y = 0 ; y < height ; y++ )
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, y, width, 1, TEXFORMAT, GL_UNSIGNED_BYTE, init );
}


//-----------------------------------------------------------------------------
static BINKTEXTURES * Create_textures( BINKSHADERS * pshaders, HBINK bink, void * user_ptr )
{
  BINKTEXTURESGL * textures;
  BINKFRAMEBUFFERS * bb;

  textures = (BINKTEXTURESGL *)BinkUtilMalloc( sizeof(*textures) );
  if ( textures == 0 )
    return 0;

  ourmemset( textures, 0, sizeof( *textures ) );

  textures->pub.user_ptr = user_ptr;
  textures->shaders = (BINKSHADERSGL*)pshaders;
  textures->bink = bink;
  textures->video_width = bink->Width;
  textures->video_height = bink->Height;

  bb = &textures->bink_buffers;

  BinkGetFrameBuffersInfo( bink, bb );

  // allocate the system memory buffers if not allocated
  if ( bb->Frames[ 0 ].YPlane.Buffer == 0 )
  {
    if ( !BinkAllocateFrameBuffers( bink, bb, 0 ) )
    {
      BinkUtilFree( textures );
      return 0;
    }
  }

  textures->uscale[0] = 1;
  textures->uscale[1] = 1;
  textures->uscale[2] = 1;
  textures->uscale[3] = 1;
  textures->texture_width[0] = textures->video_width;
  textures->texture_width[1] = textures->video_width / 2;
  textures->texture_width[2] = textures->video_width / 2;
  textures->texture_width[3] = textures->video_width;

  if ( !s_HasUnpackRowLength )
  {
    const U32 pitches[4] = {
      bb->Frames[0].YPlane.BufferPitch,
      bb->Frames[0].cRPlane.BufferPitch,
      bb->Frames[0].cBPlane.BufferPitch,
      bb->Frames[0].APlane.BufferPitch
    };
    for(int i = 0; i < 4; ++i) {
      if ( pitches[i] != (U32)textures->texture_width[i] ) {
        textures->uscale[i] = textures->texture_width[i] / (float)pitches[i];
        textures->texture_width[i] = pitches[i];
      }
    }
  }

  textures->Ytexture_draw = 0;
  if ( bb->Frames[0].YPlane.Allocate )
    CreateTexture( &textures->Ytexture_draw, textures->texture_width[0], textures->video_height, 0 );

  textures->cRtexture_draw = 0;
  if ( bb->Frames[0].cRPlane.Allocate )
    CreateTexture( &textures->cRtexture_draw, textures->texture_width[1], textures->video_height / 2, 128 );

  textures->cBtexture_draw = 0;
  if ( bb->Frames[0].cBPlane.Allocate )
    CreateTexture( &textures->cBtexture_draw, textures->texture_width[2], textures->video_height / 2, 128 );

  textures->Atexture_draw = 0;
  if ( bb->Frames[0].APlane.Allocate )
    CreateTexture( &textures->Atexture_draw, textures->texture_width[3], textures->video_height, 0 );

  textures->Htexture_draw = 0;
  if ( bb->Frames[0].HPlane.Allocate )
    CreateTexture( &textures->Htexture_draw, textures->texture_width[0], textures->video_height, 0 );

  // dummy alpha to use when there is no alpha plane
  textures->DummyAlpha = 0;
  CreateTexture( &textures->DummyAlpha, 1, 1, 255 );

  // Register our locked texture pointers with Bink
  BinkRegisterFrameBuffers( bink, bb );

  textures->pub.Ytexture = (void*)(UINTa)textures->Ytexture_draw;
  textures->pub.cRtexture = (void*)(UINTa)textures->cRtexture_draw;
  textures->pub.cBtexture = (void*)(UINTa)textures->cBtexture_draw;
  textures->pub.Atexture = (void*)(UINTa)textures->Atexture_draw;
  textures->pub.Htexture = (void*)(UINTa)textures->Htexture_draw;

  Set_draw_corners( &textures->pub, 0,0, 1,0, 0,1 );
  Set_source_rect( &textures->pub, 0,0,1,1 );
  Set_alpha_settings( &textures->pub, 1, 0 );
  Set_hdr_settings( &textures->pub, 0, 1.0f, 80 );

  textures->pub.Free_textures = Free_textures;
  textures->pub.Start_texture_update = Start_texture_update;
  textures->pub.Finish_texture_update = Finish_texture_update;
  textures->pub.Draw_textures = Draw_textures;
  textures->pub.Set_draw_position = Set_draw_position;
  textures->pub.Set_draw_corners = Set_draw_corners;
  textures->pub.Set_source_rect = Set_source_rect;
  textures->pub.Set_alpha_settings = Set_alpha_settings;
  textures->pub.Set_hdr_settings = Set_hdr_settings;

  return &textures->pub;
}  

//-----------------------------------------------------------------------------
static void Free_textures( BINKTEXTURES * ptextures )
{
  BINKTEXTURESGL * textures = (BINKTEXTURESGL*) ptextures;
  if (textures->bink_buffers.Frames[0].YPlane.Allocate)
    glDeleteTextures(1, &textures->Ytexture_draw);

  if (textures->bink_buffers.Frames[0].cRPlane.Allocate)
    glDeleteTextures(1, &textures->cRtexture_draw);

  if (textures->bink_buffers.Frames[0].cBPlane.Allocate)
    glDeleteTextures(1, &textures->cBtexture_draw);

  if (textures->bink_buffers.Frames[0].APlane.Allocate)
    glDeleteTextures(1, &textures->Atexture_draw);

  if (textures->bink_buffers.Frames[0].HPlane.Allocate)
    glDeleteTextures(1, &textures->Htexture_draw);

  if ( textures->DummyAlpha )
    glDeleteTextures(1, &textures->DummyAlpha);    

  BinkUtilFree( textures );
}

static inline void update_plane_texture_rect( GLuint tex, BINKPLANE const * plane, U32 w, U32 h )
{
  glBindTexture( GL_TEXTURE_2D, tex );

  if ( s_HasUnpackRowLength )
  {
    glPixelStorei( GL_UNPACK_ROW_LENGTH, plane->BufferPitch );
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, w, h, TEXFORMAT, GL_UNSIGNED_BYTE, (char *)plane->Buffer );
    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
  }
  else
  {
    // No unpack row length. But if the texture has the right stride, we can at least
    // upload it in one call by transferring whole rows.
    if ( plane->BufferPitch == w )
    {
      glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, w, h, TEXFORMAT, GL_UNSIGNED_BYTE, (char *)plane->Buffer );
    }
    else
    {
      // Slow path - upload strided textures line by line.
      for ( GLuint y = 0 ; y < h ; y++ )
        glTexSubImage2D( GL_TEXTURE_2D, 0, 0, y, w, 1, TEXFORMAT, GL_UNSIGNED_BYTE, (char *)plane->Buffer + y*plane->BufferPitch );
    }
  }
}

static void Start_texture_update( BINKTEXTURES * ptextures )
{
  // nothing to do here on GL.
}

static void Finish_texture_update( BINKTEXTURES * ptextures )
{
  BINKTEXTURESGL * textures = (BINKTEXTURESGL*) ptextures;
  textures->dirty = 1;
}


//-----------------------------------------------------------------------------

static void Draw_textures( BINKTEXTURES* ptextures, BINKSHADERS * pshaders, void * graphics_context )
{
  BINKTEXTURESGL * textures = (BINKTEXTURESGL*) ptextures;
  BINKSHADERSGL * shaders = (BINKSHADERSGL*) pshaders;
  F32 rgb_alpha_level = ( textures->draw_type == 1 ) ? textures->alpha : 1.0f;

  if ( shaders == 0 )
    shaders = textures->shaders;

  // always draw
  glDisable(GL_CULL_FACE);
  glColorMask(1,1,1,1);

  // vertexes
  glBindVertexArray(shaders->VAO);

  if (!s_HasVertexAttribBinding)
  {
    glBindBuffer(GL_ARRAY_BUFFER, shaders->VertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  }
#if HAS_VERTEXATTRIBBINDING
  else
  {
    glEnableVertexAttribArray(0);
    glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(0, 0);
    glBindVertexBuffer(0, shaders->VertexBuffer, 0, 8);
  }
#endif

  if ( textures->dirty )
  {
    textures->dirty = 0;
  
    BINKFRAMEPLANESET * bp_src;
    bp_src = &textures->bink_buffers.Frames[ textures->bink_buffers.FrameNum ];

    glActiveTexture(GL_TEXTURE0);

    update_plane_texture_rect(textures->Ytexture_draw,  &bp_src->YPlane,  textures->texture_width[0], textures->video_height);
    update_plane_texture_rect(textures->cRtexture_draw, &bp_src->cRPlane, textures->texture_width[1], textures->video_height/2);
    update_plane_texture_rect(textures->cBtexture_draw, &bp_src->cBPlane, textures->texture_width[2], textures->video_height/2);
    if ( textures->bink_buffers.Frames[0].APlane.Allocate )
      update_plane_texture_rect(textures->Atexture_draw, &bp_src->APlane, textures->texture_width[3], textures->video_height);
    if ( textures->bink_buffers.Frames[0].HPlane.Allocate )
      update_plane_texture_rect(textures->Htexture_draw, &bp_src->HPlane, textures->texture_width[0], textures->video_height);
  }

  // set the textures
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textures->Ytexture_draw);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, textures->cRtexture_draw);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, textures->cBtexture_draw);

  if (textures->bink_buffers.Frames[0].APlane.Allocate)
  {
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, textures->Atexture_draw);

    switch(textures->draw_type) 
    {
      case 0: // alpha blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
      case 1: // pre-multiplied alpha blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
      case 2: // opaque, copy alpha
        glDisable(GL_BLEND);
        break;
    }
  }
  else
  {
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, textures->DummyAlpha);
    glDisable(GL_BLEND);
  }

  if (textures->bink_buffers.Frames[0].HPlane.Allocate)
  {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, textures->Htexture_draw);
  }

  //glActiveTexture(GL_TEXTURE0);

  // get the right colorspace values
  F32 const * bink_consts = textures->bink->ColorSpace;

  int draw_srgb = textures->draw_flags & 0x80000000;
  int prgIdx = textures->bink_buffers.Frames[0].HPlane.Allocate ? 1 : draw_srgb ? 2 : 0;
  glUseProgram(shaders->gl[prgIdx].Program);

  if (prgIdx == 0 || prgIdx == 2)
  {
    // Standard non-hdr program
    glUniform4fv( shaders->gl[prgIdx].yscaleNPAIndex, 1, bink_consts + 12);
    glUniform4fv( shaders->gl[prgIdx].crcNPAIndex,    1, bink_consts + 0);
    glUniform4fv( shaders->gl[prgIdx].cbcNPAIndex,    1, bink_consts + 4);
    glUniform4fv( shaders->gl[prgIdx].adjNPAIndex,    1, bink_consts + 8);
  }
  else
  {
    float hdr_consts[4] = { textures->bink->ColorSpace[0], textures->exposure, textures->out_luma, 0 };
    glUniform4fv( shaders->gl[prgIdx].ctcpIndex, 1, bink_consts + 1);
    glUniform4fv( shaders->gl[prgIdx].hdrIndex, 1, hdr_consts);
  }
  glUniform4f( shaders->gl[prgIdx].AlphaNPAIndex, rgb_alpha_level, rgb_alpha_level, rgb_alpha_level, textures->alpha );

  {
    float xy_xform[2][4];
    xy_xform[0][0] = ( textures->Bx - textures->Ax ) * 2.0f;
    xy_xform[0][1] = ( textures->Cx - textures->Ax ) * 2.0f;
    xy_xform[0][2] = ( textures->By - textures->Ay ) * -2.0f, // view space has +y = up, our coords have +y = down
    xy_xform[0][3] = ( textures->Cy - textures->Ay ) * -2.0f, // view space has +y = up, our coords have +y = down
    xy_xform[1][0] = textures->Ax * 2.0f - 1.0f,
    xy_xform[1][1] = 1.0f - textures->Ay * 2.0f;
    xy_xform[1][2] = 0.0f;
    xy_xform[1][3] = 0.0f;
    glUniform4fv( shaders->gl[prgIdx].coordXYIndex, 2, xy_xform[0]);
  }

  glUniform4f( shaders->gl[prgIdx].coordUVIndex,
      (textures->u1 - textures->u0),
      (textures->v1 - textures->v0),
      textures->u0,
      textures->v0);
  glUniform4fv( shaders->gl[prgIdx].uscaleIndex, 1, textures->uscale);

  //glDrawElements( GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, 0 );
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // State Cleanup
  glUseProgram(0);
  //glDisableVertexAttribArray(0);
  //glBindVertexArray(0);
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

static void Set_draw_position( BINKTEXTURES * ptextures,
                               F32 x0, F32 y0, F32 x1, F32 y1 )
{
  BINKTEXTURESGL * textures = (BINKTEXTURESGL*) ptextures;
  textures->Ax = x0;
  textures->Ay = y0;
  textures->Bx = x1;
  textures->By = y0;
  textures->Cx = x0;
  textures->Cy = y1;
}

static void Set_draw_corners( BINKTEXTURES * ptextures,
                              F32 Ax, F32 Ay, F32 Bx, F32 By, F32 Cx, F32 Cy )
{
  BINKTEXTURESGL * textures = (BINKTEXTURESGL*) ptextures;
  textures->Ax = Ax;
  textures->Ay = Ay;
  textures->Bx = Bx;
  textures->By = By;
  textures->Cx = Cx;
  textures->Cy = Cy;
}

static void Set_source_rect( BINKTEXTURES * ptextures,
                             F32 u0, F32 v0, F32 u1, F32 v1 )
{
  BINKTEXTURESGL * textures = (BINKTEXTURESGL*) ptextures;
  textures->u0 = u0;
  textures->v0 = v0;
  textures->u1 = u1;
  textures->v1 = v1;
}

static void Set_alpha_settings( BINKTEXTURES * ptextures,
                                F32 alpha_value, S32 draw_type )
{
  BINKTEXTURESGL * textures = (BINKTEXTURESGL*) ptextures;
  textures->alpha = alpha_value;
  textures->draw_type = draw_type & 0x0FFFFFFF;
  textures->draw_flags = draw_type & 0xF0000000;
}

static void Set_hdr_settings( BINKTEXTURES * ptextures, S32 tonemap, F32 exposure, S32 out_nits ) 
{
  BINKTEXTURESGL * textures = (BINKTEXTURESGL*) ptextures;
  textures->tonemap = tonemap;
  textures->exposure = exposure;
  textures->out_luma = ((F32)out_nits)/80.f;
}

