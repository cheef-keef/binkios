// Copyright Epic Games, Inc. All Rights Reserved.
#include <string.h>
#include "bink.h"

#define BINKGLFUNCTIONS
#define BINKGLGPUFUNCTIONS
#include "binktextures.h"

#define MAX_PENDING_DECODES 3

// platform specifics

typedef void voidfunc(void);
int BinkGPU_GL_PersistentMappedBufferEnable = 1; // our "chicken bit"

#ifdef __RADNT__

#define WIN32_LEAN_AND_MEAN
#include <windows.h> 

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

#elif defined(__RADLINUX__)

#include <stddef.h>
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
#elif defined( __RADNX__ )

#include <stddef.h>

#define APIENTRY

static void * StartGLLoad( void )
{
  return 0;
}

extern voidfunc *eglGetProcAddress( const char *lpszProc );

static voidfunc *GetGLProcAddress( void * handle, char const * proc )
{
  return eglGetProcAddress( proc );
}

static void EndGLLoad( void * handle )
{
}

#else

#error Platform not supported!

#endif

#ifdef nofprintf
#define dprintf(...)
#else
#ifndef dprintf
#include <stdio.h>
#define dprintf(...) fprintf(stderr,__VA_ARGS__)
#endif
#endif

// ---- extension loading

// we run without gl headers on dynamic platforms
//   (which is everything right now)

#define LOAD_DYNAMICALLY
#ifdef LOAD_DYNAMICALLY

typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;
typedef unsigned int GLbitfield;
typedef char GLchar;
typedef unsigned char GLubyte;
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef int GLint;
typedef unsigned int GLuint;
typedef U64 GLuint64;
typedef void GLvoid;
typedef int GLsizei;
typedef short GLshort;

typedef struct __GLsync *GLsync;

#define GL_NO_ERROR                       0

#define GL_TRUE                           1
#define GL_FALSE                          0

#define GL_EXTENSIONS                     0x1F03

#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_TEXTURE_2D                     0x0DE1
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_BLEND                          0x0BE2
#define GL_ONE                            1
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_SRC_ALPHA                      0x0302
#define GL_TRIANGLE_STRIP                 0x0005

#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE_MAX_LEVEL              0x813D
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_TEXTURE_BUFFER                 0x8C2A
#define GL_RED                            0x1903
#define GL_R8UI                           0x8232
#define GL_R8                             0x8229
#define GL_RG8UI                          0x8238
#define GL_RG8                            0x822B
#define GL_R16I                           0x8233
#define GL_RG32I                          0x823B
#define GL_R32I                           0x8235
#define GL_RG16I                          0x8239
#define GL_RGBA32I                        0x8D82
#define GL_UNIFORM_BUFFER                 0x8A11
#define GL_LINK_STATUS                    0x8B82
#define GL_COMPUTE_SHADER                 0x91B9
#define GL_COMPILE_STATUS                 0x8B81
#define GL_MAJOR_VERSION                  0x821B
#define GL_MINOR_VERSION                  0x821C
#define GL_NUM_EXTENSIONS                 0x821D
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT 0x919F
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_DYNAMIC_COPY                   0x88EA
#define GL_STREAM_DRAW                    0x88E0
#define GL_READ_ONLY                      0x88B8
#define GL_READ_WRITE                     0x88BA
#define GL_WRITE_ONLY                     0x88B9
#define GL_STATIC_DRAW                    0x88E4
#define GL_MAP_WRITE_BIT                  0x0002
#define GL_MAP_INVALIDATE_BUFFER_BIT      0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT         0x0010
#define GL_MAP_PERSISTENT_BIT             0x0040
#define GL_TEXTURE_FETCH_BARRIER_BIT      0x00000008
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_CULL_FACE                      0x0B44
#define GL_SYNC_GPU_COMMANDS_COMPLETE     0x9117
#define GL_SYNC_FLUSH_COMMANDS_BIT        0x00000001
#define GL_TIMEOUT_IGNORED                0xFFFFFFFFFFFFFFFFull

#endif

#define BINKGPU_EXTENSION_LIST \
    /*  ret, name, params */ \
    GLE(void,      ActiveTexture,           GLenum texture) \
    GLE(void,      AttachShader,            GLuint program, GLuint shader) \
    GLE(void,      BindBuffer,              GLenum target, GLuint buffer) \
    GLE(void,      BindBufferBase,          GLenum target, GLuint index, GLuint buffer) \
    GLE(void,      BindBufferRange,         GLenum target, GLuint idnex, GLuint buffer, GLintptr offset, GLsizeiptr size) \
    GLE(void,      BindImageTexture,        GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format) \
    GLE(void,      BindVertexArray,         GLuint array) \
    GLE(void,      BufferData,              GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) \
    GLE(void,      BufferSubData,           GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data) \
    GLE(void,      BufferStorage,           GLenum target, GLsizeiptr size, const GLvoid *data, GLbitfield flags) /* not 4.3 core; it's ARB_buffer_storage */ \
    GLE(GLenum,    ClientWaitSync,          GLsync sync, GLbitfield flags, GLuint64 timeout) \
    GLE(void,      CompileShader,           GLuint shader) \
    GLE(GLuint,    CreateProgram,           void) \
    GLE(GLuint,    CreateShader,            GLenum type) \
    GLE(GLuint,    CreateShaderProgramv,    GLenum type, GLsizei count, const GLchar* const *strings) \
    GLE(void,      DeleteBuffers,           GLsizei n, const GLuint *buffers) \
    GLE(void,      DeleteProgram,           GLuint program) \
    GLE(void,      DeleteShader,            GLuint shader) \
    GLE(void,      DeleteSync,              GLsync sync) \
    GLE(void,      DeleteVertexArrays,      GLsizei n, const GLuint *arrays) \
    GLE(void,      DetachShader,            GLuint program, GLuint shader) \
    GLE(void,      DispatchCompute,         GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z) \
    GLE(GLsync,    FenceSync,               GLenum condition, GLbitfield flags) \
    GLE(void,      FlushMappedBufferRange,  GLenum target, GLintptr offset, GLsizeiptr length) \
    GLE(void,      GenBuffers,              GLsizei n, GLuint *buffers) \
    GLE(void,      GenVertexArrays,         GLsizei n, GLuint *arrays) \
    GLE(void,      GetIntegerv,             GLenum pname, GLint *params) \
    GLE(void,      GetProgramInfoLog,       GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) \
    GLE(void,      GetProgramiv,            GLuint program, GLenum pname, GLint *params) \
    GLE(void,      GetShaderInfoLog,        GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) \
    GLE(const GLubyte*,GetStringi,          GLenum name, GLuint index) \
    GLE(void,      ColorMask,               GLboolean r, GLboolean g, GLboolean b, GLboolean a ) \
    GLE(void,      GetShaderiv,             GLuint shader, GLenum pname, GLint *params) \
    GLE(void,      LinkProgram,             GLuint program) \
    GLE(GLvoid*,   MapBufferRange,          GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) \
    GLE(void,      MemoryBarrier,           GLbitfield barriers) \
    GLE(void,      ShaderSource,            GLuint shader, GLsizei count, const GLchar* const *string, const GLint *length) \
    GLE(void,      TexBuffer,               GLenum target, GLenum internalformat, GLuint buffer) \
    GLE(void,      TexBufferRange,          GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) \
    GLE(void,      TexStorage2D,            GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) \
    GLE(void,      TextureView,             GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers) \
    GLE(GLboolean, UnmapBuffer,             GLenum target) \
    GLE(void,      TexSubImage2D,           GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) \
    GLE(void,      UseProgram,              GLuint program) \
    GLE(void,      TextureBarrierNV,        void) \
    GLE(void,      TexParameteri,           GLenum target, GLenum pname, GLint param) \
    GLE(void,      GenTextures,             GLsizei n, GLuint *textures) \
    GLE(void,      BindTexture,             GLenum target, GLuint texture) \
    GLE(void,      DeleteTextures,          GLsizei n, const GLuint *textures) \
    GLE(GLenum,    GetError,                void) \
    GLE(void,      Disable,                 GLenum cap) \
    GLE(void,      Enable,                  GLenum cap) \
    GLE(void,      BlendFunc,               GLenum sfactor, GLenum dfactor) \
    GLE(void,      DrawArrays,              GLenum mode, GLint first, GLsizei count) \
/* end */

#define GLE(ret, name, ...) typedef ret APIENTRY name##proc(__VA_ARGS__); static name##proc * gl##name;
BINKGPU_EXTENSION_LIST
#undef GLE

static void *GLhandle;

static S32 hasext( char const * which )
{
    GLint i, num_exts = 0;

    // BinkGPU GL requires GL Core 4.3+ so we can do it the GL Core way.
    glGetIntegerv( GL_NUM_EXTENSIONS, &num_exts );
    for ( i = 0 ; i < num_exts ; ++i )
        if ( strcmp( which, (char const *)glGetStringi( GL_EXTENSIONS, i ) ) == 0 )
            return 1;

    return 0;
}

