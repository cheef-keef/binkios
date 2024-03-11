// Copyright Epic Games, Inc. All Rights Reserved.
#include <Metal/Metal.h>
#include <Foundation/Foundation.h>

struct BINK;

@protocol BinkTextures;

@protocol BinkShaders <NSObject>
    // Create a BinkTextures instance for a given Bink.
    -(id <BinkTextures> __nullable) createTexturesForBink:(struct BINK * __nonnull)bink userPtr:(void * __nullable)userPtr;
@end

@protocol BinkTextures
    // Begin updating textures for the current frame. Call before BinkDoFrame.
    -(void) startTextureUpdate;

    // Finish updating textures for the current frame. Call after BinkDoFrame is done.
    // (For async variants, you need to wait until the async processing is complete.)
    -(void) finishTextureUpdate;

    // Set the position to draw the video within the viewport. x0=0 y0=0 is the top left
    // corner of the viewport, x1=1 y1=1 is the bottom right. By default, we size the video
    // to cover the entire viewport.
    -(void) setDrawPositionX0:(float)x0 y0:(float)y0 x1:(float)x1 y1:(float)y1;

    // Alternate way to set the poosition to draw the video within the viewport with rotation
    //   support. x0=0 y0=0 is the top left corner of the viewport, x1=1 y1=1 is the bottom 
    //   right. By default, we size the video to cover the entire viewport (non-rotated).
    -(void) setDrawCornersAx:(float)Ax Ay:(float)Ay Bx:(float)Bx By:(float)By Cx:(float)Cx Cy:(float)Cy;

    // Set the draw source rectangle in normalized texture coordinates. (0,0) is the top left
    // corner of the texture, (1,1) is the bottom right. Again, the default is to draw the
    // entire video.
    -(void) setSourceRectU0:(float)u0 v0:(float)v0 u1:(float)u1 v1:(float)v1;

    // Sets the alpha settings. "alpha" is a constant alpha value applied on top of whatever
    // alpha channel the video specifies (if it has an alpha channel); isPremul denotes
    // whether the video content is premultiplied alpha or not.
    -(void) setAlpha:(float)alpha drawType:(int)type;

    // Set the hdr settings for drawing.
    //   tonemap is a boolean that specifies whether or not you want linear output (0), or filmic tonemapped output (1).
    //   exposure is a scaling factor that happens before tonemapping (1.0=normal, <1.0 darken, >1.0 brighten)
    //   out_nits specifies the maximum luminance to output when tonemapping (0 to 10000 nits)
    -(void) setHdr:(int)toneMap Exposure:(float)expose outNits:(int)outNits;

    // Write commands to draw the most recently decoded frame to the specified command
    // buffer, using the given render command encoder and shaders.
    -(void) drawTo:(id <MTLCommandBuffer> __nonnull)cbuf encoder:(id <MTLRenderCommandEncoder> __nonnull)encoder shaders:(id <BinkShaders> __nonnull)shaders format_idx:(int)format_idx;
@end

// Creation functions

// Initializes a CPU decoding BinkShaders instance for a given Metal device.
//   stateDesc is a pre-filled MTLRenderPipelineDescriptor that you set up to
//   describe your color attachments, depth/stencil attachment etc. initForMetalDevice
//   then creates internal pipeline states from that template by overriding the vertex
//   and fragment programs and setting its own blend states.
id<BinkShaders> __nullable BinkCreateMetalShaders(id <MTLDevice> __nonnull mtlDevice, MTLRenderPipelineDescriptor * __nonnull * __nonnull stateDesc);

// Initializes a GPU BinkShaders instance for a given Metal device.
// BinkGPU operation needs a mtlQueue that compute jobs get submitted to.
//   stateDesc is a pre-filled MTLRenderPipelineDescriptor that you set up to
//   describe your color attachments, depth/stencil attachment etc. initForMetalDevice
//   then creates internal pipeline states from that template by overriding the vertex
//   and fragment programs and setting its own blend states.
id<BinkShaders> __nullable BinkCreateMetalGPUShaders(id <MTLDevice> __nonnull mtlDevice, id <MTLCommandQueue> __nonnull mtlQueue, MTLRenderPipelineDescriptor * __nonnull * __nonnull stateDesc);
