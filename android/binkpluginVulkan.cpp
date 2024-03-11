// Copyright Epic Games, Inc. All Rights Reserved.
/* vim: set softtabstop=2 shiftwidth=2 expandtab : */
#include <stdio.h>

#ifdef _WIN32
#include <windows.h> // for outputdebugstring
#else
#include <malloc.h>
#endif

#include "binkplugin.h"
#include "binkpluginGPUAPIs.h"

#include "rrvulkan.h"

#define BINKVULKANFUNCTIONS
#define BINKTEXTURESCLEANUP
#include "binktextures.h"

//#define BINKVULKANGPUFUNCTIONS
//#define BINKTEXTURESCLEANUP
//#include "binktextures.h"

#ifdef __RADANDROID__
#include <android/log.h>
#endif

enum {
  kSubmitQueueDepth = 3,
  kNumDefaultRTVs = 16,
};

// figure worst case triple buffering
#define WAIT_AT_LEAST_FRAMES 3

struct QueuedFrame
{
  VkCommandBuffer cmdbuf;
  VkFence fence;
};

static BINKSHADERS *shaders;
static BINKSHADERS *software;
static BINKSHADERS *gpu;
static VkPhysicalDevice vk_physical_device;
static VkDevice vk_device;
static VkQueue gfx_queue;
static VkFormat render_target_format[2]; // [sdr, hdr]
static VkRenderPass render_pass[2]; // [sdr, hdr]
static VkRenderPass render_pass_clear[2]; // [sdr, hdr]
static VkCommandPool cmd_pool;
static unsigned queue_family;

static QueuedFrame submit_queue[ kSubmitQueueDepth ];
static unsigned submit_cur;

static VkDescriptorPool desc_pool;

// These are globals because of the way the Bink sample framework
// is set up. In your app, these would typically just be locals
// at Create_shaders / Draw_textures time.
static BINKCREATESHADERSVULKAN bink_create_shaders[2];
static BINKAPICONTEXTVULKAN bink_api_context;

static void whine_on_err( VkResult hr, char const * msg )
{
  if(hr) 
#ifdef _WIN32
    OutputDebugStringA( msg );
#elif defined __RADANDROID__
	__android_log_print(ANDROID_LOG_INFO, "Bink", "%s", msg);
#else
    fprintf(stderr, "%s", msg);
#endif
}

void shutdown_vulkan();

static int err_while_open()
{
  shutdown_vulkan();
  return 0;
}