static void load_extensions( void )
{
    GLhandle = StartGLLoad();
#define GLE(ret, name, ...) gl##name = (name##proc *) GetGLProcAddress(GLhandle, "gl" #name);
    BINKGPU_EXTENSION_LIST
#undef GLE

    // We use NULL to denote the extension isn't available
    if ( !hasext( "GL_ARB_buffer_storage" ) )
        glBufferStorage = NULL;
}
// ----

enum PROGRAMS
{
    PROG_COMPUTE_BEGIN,
    PROG_LUMA_DCT = PROG_COMPUTE_BEGIN,
    PROG_LUMA_MDCT,
    PROG_CHROMA_DCT,
    PROG_CHROMA_MDCT,
    PROG_DC_PREDICT,
    PROG_LUMA_DEBLOCK,
    PROG_CHROMA_DEBLOCK,
    PROG_FILL_ALPHA,

    PROG_COMPUTE_END,
    PROG_DRAW = PROG_COMPUTE_END,
    PROG_DRAW_HDR,
    PROG_DRAW_HDR_PQ,
    PROG_DRAW_HDR_TONEMAP,

    PROG_COUNT,
};

typedef struct BINKSHADERSGLGPU
{
    BINKSHADERS pub;

    GLuint progs[PROG_COUNT];
    GLuint vert_shader;
    GLuint frag_shader[4];
} BINKSHADERSGLGPU;

typedef union BINKGPU_GL_PLANESET
{
    GLuint planes[4];
    struct
    {
        // Bink uses Y, Cr, Cb not Y, Cb, Cr as internal plane order.
        GLuint Y;
        GLuint CrCb;
        GLuint A;
        GLuint H;
    } t;
} BINKGPU_GL_PLANESET;

typedef struct FRAME_CONSTS_BUF {
    U32 frame_flags;
    U32 fill_alpha_value;
    U32 size_in_blocks[2];

    U32 dc_offsets[4];
    U32 dc_offsets2[4];
    U32 dc_bias;
    U32 dc_max;

    U32 cb_offs_ac; // in 32-bit ints
    U32 cb_offs_dc; // in 16-bit ints

    U32 max_offs_full[2];
    U32 max_offs_sub[2];

    F32 luma_tex_scale_offs[4];
    S32 mv_x_clamp[4];
    S32 mv_y_clamp[4];
    // 4 bit vectors.
    // .x: is block row i a "top" row?
    // .y: UD flag per column
    // .z: LR flag per row
    // .w: unused
    U32 flags[(BINKGPUMAXBLOCKS8 + 31)/32 * 4];
} FRAME_CONSTS_BUF;

typedef struct INPUT_DATA_TEX {
    GLuint luma_dc_tex[3];   // all pairs of luma textures are 0=Y 1=A 2=H
    GLuint chroma_dc_tex;
    GLuint luma_ac_tex[3];
    GLuint chroma_ac_tex;
    GLuint motion_tex;
    GLsync fence;
} INPUT_DATA_TEX;

typedef struct BINKTEXTURESGLGPU
{
    BINKTEXTURES pub;

    // Textures
    GLuint TexY;
    GLuint TexCrCb;
    GLuint TexA;
    GLuint TexH;
    GLuint DummyAlpha;

    // Internals
    BINKGPUDATABUFFERS gpu;
    HBINK bink;
    U32 w_blocks;
    U32 h_blocks;
    U32 num_blocks;
    U32 slice_count;
    U32 has_alpha;
    U32 has_hdr;

    U32 full_max_x_swiz;
    U32 full_max_y_swiz;
    U32 sub_max_x_swiz;
    U32 sub_max_y_swiz;

    BINKGPU_GL_PLANESET frames[2];
    BINKGPU_GL_PLANESET output[2];
    U32 cur_frame;
    
    U32 dc_offs[ BINKMAXPLANES + 1 ];
    GLuint dc_buf_out;
    GLuint luma_dc_tex_out[3];  // all pairs of luma textures are 0=Y 1=A 2=H
    GLuint luma_dc_tex_pred;    // only one needed for Y,A,H
    GLuint chroma_dc_tex_out;
    GLuint chroma_dc_tex_pred;

    U32 ac_offs[ BINKMAXPLANES + 1 ];
    U32 cb_offs_ac, cb_offs_dc;

    // Offsets from start of per-frame upload area to relevant pieces of data
    GLuint upload_buf;
    U32 upload_frame_const_offs;
    U32 upload_dc_offs;
    U32 upload_ac_offs;
    U32 upload_motion_offs;

    U32 upload_frame_total_size;

    INPUT_DATA_TEX upload_tex[ MAX_PENDING_DECODES ];
    U32 num_upload_bufs;
    U32 upload_cur;
    U8 * upload_persistent_map;

    GLuint scan_order_buf;
    GLuint scan_order_tex;

    GLuint deblock_ud_buf;
    GLuint deblock_lr_buf;

    U32 motion_size;

    GLuint draw_vao;
    GLuint draw_consts_buf;
    F32 draw_tex_size[4];
    F32 draw_tex_clamp[4];

    FRAME_CONSTS_BUF * frame_consts;

    BINKGPURANGE ranges[ BINKMAXPLANES ][ BINKMAXSLICES ];

    // flag temps should all be 32-bit aligned
    U8 top_flag_temp[ (BINKGPUMAXBLOCKS32 + 7)/8 ];
    U8 ud_flag_temp[ (BINKGPUMAXBLOCKS8 + 7)/8 ];
    U8 lr_flag_temp[ (BINKGPUMAXBLOCKS8 + 7)/8 ];

    BINKSHADERSGLGPU * shaders;

    F32 Ax,Ay, Bx,By, Cx,Cy;
    F32 alpha;
    S32 draw_type;
    F32 u0,v0,u1,v1;

    // unused for now
    S32 tonemap;
    F32 exposure;
    F32 out_luma;

} BINKTEXTURESGLGPU;

typedef struct DEBLOCK_CONSTS_BUF {
    U32 dir_mask[2];
    U32 row_inc_full;
    U32 row_inc_sub;
} DEBLOCK_CONSTS_BUF;

typedef struct DRAW_CONSTS_BUF {
    F32 xy_transform[2][4];
    F32 uv_transform[3][4];
    F32 tex_size[4];
    F32 tex_clamp[4];
    F32 alpha_mult[4];
    F32 cmatrix[4][4];
    F32 hdr[4];
    F32 ctcp[4];
} DRAW_CONSTS_BUF;

#if defined( BINKTEXTURESINDIRECTBINKCALLS )
    RADEXPFUNC S32  indirectBinkGetGPUDataBuffersInfo( HBINK bink, BINKGPUDATABUFFERS * gpu );
    #define BinkGetGPUDataBuffersInfo indirectBinkGetGPUDataBuffersInfo
    RADEXPFUNC void indirectBinkRegisterGPUDataBuffers( HBINK bink, BINKGPUDATABUFFERS * gpu );
    #define BinkRegisterGPUDataBuffers indirectBinkRegisterGPUDataBuffers
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
static void Draw_textures( BINKTEXTURES * ptextures, BINKSHADERS * pshaders, void * graphics_context );
static void Set_draw_position( BINKTEXTURES * ptextures, F32 x0, F32 y0, F32 x1, F32 y1 );
static void Set_draw_corners( BINKTEXTURES * ptextures,
                               F32 Ax, F32 Ay, F32 Bx, F32 By, F32 Cx, F32 Cy );
static void Set_source_rect( BINKTEXTURES * ptextures, F32 u0, F32 v0, F32 u1, F32 v1 );
static void Set_alpha_settings( BINKTEXTURES * ptextures, F32 alpha_value, S32 draw_type );
static void Set_hdr_settings( BINKTEXTURES * ptextures, S32 tonemap, F32 exposure, S32 out_nits );

