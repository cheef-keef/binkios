// Copyright Epic Games, Inc. All Rights Reserved.
/* vim: set softtabstop=2 shiftwidth=2 expandtab : */
#import "binktexturesMetal.h"
#include <stdio.h>
#include "bink.h"

#define BINKMETALFUNCTIONS
#include "binktextures.h"

#if defined(__RADMAC__)
// JonO: On OSX 10.13.5 they introduced a bug to Metal where if you ask for CPU fast memory,
// they give you back write-combined GPU memory. This work-around will allocate via malloc
// instead of using the MTLBuffer as a memory store - which forces us to use the 2 copy 
// texture upload method, but works as intended otherwise. 
#define OSX_10_13_5_WORKAROUND
#endif

#if __has_feature(objc_arc)
#define BRIDGECAST __bridge
#else
#define BRIDGECAST
#endif

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

typedef struct 
{
  float xy_xform[ 2 ][ 4 ]; // 3x2 matrix
  float uv_xform[ 4 ][ 2 ]; // 2x3 matrix (column major, last column=padding)
} BINK_VTX_UNIFORMS;

typedef struct 
{
  float consta[ 4 ];
  float ycbcr_matrix[ 16 ];
  float hdr[ 4 ];
  float ctcp[ 4 ];
} BINK_FRAG_UNIFORMS;

// align must be pow2
static UINTa align_up( UINTa value, UINTa align ) { return ( value + align - 1 ) & ~( align - 1 ); }

// Determines the backing texture size for a given buffer size
static UINTa determine_tex_size( BINKPLANE *plane, U32 bufWidth, U32 bufHeight ) 
{
  if ( !plane->Allocate ) 
  {
    plane->BufferPitch = 0;
    return 0;
  }

  // Pitch needs to be a multiple of 64 bytes and we have one byte per pixel
  UINTa pitch = align_up( bufWidth, 64 );
  return pitch * bufHeight;
}

// Figures out the size for a planeset.
static UINTa determine_planeset_size( BINKFRAMEBUFFERS *fb ) 
{
  UINTa size = 0;
  size += determine_tex_size( &fb->Frames[ 0 ].YPlane, fb->YABufferWidth, fb->YABufferHeight );
  size += determine_tex_size( &fb->Frames[ 0 ].cRPlane, fb->cRcBBufferWidth, fb->cRcBBufferHeight );
  size += determine_tex_size( &fb->Frames[ 0 ].cBPlane, fb->cRcBBufferWidth, fb->cRcBBufferHeight );
  size += determine_tex_size( &fb->Frames[ 0 ].APlane, fb->YABufferWidth, fb->YABufferHeight );
  size += determine_tex_size( &fb->Frames[ 0 ].HPlane, fb->YABufferWidth, fb->YABufferHeight );
  return size;
}

@interface BinkMtlPlaneset : NSObject
  - (id)initFromBuffer:(id <MTLBuffer>)buffer dev:(id <MTLDevice>)dev offset:(NSUInteger)offset framebuf:(BINKFRAMEBUFFERS *)fb width:(int)width height:(int)height;
  - (void)dealloc;
@end

@implementation BinkMtlPlaneset
{
  @public id <MTLTexture> tex[ BINKMAXPLANES ]; // Y, Cr, Cb, Alpha, HDR
  @public U8 *plane_ptr[ BINKMAXPLANES ];
  @public dispatch_group_t inflight; // counts number of inflight draws for this planeset
  @public NSUInteger offset[ BINKMAXPLANES ];
  @public NSRange dataRange;
}

- (void)dealloc 
{
  for(int i = 0; i < BINKMAXPLANES; ++i) 
  {
#if !__has_feature(objc_arc)
    [tex[i] release];
#endif
    tex[i] = nil;
#ifdef OSX_10_13_5_WORKAROUND 
    free(plane_ptr[i]);
#endif
    plane_ptr[i] = 0;
  }
#if !__has_feature(objc_arc)
  [super dealloc];
#endif
}

- (void)_allocTex:(int)index device:(id <MTLDevice>)device fromBuffer:(id <MTLBuffer>)buffer offsPtr:(NSUInteger*)offsPtr width:(int)width height:(int)height plane:(BINKPLANE *)plane bufWidth:(U32)bufWidth bufHeight:(U32)bufHeight
{
  if ( !plane->Allocate ) 
  {
    plane->BufferPitch = 0;
    return;
  }

  MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:width height:height mipmapped:NO];
#if defined(__RADIPHONE__)
  plane->BufferPitch = (U32)align_up( bufWidth, 64 );

  // width/height passed here are the actual (visible) width/height which may be less
  // than the size we allocate storage for (happens when video size doesn't match up
  // with Bink block sizes)
  tex[ index ] = [buffer newTextureWithDescriptor:desc offset:*offsPtr bytesPerRow:plane->BufferPitch];
  plane_ptr[ index ] = (U8 *) [buffer contents] + *offsPtr;
#else
  plane->BufferPitch = (U32)bufWidth;
  tex[ index ] = [device newTextureWithDescriptor:desc];
#ifdef OSX_10_13_5_WORKAROUND
  posix_memalign((void**)&plane_ptr[ index ], 16, plane->BufferPitch*bufHeight);
