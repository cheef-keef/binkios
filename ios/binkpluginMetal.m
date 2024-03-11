// Copyright Epic Games, Inc. All Rights Reserved.
#include "binkplugin.h"
#include "binkpluginGPUAPIs.h"
#include "binktexturesMetal.h"
#include "bink.h"
#include "binkplugin.h"

#define BINKMETALFUNCTIONS
//#define BINKTEXTURESCLEANUP
#include "binktextures.h"

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

#include <Metal/Metal.h>

//#define HASGPUACC

// renderer
id <MTLDevice> _device;
id <MTLCommandQueue> _queue;
id <NSObject,BinkShaders> _binkShaders;
id <NSObject,BinkShaders> _binkShadersGpu;
id <MTLCommandBuffer> _commandBuffer;
id <MTLRenderCommandEncoder> _renderEncoder;
MTLPixelFormat _pixelFormats[2];

extern BINKPLUGINFRAMEINFO screen_data; // only used on d3d12,metal right now

@interface BINKSHADERSMETAL : NSObject {
@public
  BINKSHADERS pub;
  id<NSObject,BinkShaders> binkShaders;
}
@end
@implementation BINKSHADERSMETAL
-(void)dealloc  {
  [binkShaders release];
  [super dealloc];
}
@end

@interface BINKTEXTURESMETAL : NSObject {
@public
  BINKTEXTURES pub;

  S32 video_width, video_height;
  S32 dirty;

  BINKSHADERSMETAL * shaders;
  HBINK bink;

  id<NSObject,BinkTextures> binkTex;
}
@end
@implementation BINKTEXTURESMETAL
-(void)dealloc  {
  [binkTex release];
  [super dealloc];
}
@end

static BINKSHADERSMETAL *_shaders;
static BINKAPICONTEXTMETAL bink_api_context;

// o_O
// Note: obj-c is supposed to support @defs which would eliminate the need for this, but get a compile error on osx
//  opted for this approach instead.
#define NS_TEX_OFFSET(p) (BINKTEXTURESMETAL*)((char*)(p) - (uintptr_t)(&((BINKTEXTURESMETAL*)0)->pub))
#define NS_SHD_OFFSET(p) (BINKSHADERSMETAL*)((char*)(p) - (uintptr_t)(&((BINKSHADERSMETAL*)0)->pub))

static void Start_texture_update( BINKTEXTURES * ptex ) 
{
  BINKTEXTURESMETAL *p = NS_TEX_OFFSET(ptex);
  [p->binkTex startTextureUpdate];
}

static void Finish_texture_update( BINKTEXTURES * ptex ) 
{
  BINKTEXTURESMETAL *p = NS_TEX_OFFSET(ptex);
  [p->binkTex finishTextureUpdate];
}

static void Draw_textures( BINKTEXTURES * ptex, BINKSHADERS * pshaders, void * graphics_context ) 
{
  BINKAPICONTEXTMETAL *api_context = (BINKAPICONTEXTMETAL*)graphics_context;
  BINKTEXTURESMETAL *p = NS_TEX_OFFSET(ptex);
  BINKSHADERSMETAL *s = NS_SHD_OFFSET(pshaders);
  if ( pshaders == 0 ) s = p->shaders; 
  [p->binkTex drawTo:_commandBuffer encoder:_renderEncoder shaders:s->binkShaders format_idx:api_context->format_idx];
}

static void Set_draw_position( BINKTEXTURES * ptex, F32 x0, F32 y0, F32 x1, F32 y1 ) 
{
  BINKTEXTURESMETAL *p = NS_TEX_OFFSET(ptex);
  [p->binkTex setDrawPositionX0:x0 y0:y0 x1:x1 y1:y1];
}

static void Set_draw_corners( BINKTEXTURES * ptex, F32 Ax, F32 Ay, F32 Bx, F32 By, F32 Cx, F32 Cy ) 
{
  BINKTEXTURESMETAL *p = NS_TEX_OFFSET(ptex);
  [p->binkTex setDrawCornersAx:Ax Ay:Ay Bx:Bx By:By Cx:Cx Cy:Cy];
}