static void set_tex_defaults( GLenum target, GLenum filter )
{
    glTexParameteri( target, GL_TEXTURE_MAX_LEVEL, 0 );
    glTexParameteri( target, GL_TEXTURE_MIN_FILTER, filter );
    glTexParameteri( target, GL_TEXTURE_MAG_FILTER, filter );
    glTexParameteri( target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
}

static GLuint mk_tex_bind( GLenum target )
{
    GLuint tex;
    glGenTextures( 1, &tex );
    glBindTexture( target, tex );
    return tex;
}

static GLuint make_gl_buf( GLenum type, size_t size, void const * initial, GLenum usage )
{
    GLuint id;
    glGenBuffers( 1, &id );
    glBindBuffer( type, id );
    glBufferData( type, size, initial, usage );
    return id;
}

static GLuint make_gl_tex( GLenum internalformat, GLsizei w, GLsizei h )
{
    GLuint tex = mk_tex_bind( GL_TEXTURE_2D );
    set_tex_defaults( GL_TEXTURE_2D, GL_NEAREST );
    glTexStorage2D( GL_TEXTURE_2D, 1, internalformat, w, h );

    return tex;
}

static GLuint make_gl_view( GLuint orig_tex, GLenum internalformat )
{
    GLuint tex;
    glGenTextures( 1, &tex );
    glTextureView( tex, GL_TEXTURE_2D, orig_tex, internalformat, 0, 1, 0, 1 );
    set_tex_defaults( GL_TEXTURE_2D, GL_LINEAR );
    return tex;
}

static GLuint make_gl_tex_buf( GLuint buffer, GLenum internalformat )
{
    GLuint tex = mk_tex_bind( GL_TEXTURE_BUFFER );
    glTexBuffer( GL_TEXTURE_BUFFER, internalformat, buffer );
    return tex;
}

static GLuint make_gl_tex_buf_range( GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size )
{
    GLuint tex = mk_tex_bind( GL_TEXTURE_BUFFER );
    glTexBufferRange( GL_TEXTURE_BUFFER, internalformat, buffer, offset, size );
    return tex;
}

static BINKGPU_GL_PLANESET make_plane_set( GLsizei w, GLsizei h, U32 has_alpha, U32 has_hdr )
{
    BINKGPU_GL_PLANESET p;
    p.t.Y = make_gl_tex( GL_R8UI, w, h );
    p.t.CrCb = make_gl_tex( GL_RG8UI, w / 2, h / 2 );
    p.t.A = has_alpha ? make_gl_tex( GL_R8UI, w, h ) : 0;
    p.t.H = has_hdr ? make_gl_tex( GL_R8UI, w, h ) : 0;
    return p;
}

static BINKGPU_GL_PLANESET float_views_plane_set( BINKGPU_GL_PLANESET const * in )
{
    BINKGPU_GL_PLANESET p;
    p.t.Y = make_gl_view( in->t.Y, GL_R8 );
    p.t.CrCb = make_gl_view( in->t.CrCb, GL_RG8 );
    p.t.A = 0;
    if ( in->t.A )
        p.t.A = make_gl_view( in->t.A, GL_R8 );
    if ( in->t.H )
        p.t.H = make_gl_view( in->t.H, GL_R8 );
    return p;
}

static void delete_plane_set( BINKGPU_GL_PLANESET * p )
{
    glDeleteTextures( 4, p->planes );
}

static void update_streaming_ubo( GLuint buffer, size_t size, void const * data )
{
    glBindBuffer( GL_UNIFORM_BUFFER, buffer );
    glBufferSubData( GL_UNIFORM_BUFFER, 0, size, data );
}

#include "binkgpu_gl_shaders.inl"

static S32 check_prog_ok( GLuint * prog )
{
    GLint ok = 0;
    glGetProgramiv( *prog, GL_LINK_STATUS, &ok );
    if ( !ok )
    {
        char errors[ 512 ];

        glGetProgramInfoLog( *prog, sizeof( errors ) - 2, &ok, errors );
        dprintf( "%s\n", errors );

        glDeleteProgram( *prog );
        *prog = 0;

        return 0;
    }
    
    return 1;
}

static S32 start_compile_program( GLuint * prog, char const * const * strings, int count )
{
    if ( *prog )
        return 1;

    *prog = glCreateShaderProgramv( GL_COMPUTE_SHADER, count, (GLchar const **) strings );
    return *prog != 0;
}

static GLuint compile_shader( GLenum type, char const * const * strings, int count )
{
    GLuint shader;
    GLint ok = 0;

    shader = glCreateShader( type );
    glShaderSource( shader, count, (GLchar const **) strings, 0 );
    glCompileShader( shader );

    glGetShaderiv( shader, GL_COMPILE_STATUS, &ok );
    if ( !ok )
    {
        char errors[ 512 ];

        glGetShaderInfoLog( shader, sizeof( errors ) - 2, &ok, errors );
        dprintf( "%s\n", errors );

        glDeleteShader( shader );
        shader = 0;
    }

    return shader;
}

static void APIENTRY null_barrier( void )
{
}

BINKSHADERS * Create_Bink_shaders( void * ptr )
{
    BINKSHADERSGLGPU * shaders;

    GLint major_ver = 0, minor_ver = 0;
    int i;

    shaders = (BINKSHADERSGLGPU*) BinkUtilMalloc( sizeof( BINKSHADERSGLGPU ) );
    if ( shaders == 0 )
      return 0;

    memset( shaders, 0, sizeof( *shaders ) );

    load_extensions();

    // we need at least GL 4.3 for compute
    glGetIntegerv( GL_MAJOR_VERSION, &major_ver );
    glGetIntegerv( GL_MINOR_VERSION, &minor_ver );
    if ( major_ver < 4 || ( major_ver == 4 && minor_ver < 3 ) )
        return 0;

    if (!glTextureBarrierNV)
        glTextureBarrierNV = null_barrier;

    // kick off compilations but don't check results yet so we get
    // potential parallelism.
    for ( i = 0 ; i < 4 ; i++ )
    {
        if ( !start_compile_program( &shaders->progs[PROG_LUMA_DCT + i], cshader_bink2block[i], NUMFRAGMENTS_cshader_bink2block ) )
            goto error;
    }

    if ( !start_compile_program( &shaders->progs[PROG_DC_PREDICT], cshader_dc_predict[0], NUMFRAGMENTS_cshader_dc_predict ) )
        goto error;
    if ( !start_compile_program( &shaders->progs[PROG_LUMA_DEBLOCK], cshader_deblock_full_plane[0], NUMFRAGMENTS_cshader_deblock_full_plane ) )
        goto error;
    if ( !start_compile_program( &shaders->progs[PROG_CHROMA_DEBLOCK], cshader_deblock_sub_plane[0], NUMFRAGMENTS_cshader_deblock_sub_plane ) )
        goto error;
    if ( !start_compile_program( &shaders->progs[PROG_FILL_ALPHA], cshader_fill_alpha_plane[0], NUMFRAGMENTS_cshader_fill_alpha_plane ) )
        goto error;

    if (shaders->vert_shader == 0)
        shaders->vert_shader = compile_shader( GL_VERTEX_SHADER, vshader_draw_gl[0], NUMFRAGMENTS_vshader_draw_gl );

    for (i = 0; i < 4; ++i) {
        if ( !shaders->progs[PROG_DRAW + i] )
        {
            switch (i) {
                case 0: shaders->frag_shader[i] = compile_shader( GL_FRAGMENT_SHADER, pshader_draw_gl[0], NUMFRAGMENTS_pshader_draw_gl ); break;
                case 1: shaders->frag_shader[i] = compile_shader( GL_FRAGMENT_SHADER, pshader_draw_pixel_ictcp_gl[0], NUMFRAGMENTS_pshader_draw_pixel_ictcp_gl ); break;
                case 2: shaders->frag_shader[i] = compile_shader( GL_FRAGMENT_SHADER, pshader_draw_pixel_ictcp_gl[1], NUMFRAGMENTS_pshader_draw_pixel_ictcp_gl ); break;
                case 3: shaders->frag_shader[i] = compile_shader( GL_FRAGMENT_SHADER, pshader_draw_pixel_ictcp_gl[2], NUMFRAGMENTS_pshader_draw_pixel_ictcp_gl ); break;
            }

            if ( shaders->vert_shader && shaders->frag_shader[i] )
            {
                GLuint prog = glCreateProgram();

                glAttachShader( prog, shaders->vert_shader );
                glAttachShader( prog, shaders->frag_shader[i] );
                glLinkProgram( prog );
                glDetachShader( prog, shaders->vert_shader );
                glDetachShader( prog, shaders->frag_shader[i] );
                
                check_prog_ok( &prog );
                shaders->progs[PROG_DRAW + i] = prog;
            }

            if ( !shaders->progs[PROG_DRAW + i] )
                goto error;
        }
    }

    // check for compilation errors here so we can report errors.
    for ( i = PROG_COMPUTE_BEGIN ; i < PROG_COMPUTE_END ; i++ )
        if ( !check_prog_ok( &shaders->progs[i] ) )
            goto error;

    shaders->pub.Create_textures = Create_textures;
    shaders->pub.Free_shaders = Free_shaders;

    return &shaders->pub;

error:
    Free_shaders( &shaders->pub );
    return 0;
}


static void Free_shaders( BINKSHADERS * pshaders )
{
    BINKSHADERSGLGPU * shaders = (BINKSHADERSGLGPU*)pshaders;

    int i;

    for ( i = 0 ; i < PROG_COUNT ; i++ )
    {
        glDeleteProgram( shaders->progs[i] );
        shaders->progs[i] = 0;
    }

    glDeleteShader( shaders->vert_shader );
    for (i = 0; i < 4; ++i) {
        glDeleteShader( shaders->frag_shader[i] );
    }

    BinkUtilFree( shaders );

    EndGLLoad( GLhandle );
}

static U32 align_up( U32 x, U32 multiple_of )
{
    return ( ( x + multiple_of - 1 ) / multiple_of ) * multiple_of;
}

static U32 upload_buf_alloc( BINKTEXTURESGLGPU * ctx, U32 size_bytes, U32 align )
{
    U32 offs = align_up( ctx->upload_frame_total_size, align );
    ctx->upload_frame_total_size = offs + size_bytes;
    return offs;
}

static BINKTEXTURES * Create_textures( BINKSHADERS * shaders, HBINK bink, void * user_ptr )
{
    BINKTEXTURESGLGPU * ctx;
    GLshort scan_order[ 64 ];
    DEBLOCK_CONSTS_BUF deblockc;
    U32 i;
    GLint gl_align;
    U32 dc_align;
    U32 full_dc_size, sub_dc_size;

    ctx = (BINKTEXTURESGLGPU *)BinkUtilMalloc( sizeof(BINKTEXTURESGLGPU) );
    if ( ctx == 0 )
      return 0;

    memset( ctx, 0, sizeof( *ctx ) );

    // if this returns 0, it means this file can't be GPU-decoded.
    if ( !BinkGetGPUDataBuffersInfo( bink, &ctx->gpu ) )
    {
        BinkUtilFree( ctx );
        dprintf("File cannot be GPU decoded\n");
        return 0;
    }

    // eat all existing GL errors before we start creating things
    while ( glGetError() != GL_NO_ERROR );

    ctx->pub.user_ptr = user_ptr;
    ctx->shaders = (BINKSHADERSGLGPU*)shaders;
    ctx->bink = bink;
    ctx->w_blocks = ( bink->Width + 31 ) / 32;
    ctx->h_blocks = ( bink->Height + 31 ) / 32;
    ctx->num_blocks = ctx->w_blocks * ctx->h_blocks;
    ctx->slice_count = bink->slices.slice_count < 1 ? 1 : bink->slices.slice_count;
    ctx->has_alpha = ( bink->OpenFlags & BINKALPHA ) != 0 ? 1 : 0;
    ctx->has_hdr = ( bink->OpenFlags & BINKHDR ) != 0 ? 1 : 0;

    for ( i = 0 ; i < 2 ; i++ )
    {
        ctx->frames[i] = make_plane_set( ctx->gpu.BufferWidth, ctx->gpu.BufferHeight, ctx->has_alpha, ctx->has_hdr );
        ctx->output[i] = float_views_plane_set( &ctx->frames[i] ); // create floating-point views
    }

    { 
        static U8 b255 = 255;
        ctx->DummyAlpha = make_gl_tex( GL_R8, 1, 1 );
        glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &b255 );
    }      

    // NOTE: I've seen some NV cards return "16" for this but then produce
    // memory corruption in the UMD and eventually crash the kernel-mode driver
    // if the actual alignment is less than 128. Use a min of 256, which matches
    // what some older AMD cards report; this is a value that should work for all
    // GL4 HW in circulation even when drivers report something too small here.
    glGetIntegerv( GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, &gl_align );
    dc_align = gl_align;
    if ( dc_align < 256 )
        dc_align = 256;

    // Frame constants are at start of upload buffer
    ctx->upload_frame_const_offs = upload_buf_alloc( ctx, (U32) sizeof( FRAME_CONSTS_BUF ), dc_align );

    // Determine DC buffer layout
    full_dc_size = ctx->num_blocks * 16 * sizeof( U16 );
    sub_dc_size = ctx->num_blocks * 4 * sizeof( U16 );
    ctx->dc_offs[0] = 0;
    ctx->dc_offs[1] = align_up( ctx->dc_offs[0] + full_dc_size, dc_align );
    ctx->dc_offs[2] =           ctx->dc_offs[1] + sub_dc_size; // no align-up since Cr/Cb share a texture.
    ctx->dc_offs[3] = align_up( ctx->dc_offs[2] + sub_dc_size, dc_align );
    ctx->dc_offs[4] = align_up( ctx->dc_offs[3] + full_dc_size * ctx->has_alpha, dc_align );
    ctx->dc_offs[5] = align_up( ctx->dc_offs[4] + full_dc_size * ctx->has_hdr, dc_align );

    ctx->upload_dc_offs = upload_buf_alloc( ctx, ctx->dc_offs[ BINKMAXPLANES ], dc_align );

    // Determine AC buffer layout
    ctx->ac_offs[0] = 0;
    ctx->ac_offs[1] = align_up( ctx->ac_offs[0] + ctx->gpu.AcBufSize[0], dc_align );
    ctx->ac_offs[2] =           ctx->ac_offs[1] + ctx->gpu.AcBufSize[1]; // no align-up since Cr/Cb share a texture.
    ctx->ac_offs[3] = align_up( ctx->ac_offs[2] + ctx->gpu.AcBufSize[2], dc_align );
    ctx->ac_offs[4] = align_up( ctx->ac_offs[3] + ctx->gpu.AcBufSize[3] * ctx->has_alpha, dc_align );
    ctx->ac_offs[5] = align_up( ctx->ac_offs[4] + ctx->gpu.AcBufSize[4] * ctx->has_hdr, dc_align );

    ctx->upload_ac_offs = upload_buf_alloc( ctx, ctx->ac_offs[ BINKMAXPLANES ], dc_align );

    ctx->cb_offs_ac = ctx->gpu.AcBufSize[1] / sizeof( U32 );
    ctx->cb_offs_dc = sub_dc_size / sizeof( U16 );

    // Motion buffer layout
    ctx->motion_size = ctx->num_blocks * 4 * sizeof( BINKGPUMOVEC );
    ctx->upload_motion_offs = upload_buf_alloc( ctx, ctx->motion_size, dc_align );

    // Alloc upload buffer and prepare associated views
    if ( glBufferStorage && BinkGPU_GL_PersistentMappedBufferEnable )
    {
        U32 total_buf_size;

        ctx->upload_frame_total_size = align_up( ctx->upload_frame_total_size, dc_align );
        ctx->num_upload_bufs = ( bink->OpenFlags & BINKUSETRIPLEBUFFERING ) ? 3 : 2;
        if ( ctx->num_upload_bufs > MAX_PENDING_DECODES )
            ctx->num_upload_bufs = MAX_PENDING_DECODES;

        total_buf_size = ctx->num_upload_bufs * ctx->upload_frame_total_size;

        glGenBuffers( 1, &ctx->upload_buf );
        glBindBuffer( GL_TEXTURE_BUFFER, ctx->upload_buf );
        glBufferStorage( GL_TEXTURE_BUFFER, total_buf_size, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT );

        ctx->upload_persistent_map = (U8 *)glMapBufferRange( GL_TEXTURE_BUFFER, 0, total_buf_size, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT );
        if ( !ctx->upload_persistent_map )
        {
            // If we couldn't successfully create and map a persistent mapped buffer,
            // delete the persistent buffer and fall back to regular single-buffer,
            // invalidate-based path.
            glDeleteBuffers( 1, &ctx->upload_buf );
            ctx->upload_buf = 0;
        }
    }

    if ( !ctx->upload_persistent_map )
    {
        ctx->num_upload_bufs = 1;
        ctx->upload_buf = make_gl_buf( GL_TEXTURE_BUFFER, ctx->upload_frame_total_size, NULL, GL_DYNAMIC_DRAW );
    }

    for ( i = 0 ; i < ctx->num_upload_bufs ; ++i )
    {
        INPUT_DATA_TEX * intex = &ctx->upload_tex[ i ];
        GLintptr base = ctx->upload_frame_total_size * i;

        intex->luma_dc_tex[0] = make_gl_tex_buf_range( ctx->upload_buf, GL_R16I, base + ctx->upload_dc_offs + ctx->dc_offs[BINKGPUPLANE_Y], full_dc_size );
        if ( ctx->has_alpha )
            intex->luma_dc_tex[1] = make_gl_tex_buf_range( ctx->upload_buf, GL_R16I, base + ctx->upload_dc_offs + ctx->dc_offs[BINKGPUPLANE_A], full_dc_size );
        if ( ctx->has_hdr )
            intex->luma_dc_tex[2] = make_gl_tex_buf_range( ctx->upload_buf, GL_R16I, base + ctx->upload_dc_offs + ctx->dc_offs[BINKGPUPLANE_H], full_dc_size );
        intex->chroma_dc_tex = make_gl_tex_buf_range( ctx->upload_buf, GL_R16I, base + ctx->upload_dc_offs + ctx->dc_offs[BINKGPUPLANE_CR], 2 * sub_dc_size );

        intex->luma_ac_tex[0] = make_gl_tex_buf_range( ctx->upload_buf, GL_R32I, base + ctx->upload_ac_offs + ctx->ac_offs[BINKGPUPLANE_Y], ctx->gpu.AcBufSize[0] );
        if ( ctx->has_alpha )
            intex->luma_ac_tex[1] = make_gl_tex_buf_range( ctx->upload_buf, GL_R32I, base + ctx->upload_ac_offs + ctx->ac_offs[BINKGPUPLANE_A], ctx->gpu.AcBufSize[3] );
        if ( ctx->has_hdr )
            intex->luma_ac_tex[2] = make_gl_tex_buf_range( ctx->upload_buf, GL_R32I, base + ctx->upload_ac_offs + ctx->ac_offs[BINKGPUPLANE_H], ctx->gpu.AcBufSize[4] );
        intex->chroma_ac_tex = make_gl_tex_buf_range( ctx->upload_buf, GL_R32I, base + ctx->upload_ac_offs + ctx->ac_offs[BINKGPUPLANE_CR], ctx->gpu.AcBufSize[1] + ctx->gpu.AcBufSize[2] );

        intex->motion_tex = make_gl_tex_buf_range( ctx->upload_buf, GL_RG16I, base + ctx->upload_motion_offs, ctx->motion_size );
    }

    // Scratch buffers and views
    ctx->dc_buf_out = make_gl_buf( GL_TEXTURE_BUFFER, ctx->dc_offs[BINKMAXPLANES], NULL, GL_DYNAMIC_COPY );
    ctx->luma_dc_tex_out[0] = make_gl_tex_buf_range( ctx->dc_buf_out, GL_R16I, ctx->dc_offs[BINKGPUPLANE_Y], full_dc_size );
    if ( ctx->has_alpha )
        ctx->luma_dc_tex_out[1] = make_gl_tex_buf_range( ctx->dc_buf_out, GL_R16I, ctx->dc_offs[BINKGPUPLANE_A], full_dc_size );
    if ( ctx->has_hdr )
        ctx->luma_dc_tex_out[2] = make_gl_tex_buf_range( ctx->dc_buf_out, GL_R16I, ctx->dc_offs[BINKGPUPLANE_H], full_dc_size );
    ctx->chroma_dc_tex_out = make_gl_tex_buf_range( ctx->dc_buf_out, GL_R16I, ctx->dc_offs[BINKGPUPLANE_CR], 2 * sub_dc_size );
    ctx->luma_dc_tex_pred = make_gl_tex_buf( ctx->dc_buf_out, GL_RGBA32I );
    ctx->chroma_dc_tex_pred = make_gl_tex_buf( ctx->dc_buf_out, GL_RG32I );

    for ( i = 0 ; i < 64 ; i++ )
        scan_order[i] = (GLshort)( ctx->gpu.ScanOrderTable[i] * 8 );
    ctx->scan_order_buf = make_gl_buf( GL_UNIFORM_BUFFER, sizeof( scan_order ), scan_order, GL_STATIC_DRAW );
    ctx->scan_order_tex = make_gl_tex_buf( ctx->scan_order_buf, GL_R16I );

    deblockc.dir_mask[0]  = ~0u; // step in x
    deblockc.dir_mask[1]  = 0;
    deblockc.row_inc_full = 0;
    deblockc.row_inc_sub  = 0;
    ctx->deblock_ud_buf = make_gl_buf( GL_UNIFORM_BUFFER, sizeof( DEBLOCK_CONSTS_BUF ), &deblockc, GL_STATIC_DRAW );

    deblockc.dir_mask[0]  = 0;
    deblockc.dir_mask[1]  = ~0u; // step in y
    deblockc.row_inc_full = ( ctx->w_blocks - 1 ) * 16;
    deblockc.row_inc_sub  = ( ctx->w_blocks - 1 ) * 4;
    ctx->deblock_lr_buf = make_gl_buf( GL_UNIFORM_BUFFER, sizeof( DEBLOCK_CONSTS_BUF ), &deblockc, GL_STATIC_DRAW );

    glGenVertexArrays( 1, &ctx->draw_vao );
    ctx->draw_consts_buf = make_gl_buf( GL_UNIFORM_BUFFER, sizeof( DRAW_CONSTS_BUF ), NULL, GL_STREAM_DRAW );

    // unbind handles
    glBindTexture( GL_TEXTURE_2D, 0 );
    glBindTexture( GL_TEXTURE_BUFFER, 0 );
    glBindBuffer( GL_TEXTURE_BUFFER, 0 );
    glBindBuffer( GL_UNIFORM_BUFFER, 0 );

    // if there was any error, bail!
    if ( glGetError() != GL_NO_ERROR )
    {
        Free_textures( &ctx->pub );
        dprintf("GL Error when creating textures\n");
        return 0;
    }

    // prepare some constants
    ctx->full_max_x_swiz = ( ctx->w_blocks - 1 ) * 16 + 0x5;
    ctx->sub_max_x_swiz  = ( ctx->w_blocks - 1 ) * 4 + 1;
    ctx->full_max_y_swiz = ( ctx->h_blocks - 1 ) * ctx->w_blocks * 16;
    ctx->sub_max_y_swiz  = ( ctx->h_blocks - 1 ) * ctx->w_blocks * 4;
    if ( ( ( ctx->gpu.BufferHeight - 1 ) & 31 ) < 16 )
        ctx->full_max_y_swiz |= 0x2;
    else
    {
        ctx->full_max_y_swiz |= 0xa;
        ctx->sub_max_y_swiz  |= 0x2;
    }

    ctx->draw_tex_size[0] = (F32)bink->Width / (F32)ctx->gpu.BufferWidth;
    ctx->draw_tex_size[1] = (F32)bink->Height / (F32)ctx->gpu.BufferHeight;
    ctx->draw_tex_size[2] = (F32)( bink->Width / 2 ) / (F32)( ctx->gpu.BufferWidth / 2 );
    ctx->draw_tex_size[3] = (F32)( bink->Height / 2 ) / (F32)( ctx->gpu.BufferHeight / 2 );

    ctx->draw_tex_clamp[0] = 0.5f / (F32)ctx->gpu.BufferWidth;
    ctx->draw_tex_clamp[1] = 0.5f / (F32)ctx->gpu.BufferHeight;
    ctx->draw_tex_clamp[2] = 0.5f / (F32)( ctx->gpu.BufferWidth / 2 );
    ctx->draw_tex_clamp[3] = 0.5f / (F32)( ctx->gpu.BufferHeight / 2 );

    for ( i = 0 ; i < BINKMAXPLANES ; i++ )
        ctx->gpu.AcRanges[ i ] = ctx->ranges[ i ];

    ctx->gpu.TopFlag = ctx->top_flag_temp;
    ctx->gpu.DeblockUdFlag = ctx->ud_flag_temp;
    ctx->gpu.DeblockLrFlag = ctx->lr_flag_temp;

    BinkRegisterGPUDataBuffers( bink, &ctx->gpu );

    Set_draw_corners( &ctx->pub, 0,0, 1,0, 0,1 );
    Set_source_rect( &ctx->pub, 0,0,1,1 );
    Set_alpha_settings( &ctx->pub, 1,0 );
    Set_hdr_settings( &ctx->pub, 0, 1.0f, 80 );

    ctx->pub.Free_textures = Free_textures;
    ctx->pub.Start_texture_update = Start_texture_update;
    ctx->pub.Finish_texture_update = Finish_texture_update;
    ctx->pub.Draw_textures = Draw_textures;
    ctx->pub.Set_draw_position = Set_draw_position;
    ctx->pub.Set_draw_corners = Set_draw_corners;
    ctx->pub.Set_source_rect = Set_source_rect;
    ctx->pub.Set_alpha_settings = Set_alpha_settings;
    ctx->pub.Set_hdr_settings = Set_hdr_settings;

    return &ctx->pub;
}