#else
  plane_ptr[ index ] = (U8 *) [buffer contents] + *offsPtr; // Normally you'd be able to do this -- but gg
#endif
#endif
  offset[ index ] = *offsPtr;

  *offsPtr += plane->BufferPitch * bufHeight;
}

- (id)initFromBuffer:(id <MTLBuffer>)buffer dev:(id <MTLDevice>)dev offset:(NSUInteger)offs framebuf:(BINKFRAMEBUFFERS *)fb width:(int)width height:(int)height
{
  self = [super init];
  if (!self)
    return nil;

  BINKFRAMEPLANESET *ps = &fb->Frames[ 0 ];
  [self _allocTex:0 device:dev fromBuffer:buffer offsPtr:&offs width:width height:height plane:&ps->YPlane bufWidth:fb->YABufferWidth bufHeight:fb->YABufferHeight];
  [self _allocTex:1 device:dev fromBuffer:buffer offsPtr:&offs width:width/2 height:height/2 plane:&ps->cRPlane bufWidth:fb->cRcBBufferWidth bufHeight:fb->cRcBBufferHeight];
  [self _allocTex:2 device:dev fromBuffer:buffer offsPtr:&offs width:width/2 height:height/2 plane:&ps->cBPlane bufWidth:fb->cRcBBufferWidth bufHeight:fb->cRcBBufferHeight];
  [self _allocTex:3 device:dev fromBuffer:buffer offsPtr:&offs width:width height:height plane:&ps->APlane bufWidth:fb->YABufferWidth bufHeight:fb->YABufferHeight];
  [self _allocTex:4 device:dev fromBuffer:buffer offsPtr:&offs width:width height:height plane:&ps->HPlane bufWidth:fb->YABufferWidth bufHeight:fb->YABufferHeight];

  inflight = dispatch_group_create();
  return self;
}

@end

#define DRAW_STATE_LIST \
  /*name,                                progName,                             blend,  srcBlend,                       dstBlend */ \
  T(DRAW_STATE_OPAQUE,                   @"pshader_draw_pixel_metal_0",        NO,     MTLBlendFactorOne,              MTLBlendFactorZero) \
  T(DRAW_STATE_ONEALPHA,                 @"pshader_draw_pixel_metal_0",        YES,    MTLBlendFactorSourceAlpha,      MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_ONEALPHA_PM,              @"pshader_draw_pixel_metal_0",        YES,    MTLBlendFactorOne,              MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_TEXALPHA,                 @"pshader_draw_pixel_metal_1",        YES,    MTLBlendFactorSourceAlpha,      MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_TEXALPHA_PM,              @"pshader_draw_pixel_metal_1",        YES,    MTLBlendFactorOne,              MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_OPAQUE_TEXALPHA,          @"pshader_draw_pixel_metal_1",        NO,     MTLBlendFactorOne,              MTLBlendFactorZero) \
  T(DRAW_STATE_SRGB_OPAQUE,              @"pshader_draw_pixel_metal_2",        NO,     MTLBlendFactorOne,              MTLBlendFactorZero) \
  T(DRAW_STATE_SRGB_ONEALPHA,            @"pshader_draw_pixel_metal_2",        YES,    MTLBlendFactorSourceAlpha,      MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_SRGB_ONEALPHA_PM,         @"pshader_draw_pixel_metal_2",        YES,    MTLBlendFactorOne,              MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_SRGB_TEXALPHA,            @"pshader_draw_pixel_metal_3",        YES,    MTLBlendFactorSourceAlpha,      MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_SRGB_TEXALPHA_PM,         @"pshader_draw_pixel_metal_3",        YES,    MTLBlendFactorOne,              MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_SRGB_OPAQUE_TEXALPHA,     @"pshader_draw_pixel_metal_3",        NO,     MTLBlendFactorOne,              MTLBlendFactorZero) \
  T(DRAW_STATE_HDR_OPAQUE,               @"pshader_draw_pixel_ictcp_metal_0",  NO,     MTLBlendFactorOne,              MTLBlendFactorZero) \
  T(DRAW_STATE_HDR_ONEALPHA,             @"pshader_draw_pixel_ictcp_metal_0",  YES,    MTLBlendFactorSourceAlpha,      MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_ONEALPHA_PM,          @"pshader_draw_pixel_ictcp_metal_0",  YES,    MTLBlendFactorOne,              MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_TEXALPHA,             @"pshader_draw_pixel_ictcp_metal_4",  YES,    MTLBlendFactorSourceAlpha,      MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_TEXALPHA_PM,          @"pshader_draw_pixel_ictcp_metal_4",  YES,    MTLBlendFactorOne,              MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_OPAQUE_TEXALPHA,      @"pshader_draw_pixel_ictcp_metal_4",  NO,     MTLBlendFactorOne,              MTLBlendFactorZero) \
  T(DRAW_STATE_HDR_TM_OPAQUE,            @"pshader_draw_pixel_ictcp_metal_2",  NO,     MTLBlendFactorOne,              MTLBlendFactorZero) \
  T(DRAW_STATE_HDR_TM_ONEALPHA,          @"pshader_draw_pixel_ictcp_metal_2",  YES,    MTLBlendFactorSourceAlpha,      MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_TM_ONEALPHA_PM,       @"pshader_draw_pixel_ictcp_metal_2",  YES,    MTLBlendFactorOne,              MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_TM_TEXALPHA,          @"pshader_draw_pixel_ictcp_metal_6",  YES,    MTLBlendFactorSourceAlpha,      MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_TM_TEXALPHA_PM,       @"pshader_draw_pixel_ictcp_metal_6",  YES,    MTLBlendFactorOne,              MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_TM_OPAQUE_TEXALPHA,   @"pshader_draw_pixel_ictcp_metal_6",  NO,     MTLBlendFactorOne,              MTLBlendFactorZero) \
  T(DRAW_STATE_HDR_PQ_OPAQUE,            @"pshader_draw_pixel_ictcp_metal_1",  NO,     MTLBlendFactorOne,              MTLBlendFactorZero) \
  T(DRAW_STATE_HDR_PQ_ONEALPHA,          @"pshader_draw_pixel_ictcp_metal_1",  YES,    MTLBlendFactorSourceAlpha,      MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_PQ_ONEALPHA_PM,       @"pshader_draw_pixel_ictcp_metal_1",  YES,    MTLBlendFactorOne,              MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_PQ_TEXALPHA,          @"pshader_draw_pixel_ictcp_metal_5",  YES,    MTLBlendFactorSourceAlpha,      MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_PQ_TEXALPHA_PM,       @"pshader_draw_pixel_ictcp_metal_5",  YES,    MTLBlendFactorOne,              MTLBlendFactorOneMinusSourceAlpha) \
  T(DRAW_STATE_HDR_PQ_OPAQUE_TEXALPHA,   @"pshader_draw_pixel_ictcp_metal_5",  NO,     MTLBlendFactorOne,              MTLBlendFactorZero) \
  /* end */