static void Set_source_rect( BINKTEXTURES * ptex, F32 u0, F32 v0, F32 u1, F32 v1 ) 
{
  BINKTEXTURESMETAL *p = NS_TEX_OFFSET(ptex);
  [p->binkTex setSourceRectU0:u0 v0:v0 u1:u1 v1:v1];
}

static void Set_alpha_settings( BINKTEXTURES * ptex, F32 alpha_value, S32 draw_type ) 
{
  BINKTEXTURESMETAL *p = NS_TEX_OFFSET(ptex);
  [p->binkTex setAlpha:alpha_value drawType:draw_type];
}

static void Set_hdr_settings( BINKTEXTURES * ptex, S32 tonemap, F32 exposure, S32 out_nits ) 
{
  BINKTEXTURESMETAL *p = NS_TEX_OFFSET(ptex);
  [p->binkTex setHdr:tonemap Exposure:exposure outNits:out_nits];
}

static void Free_textures( BINKTEXTURES *ptex ) 
{
  BINKTEXTURESMETAL *p = NS_TEX_OFFSET(ptex);
  [p release];
}

static BINKTEXTURES * Create_textures( BINKSHADERS * pshaders, HBINK bink, void * user_ptr ) 
{
  BINKSHADERSMETAL *shaders = NS_SHD_OFFSET(pshaders);
  BINKTEXTURESMETAL *textures = [BINKTEXTURESMETAL new];
  if ( textures == nil ) 
  {
    return 0;
  }

  textures->binkTex = (id<NSObject,BinkTextures>)[_binkShaders createTexturesForBink:bink userPtr:user_ptr];

  textures->pub.user_ptr = user_ptr;
  textures->shaders = shaders;
  textures->bink = bink;
  textures->video_width = bink->Width;
  textures->video_height = bink->Height;

  textures->pub.Free_textures = Free_textures;
  textures->pub.Start_texture_update = Start_texture_update;
  textures->pub.Finish_texture_update = Finish_texture_update;
  textures->pub.Draw_textures = Draw_textures;
  textures->pub.Set_draw_position = Set_draw_position;
  textures->pub.Set_draw_corners = Set_draw_corners;
  textures->pub.Set_source_rect = Set_source_rect;
  textures->pub.Set_alpha_settings = Set_alpha_settings;
  textures->pub.Set_hdr_settings = Set_hdr_settings;

  Set_draw_corners( &textures->pub, 0,0, 1,0, 0,1 );
  Set_source_rect( &textures->pub, 0,0,1,1 );
  Set_alpha_settings( &textures->pub, 1, 0 );
  Set_hdr_settings( &textures->pub, 0, 1.0f, 80 );

  return &textures->pub;
}  

static void Free_shaders( BINKSHADERS * pshaders ) 
{
  BINKSHADERSMETAL *p = NS_SHD_OFFSET(pshaders);
  [p release];
}