static void flush_acs( GLenum target, GLintptr base, BINKGPURANGE const * ranges, U32 num_ranges )
{
    U32 i;
    for ( i = 0 ; i < num_ranges ; i++ )
    {
        if ( ranges[i].Size )
            glFlushMappedBufferRange( target, base + ranges[i].Offset, ranges[i].Size );
    }
}

static void unmap_buffers( BINKTEXTURESGLGPU * ctx )
{
    U32 i;

    glBindBuffer( GL_TEXTURE_BUFFER, ctx->upload_buf );

    glFlushMappedBufferRange( GL_TEXTURE_BUFFER, ctx->upload_frame_const_offs, sizeof( FRAME_CONSTS_BUF ) );
    glFlushMappedBufferRange( GL_TEXTURE_BUFFER, ctx->upload_dc_offs, ctx->dc_offs[ BINKMAXPLANES ] );
    for ( i = 0 ; i < BINKMAXPLANES ; i++ )
    {
        if ( ctx->gpu.Ac[i] )
            flush_acs( GL_TEXTURE_BUFFER, ctx->upload_ac_offs + ctx->ac_offs[i], ctx->gpu.AcRanges[i], ctx->slice_count );
    }
    glFlushMappedBufferRange( GL_TEXTURE_BUFFER, ctx->upload_motion_offs, ctx->motion_size );

    if ( !ctx->upload_persistent_map )
        glUnmapBuffer( GL_TEXTURE_BUFFER );

    glBindBuffer( GL_TEXTURE_BUFFER, 0 );
    
    // NULL the pointers.
    for ( i = 0 ; i < BINKMAXPLANES ; i++ )
    {
        ctx->gpu.Dc[ i ] = NULL;
        ctx->gpu.Ac[ i ] = NULL;
    }
    ctx->gpu.Motion = NULL;
    ctx->frame_consts = NULL;
}