typedef enum
{
#define T(name, progName, blend, srcBlend, dstBlend) name,
  DRAW_STATE_LIST
#undef T

  DRAW_STATE_COUNT,
} DRAW_STATE;

@interface BinkTexMtlShaders : NSObject<BinkShaders>
  @property (readonly) id <MTLDevice> device;
  -(id <BinkTextures>) createTexturesForBink:(HBINK)bink userPtr:(void *)userPtr;
  -(id) initForDevice:(id <MTLDevice>)mtlDevice stateDesc:(MTLRenderPipelineDescriptor **)stateDesc;
  -(void) activateOn:(id <MTLRenderCommandEncoder>)encoder usingState:(DRAW_STATE)state format_idx:(int)format_idx;
@end

@interface BinkTexMtlTextures : NSObject<BinkTextures>
  -(id <BinkTextures>) initFromShaders:(BinkTexMtlShaders *)shaders bink:(HBINK)bink userPtr:(void *)userPtr;
  -(void) startTextureUpdate;
  -(void) finishTextureUpdate;
  -(void) setDrawPositionX0:(float)x0 y0:(float)y0 x1:(float)x1 y1:(float)y1;
  -(void) setDrawCornersAx:(float)Ax Ay:(float)Ay Bx:(float)Bx By:(float)By Cx:(float)Cx Cy:(float)Cy;
  -(void) setSourceRectU0:(float)u0 v0:(float)v0 u1:(float)u1 v1:(float)v1;
  -(void) setAlpha:(float)alpha drawType:(int)type;
  -(void) setHdr:(int)toneMap Exposure:(float)expose outNits:(int)outNits;
  -(void) drawTo:(id <MTLCommandBuffer>)cbuf encoder:(id <MTLRenderCommandEncoder>)encoder shaders:(id <BinkShaders>)shaders format_idx:(int)format_idx;
@end

#ifdef __RADMAC__
#include "binktex_mac_metal_shaders.inl"
#else
#include "binktex_metal_shaders.inl"
#endif

@implementation BinkTexMtlShaders
{
  id <MTLDevice> _device;
  id <MTLRenderPipelineState> _pipelineState[2][DRAW_STATE_COUNT];
  id <MTLDepthStencilState> _depthState;
}

@synthesize device = _device;

