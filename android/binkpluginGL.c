// Copyright Epic Games, Inc. All Rights Reserved.
#include "binkplugin.h"
#include "binkpluginGPUAPIs.h"
#include "binkpluginos.h"

#if defined( __RADMAC__ )
#define FB_WORKAROUND
#endif

#define BINKGLFUNCTIONS
#define BINKTEXTURESCLEANUP
#include "binktextures.h"

#if defined( __RADNT__ ) || defined( __RADLINUX__ )

#define HASGPUACC

#define BINKGLGPUFUNCTIONS
#define BINKTEXTURESCLEANUP
#include "binktextures.h"

#endif

#if defined( __RADIPHONE__ ) 
  // direct linking on iphone
  #include <OpenGLES/ES2/gl.h>
  #include <OpenGLES/ES2/glext.h>
#endif

#if defined( __RADEMSCRIPTEN__ ) 
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#if defined( __RADIPHONE__ ) || defined( __RADANDROID__ ) || defined(__RADEMSCRIPTEN__) || defined(BUILDING_FOR_UNREAL_ONLY)
#define USING_GLES
#endif

static BINKSHADERS * shaders;

#ifdef HASGPUACC
static BINKSHADERS * software; // this is the alternative software to GPU ACC
#endif

#if defined( __RADNT__ ) || defined( __RADLINUX__ ) || defined( __RADMAC__ ) || defined( __RADANDROID__ )
#define LOAD_DYNAMICALLY
#endif

#ifdef LOAD_DYNAMICALLY

typedef void voidfunc(void);

#ifdef __RADNT__

#define TARGET_GL_CORE

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
  {
    xglGetProcAddressptr = (xglGetProcAddressTYPE*)dlsym( handle, "glXGetProcAddress" );
  }

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
#include <memory.h>

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


#endif

typedef unsigned int GLbitfield;
typedef float GLfloat;
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLsizei;
typedef int GLint;

#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_FRAMEBUFFER                    0x8D40
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_VIEWPORT                       0x0BA2
#define GL_FRAMEBUFFER_BINDING            0x8CA6
#define GL_MAX_DRAW_BUFFERS               0x8824
#define GL_DRAW_BUFFER0                   0x8825
#define GL_TEXTURE_2D                     0x0DE1
#define GL_ALL_ATTRIB_BITS                0x000fffff
#define GL_CLIENT_ALL_ATTRIB_BITS         0xFFFFFFFF
#define GL_VERTEX_ARRAY_BINDING           0x85B5
#define GL_BLEND                          0x0BE2
#define GL_BLEND_COLOR                    0x8005
#define GL_BLEND_DST_RGB                  0x80C8
#define GL_BLEND_SRC_RGB                  0x80C9
#define GL_BLEND_DST_ALPHA                0x80CA
#define GL_BLEND_SRC_ALPHA                0x80CB
#define GL_CURRENT_PROGRAM                0x8B8D
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_UNIFORM_BUFFER                 0x8A11
#define GL_ARRAY_BUFFER_BINDING           0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING   0x8895
#define GL_UNIFORM_BUFFER_BINDING         0x8A28
#define GL_TEXTURE_BINDING_2D             0x8069
#define GL_TEXTURE_BINDING_BUFFER         0x8c2c
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE_BUFFER                 0x8C2A
#define GL_SCISSOR_BOX                    0x0C10

#define BINKGL_LIST \
   /*  ret, name, params */ \
    GLE(void,      Clear,                  GLbitfield mask) \
    GLE(void,      ClearColor,             GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) \
    GLE(void,      BindFramebuffer,        GLenum target, GLuint framebuffer) \
    GLE(void,      DeleteFramebuffers,     GLsizei n, const GLuint *framebuffers) \
    GLE(void,      GenFramebuffers,        GLsizei n, GLuint *framebuffers) \
    GLE(void,      Viewport,               GLint x, GLint y, GLsizei width, GLsizei height) \
    GLE(void,      Scissor,                GLint x, GLint y, GLsizei width, GLsizei height) \
    GLE(void,      GetIntegerv,            GLenum pname, GLint * params) \
    GLE(void,      GetIntegeri_v,          GLenum pname, GLuint index, GLint * params) \
    GLE(void,      GetFloati_v,            GLenum pname, GLuint index, GLfloat * params) \
    GLE(void,      FramebufferTexture2D,   GLenum target, GLenum attachment, GLenum texttype, GLuint texture, GLint level) \
    GLE(void,      DrawBuffer,             GLenum bufs) \
    GLE(void,      DrawBuffers,            GLsizei n, const GLenum *bufs) \
    GLE(GLenum,    GetError,               void) \
    GLE(GLenum,    CheckFramebufferStatus, GLenum target) \
    GLE(void,      PushAttrib,             GLbitfield mask) \
    GLE(void,      PopAttrib,              void) \
    GLE(void,      PushClientAttrib,       GLbitfield mask) \
    GLE(void,      PopClientAttrib,        void) \
    GLE(void,      BindVertexArray,        GLint vao) \
    GLE(void,      Enable,                 GLenum cap) \
    GLE(void,      Disable,                GLenum cap) \
    GLE(void,      Enablei,                GLenum cap, GLuint idx) \
    GLE(void,      Disablei,               GLenum cap, GLuint idx) \
    GLE(void,      BlendFuncSeparatei,     GLuint index, GLenum sfactorRgb, GLenum dfactorRgb, GLenum sfactorAlpha, GLenum dfactorAlpha) \
    GLE(void,      UseProgram,             GLuint program) \
    GLE(void,      BindBuffer,             GLenum target, GLuint buffer) \
    GLE(void,      ActiveTexture,          GLenum texture) \
    GLE(void,      BindTexture,            GLenum target, GLuint texture) \