static void Free_textures( BINKTEXTURES * ptextures  )
{
    U32 i;
    BINKTEXTURESGLGPU * ctx = (BINKTEXTURESGLGPU *)ptextures;

    unmap_buffers( ctx );
    BinkRegisterGPUDataBuffers( ctx->bink, NULL );

    for ( i = 0 ; i < 2 ; i++ )
    {
        delete_plane_set( ctx->frames + i );
        delete_plane_set( ctx->output + i );
    }
    glDeleteBuffers( 1, &ctx->upload_buf );
    for ( i = 0 ; i < ctx->num_upload_bufs ; ++i )
    {
        INPUT_DATA_TEX * intex = &ctx->upload_tex[ i ];
        glDeleteTextures( 3, intex->luma_dc_tex );
        glDeleteTextures( 1, &intex->chroma_dc_tex );
        glDeleteTextures( 3, intex->luma_ac_tex );
        glDeleteTextures( 1, &intex->chroma_ac_tex );
        glDeleteTextures( 1, &intex->motion_tex );
        glDeleteSync( intex->fence );
    }
    glDeleteBuffers( 1, &ctx->dc_buf_out );
    glDeleteTextures( 3, ctx->luma_dc_tex_out );
    glDeleteTextures( 1, &ctx->luma_dc_tex_pred );
    glDeleteTextures( 1, &ctx->chroma_dc_tex_out );
    glDeleteTextures( 1, &ctx->chroma_dc_tex_pred );
    glDeleteBuffers( 1, &ctx->scan_order_buf );
    glDeleteTextures( 1, &ctx->scan_order_tex );
    glDeleteBuffers( 1, &ctx->deblock_ud_buf );
    glDeleteBuffers( 1, &ctx->deblock_lr_buf );
    glDeleteVertexArrays( 1, &ctx->draw_vao );
    glDeleteBuffers( 1, &ctx->draw_consts_buf );
    glDeleteTextures( 1, &ctx->DummyAlpha );

    BinkUtilFree( ctx );
}