-(id <MTLLibrary>) _loadLibrary:(unsigned char const *)block size:(size_t)size
{
  NSError *err = nil;
  dispatch_data_t data = dispatch_data_create(block, size, nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
  id <MTLLibrary> lib = [_device newLibraryWithData:data error:&err];
  if (err)
    NSLog(@"BinkTextures error while loading shader library: %@", err);
  return lib;
}

-(id) initForDevice:(id <MTLDevice>)device stateDesc:(MTLRenderPipelineDescriptor **)stateDesc
{
  static const struct stateDesc_t
  {
    NSString *progName;
    BOOL blend;
    MTLBlendFactor srcBlend;
    MTLBlendFactor dstBlend;
  } states[DRAW_STATE_COUNT] = {
#define T(name, progName, blend, srcBlend, dstBlend) { progName, blend, srcBlend, dstBlend },
    DRAW_STATE_LIST
#undef T
  };

  self = [super init];
  if (!self)
    return nil;

  _device = device;

  // Load the libraries
  id<MTLLibrary> libDrawVertex = [self _loadLibrary:vshader_draw_vertex_metal_lib size:sizeof(vshader_draw_vertex_metal_lib)];
  id<MTLLibrary> libDrawPixel = [self _loadLibrary:pshader_draw_pixel_metal_lib size:sizeof(pshader_draw_pixel_metal_lib)];
  id<MTLLibrary> libDrawPixelHdr = [self _loadLibrary:pshader_draw_pixel_ictcp_metal_lib size:sizeof(pshader_draw_pixel_ictcp_metal_lib)];

  // Load the vertex program
  id <MTLFunction> vertexProgram = [libDrawVertex newFunctionWithName:@"vshader_draw_vertex_metal_0"];

  for(int j = 0; j < 2; ++j) 
  {
    [stateDesc[j] setVertexFunction:vertexProgram];

    for (int i = 0; i < DRAW_STATE_COUNT; ++i)
    {
      const struct stateDesc_t *desc = &states[i];

      // Fragment program
      if(i >= DRAW_STATE_HDR_OPAQUE) 
        [stateDesc[j] setFragmentFunction:[libDrawPixelHdr newFunctionWithName:desc->progName]];
      else 
        [stateDesc[j] setFragmentFunction:[libDrawPixel newFunctionWithName:desc->progName]];

      // Blending
      stateDesc[j].colorAttachments[0].blendingEnabled = desc->blend;
      stateDesc[j].colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
      stateDesc[j].colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
      stateDesc[j].colorAttachments[0].sourceRGBBlendFactor = desc->srcBlend;
      stateDesc[j].colorAttachments[0].sourceAlphaBlendFactor = desc->srcBlend;
      stateDesc[j].colorAttachments[0].destinationRGBBlendFactor = desc->dstBlend;
      stateDesc[j].colorAttachments[0].destinationAlphaBlendFactor = desc->dstBlend;

      NSError* error = NULL;
      _pipelineState[j][i] = [device newRenderPipelineStateWithDescriptor:stateDesc[j] error:&error];
      if (!_pipelineState[j][i]) 
      {
        NSLog(@"Failed to create BinkTex pipeline state, error %@", error);
        return nil;
      }
    }
  }

  MTLDepthStencilDescriptor *depthStateDesc = [MTLDepthStencilDescriptor new];
  depthStateDesc.depthCompareFunction = MTLCompareFunctionAlways;
  depthStateDesc.depthWriteEnabled = NO;
  _depthState = [device newDepthStencilStateWithDescriptor:depthStateDesc];

  return self;
}

-(void) activateOn:(id <MTLRenderCommandEncoder>)encoder usingState:(DRAW_STATE)state format_idx:(int)format_idx
{
  [encoder setDepthStencilState:_depthState];
  [encoder setRenderPipelineState:_pipelineState[format_idx][state]];
}

-(id) createTexturesForBink:(HBINK)bink userPtr:(void *)userPtr 
{
  return [[BinkTexMtlTextures alloc] initFromShaders:self bink:bink userPtr:userPtr];
}

@end

id <BinkShaders> BinkCreateMetalShaders(id <MTLDevice> device, MTLRenderPipelineDescriptor **descs) 
{
  return [[BinkTexMtlShaders alloc] initForDevice:device stateDesc:descs];
}

@implementation BinkTexMtlTextures
{
  BINKFRAMEBUFFERS fbuf;      // Bink framebuffer description
  HBINK bink;
  void *user_ptr;

  BinkMtlPlaneset *planeset[ BINKMAXFRAMEBUFFERS ];

  id <MTLBuffer> dataBuffer;  // contains all our data

  float Ax, Ay, Bx, By, Cx, Cy; // draw position
  float u0, v0, u1, v1; // source rect

  // alpha blending
  int draw_type;
  int draw_flags;
  float alpha;
  BOOL dirty;

  int tonemap;
  float exposure;
  float out_luma;
}

-(id <BinkTextures>) initFromShaders:(BinkTexMtlShaders *)shaders bink:(HBINK)thebink userPtr:(void *)userPtr 
{
  if ( !thebink )
    return nil;

  self = [super init];
  if ( !self )
    return nil;

  id <MTLDevice> device = [shaders device];

  bink = thebink;
  user_ptr = userPtr;
  BinkGetFrameBuffersInfo( bink, &fbuf );

  // Our data buffer has storage for our textures
  UINTa planeset_size = determine_planeset_size( &fbuf );
  UINTa buffer_size = fbuf.TotalFrames * planeset_size;
  dataBuffer = [device newBufferWithLength:buffer_size 
#if defined(__RADMAC__)
    options:MTLResourceCPUCacheModeDefaultCache|MTLResourceStorageModeManaged
#else
    options:MTLResourceCPUCacheModeDefaultCache
#endif
  ];
  if ( !dataBuffer )
    return nil;

  // We have the underlying data buffer, now carve out the texture allocs
  NSUInteger offset = 0;
  for ( int i = 0; i < fbuf.TotalFrames; ++i ) 
  {
    planeset[ i ] = [[BinkMtlPlaneset alloc] initFromBuffer:dataBuffer dev:device offset:offset framebuf:&fbuf width:bink->Width height:bink->Height];
    planeset[ i ]->dataRange = NSMakeRange( offset, offset + planeset_size );
    offset += planeset_size;

    // set pointers in bink copy
    BINKFRAMEPLANESET *ps = &fbuf.Frames[ i ];
    BINKFRAMEPLANESET *proto_ps = &fbuf.Frames[ 0 ];

    ps->YPlane.Buffer = planeset[ i ]->plane_ptr[ 0 ];
    ps->YPlane.BufferPitch  = proto_ps->YPlane.BufferPitch;

    ps->cRPlane.Buffer = planeset[ i ]->plane_ptr[ 1 ];
    ps->cRPlane.BufferPitch = proto_ps->cRPlane.BufferPitch;

    ps->cBPlane.Buffer = planeset[ i ]->plane_ptr[ 2 ];
    ps->cBPlane.BufferPitch = proto_ps->cBPlane.BufferPitch;

    ps->APlane.Buffer = planeset[ i ]->plane_ptr[ 3 ];
    ps->APlane.BufferPitch  = proto_ps->APlane.BufferPitch;

    ps->HPlane.Buffer = planeset[ i ]->plane_ptr[ 4 ];
    ps->HPlane.BufferPitch  = proto_ps->HPlane.BufferPitch;
  }

  // Initial parameters
  [self setDrawCornersAx:0.0f Ay:0.0f Bx:1.0f By:0.0f Cx:0.0f Cy:1.0f];
  [self setSourceRectU0:0.0f v0:0.0f u1:1.0f v1:1.0f];
  [self setAlpha:1.0f drawType:0];
  [self setHdr:0 Exposure:1.0f outNits:80];

  // register our framebuffers
  BinkRegisterFrameBuffers( bink, &fbuf );

  return self;
}

-(void) startTextureUpdate 
{
  // Determine next slot that Bink is going to decode to
  int next_slot = fbuf.FrameNum + 1;
  if ( next_slot >= fbuf.TotalFrames )
    next_slot = 0;

  // Wait until this planeset isn't busy anymore so it's safe to write to.
  dispatch_group_wait( planeset[ next_slot ]->inflight, DISPATCH_TIME_FOREVER );
}

-(void) finishTextureUpdate 
{
  dirty = 1;
#if defined(__RADMAC__)
  //[dataBuffer didModifyRange:planeset[ fbuf.FrameNum ]->dataRange];
#endif
}

-(void) setDrawPositionX0:(float)pos_x0 y0:(float)pos_y0 x1:(float)pos_x1 y1:(float)pos_y1 
{
  Ax = pos_x0;
  Ay = pos_y0;
  Bx = pos_x1;
  By = pos_y0;
  Cx = pos_x0;
  Cy = pos_y1;
}

-(void) setDrawCornersAx:(float)pos_Ax Ay:(float)pos_Ay Bx:(float)pos_Bx By:(float)pos_By Cx:(float)pos_Cx Cy:(float)pos_Cy 
{
  Ax = pos_Ax;
  Ay = pos_Ay;
  Bx = pos_Bx;
  By = pos_By;
  Cx = pos_Cx;
  Cy = pos_Cy;
}

-(void) setSourceRectU0:(float)new_u0 v0:(float)new_v0 u1:(float)new_u1 v1:(float)new_v1 
{
  u0 = new_u0;
  v0 = new_v0;
  u1 = new_u1;
  v1 = new_v1;
}

-(void) setAlpha:(float)alphaLevel drawType:(int)type 
{
  alpha = alphaLevel;
  draw_type = type & 0x0FFFFFFF;
  draw_flags = type & 0xF0000000;
}

-(void) setHdr:(int)toneMap Exposure:(float)expose outNits:(int)new_outNits 
{
  tonemap = toneMap;
  exposure = expose;
  out_luma = new_outNits/80.f;
}

-(void) drawTo:(id <MTLCommandBuffer>)cbuf encoder:(id <MTLRenderCommandEncoder>)encoder shaders:(id <BinkShaders>)inShaders format_idx:(int)format_idx
{
  // If the passed-in shaders instance is not the right type, bail.
  if (![inShaders isKindOfClass:[BinkTexMtlShaders class]])
    return;

  BinkTexMtlShaders *shaders = (BinkTexMtlShaders *)inShaders;

  bool has_alpha = fbuf.Frames[0].APlane.Allocate;
  bool has_hdr = fbuf.Frames[0].HPlane.Allocate;

  // Vertex uniforms
  BINK_VTX_UNIFORMS vuni;
  vuni.xy_xform[ 0 ][ 0 ] = ( Bx - Ax ) *  2.0f;
  vuni.xy_xform[ 0 ][ 1 ] = ( Cx - Ax ) *  2.0f;
  vuni.xy_xform[ 0 ][ 2 ] = ( By - Ay ) * -2.0f; // view space has +y = up, our coords have +y = down
  vuni.xy_xform[ 0 ][ 3 ] = ( Cy - Ay ) * -2.0f; // view space has +y = up, our coords have +y = down
  vuni.xy_xform[ 1 ][ 0 ] = Ax * 2.0f - 1.0f;
  vuni.xy_xform[ 1 ][ 1 ] = 1.0f - Ay * 2.0f;

  // UV matrix from UV rect
  vuni.uv_xform[ 0 ][ 0 ] = u1 - u0;
  vuni.uv_xform[ 0 ][ 1 ] = 0.0f;
  vuni.uv_xform[ 1 ][ 0 ] = 0.0f;
  vuni.uv_xform[ 1 ][ 1 ] = v1 - v0;
  vuni.uv_xform[ 2 ][ 0 ] = u0;
  vuni.uv_xform[ 2 ][ 1 ] = v0;

  // Fragment uniforms
  BINK_FRAG_UNIFORMS funi;

  float rgb_alpha = draw_type == 1 ? alpha : 1.0f;
  funi.consta[ 0 ] = rgb_alpha;
  funi.consta[ 1 ] = rgb_alpha;
  funi.consta[ 2 ] = rgb_alpha;
  funi.consta[ 3 ] = alpha;

  // colorspace stuff
  if ( has_hdr ) 
  {
    // HDR
    funi.hdr[ 0 ] = bink->ColorSpace[0];
    funi.hdr[ 1 ] = exposure;
    funi.hdr[ 2 ] = out_luma;
    funi.hdr[ 3 ] = 0;
    memcpy(funi.ctcp, bink->ColorSpace + 1, 4 * sizeof(F32));
  } 
  else 
  {
    // normal
    memcpy( funi.ycbcr_matrix, bink->ColorSpace, sizeof( bink->ColorSpace ) );
  }

  // Figure out which state to use
  DRAW_STATE state = DRAW_STATE_OPAQUE; // default to opaque
  if(draw_type != 2) 
  {
    if ( has_alpha )
      state = draw_type == 1 ? DRAW_STATE_TEXALPHA_PM : DRAW_STATE_TEXALPHA;
    else if ( alpha < 0.999f )
      state = draw_type == 1 ? DRAW_STATE_ONEALPHA_PM : DRAW_STATE_ONEALPHA;
  }
  else 
  {
    if ( has_alpha )
      state = DRAW_STATE_OPAQUE_TEXALPHA;
  }

  if ( has_hdr ) 
  {
    if(tonemap == 2) 
      state = (DRAW_STATE)((int)state + (int)(DRAW_STATE_HDR_PQ_OPAQUE));
    else if(tonemap == 1)
      state = (DRAW_STATE)((int)state + (int)(DRAW_STATE_HDR_TM_OPAQUE));
    else
      state = (DRAW_STATE)((int)state + (int)(DRAW_STATE_HDR_OPAQUE));
  }
  else if(draw_flags & 0x80000000)
  {
    state = (DRAW_STATE)((int)state + (int)(DRAW_STATE_SRGB_OPAQUE));
  }

#ifdef __RADMAC__
  if(dirty) 
  {
    dirty = 0;
    BINKFRAMEPLANESET *ps = &fbuf.Frames[fbuf.FrameNum];
    U32 pitches[5] = {
      ps->YPlane.BufferPitch,
      ps->cRPlane.BufferPitch,
      ps->cBPlane.BufferPitch,
      ps->APlane.BufferPitch,
      ps->HPlane.BufferPitch
    };
    U32 bytesPerImage[5] = {
      pitches[0]*bink->Height,
      pitches[1]*bink->Height/2,
      pitches[2]*bink->Height/2,
      pitches[3]*bink->Height,
      pitches[4]*bink->Height,
    };

#if defined(BUILDING_WITH_UNITY) || defined(OSX_10_13_5_WORKAROUND) // 2 copies, synchronous method on mac unity
    U8 *buffers[5] = {
      ps->YPlane.Buffer,
      ps->cRPlane.Buffer,
      ps->cBPlane.Buffer,
      ps->APlane.Buffer,
      ps->HPlane.Buffer
    };

    MTLRegion regions[5] = {
      { {0,0,0}, {bink->Width, bink->Height, 1} },
      { {0,0,0}, {bink->Width/2, bink->Height/2, 1} },
      { {0,0,0}, {bink->Width/2, bink->Height/2, 1} },
      { {0,0,0}, {bink->Width, bink->Height, 1} },
      { {0,0,0}, {bink->Width, bink->Height, 1} },
    };
    [planeset[fbuf.FrameNum]->tex[0] replaceRegion:regions[0] mipmapLevel:0 slice:0 withBytes:buffers[0] bytesPerRow:pitches[0] bytesPerImage:bytesPerImage[0]];
    [planeset[fbuf.FrameNum]->tex[1] replaceRegion:regions[1] mipmapLevel:0 slice:0 withBytes:buffers[1] bytesPerRow:pitches[1] bytesPerImage:bytesPerImage[1]];
    [planeset[fbuf.FrameNum]->tex[2] replaceRegion:regions[2] mipmapLevel:0 slice:0 withBytes:buffers[2] bytesPerRow:pitches[2] bytesPerImage:bytesPerImage[2]];
    if(has_alpha) [planeset[fbuf.FrameNum]->tex[3] replaceRegion:regions[3] mipmapLevel:0 slice:0 withBytes:buffers[3] bytesPerRow:pitches[3] bytesPerImage:bytesPerImage[3]];
    if(has_hdr)   [planeset[fbuf.FrameNum]->tex[4] replaceRegion:regions[4] mipmapLevel:0 slice:0 withBytes:buffers[4] bytesPerRow:pitches[4] bytesPerImage:bytesPerImage[4]];
#else // 1 copy, async method
    MTLSize sizes[5] = {
      {bink->Width, bink->Height, 1},
      {bink->Width/2, bink->Height/2, 1},
      {bink->Width/2, bink->Height/2, 1},
      {bink->Width, bink->Height, 1},
      {bink->Width, bink->Height, 1},
    };
    MTLOrigin origin = {0,0,0};
    id<MTLCommandQueue> q = [cbuf commandQueue];
    id<MTLCommandBuffer> cmdbuf = [q commandBuffer];
    [cmdbuf setLabel:@"BinkBlitCommandBuf"];
    id <MTLBlitCommandEncoder> enc = [cmdbuf blitCommandEncoder];
    [enc copyFromBuffer:dataBuffer sourceOffset:planeset[fbuf.FrameNum]->offset[0] sourceBytesPerRow:pitches[0] sourceBytesPerImage:bytesPerImage[0] sourceSize:sizes[0] toTexture:planeset[fbuf.FrameNum]->tex[0] destinationSlice:0 destinationLevel:0 destinationOrigin:origin];
    [enc copyFromBuffer:dataBuffer sourceOffset:planeset[fbuf.FrameNum]->offset[1] sourceBytesPerRow:pitches[1] sourceBytesPerImage:bytesPerImage[1] sourceSize:sizes[1] toTexture:planeset[fbuf.FrameNum]->tex[1] destinationSlice:0 destinationLevel:0 destinationOrigin:origin];
    [enc copyFromBuffer:dataBuffer sourceOffset:planeset[fbuf.FrameNum]->offset[2] sourceBytesPerRow:pitches[2] sourceBytesPerImage:bytesPerImage[2] sourceSize:sizes[2] toTexture:planeset[fbuf.FrameNum]->tex[2] destinationSlice:0 destinationLevel:0 destinationOrigin:origin];
    if(has_alpha) [enc copyFromBuffer:dataBuffer sourceOffset:planeset[fbuf.FrameNum]->offset[3] sourceBytesPerRow:pitches[3] sourceBytesPerImage:bytesPerImage[3] sourceSize:sizes[3] toTexture:planeset[fbuf.FrameNum]->tex[3] destinationSlice:0 destinationLevel:0 destinationOrigin:origin];
    if(has_hdr)   [enc copyFromBuffer:dataBuffer sourceOffset:planeset[fbuf.FrameNum]->offset[4] sourceBytesPerRow:pitches[4] sourceBytesPerImage:bytesPerImage[4] sourceSize:sizes[4] toTexture:planeset[fbuf.FrameNum]->tex[4] destinationSlice:0 destinationLevel:0 destinationOrigin:origin];
    [enc endEncoding];

    // We're about to enqueue, which counts as "blit in-flight".
    __block dispatch_group_t block_inflight_group = planeset[ fbuf.FrameNum ]->inflight;
    dispatch_group_enter(block_inflight_group);

    // Set up completion handler: when this command buffer completes,
    // flag the blit as no longer in-flight.
    [cmdbuf addCompletedHandler:^(id <MTLCommandBuffer> buffer) {
      dispatch_group_leave(block_inflight_group);
    }];

    // Commit this command buffer (i.e. kick it off to the GPU)
    [cmdbuf commit];	
#endif
  }
#endif

  // Draw our quad
  [shaders activateOn:encoder usingState:state format_idx:format_idx];

  [encoder setVertexBytes:&vuni length:sizeof(vuni) atIndex:0];
  [encoder setFragmentBytes:&funi length:sizeof(funi) atIndex:0];
  [encoder setFragmentTextures:planeset[ fbuf.FrameNum ]->tex withRange: NSMakeRange(0, 3)];
  if(has_alpha) [encoder setFragmentTextures:planeset[ fbuf.FrameNum ]->tex+3 withRange: NSMakeRange(3, 1)];
  if(has_hdr)   [encoder setFragmentTextures:planeset[ fbuf.FrameNum ]->tex+4 withRange: NSMakeRange(4, 1)];

  [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:1];

  // Queue our completion handlers
  __block dispatch_group_t block_inflight_group = planeset[ fbuf.FrameNum ]->inflight;
  dispatch_group_enter( block_inflight_group ); // Enter inflight now that we scheduled a draw
  [cbuf addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
    dispatch_group_leave( block_inflight_group ); // Leave inflight once draw is complete
  }];
}