#define GLE(ret, name, ...) typedef ret APIENTRY name##proc(__VA_ARGS__); static name##proc * gl##name;
BINKGL_LIST
#undef GLE

static void *GLhandle;

static void load_extensions( void )
{
  if ( GLhandle == 0 )
  {
    GLhandle= StartGLLoad();
#define GLE(ret, name, ...) gl##name = (name##proc *) GetGLProcAddress(GLhandle, "gl" #name);
    BINKGL_LIST
#undef GLE
  }
}

#endif

static int attatched = 0;
static GLuint rfb;

int setup_gl( void * device, BINKPLUGININITINFO * info, S32 gpu_assisted, void ** context )
{
#ifdef LOAD_DYNAMICALLY
  load_extensions();
#endif

  if ( shaders == 0 )
  {
    shaders = Create_Bink_shadersGL( 0 );
    if ( shaders == 0 )
      return 0;
  }

#ifdef HASGPUACC 
  if ( gpu_assisted )
  {
    // if we have requested gpu, software is not set, then
    //   we haven't started gpu. start it, and then make gpu 
    //   the priority. 
    if ( software == 0 )
    {
      BINKSHADERS * g;
      
      g = Create_Bink_shadersGLGPU( 0 );
      if ( g )
      {
        // switch the gpu shaders to have priority
        software = shaders;
        shaders = g;
      }
    }
  }
#endif

  glGenFramebuffers(1, &rfb );

  return 1;
}

void shutdown_gl( void )
{
  if ( shaders )
  {
    Free_Bink_shaders( shaders );
    shaders = 0;
  }

#ifdef HASGPUACC
  if ( software )
  {
    Free_Bink_shaders( software );
    software = 0;
  }
#endif

  glDeleteFramebuffers( 1, &rfb );

#ifdef LOAD_DYNAMICALLY
  EndGLLoad( GLhandle );
  GLhandle = 0;
#endif 
}


void * createtextures_gl( void * bink )
{
  void * ret = 0;
  if ( shaders )
  {
    ret = Create_Bink_textures( shaders, (HBINK)bink, 0 );
#ifdef HASGPUACC
    if ( ret == 0 )
      if ( software )
        ret = Create_Bink_textures( software, (HBINK)bink, 0 );
#endif
  }
  return ret;
}

#ifndef USING_GLES
static S32 got_views = 0;
static GLint vps[4];  
static GLint scissor_rect[4];  
static GLint rtv;
static GLint maxbufs;
#define RESTORE_MAX 16
static GLenum drawbufs[RESTORE_MAX];
#endif

#define MAX_DRAW_BUFFERS 8 // TODO: GL_MAX_DRAW_BUFFERS

// Save/Restore State
static struct {
	GLint vao; 
	GLint blend[MAX_DRAW_BUFFERS];
	//float blendColor[MAX_DRAW_BUFFERS][4]; 
	GLint blendDstAlpha[MAX_DRAW_BUFFERS];
	GLint blendDstRgb[MAX_DRAW_BUFFERS];
	GLint blendSrcAlpha[MAX_DRAW_BUFFERS];
	GLint blendSrcRgb[MAX_DRAW_BUFFERS];
	GLint program;
	GLint arrayBuffer;
	GLint elementArrayBuffer;
	GLint uniformBuffer;
  GLint texture2D[5];
  GLint textureBuffer[5];
} state;
//