int setup_vulkan( void * device, BINKPLUGININITINFO * info, S32 gpu_assisted, void ** context )
{
  if(!vk_device)
  {
    vk_physical_device = (VkPhysicalDevice)info->physical_device;
    vk_device = (VkDevice)device;
    gfx_queue = (VkQueue)info->queue;
    render_target_format[0] = (VkFormat)info->sdr_and_hdr_render_target_formats[0];
    render_target_format[1] = (VkFormat)info->sdr_and_hdr_render_target_formats[1];

    rr_setup_vulkan();

    // Select graphics queue family
    {
      uint32_t count;
      pvkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &count, NULL);
      VkQueueFamilyProperties *queues = (VkQueueFamilyProperties *)malloc(sizeof(VkQueueFamilyProperties) * count);
      pvkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &count, queues);
      for (uint32_t i = 0; i < count; i++) {
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
          queue_family = i;
          break;
        }
      }
      free(queues);
    }
    
    {
      VkCommandPoolCreateInfo info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      info.queueFamilyIndex = queue_family;
      if(pvkCreateCommandPool(vk_device, &info, NULL, &cmd_pool))
        return err_while_open();
    }

    for(int i = 0; i < kSubmitQueueDepth; ++i) 
    {
      {
        VkFenceCreateInfo info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if(pvkCreateFence(vk_device, &info, NULL, &submit_queue[i].fence))
          return err_while_open();
      }
      {
        VkCommandBufferAllocateInfo info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        info.commandPool = cmd_pool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;
        if(pvkAllocateCommandBuffers(vk_device, &info, &submit_queue[i].cmdbuf)) 
          return err_while_open();
      }
    }

    // Create the Render Pass
    for(int i = 0; i < 2; ++i)
    {
      VkAttachmentDescription attachment = {};
      attachment.format = render_target_format[i];
      attachment.samples = VK_SAMPLE_COUNT_1_BIT;
      attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      VkAttachmentReference color_attachment = {};
      color_attachment.attachment = 0;
      color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      VkSubpassDescription subpass = {};
      subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.colorAttachmentCount = 1;
      subpass.pColorAttachments = &color_attachment;
      VkSubpassDependency dependency = {};
      dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
      dependency.dstSubpass = 0;
      dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dependency.srcAccessMask = 0;
      dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      VkRenderPassCreateInfo info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
      info.attachmentCount = 1;
      info.pAttachments = &attachment;
      info.subpassCount = 1;
      info.pSubpasses = &subpass;
      info.dependencyCount = 1;
      info.pDependencies = &dependency;
      if(pvkCreateRenderPass(vk_device, &info, NULL, &render_pass_clear[i])) {
        return err_while_open();
      }
      attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      if(pvkCreateRenderPass(vk_device, &info, NULL, &render_pass[i])) {
        return err_while_open();
      }
    }

    bink_create_shaders[0].physical_device = vk_physical_device;
    bink_create_shaders[0].logical_device = vk_device;
    bink_create_shaders[0].queue = gfx_queue;
    bink_create_shaders[0].render_target_formats[0] = render_target_format[0];
    bink_create_shaders[0].render_target_formats[1] = render_target_format[1];

    bink_create_shaders[1] = bink_create_shaders[0];
  }

  if ( software == 0 )
  {
    software = Create_Bink_shadersVulkan( bink_create_shaders+0 );
    if ( software == 0 )
      return err_while_open();
    shaders = software;
  }

  /*
  if ( gpu_assisted && gpu == 0 )
  {
    gpu = Create_Bink_shadersVulkanGPU( bink_create_shaders+1);
    if ( gpu )
    {
      // switch the gpu shaders to have priority
      shaders = gpu;
    }
  }
  */

  *context = &bink_api_context;

  return 1;
}

static U64 vulkan_frame = 0;

struct RenderTargetView
{
  U64 frame;
  VkImage image;
  VkImageView view;
  VkFramebuffer framebuffer;
  S32 width;
  S32 height;
  U32 format_idx;
};

static RenderTargetView rtv_tbl[kNumDefaultRTVs];

static RenderTargetView * allocaterendertargetview_vulkan( VkImage image, S32 width, S32 height, U32 format_idx )
{
  int i;

  // do we already have this one?
  for(i = 0; i < kNumDefaultRTVs; ++i) 
  {
    if( ( rtv_tbl[i].image == image ) && ( rtv_tbl[i].width == width ) && ( rtv_tbl[i].height == height ) && ( rtv_tbl[i].format_idx == format_idx ) )
    {
      goto found;
    }
  }

  // do we already have an empty?
  for(i = 0; i < kNumDefaultRTVs; ++i) 
  {
    if( rtv_tbl[i].image == 0 )
    {
      rtv_tbl[i].image = image;
      rtv_tbl[i].width = width;
      rtv_tbl[i].height = height;
      rtv_tbl[i].format_idx = format_idx;

      // Create image view
      {
        VkImageViewCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = render_target_format[format_idx];
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        info.subresourceRange = image_range;
        info.image = image;
        whine_on_err(pvkCreateImageView(vk_device, &info, NULL, &rtv_tbl[i].view), "vkCreateImageView error?");
      }

      // Create Framebuffer
      {
        VkFramebufferCreateInfo info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        info.renderPass = render_pass[format_idx];
        info.attachmentCount = 1;
        info.pAttachments = &rtv_tbl[i].view;
        info.width = width;
        info.height = height;
        info.layers = 1;
        whine_on_err(pvkCreateFramebuffer(vk_device, &info, NULL, &rtv_tbl[i].framebuffer), "vkCreateFramebuffer error?");
      }

     found:
      rtv_tbl[i].frame = vulkan_frame;
      return rtv_tbl + i;
    }
  }

  whine_on_err((VkResult)1, "out of render target views!\n" );
  return 0;
}