@end

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

typedef struct BINKSHADERSMETAL
{
  BINKSHADERS pub;
  BinkTexMtlShaders * binkShaders;
} BINKSHADERSMETAL; 


typedef struct BINKTEXTURESMETAL
{
  BINKTEXTURES pub;

  BinkTexMtlTextures * binkTex;
  BINKSHADERSMETAL * shaders;
} BINKTEXTURESMETAL;

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

//-----------------------------------------------------------------------------
RADDEFFUNC BINKSHADERS * Create_Bink_shaders( void * _device )
{
  BINKSHADERSMETAL * shaders;
  BINKCREATESHADERSMETAL * i = (BINKCREATESHADERSMETAL*) _device;

  shaders = (BINKSHADERSMETAL*) BinkUtilMalloc( sizeof( *shaders ) );
  if ( shaders == 0 )
    return 0;
  ourmemset( shaders, 0, sizeof( *shaders ) );

  shaders->binkShaders = (BinkTexMtlShaders*) BinkCreateMetalShaders( i->mtlDevice, (BRIDGECAST MTLRenderPipelineDescriptor**) i->stateDesc );
  if ( !shaders->binkShaders )
  {
    BinkUtilFree( shaders );
    return 0;
  }
  
  shaders->pub.Create_textures = Create_textures;
  shaders->pub.Free_shaders = Free_shaders;

  return &shaders->pub;
}