static void Start_texture_update( BINKTEXTURES * ptextures )
{
    U8 * upload_base;
    U32 i;
    BINKTEXTURESGLGPU * ctx = (BINKTEXTURESGLGPU *)ptextures;

    // if we still have a mapping around, nothing to do.
    if ( ctx->frame_consts )
        return;

    // Advance upload buffer index
    ctx->upload_cur = ( ctx->upload_cur + 1 ) % ctx->num_upload_bufs;

    if ( !ctx->upload_persistent_map )
    {
        glBindBuffer( GL_TEXTURE_BUFFER, ctx->upload_buf );
        upload_base = (U8 *)glMapBufferRange( GL_TEXTURE_BUFFER, 0, ctx->upload_frame_total_size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT );
    }
    else
    {
        // Wait until we're safe to write to the buffer
        if ( ctx->upload_tex[ ctx->upload_cur ].fence )
        {
            glClientWaitSync( ctx->upload_tex[ ctx->upload_cur ].fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED );
            glDeleteSync( ctx->upload_tex[ ctx->upload_cur ].fence );
            ctx->upload_tex[ ctx->upload_cur ].fence = 0;
        }

        upload_base = ctx->upload_persistent_map + ctx->upload_cur * ctx->upload_frame_total_size;
    }

    ctx->frame_consts = (FRAME_CONSTS_BUF *)( upload_base + ctx->upload_frame_const_offs );
    for ( i = 0 ; i < BINKMAXPLANES ; i++ )
    {
        ctx->gpu.Dc[ i ] = upload_base + ctx->upload_dc_offs + ctx->dc_offs[i];
        ctx->gpu.Ac[ i ] = upload_base + ctx->upload_ac_offs + ctx->ac_offs[i];
    }

    if ( !ctx->has_alpha )
    {
        ctx->gpu.Dc[ BINKGPUPLANE_A ] = NULL;
        ctx->gpu.Ac[ BINKGPUPLANE_A ] = NULL;
    }

    if ( !ctx->has_hdr )
    {
        ctx->gpu.Dc[ BINKGPUPLANE_H ] = NULL;
        ctx->gpu.Ac[ BINKGPUPLANE_H ] = NULL;
    }

    ctx->gpu.Motion = (BINKGPUMOVEC *)( upload_base + ctx->upload_motion_offs );
    
    if ( !ctx->upload_persistent_map )
        glBindBuffer( GL_TEXTURE_BUFFER, 0 );

    // reset the cpu flag
    ctx->gpu.HasDataBeenDecoded = 0;
}

static void memory_barrier( GLbitfield barriers )
{
    glMemoryBarrier( barriers );

    // NOTE(fg): This is a workaround for glMemoryBarrier not being reliable
    // on some AMD drivers. Thanks to Graham Sellers for the suggestion!
    glTextureBarrierNV();
}

static void bind_texture( GLuint index, GLenum target, GLuint tex )
{
    glActiveTexture( GL_TEXTURE0 + index );
    glBindTexture( target, tex );
}

static void mdct_pass( GLuint shader, S32 bytesize, BINKTEXTURESGLGPU * ctx, GLuint tex_in, GLuint tex_out, GLuint tex_ac, GLuint tex_dc_in, GLuint tex_dc_out, GLuint motion_tex, GLintptr upload_offs )
{
    glUseProgram( shader );
    bind_texture( 0, GL_TEXTURE_BUFFER, ctx->scan_order_tex );
    bind_texture( 1, GL_TEXTURE_BUFFER, tex_ac );
    bind_texture( 2, GL_TEXTURE_BUFFER, tex_dc_in );
    bind_texture( 3, GL_TEXTURE_2D, tex_in );
    bind_texture( 4, GL_TEXTURE_BUFFER, motion_tex );

    glBindImageTexture( 0, tex_out, 0, GL_FALSE, 0, GL_WRITE_ONLY, (bytesize) ? GL_R8UI : GL_RG8UI );
    glBindImageTexture( 1, tex_dc_out, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16I );
    glBindBufferRange( GL_UNIFORM_BUFFER, 0, ctx->upload_buf, upload_offs + ctx->upload_frame_const_offs, sizeof( FRAME_CONSTS_BUF ) );
    glDispatchCompute( ctx->w_blocks, ctx->h_blocks, 1 );
}

static void dc_predict_pass( GLuint shader, BINKTEXTURESGLGPU * ctx, GLintptr upload_offs )
{
    glUseProgram( shader );
    glBindBufferRange( GL_UNIFORM_BUFFER, 0, ctx->upload_buf, upload_offs + ctx->upload_frame_const_offs, sizeof( FRAME_CONSTS_BUF ) );
    glBindImageTexture( 0, ctx->luma_dc_tex_pred, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32I );
    glBindImageTexture( 1, ctx->chroma_dc_tex_pred, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32I );
    glDispatchCompute( 3 + ctx->has_alpha + ctx->has_hdr, 1, 1 );
}

static void dct_pass( GLuint shader, S32 bytesize, BINKTEXTURESGLGPU * ctx, GLuint tex_out, GLuint tex_ac, GLuint tex_dc, GLintptr upload_offs )
{
    glUseProgram( shader );
    bind_texture( 0, GL_TEXTURE_BUFFER, ctx->scan_order_tex );
    bind_texture( 1, GL_TEXTURE_BUFFER, tex_ac );
    bind_texture( 2, GL_TEXTURE_BUFFER, tex_dc );
    glBindImageTexture( 0, tex_out, 0, GL_FALSE, 0, GL_WRITE_ONLY, (bytesize) ? GL_R8UI : GL_RG8UI );
    glBindBufferRange( GL_UNIFORM_BUFFER, 0, ctx->upload_buf, upload_offs + ctx->upload_frame_const_offs, sizeof( FRAME_CONSTS_BUF ) );
    glDispatchCompute( ctx->w_blocks, ctx->h_blocks, 1 );
}

static void deblock_pass( GLuint shader, S32 bytesize, BINKTEXTURESGLGPU * ctx, GLuint pixel_tex, GLuint dc_tex, GLuint dir_buf, GLuint motion_tex, GLintptr upload_offs )
{
    glUseProgram( shader );
    bind_texture( 0, GL_TEXTURE_BUFFER, motion_tex );
    bind_texture( 1, GL_TEXTURE_BUFFER, dc_tex );
    bind_texture( 2, GL_TEXTURE_2D, pixel_tex );
    glBindImageTexture( 0, pixel_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, (bytesize) ? GL_R8UI : GL_RG8UI );
    glBindBufferRange( GL_UNIFORM_BUFFER, 0, ctx->upload_buf, upload_offs + ctx->upload_frame_const_offs, sizeof( FRAME_CONSTS_BUF ) );
    glBindBufferBase( GL_UNIFORM_BUFFER, 1, dir_buf );
    glDispatchCompute( ( ctx->w_blocks + 1 ) / 2, ( ctx->h_blocks + 1 ) / 2, 1 );
}

static void fill_alpha( GLuint shader, BINKTEXTURESGLGPU * ctx, GLuint pixel_tex, GLintptr upload_offs )
{
    glUseProgram( shader );
    bind_texture( 0, GL_TEXTURE_2D, pixel_tex );
    glBindImageTexture( 0, pixel_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI );
    glBindBufferRange( GL_UNIFORM_BUFFER, 0, ctx->upload_buf, upload_offs + ctx->upload_frame_const_offs, sizeof( FRAME_CONSTS_BUF ) );
    glDispatchCompute( ( ctx->w_blocks * 4 ), ( ctx->h_blocks * 4 ), 1 );
}