static void freerendertargetviews_vulkan( )
{
  // free anything not used in 3 frames
  for(int i = 0; i < kNumDefaultRTVs; ++i) 
  {
    if( ( rtv_tbl[i].image ) && ( vulkan_frame - rtv_tbl[i].frame ) >= WAIT_AT_LEAST_FRAMES )
    {
      // not used in 3 frames
      pvkDestroyFramebuffer(vk_device, rtv_tbl[i].framebuffer, NULL);
      pvkDestroyImageView(vk_device, rtv_tbl[i].view, NULL);
      rtv_tbl[i].framebuffer = 0;
      rtv_tbl[i].view = 0;
      rtv_tbl[i].image = 0;
    }
  }
}

void flushrendertargets_vulkan( )
{
  vulkan_frame += WAIT_AT_LEAST_FRAMES;
  freerendertargetviews_vulkan();  
}

void shutdown_vulkan( )
{
  for(int i = 0; i < kSubmitQueueDepth; ++i) {
    pvkWaitForFences(vk_device, 1, &submit_queue[i].fence, VK_TRUE, (U64)-1);
  }

  if ( gpu )
  {
    Free_Bink_shaders( gpu );
    gpu = 0;
  }
  if ( software )
  {
    Free_Bink_shaders( software );
    software = 0;
  }
  shaders = 0;

  flushrendertargets_vulkan();

  for(int i = 0; i < kSubmitQueueDepth; ++i) {
    pvkFreeCommandBuffers(vk_device, cmd_pool, 1, &submit_queue[i].cmdbuf);
    pvkDestroyFence(vk_device, submit_queue[i].fence, NULL);
  }
  pvkDestroyCommandPool(vk_device, cmd_pool, NULL);
  for(int i = 0; i < 2; ++i) {
    pvkDestroyRenderPass(vk_device, render_pass[i], NULL);
    pvkDestroyRenderPass(vk_device, render_pass_clear[i], NULL);
  }
  pvkDestroyDescriptorPool(vk_device, desc_pool, NULL);

  rr_shutdown_vulkan();
}

void * createtextures_vulkan( void * bink )
{
  void * ret = 0;
  if ( shaders )
  {
    ret = Create_Bink_textures( shaders, (HBINK)bink, 0 );
    if ( ret == 0 )
      if ( software )
        ret = Create_Bink_textures( software, (HBINK)bink, 0 );
  }
  return ret;
}

void begindraw_vulkan( void ) 
{
  VkCommandBuffer cmdbuf = submit_queue[submit_cur].cmdbuf;
  VkFence fence = submit_queue[submit_cur].fence;
  
  ++vulkan_frame;
  
  whine_on_err( pvkWaitForFences(vk_device, 1, &fence, VK_TRUE, (U64)-1), "vkWaitForFences failed?\n" );  
  whine_on_err( pvkResetFences(vk_device, 1, &fence), "vkResetFances failed?\n" );

  whine_on_err( pvkResetCommandBuffer(cmdbuf, 0), "command buffer reset failed.\n" );

  VkCommandBufferBeginInfo info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  whine_on_err( pvkBeginCommandBuffer(cmdbuf, &info), "vkBeginCommandBuffer failed.\n" );

  bink_api_context.cmdbuf = cmdbuf;
}