static void Free_shaders( BINKSHADERS * pshaders )
{
  BINKSHADERSMETAL * p = (BINKSHADERSMETAL*)pshaders;
#if !__has_feature(objc_arc)
  [p->binkShaders release];
#endif
  p->binkShaders = nil;
  BinkUtilFree( p );
}


static BINKTEXTURES * Create_textures( BINKSHADERS * pshaders, HBINK bink, void * user_ptr )
{
  BINKTEXTURESMETAL * textures;

  textures = (BINKTEXTURESMETAL *)BinkUtilMalloc( sizeof(*textures) );
  if ( textures == 0 )
    return 0;
  ourmemset( textures, 0, sizeof( *textures ) );

  textures->pub.user_ptr = user_ptr;

  textures->pub.Free_textures = Free_textures;
  textures->pub.Start_texture_update = Start_texture_update;
  textures->pub.Finish_texture_update = Finish_texture_update;
  textures->pub.Draw_textures = Draw_textures;
  textures->pub.Set_draw_position = Set_draw_position;
  textures->pub.Set_draw_corners = Set_draw_corners;
  textures->pub.Set_source_rect = Set_source_rect;
  textures->pub.Set_alpha_settings = Set_alpha_settings;
  textures->pub.Set_hdr_settings = Set_hdr_settings;

  textures->shaders = (BINKSHADERSMETAL *)pshaders;
  
  textures->binkTex = [[BinkTexMtlTextures alloc] initFromShaders:textures->shaders->binkShaders bink:bink userPtr:user_ptr];
  if ( textures->binkTex == 0 )
  {
    BinkUtilFree( textures );
    return 0;
  }
  
  return &textures->pub;
}  