int setup_metal( void *device_in, BINKPLUGININITINFO * info, S32 gpu_assisted, void ** context )
{
  _device = (id)device_in;
  if(_device == nil) 
  {
    return 0;
  }
  [_device retain];

  _queue = nil;
  if ( ( info ) && ( info->queue ) )
    _queue = (id)info->queue;

  if(_queue == nil) 
  {
    _queue = [_device newCommandQueue]; // Fallback, but can't properly sync with main command queue.
  } 
  else 
  {
    [_queue retain];
  }

  // Create Bink shaders
  MTLRenderPipelineDescriptor *psd[2];
  for(int i = 0; i < 2; ++i) 
  {
      psd[i] = [[MTLRenderPipelineDescriptor new] autorelease];
      psd[i].label = @"BinkTexPipeline";
      [psd[i] setSampleCount: 1]; // TODO: no multisampling for you!
      _pixelFormats[i] = (MTLPixelFormat)info->sdr_and_hdr_render_target_formats[i];
      psd[i].colorAttachments[0].pixelFormat = _pixelFormats[i];
  }

  if ( _shaders == 0 ) 
  {
    _shaders = [BINKSHADERSMETAL new];
    if(_shaders == nil) 
    {
      return 0;
    }
    _shaders->binkShaders = _binkShaders;
    _shaders->pub.Create_textures = Create_textures;
    _shaders->pub.Free_shaders = Free_shaders;
  } 
  else 
  {
    [_shaders->binkShaders release];
    _shaders->binkShaders = nil;
  }

#ifdef HASGPUACC 
  if(gpu_assisted) 
  {
    if(_binkShadersGpu == nil) 
    {
      // GPU decoding - this has abysmal performance on A7 devices due to various
      // compute limitations. It's much better on A8 but still significantly
      // slower than just decoding on the CPU. For now, you're better off just using
      // CPU decoding. This is likely to change with newer hardware.
      _binkShadersGpu = BinkCreateMetalGPUShaders(_device, _queue, psd);
    }
    _shaders->binkShaders = _binkShadersGpu;
  }
  else
#endif
  {
    if(_binkShaders == nil) 
    {
      // CPU decoding - use this!
      _binkShaders = BinkCreateMetalShaders(_device, psd);
    }
    _shaders->binkShaders = _binkShaders;
  }
  [_shaders->binkShaders retain];

  *context = &bink_api_context;
  return 1;
}

void shutdown_metal( void ) 
{
  [_binkShaders release];
  _binkShaders = nil;
  [_binkShadersGpu release];
  _binkShadersGpu = nil;
  [_shaders release];
  _shaders = nil;
  [_queue release];
  _queue = nil;
  [_device release];
  _device = nil;
}

void * createtextures_metal( void * bink ) 
{
  if ( _shaders ) 
  {
    return Create_Bink_textures( &_shaders->pub, (HBINK)bink, 0 );
  }
  return NULL;
}

void clearrendertarget_metal( void ) 
{
  if ( _renderEncoder != nil ) 
  {
    [_renderEncoder popDebugGroup];
    [_renderEncoder endEncoding];
    _renderEncoder = nil;
  }
}
  
void selectrendertarget_metal( void * texture_target, S32 width, S32 height, S32 do_clear, U32 sdr_or_hdr, S32 current_resource_state ) 
{
  clearrendertarget_metal();

  bink_api_context.format_idx = sdr_or_hdr;

  MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor renderPassDescriptor];
  rpd.colorAttachments[0].texture = [(id<MTLTexture>)texture_target newTextureViewWithPixelFormat:_pixelFormats[sdr_or_hdr]];
  rpd.colorAttachments[0].loadAction = do_clear ? MTLLoadActionClear : MTLLoadActionLoad;
  rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

  _renderEncoder = [_commandBuffer renderCommandEncoderWithDescriptor:rpd];
  _renderEncoder.label = @"BinkPluginRenderEncoder";

  [_renderEncoder pushDebugGroup:@"DrawBink"];
}

void selectscreenrendertarget_metal( void * screen_texture_target, S32 width, S32 height, U32 sdr_or_hdr, S32 current_resource_state )
{
  selectrendertarget_metal( screen_texture_target, width, height, 0, sdr_or_hdr, current_resource_state );
}

void begindraw_metal( void ) 
{
  if(screen_data.cmdBuf) 
  {
    _commandBuffer = (id)screen_data.cmdBuf;
  } 
  else 
  {
    _commandBuffer = [_queue commandBuffer];
    _commandBuffer.label = @"BinkPluginCommand";
  }
}
  
void enddraw_metal( void ) {
  clearrendertarget_metal();
  if(!screen_data.cmdBuf) 
  {
    [_commandBuffer commit];
  }
  _commandBuffer = nil;
  screen_data.cmdBuf = nil; // don't keep this around!
}