static void transition_resource( VkCommandBuffer cmdbuf, VkImage image, VkImageLayout before, VkImageLayout after, S32 read_after )
{
  if(before == after) 
      return;
  VkImageMemoryBarrier imb = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imb.image = image;
  imb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imb.subresourceRange.baseMipLevel = 0;
  imb.subresourceRange.levelCount = 1;
  imb.subresourceRange.layerCount = 1;
  if ( read_after )
  {
    imb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  }
  else
  {
    imb.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imb.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  }
  imb.oldLayout = before;
  imb.newLayout = after;
  pvkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &imb);
}

static RenderTargetView * selected_rtv = 0;
static S32 old_resource_state = 0;

void clearrendertarget_vulkan( void )
{
  if ( selected_rtv )
  {
    VkCommandBuffer cmdbuf = submit_queue[submit_cur].cmdbuf;
    pvkCmdEndRenderPass(cmdbuf);

    transition_resource( cmdbuf, selected_rtv->image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkImageLayout)old_resource_state, 1 );
    selected_rtv = 0;
  }
}

void selectrendertarget_vulkan( void * texture_target, S32 width, S32 height, S32 do_clear, U32 format_idx, S32 current_resource_state )
{
  clearrendertarget_vulkan();

  VkImage image = (VkImage)texture_target;

  RenderTargetView * rtv = allocaterendertargetview_vulkan( image, width, height, format_idx );

  VkCommandBuffer cmdbuf = submit_queue[submit_cur].cmdbuf;

  current_resource_state = current_resource_state <= 0 ? VK_IMAGE_LAYOUT_GENERAL : current_resource_state;

  transition_resource( cmdbuf, image, (VkImageLayout)current_resource_state, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0 );
  old_resource_state = current_resource_state;

  VkClearValue clear_value = {};
  VkRenderPassBeginInfo rpinfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  rpinfo.renderPass = do_clear ? render_pass_clear[ format_idx ] : render_pass[ format_idx ];
  rpinfo.framebuffer = rtv->framebuffer;
  rpinfo.renderArea.extent.width = width;
  rpinfo.renderArea.extent.height = height;
  rpinfo.clearValueCount = 1;
  rpinfo.pClearValues = &clear_value;
  pvkCmdBeginRenderPass(cmdbuf, &rpinfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport;
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = (float)width;
  viewport.height = (float)height;
  viewport.minDepth = (float)0.0f;
  viewport.maxDepth = (float)1.0f;
  pvkCmdSetViewport(cmdbuf, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.extent.width = width;
  scissor.extent.height = height;
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  pvkCmdSetScissor(cmdbuf, 0, 1, &scissor);

  selected_rtv = rtv;
}

void selectscreenrendertarget_vulkan( void * screen_texture_target, S32 width, S32 height, U32 format_idx, S32 current_resource_state )
{
  selectrendertarget_vulkan( screen_texture_target, width, height, 0, format_idx, current_resource_state );   
}

void enddraw_vulkan( void )
{
  clearrendertarget_vulkan();

  freerendertargetviews_vulkan();

  VkCommandBuffer cmdbuf = submit_queue[submit_cur].cmdbuf;
  VkFence fence = submit_queue[submit_cur].fence;

  // Close command list
  whine_on_err( pvkEndCommandBuffer(cmdbuf),  "Command list close failed.\n" );

  VkSubmitInfo info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  info.pWaitDstStageMask = &wait_stage;
  info.commandBufferCount = 1;
  info.pCommandBuffers = &cmdbuf;
  // Once the GPU is done with it, signal the completion fence
  whine_on_err( pvkQueueSubmit(gfx_queue, 1, &info, fence), "vkQueueSubmit failed?\n" );

  // Advance to the next slot in the submisison queue, and retire
  // the currently in-flight frame in that slot.
  submit_cur = ( submit_cur + 1 ) % kSubmitQueueDepth;
}

// @cdep pre $path($clipfilename($file)/../vulkan, *.c;*.cpp;*.h  )