void selectrendertarget_gl( void * texture_target, S32 width, S32 height, S32 do_clear, U32 sdr_or_hdr, S32 current_resource_state ) 
{
  if ( texture_target )
  {
#ifndef USING_GLES
    if ( got_views == 0 )
    {
      S32 i;

      rtv = 0;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &rtv);

      vps[0]=0; vps[1]=0; vps[2]=256; vps[3]=256; // default to restoring to *something*
      scissor_rect[0]=0; scissor_rect[1]=0; scissor_rect[2]=256; scissor_rect[3]=256; // default to restoring to *something*
      glGetIntegerv(GL_VIEWPORT, vps); 
      glGetIntegerv(GL_SCISSOR_BOX, scissor_rect); 
      
      maxbufs = 1;
      glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxbufs);
      if ( maxbufs > RESTORE_MAX )
        maxbufs = RESTORE_MAX;

      for( i = 0 ; i < maxbufs; i++ )
      {
        drawbufs[i] = 0;
        glGetIntegerv(GL_DRAW_BUFFER0+i, (GLint*)&drawbufs[i]);
      }

      got_views = 1;
    }
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, rfb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (GLint)(UINTa)texture_target, 0);
    attatched = 1;

    glViewport(0,0,width,height);
    glScissor(0,0,width,height);
    
#ifndef USING_GLES
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
#endif

    if(do_clear) 
    {
      glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
      glClear( GL_COLOR_BUFFER_BIT );
    }
  }
}


void clearrendertarget_gl( void )
{
  if ( attatched )
  {
    glBindFramebuffer(GL_FRAMEBUFFER, rfb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0); // detatch
    attatched = 0;
  }

#ifndef USING_GLES

  if ( got_views )
  {
    glBindFramebuffer( GL_FRAMEBUFFER, rtv );
    glViewport( vps[0], vps[1], vps[2], vps[3] );
    glScissor( scissor_rect[0], scissor_rect[1], scissor_rect[2], scissor_rect[3] );
    glDrawBuffers( maxbufs, drawbufs );

    got_views = 0;
  }

#else

  // Reset to backbuffer (assume that's what it was)
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

#endif
}
  
void begindraw_gl( void )
{
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state.arrayBuffer);
  glGetIntegerv(GL_CURRENT_PROGRAM, &state.program);

#ifndef USING_GLES
  {
  S32 i;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &state.elementArrayBuffer);

  for(i = 0; i < MAX_DRAW_BUFFERS; ++i) 
  {
    glGetIntegeri_v(GL_BLEND, i, &state.blend[i]);
    //glGetFloati_v(GL_BLEND_COLOR, i, state.blendColor[i]);
    glGetIntegeri_v(GL_BLEND_DST_ALPHA, i, &state.blendDstAlpha[i]);
    glGetIntegeri_v(GL_BLEND_DST_RGB, i, &state.blendDstRgb[i]);
    glGetIntegeri_v(GL_BLEND_SRC_ALPHA, i, &state.blendSrcAlpha[i]);
    glGetIntegeri_v(GL_BLEND_SRC_RGB, i, &state.blendSrcRgb[i]);
  }

  for(i = 0; i < 5; ++i) 
  {
    glActiveTexture(GL_TEXTURE0+i);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, state.texture2D+i);
    glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, state.textureBuffer+i);
  }

  glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &state.uniformBuffer);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state.vao);
  }
#endif
}

void enddraw_gl( void )
{
  clearrendertarget_gl();

#ifndef USING_GLES
  {
    S32 i;
    glBindVertexArray(state.vao);
    glBindBuffer(GL_UNIFORM_BUFFER, state.uniformBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.elementArrayBuffer);

    for(i = 0; i < MAX_DRAW_BUFFERS; ++i) 
    {
      if(state.blend[i]) { glEnablei(GL_BLEND, i); } else { glDisablei(GL_BLEND, i); }
      //glBlendColori(i, state.blendColor[i][0], state.blendColor[i][1], state.blendColor[i][2], state.blendColor[i][3]);
      glBlendFuncSeparatei(i, state.blendSrcRgb[i], state.blendDstRgb[i], state.blendSrcAlpha[i], state.blendDstAlpha[i]);
    }

    for(i = 0; i < 5; ++i) 
    {
      glActiveTexture(GL_TEXTURE0+i);
      glBindTexture(GL_TEXTURE_2D, state.texture2D[i]);
      glBindTexture(GL_TEXTURE_BUFFER, state.textureBuffer[i]);
    }
    glActiveTexture(GL_TEXTURE0);
  }
#endif

  glUseProgram(state.program);
  glBindBuffer(GL_ARRAY_BUFFER, state.arrayBuffer);

  glGetError();
}