static void Free_textures( BINKTEXTURES * ptextures )
{
  BINKTEXTURESMETAL * p = (BINKTEXTURESMETAL*) ptextures;
#if !__has_feature(objc_arc)
  [p->binkTex release];
#endif
  p->binkTex = nil;
  BinkUtilFree( p );
}


static void Start_texture_update( BINKTEXTURES * ptextures )
{
  BINKTEXTURESMETAL * p = (BINKTEXTURESMETAL*) ptextures;
  [p->binkTex startTextureUpdate];
}

static void Finish_texture_update( BINKTEXTURES * ptextures )
{
  BINKTEXTURESMETAL * p = (BINKTEXTURESMETAL*) ptextures;
  [p->binkTex finishTextureUpdate];
}

static void Draw_textures( BINKTEXTURES* ptextures, BINKSHADERS * pshaders, void * graphics_context )
{
  BINKTEXTURESMETAL * p = (BINKTEXTURESMETAL*) ptextures;
  BINKSHADERSMETAL * s = (BINKSHADERSMETAL*) pshaders;
  BINKAPICONTEXTMETAL * context = (BINKAPICONTEXTMETAL*)graphics_context;

  if ( pshaders == 0 ) s = p->shaders; 
  [p->binkTex drawTo:context->command_buffer encoder:context->command_encoder shaders:s->binkShaders format_idx:context->format_idx];

}