static void Finish_texture_update( BINKTEXTURES * ptextures )
{
    FRAME_CONSTS_BUF frame_consts;
    BINKGPU_GL_PLANESET * tex_in, * tex_out;
    U32 i;
    F32 rcp_w, rcp_h;
    S32 do_alpha;
    BINKTEXTURESGLGPU * ctx = (BINKTEXTURESGLGPU *)ptextures;
    INPUT_DATA_TEX * intex;
    BINKSHADERSGLGPU * shaders;
    GLintptr upload_offs;

    shaders = ctx->shaders;

    // if the user didn't have anything mapped (no frame data uploaded), nothing to do!
    if ( !ctx->frame_consts )
        return;


    // have we decoded anything over on the cpu?
    if ( ctx->gpu.HasDataBeenDecoded == 0 )
    {
      unmap_buffers( ctx );
      return;
    }
    
    // reset the cpu flag
    ctx->gpu.HasDataBeenDecoded = 0;

    
    // prepare frame constants buffer and upload it
    frame_consts.frame_flags = ctx->gpu.Flags;
    frame_consts.fill_alpha_value = ctx->gpu.FillAlphaValue;
    frame_consts.size_in_blocks[0] = ctx->w_blocks;
    frame_consts.size_in_blocks[1] = ctx->h_blocks;
    frame_consts.dc_offsets[0] = ctx->dc_offs[0] / ( 16 * sizeof( U16 ) );
    frame_consts.dc_offsets[1] = ctx->dc_offs[1] / (  4 * sizeof( U16 ) );
    frame_consts.dc_offsets[2] = ctx->dc_offs[2] / (  4 * sizeof( U16 ) );
    frame_consts.dc_offsets[3] = ctx->dc_offs[3] / ( 16 * sizeof( U16 ) );
    frame_consts.dc_offsets2[0] = ctx->dc_offs[4] / ( 16 * sizeof( U16 ) );
    frame_consts.dc_bias = 0;
    frame_consts.dc_max = 0;
    if ( ctx->gpu.DcThreshold )
    {
        frame_consts.dc_bias = ctx->gpu.DcThreshold - 1;
        frame_consts.dc_max = 2*ctx->gpu.DcThreshold - 1;
    }
    frame_consts.cb_offs_ac = ctx->cb_offs_ac;
    frame_consts.cb_offs_dc = ctx->cb_offs_dc;
    frame_consts.max_offs_full[0] = ctx->full_max_x_swiz;
    frame_consts.max_offs_full[1] = ctx->full_max_y_swiz;
    frame_consts.max_offs_sub[0] = ctx->sub_max_x_swiz;
    frame_consts.max_offs_sub[1] = ctx->sub_max_y_swiz;

    // +1.0 is the right offset for gather as per GLSL spec (middle of
    // footprint for (0,0)-(1,1) texel rect). +0.5 is the right offset
    // when address calculation is done as for NEAREST (which is what I
    // seem to get for integer textures right now on GCN with Catalyst
    // version 13.15.100.1. we use +0.75 because it's in the middle between
    // both; with either interpretation, we get +-0.25 texels of slack for
    // FP round-off error etc.
    rcp_w = 1.0f / (F32)ctx->gpu.BufferWidth;
    rcp_h = 1.0f / (F32)ctx->gpu.BufferHeight;

    frame_consts.luma_tex_scale_offs[0] = rcp_w;
    frame_consts.luma_tex_scale_offs[1] = rcp_h;
    frame_consts.luma_tex_scale_offs[2] = 0.75f * rcp_w;
    frame_consts.luma_tex_scale_offs[3] = 0.75f * rcp_h;
    for ( i = 0 ; i < 4 ; i++ )
    {
        frame_consts.mv_x_clamp[i] = ctx->gpu.MotionXClamp[i];
        frame_consts.mv_y_clamp[i] = ctx->gpu.MotionYClamp[i];
    }

    #define bink_GET32_LE(ptr)     *((const U32 *)(ptr))

    for ( i = 0 ; i < ( ctx->h_blocks + 31 ) / 32 ; i++ )
        frame_consts.flags[ i*4 + 0 ] = bink_GET32_LE( ctx->top_flag_temp + i*4 );

    for ( i = 0 ; i < ( ctx->w_blocks*4 + 31 ) / 32 ; i++ )
        frame_consts.flags[ i*4 + 1 ] = bink_GET32_LE( ctx->ud_flag_temp + i*4 );

    for ( i = 0 ; i < ( ctx->h_blocks*4 + 31 ) / 32 ; i++ )
        frame_consts.flags[ i*4 + 2 ] = bink_GET32_LE( ctx->lr_flag_temp + i*4 );

    // copy frame constants to mapped buffer and unmap
    memcpy( ctx->frame_consts, &frame_consts, sizeof( frame_consts ) );
    unmap_buffers( ctx );

    // flip active frame and start decoding
    tex_in = &ctx->frames[ ctx->cur_frame ];
    ctx->cur_frame ^= 1;
    tex_out = &ctx->frames[ ctx->cur_frame ];

    // Upload buffer offset to use
    upload_offs = ctx->upload_cur * ctx->upload_frame_total_size;
    intex = &ctx->upload_tex[ ctx->upload_cur ];

    // handle constant alpha flag
    do_alpha = ctx->has_alpha;
    if ( ctx->gpu.Flags & BINKGPUCONSTANTA )
    {
        if ( !do_alpha )
        {
            // if not decoding alpha turn off constanta
            ctx->gpu.Flags &= ~BINKGPUCONSTANTA;
        }
        do_alpha = 0; // turn off normal alpha with constant alpha
    }

    mdct_pass( shaders->progs[PROG_LUMA_MDCT], 1, ctx, tex_in->t.Y, tex_out->t.Y, intex->luma_ac_tex[0], intex->luma_dc_tex[0], ctx->luma_dc_tex_out[0], intex->motion_tex, upload_offs );
    mdct_pass( shaders->progs[PROG_CHROMA_MDCT], 0, ctx, tex_in->t.CrCb, tex_out->t.CrCb, intex->chroma_ac_tex, intex->chroma_dc_tex, ctx->chroma_dc_tex_out, intex->motion_tex, upload_offs );
    if ( do_alpha )
        mdct_pass( shaders->progs[PROG_LUMA_MDCT], 1, ctx, tex_in->t.A, tex_out->t.A, intex->luma_ac_tex[1], intex->luma_dc_tex[1], ctx->luma_dc_tex_out[1], intex->motion_tex, upload_offs );
    if ( ctx->has_hdr )
        mdct_pass( shaders->progs[PROG_LUMA_MDCT], 1, ctx, tex_in->t.H, tex_out->t.H, intex->luma_ac_tex[2], intex->luma_dc_tex[2], ctx->luma_dc_tex_out[2], intex->motion_tex, upload_offs );

    // ---- first pass barrier: MDCT blocks -> dc_predict (need to sync dc_flags access)
    memory_barrier( GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
    dc_predict_pass( shaders->progs[PROG_DC_PREDICT], ctx, upload_offs );

    // ---- second pass barrier: dc_predict -> DCT blocks (again, dc_flags)
    memory_barrier( GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
    dct_pass( shaders->progs[PROG_LUMA_DCT], 1, ctx, tex_out->t.Y, intex->luma_ac_tex[0], ctx->luma_dc_tex_out[0], upload_offs );
    dct_pass( shaders->progs[PROG_CHROMA_DCT], 0, ctx, tex_out->t.CrCb, intex->chroma_ac_tex, ctx->chroma_dc_tex_out, upload_offs );
    if ( do_alpha )
        dct_pass( shaders->progs[PROG_LUMA_DCT], 1, ctx, tex_out->t.A, intex->luma_ac_tex[1], ctx->luma_dc_tex_out[1], upload_offs );
    if ( ctx->has_hdr )
        dct_pass( shaders->progs[PROG_LUMA_DCT], 1, ctx, tex_out->t.H, intex->luma_ac_tex[2], ctx->luma_dc_tex_out[2], upload_offs );

    // handle the constant alpha 
    if ( ctx->gpu.Flags & BINKGPUCONSTANTA )
        fill_alpha( shaders->progs[PROG_FILL_ALPHA], ctx, tex_out->t.A, upload_offs );

    if ( ctx->gpu.Flags & BINKGPUDEBLOCKUD )
    {
        // ---- third pass barrier: DCT blocks -> UandD deblock (image)
        memory_barrier( GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
        deblock_pass( shaders->progs[PROG_LUMA_DEBLOCK], 1, ctx, tex_out->t.Y, ctx->luma_dc_tex_out[0], ctx->deblock_ud_buf, intex->motion_tex, upload_offs );
        deblock_pass( shaders->progs[PROG_CHROMA_DEBLOCK], 0, ctx, tex_out->t.CrCb, ctx->chroma_dc_tex_out, ctx->deblock_ud_buf, intex->motion_tex, upload_offs );
        // alpha doesn't get deblocked
        // hdr doesn't get deblocked
    }

    if ( ctx->gpu.Flags & BINKGPUDEBLOCKLR )
    {
        // ---- fourth pass barrier: UandD deblock -> LtoR deblock (image)
        memory_barrier( GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
        deblock_pass( shaders->progs[PROG_LUMA_DEBLOCK], 1, ctx, tex_out->t.Y, ctx->luma_dc_tex_out[0], ctx->deblock_lr_buf, intex->motion_tex, upload_offs );
        deblock_pass( shaders->progs[PROG_CHROMA_DEBLOCK], 0, ctx, tex_out->t.CrCb, ctx->chroma_dc_tex_out, ctx->deblock_lr_buf, intex->motion_tex, upload_offs );
        // alpha doesn't get deblocked
        // hdr doesn't get deblocked
    }

    // finally, make sure texture changes are visible to app
    memory_barrier( GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

    // upload fence
    if ( ctx->upload_persistent_map )
        ctx->upload_tex[ ctx->upload_cur ].fence = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );

    ctx->TexY = ctx->output[ ctx->cur_frame ].t.Y;
    ctx->TexCrCb = ctx->output[ ctx->cur_frame ].t.CrCb;
    ctx->TexA = ctx->output[ ctx->cur_frame ].t.A;
    ctx->TexH = ctx->output[ ctx->cur_frame ].t.H;

    // public ptrs
    ctx->pub.Ytexture = (void*)(UINTa)ctx->TexY;
    ctx->pub.cRtexture = (void*)(UINTa)ctx->TexCrCb;
    ctx->pub.cBtexture = 0;
    ctx->pub.Atexture = (void*)(UINTa)ctx->TexA;
    ctx->pub.Htexture = (void*)(UINTa)ctx->TexH;

    // unbind everything
    glUseProgram( 0 );
    for ( i = 0 ; i < 2; i++ )
        glBindBufferBase( GL_UNIFORM_BUFFER, i, 0 );
    for ( i = 0 ; i < 5 ; i++ )
        bind_texture( i, GL_TEXTURE_2D, 0 );
    for ( i = 0 ; i < 2 ; i++ )
        glBindImageTexture( i, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8 );
}


// GLSL shader source is in binkgpu_gl_shaders.inl.
static void Draw_textures( BINKTEXTURES * ptextures, BINKSHADERS * pshaders, void * context )
{
    DRAW_CONSTS_BUF dcb;
    int i;
    BINKTEXTURESGLGPU * ctx = (BINKTEXTURESGLGPU *)ptextures;
    BINKSHADERSGLGPU * shaders = (BINKSHADERSGLGPU*) pshaders;

    if ( shaders == 0 )
      shaders = ctx->shaders;

    dcb.xy_transform[0][0] = (ctx->Bx - ctx->Ax) * 2.0f;
    dcb.xy_transform[0][1] = (ctx->Cx - ctx->Ax) * 2.0f;
    dcb.xy_transform[0][2] = (ctx->By - ctx->Ay) * -2.0f;
    dcb.xy_transform[0][3] = (ctx->Cy - ctx->Ay) * -2.0f;
    dcb.xy_transform[1][0] = ctx->Ax * 2.0f - 1.0f;
    dcb.xy_transform[1][1] = 1.0f - ctx->Ay * 2.0f;
    dcb.xy_transform[1][2] = 0.0f;
    dcb.xy_transform[1][3] = 0.0f;

    // UV matrix
    {
        // Set up matrix columns for UV transform (just scale+translate)
        // X column
        dcb.uv_transform[0][0] = ( ctx->u1 - ctx->u0 ) * ctx->draw_tex_size[0];
        dcb.uv_transform[0][1] = 0.0f;
        dcb.uv_transform[0][2] = ( ctx->u1 - ctx->u0 ) * ctx->draw_tex_size[2];
        dcb.uv_transform[0][3] = 0.0f;

        // Y column
        dcb.uv_transform[1][0] = 0.0f;
        dcb.uv_transform[1][1] = ( ctx->v1 - ctx->v0 ) * ctx->draw_tex_size[1];
        dcb.uv_transform[1][2] = 0.0f;
        dcb.uv_transform[1][3] = ( ctx->v1 - ctx->v0 ) * ctx->draw_tex_size[3];

        // W column (translation)
        dcb.uv_transform[2][0] = ctx->u0 * ctx->draw_tex_size[0];
        dcb.uv_transform[2][1] = ctx->v0 * ctx->draw_tex_size[1];
        dcb.uv_transform[2][2] = ctx->u0 * ctx->draw_tex_size[2];
        dcb.uv_transform[2][3] = ctx->v0 * ctx->draw_tex_size[3];
    }

    for ( i = 0 ; i < 4 ; i++ )
    {
        dcb.tex_size[i] = ctx->draw_tex_size[i];
        dcb.tex_clamp[i] = ctx->draw_tex_clamp[i];
    }

    dcb.alpha_mult[0] = ( ctx->draw_type == 1 ) ? ctx->alpha : 1.0f;
    dcb.alpha_mult[1] = dcb.alpha_mult[0];
    dcb.alpha_mult[2] = dcb.alpha_mult[0];
    dcb.alpha_mult[3] = ctx->alpha;

    memcpy( dcb.cmatrix, ctx->bink->ColorSpace, sizeof( dcb.cmatrix ) );

    // HDR consts
    dcb.hdr[0] = ctx->bink->ColorSpace[0];
    dcb.hdr[1] = ctx->exposure;
    dcb.hdr[2] = ctx->out_luma;
    dcb.hdr[3] = 0;
    memcpy(dcb.ctcp, ctx->bink->ColorSpace+1, sizeof(dcb.ctcp));

    update_streaming_ubo( ctx->draw_consts_buf, sizeof( dcb ), &dcb );

    if(ctx->has_hdr) 
    {
        if(ctx->tonemap == 2)
            glUseProgram( shaders->progs[ PROG_DRAW_HDR_PQ ] );
        else if(ctx->tonemap == 1)
            glUseProgram( shaders->progs[ PROG_DRAW_HDR_TONEMAP ] );
        else
            glUseProgram( shaders->progs[ PROG_DRAW_HDR ] );
    }
    else
    {
        glUseProgram( shaders->progs[PROG_DRAW] );
    }
    glBindBufferBase( GL_UNIFORM_BUFFER, 0, ctx->draw_consts_buf );

    // always draw
    glDisable( GL_CULL_FACE );
    glColorMask(1,1,1,1);

    if ( ( ctx->alpha >= 0.999f ) && !ctx->has_alpha )
    {
        glDisable( GL_BLEND );
        bind_texture( 2, GL_TEXTURE_2D, (GLuint)ctx->DummyAlpha );
    }
    else
    {
        if ( ctx->draw_type == 2) 
        {
            glDisable( GL_BLEND );
        }
        else
        {
            glEnable( GL_BLEND );
            glBlendFunc( ctx->draw_type == 1 ? GL_ONE : GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        }
        bind_texture( 2, GL_TEXTURE_2D, (GLuint)ctx->TexA );
    }

    bind_texture( 0, GL_TEXTURE_2D, (GLuint)ctx->TexY );
    bind_texture( 1, GL_TEXTURE_2D, (GLuint)ctx->TexCrCb );
    if(ctx->has_hdr)
        bind_texture( 3, GL_TEXTURE_2D, (GLuint)ctx->TexH );
    glBindVertexArray( ctx->draw_vao );
    glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

    // unbind our objects to make sure nobody else modifies them accidentally
    glUseProgram( 0 );
    glBindVertexArray( 0 );
    glBindBufferBase( GL_UNIFORM_BUFFER, 0, 0 );
    for ( i = 0 ; i < 3 ; i++ )
        bind_texture( i, GL_TEXTURE_2D, 0 );
}


static void Set_draw_position( BINKTEXTURES * ptextures,
                               F32 x0, F32 y0, F32 x1, F32 y1 )
{
  BINKTEXTURESGLGPU * textures = (BINKTEXTURESGLGPU*) ptextures;
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
  BINKTEXTURESGLGPU * textures = (BINKTEXTURESGLGPU*) ptextures;
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
    BINKTEXTURESGLGPU * textures = (BINKTEXTURESGLGPU*) ptextures;
    textures->u0 = u0;
    textures->v0 = v0;
    textures->u1 = u1;
    textures->v1 = v1;
}

static void Set_alpha_settings( BINKTEXTURES * ptextures,
                                F32 alpha_value, S32 draw_type )
{
    BINKTEXTURESGLGPU * textures = (BINKTEXTURESGLGPU*) ptextures;
    textures->alpha = alpha_value;
    textures->draw_type = draw_type;
}


static void Set_hdr_settings( BINKTEXTURES * ptextures, S32 tonemap, F32 exposure, S32 out_nits ) 
{
    // Unused in GL right now
    BINKTEXTURESGLGPU * textures = (BINKTEXTURESGLGPU*) ptextures;
    textures->tonemap = tonemap;
    textures->exposure = exposure;
    textures->out_luma = ((F32)out_nits)/80.f;
}