static void Set_draw_position( BINKTEXTURES * ptextures,
                               F32 x0, F32 y0, F32 x1, F32 y1 )
{
  BINKTEXTURESMETAL * p = (BINKTEXTURESMETAL*) ptextures;
  [p->binkTex setDrawPositionX0:x0 y0:y0 x1:x1 y1:y1];
}

static void Set_draw_corners( BINKTEXTURES * ptextures,
                              F32 Ax, F32 Ay, F32 Bx, F32 By, F32 Cx, F32 Cy )
{
  BINKTEXTURESMETAL * p = (BINKTEXTURESMETAL*) ptextures;
  [p->binkTex setDrawCornersAx:Ax Ay:Ay Bx:Bx By:By Cx:Cx Cy:Cy];
}

static void Set_source_rect( BINKTEXTURES * ptextures,
                             F32 u0, F32 v0, F32 u1, F32 v1 )
{
  BINKTEXTURESMETAL * p = (BINKTEXTURESMETAL*) ptextures;
  [p->binkTex setSourceRectU0:u0 v0:v0 u1:u1 v1:v1];
}

static void Set_alpha_settings( BINKTEXTURES * ptextures,
                                F32 alpha_value, S32 draw_type )
{
  BINKTEXTURESMETAL * p = (BINKTEXTURESMETAL*) ptextures;
  [p->binkTex setAlpha:alpha_value drawType:draw_type];
}

static void Set_hdr_settings( BINKTEXTURES * ptextures, S32 tonemap, F32 exposure, S32 out_nits ) 
{
  BINKTEXTURESMETAL * p = (BINKTEXTURESMETAL*) ptextures;
  [p->binkTex setHdr:tonemap Exposure:exposure outNits:out_nits];
}
