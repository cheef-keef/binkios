// Copyright Epic Games, Inc. All Rights Reserved.
/* vim: set softtabstop=2 shiftwidth=2 expandtab : */

// Note: Unity does triple buffering -- use BINKUSETRIPLEBUFFERING in BinkOpen

#include <malloc.h>
#include <memory.h>

#include "rrvulkan.h"

#include "bink.h"

#define BINKVULKANFUNCTIONS
#include "binktextures.h"

// include shader code (shader source included as comments)
struct CompiledShader
{
  unsigned * bytecode;
  unsigned size;
};

#include "binktex_vulkan_shaders.inl"

// Warning: You cannot make BINKCONSTSIZE ANY bigger without possibly breaking some archs. 
// Its implemented below via Push constants which have a requirement of only 128
// bytes max size according to spec. 
#define BINKCONSTSIZE   (8*4*sizeof(F32))

#define STATE_LIST \
  /*name,                      hdr,    shader_idx,  blend,      srcBlend,                   dstBlend */ \
T(STATE_OPAQUE,                false,  0,           VK_FALSE,   VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ZERO) \
T(STATE_OPAQUEALPHA,           false,  1,           VK_FALSE,   VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ZERO) \
T(STATE_ONEALPHA,              false,  0,           VK_TRUE,    VK_BLEND_FACTOR_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_ONEALPHA_PM,           false,  0,           VK_TRUE,    VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_TEXALPHA,              false,  1,           VK_TRUE,    VK_BLEND_FACTOR_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_TEXALPHA_PM,           false,  1,           VK_TRUE,    VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_SRGB_OPAQUE,           false,  2,           VK_FALSE,   VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ZERO) \
T(STATE_SRGB_OPAQUEALPHA,      false,  3,           VK_FALSE,   VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ZERO) \
T(STATE_SRGB_ONEALPHA,         false,  2,           VK_TRUE,    VK_BLEND_FACTOR_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_SRGB_ONEALPHA_PM,      false,  2,           VK_TRUE,    VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_SRGB_TEXALPHA,         false,  3,           VK_TRUE,    VK_BLEND_FACTOR_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_SRGB_TEXALPHA_PM,      false,  3,           VK_TRUE,    VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_OPAQUE,            true,   0,           VK_FALSE,   VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ZERO) \
T(STATE_HDR_OPAQUEALPHA,       true,   4,           VK_FALSE,   VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ZERO) \
T(STATE_HDR_ONEALPHA,          true,   0,           VK_TRUE,    VK_BLEND_FACTOR_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_ONEALPHA_PM,       true,   0,           VK_TRUE,    VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_TEXALPHA,          true,   4,           VK_TRUE,    VK_BLEND_FACTOR_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_TEXALPHA_PM,       true,   4,           VK_TRUE,    VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_TM_OPAQUE,         true,   2,           VK_FALSE,   VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ZERO) \
T(STATE_HDR_TM_OPAQUEALPHA,    true,   6,           VK_FALSE,   VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ZERO) \
T(STATE_HDR_TM_ONEALPHA,       true,   2,           VK_TRUE,    VK_BLEND_FACTOR_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_TM_ONEALPHA_PM,    true,   2,           VK_TRUE,    VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_TM_TEXALPHA,       true,   6,           VK_TRUE,    VK_BLEND_FACTOR_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_TM_TEXALPHA_PM,    true,   6,           VK_TRUE,    VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_PQ_OPAQUE,         true,   1,           VK_FALSE,   VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ZERO) \
T(STATE_HDR_PQ_OPAQUEALPHA,    true,   5,           VK_FALSE,   VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ZERO) \
T(STATE_HDR_PQ_ONEALPHA,       true,   1,           VK_TRUE,    VK_BLEND_FACTOR_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_PQ_ONEALPHA_PM,    true,   1,           VK_TRUE,    VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_PQ_TEXALPHA,       true,   5,           VK_TRUE,    VK_BLEND_FACTOR_SRC_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
T(STATE_HDR_PQ_TEXALPHA_PM,    true,   5,           VK_TRUE,    VK_BLEND_FACTOR_ONE,        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) \
/* end */

enum StateId
{
#define T(name, hdr, shader_idx, blend, srcBlend, dstBlend) name,
  STATE_LIST
#undef T
  STATE_COUNT
};

// Planes
enum
{
  BINKPLANEY = 0,
  BINKPLANECR,
  BINKPLANECB,
  BINKPLANEA,
  BINKPLANEH,
};

struct vulkan_texture_t {
  VkImage image;
  VkImageView view;
  VkDeviceMemory device_mem;
  VkDeviceSize device_size;
  void *ptr;
};

// linked list of semaphores currently in-flight.
typedef struct BINKSHADERSVK 
{
  BINKSHADERS pub;

  // Root of a linked list so that we can iterate through all BINKTEXTURES instances.
  struct BINKTEXTURESVK *root;

  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue queue;
  VkFormat render_target_format[ 2 ]; // [sdr, hdr]
  VkRenderPass render_pass[ 2 ];  // [sdr, hdr]
  VkPhysicalDeviceMemoryProperties device_mem_props;

  unsigned queue_family;

  VkSampler sampler;

  VkDescriptorPool descriptor_pool;
  VkPipelineCache pipeline_cache;
  VkPipelineLayout pipeline_layout;
  VkDescriptorSetLayout descriptor_set_layout;
  VkPipeline pipeline_state[ STATE_COUNT ][ 2 ]; // [state][sdr, hdr]
  VkCommandPool cmdpool;
} BINKSHADERSVK;

typedef struct BINKTEXTURESVK 
{
  BINKTEXTURES pub;

  // A linked list so that BINKSHADERSVK can iterate through all instances.
  // Warning: NOT THREAD SAFE
  BINKTEXTURESVK *prev, *next;

  S32 plane_present[ BINKMAXPLANES ];

  vulkan_texture_t tex[ BINKMAXFRAMEBUFFERS ][ BINKMAXPLANES ];
  VkDescriptorSet descriptor_set[ BINKMAXFRAMEBUFFERS ];

  BINKSHADERSVK * shaders;
  HBINK bink;
  // this is the Bink info on the textures
  BINKFRAMEBUFFERS bink_buffers;

  F32 Ax,Ay, Bx,By, Cx,Cy;
  F32 alpha;
  S32 draw_type;
  S32 draw_flags;
  F32 u0,v0,u1,v1;
  S32 tonemap;
  F32 exposure;
  F32 out_luma;
} BINKTEXTURESVK;

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

//-----------------------------------------------------------------------------

// This function is used to request a device memory type that supports all the property flags we request (e.g. device local, host visibile)
// Upon success it will return the index of the memory type that fits our requestes memory properties
// This is necessary as implementations can offer an arbitrary number of memory types with different
// memory properties.
// You can check http://vulkan.gpuinfo.org/ for details on different memory configurations
static int vulkan_get_memory_index(VkPhysicalDeviceMemoryProperties *device_mem_props, uint32_t typeBits, VkMemoryPropertyFlags properties) {
  // Iterate over all memory types available for the device used in this example
  for (unsigned i = 0; i < device_mem_props->memoryTypeCount; ++i) {
    if (typeBits & 1) {
      if ((device_mem_props->memoryTypes[i].propertyFlags & properties) == properties) {
        return i;
      }
    }
    typeBits >>= 1;
  }
  return -1;
}

RADDEFFUNC BINKSHADERS * Create_Bink_shaders( void *vkdev ) 
{
  VkPipelineShaderStageCreateInfo shaderStages[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};

  BINKSHADERSVK * shaders = (BINKSHADERSVK*) BinkUtilMalloc( sizeof( *shaders ) );
  if ( shaders == 0 )
    return 0;

  memset( shaders, 0, sizeof( *shaders ) );

  BINKCREATESHADERSVULKAN *bink_vkdev = (BINKCREATESHADERSVULKAN *)vkdev;

  shaders->physical_device = (VkPhysicalDevice)bink_vkdev->physical_device;
  shaders->device = (VkDevice)bink_vkdev->logical_device;
  shaders->queue = (VkQueue)bink_vkdev->queue;
  shaders->render_target_format[0] = (VkFormat)bink_vkdev->render_target_formats[0];
  shaders->render_target_format[1] = (VkFormat)bink_vkdev->render_target_formats[1];
  pvkGetPhysicalDeviceMemoryProperties(shaders->physical_device, &shaders->device_mem_props);

  // Find queue family...
  {
    unsigned count;
    pvkGetPhysicalDeviceQueueFamilyProperties(shaders->physical_device, &count, NULL);
    VkQueueFamilyProperties *queues = (VkQueueFamilyProperties *)BinkUtilMalloc(sizeof(VkQueueFamilyProperties) * count);
    pvkGetPhysicalDeviceQueueFamilyProperties(shaders->physical_device, &count, queues);
    for (unsigned i = 0; i < count; ++i) {
      if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        shaders->queue_family = i;
        break;
      }
    }
    BinkUtilFree(queues);
  }

  // Create command pool...
  {
      VkCommandPoolCreateInfo info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      info.queueFamilyIndex = shaders->queue_family;
      if(pvkCreateCommandPool(shaders->device, &info, NULL, &shaders->cmdpool)) {
        goto fail;
      }
  }

  // Create Descriptor Pool
  {
    VkDescriptorPoolSize pool_sizes[] =
    {
      //{ VK_DESCRIPTOR_TYPE_SAMPLER, 1 },
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
      //{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0 },
      //{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0 },
      //{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 0 },
      //{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 0 },
      //{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0 },
      //{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0 },
      //{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0 },
      //{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 0 },
      //{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0 }
    };
    VkDescriptorPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * sizeof(pool_sizes) / sizeof(pool_sizes[0]);
    pool_info.poolSizeCount = (uint32_t)(sizeof(pool_sizes) / sizeof(pool_sizes[0]));
    pool_info.pPoolSizes = pool_sizes;
    if(pvkCreateDescriptorPool(shaders->device, &pool_info, NULL, &shaders->descriptor_pool)) {
      goto fail;
    }
  }

  {
    VkDescriptorSetLayoutBinding bindings[5] = {};
    // Y/I0,Cb,Cr,A,I1
    for(int i = 0; i < 5; ++i) {
      bindings[i].binding = i;
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo descriptorLayout = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descriptorLayout.pBindings = bindings;
    descriptorLayout.bindingCount = sizeof(bindings)/sizeof(bindings[0]);
    if(pvkCreateDescriptorSetLayout(shaders->device, &descriptorLayout, NULL, &shaders->descriptor_set_layout)) {
      goto fail;
    }

    // Create the pipeline layout that is used to generate the rendering pipelines that are based on this descriptor set layout
    // In a more complex scenario you would have different pipeline layouts for different descriptor set layouts that could be reused
    {
      VkPushConstantRange pcs[1] = {};
      pcs[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
      pcs[0].size = BINKCONSTSIZE;

      VkPipelineLayoutCreateInfo info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
      info.setLayoutCount = 1;
      info.pSetLayouts = &shaders->descriptor_set_layout;
      info.pushConstantRangeCount = 1;
      info.pPushConstantRanges = pcs;
      if(pvkCreatePipelineLayout(shaders->device, &info, NULL, &shaders->pipeline_layout)) {
        goto fail;
      }
    }
  }

  {
    VkPipelineCacheCreateInfo info = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    if(pvkCreatePipelineCache(shaders->device, &info, NULL, &shaders->pipeline_cache)) {
      goto fail;
    }
  }

  for(int i = 0; i < 2; ++i) 
  {
    VkAttachmentDescription attachment = {};
    attachment.format = shaders->render_target_format[i];
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
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
    if(pvkCreateRenderPass(shaders->device, &info, NULL, &shaders->render_pass[i])) {
      goto fail;
    }
  }

  // Vertex shader

  VkShaderModule vert_shader_module;
  {
    // Create a new shader module that will be used for pipeline creation
    VkShaderModuleCreateInfo moduleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    moduleCreateInfo.pCode = vshader_vert_vulkan_arr[0].bytecode;
    moduleCreateInfo.codeSize = vshader_vert_vulkan_arr[0].size;
    if (pvkCreateShaderModule(shaders->device, &moduleCreateInfo, NULL, &vert_shader_module)) {
      goto fail;
    }

    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vert_shader_module;
    shaderStages[0].pName = "main";
  }

  // Create the various shader permutations
  for ( unsigned i = 0 ; i < STATE_COUNT; ++i ) 
  {
    static const struct StateDesc
    {
      bool hdr;
      int shader_idx;
      bool blend;
      VkBlendFactor src_blend;
      VkBlendFactor dst_blend;
    } states[STATE_COUNT] = {
#define T(name, hdr, shaderIdx, blend, srcBlend, dstBlend) { hdr, shaderIdx, blend, srcBlend, dstBlend },
      STATE_LIST
#undef T
    };
    StateDesc const * state = &states[i];


    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; // Yes... thats right - the primitive type you are drawing is SPECIFIED HERE ... wtf vulkan

    VkPipelineRasterizationStateCreateInfo rasterizationState = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationState.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState blend_state[1] = {};
    blend_state[0].colorWriteMask = 0xf;
    blend_state[0].blendEnable = state->blend;
    blend_state[0].srcColorBlendFactor = state->src_blend;
    blend_state[0].dstColorBlendFactor = state->dst_blend;
    blend_state[0].colorBlendOp = VK_BLEND_OP_ADD;
    blend_state[0].srcAlphaBlendFactor = state->src_blend;
    blend_state[0].dstAlphaBlendFactor = state->dst_blend;
    blend_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blend_state[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = blend_state;

    VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkDynamicState dynamicStateEnables[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.pDynamicStates = dynamicStateEnables;
    dynamicState.dynamicStateCount = sizeof(dynamicStateEnables) / sizeof(dynamicStateEnables[0]);

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.front.compareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineMultisampleStateCreateInfo multisampleState = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineVertexInputStateCreateInfo vertexInputState = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    // Fragment shader
    VkShaderModule frag_shader_module;
    {
      // Create a new shader module that will be used for pipeline creation
			VkShaderModuleCreateInfo moduleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
      CompiledShader *sh = state->hdr ? &pshader_ictcp_vulkan_arr[ state->shader_idx ] : &pshader_ycbcr_vulkan_arr[ state->shader_idx ];
			moduleCreateInfo.pCode = sh->bytecode;
			moduleCreateInfo.codeSize = sh->size;
			if(pvkCreateShaderModule(shaders->device, &moduleCreateInfo, NULL, &frag_shader_module)) {
        goto fail;
      }

      shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      shaderStages[1].module = frag_shader_module;
      shaderStages[1].pName = "main";
    }

    for(int j = 0; j < 2; ++j) {
      VkGraphicsPipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
      pipelineCreateInfo.layout = shaders->pipeline_layout;
      pipelineCreateInfo.renderPass = shaders->render_pass[j];
      pipelineCreateInfo.stageCount = sizeof(shaderStages)/sizeof(shaderStages[0]);
      pipelineCreateInfo.pStages = shaderStages;
      pipelineCreateInfo.pVertexInputState = &vertexInputState;
      pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
      pipelineCreateInfo.pRasterizationState = &rasterizationState;
      pipelineCreateInfo.pColorBlendState = &colorBlendState;
      pipelineCreateInfo.pMultisampleState = &multisampleState;
      pipelineCreateInfo.pViewportState = &viewportState;
      pipelineCreateInfo.pDepthStencilState = &depthStencilState;
      pipelineCreateInfo.pDynamicState = &dynamicState;

      // Create rendering pipeline using the specified states
      if(pvkCreateGraphicsPipelines(shaders->device, shaders->pipeline_cache, 1, &pipelineCreateInfo, NULL, &shaders->pipeline_state[i][j])) {
        goto fail;
      } 
    }

    // Shader modules are no longer needed once the graphics pipeline has been created
    pvkDestroyShaderModule(shaders->device, shaderStages[1].module, NULL);
  }
  pvkDestroyShaderModule(shaders->device, shaderStages[0].module, NULL);

  // Samplers ...
  {
    VkSamplerCreateInfo info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    info.maxAnisotropy = 1.0f;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if(pvkCreateSampler(shaders->device, &info, NULL, &shaders->sampler)) {
      goto fail;
    }
  }

  shaders->pub.Create_textures = Create_textures;
  shaders->pub.Free_shaders = Free_shaders;

  return &shaders->pub;
fail:
  Free_shaders(&shaders->pub);
  return 0;
}

//-----------------------------------------------------------------------------
static void Free_shaders( BINKSHADERS * pshaders )
{
  BINKSHADERSVK * shaders = (BINKSHADERSVK*)pshaders;

  for(int i = 0; i < 2; ++i)
    pvkDestroyRenderPass(shaders->device, shaders->render_pass[i], NULL);
  pvkDestroySampler(shaders->device, shaders->sampler, NULL);
  for(int i = 0 ; i < STATE_COUNT; ++i ) 
    for(int j = 0; j < 2; ++j)
      pvkDestroyPipeline(shaders->device, shaders->pipeline_state[i][j], NULL);
  pvkDestroyPipelineCache(shaders->device, shaders->pipeline_cache, NULL);
  pvkDestroyPipelineLayout(shaders->device, shaders->pipeline_layout, NULL);
  pvkDestroyDescriptorPool(shaders->device, shaders->descriptor_pool, NULL);
  pvkDestroyDescriptorSetLayout(shaders->device, shaders->descriptor_set_layout, NULL);
  pvkDestroyCommandPool(shaders->device, shaders->cmdpool, NULL);

  shaders->device = 0;
  BinkUtilFree(shaders);
}

//-----------------------------------------------------------------------------

static vulkan_texture_t vk_create_image(BINKSHADERSVK *shaders, int width, int height, void **tex_ptr, U32 *pitch) {
  vulkan_texture_t ret = {};

  // Define the texture format/etc/etc
  VkImageCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  info.imageType = VK_IMAGE_TYPE_2D;
  info.extent.width = width;
  info.extent.height = height;
  info.extent.depth = 1;
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.format = VK_FORMAT_R8_UNORM;
  info.tiling = VK_IMAGE_TILING_LINEAR;
  info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  if(pvkCreateImage(shaders->device, &info, NULL, &ret.image)) {
    return ret;
  }

  // Allocate the memory for this texture
  VkMemoryRequirements memReqs;
  pvkGetImageMemoryRequirements(shaders->device, ret.image, &memReqs);
  VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocInfo.allocationSize = memReqs.size;
  allocInfo.memoryTypeIndex = vulkan_get_memory_index(&shaders->device_mem_props, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
  if (pvkAllocateMemory(shaders->device, &allocInfo, NULL, &ret.device_mem)) {
    return ret;
  }
  pvkBindImageMemory(shaders->device, ret.image, ret.device_mem, 0);
  ret.device_size = memReqs.size;

  // Allocate the image view...
  VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  view.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view.format = info.format;
  view.components.r = VK_COMPONENT_SWIZZLE_R;
  view.components.g = VK_COMPONENT_SWIZZLE_G;
  view.components.b = VK_COMPONENT_SWIZZLE_B;
  view.components.a = VK_COMPONENT_SWIZZLE_A;
  view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view.subresourceRange.baseMipLevel = 0;
  view.subresourceRange.baseArrayLayer = 0;
  view.subresourceRange.layerCount = 1;
  view.subresourceRange.levelCount = 1;
  view.image = ret.image;
  if(pvkCreateImageView(shaders->device, &view, NULL, &ret.view)) {
    return ret;
  }

  {
    VkImageSubresource sr = { VK_IMAGE_ASPECT_COLOR_BIT };
	VkSubresourceLayout l = {};
	pvkGetImageSubresourceLayout(shaders->device, ret.image, &sr, &l);
	*pitch = (U32)l.rowPitch;
  }

  pvkMapMemory(shaders->device, ret.device_mem, 0, memReqs.size, 0, &ret.ptr);
  *tex_ptr = ret.ptr;

  {
    VkCommandBufferAllocateInfo cmdbufinfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdbufinfo.commandPool = shaders->cmdpool;
    cmdbufinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdbufinfo.commandBufferCount = 1;
    VkCommandBuffer cmdbuf;
    if(pvkAllocateCommandBuffers(shaders->device, &cmdbufinfo, &cmdbuf)) {
      return ret;
    }
    VkCommandBufferBeginInfo cmdBufInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if(pvkBeginCommandBuffer(cmdbuf, &cmdBufInfo)) {
      return ret;
    }

    VkImageMemoryBarrier imageMemoryBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = ret.image;
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    pvkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);

    pvkEndCommandBuffer(cmdbuf);

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdbuf;

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence;
    pvkCreateFence(shaders->device, &fenceInfo, NULL, &fence);
    pvkQueueSubmit(shaders->queue, 1, &submitInfo, fence);
    pvkWaitForFences(shaders->device, 1, &fence, VK_TRUE, 100000000000);
    pvkDestroyFence(shaders->device, fence, 0);
    pvkFreeCommandBuffers(shaders->device, shaders->cmdpool, 1, &cmdbuf);
  }

  return ret;
}

static BINKTEXTURES * Create_textures( BINKSHADERS * pshaders, HBINK bink, void * user_ptr )
{
  BINKSHADERSVK * shaders = (BINKSHADERSVK*)pshaders;
  BINKTEXTURESVK * textures;

  textures = (BINKTEXTURESVK *)BinkUtilMalloc( sizeof(*textures) );
  if ( textures == 0 )
    return 0;

  memset( textures, 0, sizeof( *textures ) );

  textures->pub.user_ptr = user_ptr;
  textures->shaders = shaders;
  textures->bink = bink;

  BinkGetFrameBuffersInfo( bink, &textures->bink_buffers );

  BINKFRAMEBUFFERS * bb = &textures->bink_buffers;

  textures->plane_present[ BINKPLANEY  ] = bb->Frames[ 0 ].YPlane.Allocate;
  textures->plane_present[ BINKPLANECR ] = bb->Frames[ 0 ].cRPlane.Allocate;
  textures->plane_present[ BINKPLANECB ] = bb->Frames[ 0 ].cBPlane.Allocate;
  textures->plane_present[ BINKPLANEA  ] = bb->Frames[ 0 ].APlane.Allocate;
  textures->plane_present[ BINKPLANEH  ] = bb->Frames[ 0 ].HPlane.Allocate;

  // Textures ...
  for ( int i = 0 ; i < bb->TotalFrames ; i++ ) 
  {
    BINKFRAMEPLANESET * ps = &bb->Frames[i];
    if (ps->YPlane.Allocate)  { textures->tex[i][0] = vk_create_image(shaders, bb->YABufferWidth, bb->YABufferHeight, &ps->YPlane.Buffer, &ps->YPlane.BufferPitch); }
    if (ps->cRPlane.Allocate) { textures->tex[i][1] = vk_create_image(shaders, bb->cRcBBufferWidth, bb->cRcBBufferHeight, &ps->cRPlane.Buffer, &ps->cRPlane.BufferPitch); }
    if (ps->cBPlane.Allocate) { textures->tex[i][2] = vk_create_image(shaders, bb->cRcBBufferWidth, bb->cRcBBufferHeight, &ps->cBPlane.Buffer, &ps->cBPlane.BufferPitch); }
    if (ps->APlane.Allocate)  { textures->tex[i][3] = vk_create_image(shaders, bb->YABufferWidth, bb->YABufferHeight, &ps->APlane.Buffer, &ps->APlane.BufferPitch); }
    if (ps->HPlane.Allocate)  { textures->tex[i][4] = vk_create_image(shaders, bb->YABufferWidth, bb->YABufferHeight, &ps->HPlane.Buffer, &ps->HPlane.BufferPitch); }

    // Make the descriptor sets...
    // Allocate a new descriptor set from the global descriptor pool
    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = shaders->descriptor_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &shaders->descriptor_set_layout;
    if (pvkAllocateDescriptorSets(shaders->device, &allocInfo, &textures->descriptor_set[i])) {
      return 0;
    }

    // Update the descriptor set determining the shader binding points
    // For every binding point used in a shader there needs to be one
    // descriptor set matching that binding point

    VkDescriptorImageInfo tex_desc[5] = {};
		VkWriteDescriptorSet writes[5] = {};
    int num_writes = 0;
    for(int j = 0; j < 5; ++j) {
      if(!textures->tex[i][j].image) {
        continue;
      }
      tex_desc[num_writes].sampler = shaders->sampler;
      tex_desc[num_writes].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      tex_desc[num_writes].imageView = textures->tex[i][j].view;
      writes[num_writes].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[num_writes].dstSet = textures->descriptor_set[i];
      writes[num_writes].descriptorCount = 1;
      writes[num_writes].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      writes[num_writes].pImageInfo = &tex_desc[num_writes];
      writes[num_writes].dstBinding = j;
      ++num_writes;
    }
    pvkUpdateDescriptorSets(shaders->device, num_writes, writes, 0, NULL);
  }

  // Register our locked texture pointers with Bink
  BinkRegisterFrameBuffers( bink, bb );

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
static void Free_textures( BINKTEXTURES* ptextures )
{
  BINKTEXTURESVK * ctx = (BINKTEXTURESVK*) ptextures;
  BINKSHADERSVK * shaders = ctx->shaders;
  BINKFRAMEBUFFERS * bb = &ctx->bink_buffers;
  for(int i = 0 ; i < bb->TotalFrames ; ++i) {
    for(int j = 0; j < 5; ++j) {
      if(!ctx->tex[i][j].image) continue;
      pvkDestroyImageView(shaders->device, ctx->tex[i][j].view, NULL);
      pvkUnmapMemory(shaders->device, ctx->tex[i][j].device_mem);
      pvkFreeMemory(shaders->device, ctx->tex[i][j].device_mem, NULL);
      pvkDestroyImage(shaders->device, ctx->tex[i][j].image, NULL);
    }
    pvkFreeDescriptorSets(shaders->device, shaders->descriptor_pool, 1, ctx->descriptor_set+i);
  }

  BinkUtilFree( ctx );
}

//-----------------------------------------------------------------------------
static void Draw_textures( BINKTEXTURES * ptextures,
                           BINKSHADERS * pshaders,
                           void * graphics_context )
{
  F32 consts[BINKCONSTSIZE/4];
  BINKTEXTURESVK * textures = (BINKTEXTURESVK*) ptextures;
  BINKSHADERSVK * shaders = (BINKSHADERSVK*) pshaders;
  BINKAPICONTEXTVULKAN *api_context = (BINKAPICONTEXTVULKAN*)graphics_context;
  VkCommandBuffer cmdBuf = (VkCommandBuffer)api_context->cmdbuf;

  if ( graphics_context == 0 )
    return;

  if ( shaders == 0 )
    shaders = textures->shaders;

  int frame_num = textures->bink_buffers.FrameNum;

  // Pixel shader alpha constants
  consts[2] = consts[1] = consts[0] = textures->draw_type == 1 ? textures->alpha : 1;
  consts[3] = textures->alpha;

  // colorspace stuff
  if ( textures->plane_present[ BINKPLANEH ] )
  {
    // HDR
    consts[  4 ] = textures->bink->ColorSpace[0];
    consts[  5 ] = textures->exposure;
    consts[  6 ] = textures->out_luma;
    consts[  7 ] = 0;
    memcpy(consts + 8, textures->bink->ColorSpace + 1, 4 * sizeof(F32));
  } 
  else 
  {
    // set the constants for the type of ycrcb we have
    memcpy( consts + 4, textures->bink->ColorSpace, sizeof( textures->bink->ColorSpace ) );
  }

  // Vertex shader constant
  consts[ 20 ] = ( textures->Bx - textures->Ax ) * 2.0f;
  consts[ 21 ] = ( textures->Cx - textures->Ax ) * 2.0f;
  consts[ 22 ] = ( textures->By - textures->Ay ) * 2.0f;
  consts[ 23 ] = ( textures->Cy - textures->Ay ) * 2.0f;
  consts[ 24 ] = textures->Ax * 2.0f - 1.0f;
  consts[ 25 ] = textures->Ay * 2.0f - 1.0f;
  consts[ 27 ] = 0.0f;
  consts[ 28 ] = 0.0f;

  consts[ 28 ] = textures->u1 - textures->u0;
  consts[ 29 ] = textures->v1 - textures->v0;
  consts[ 30 ] = textures->u0;
  consts[ 31 ] = textures->v0;
  
  // set the constant buffer for the next draw
  pvkCmdPushConstants(cmdBuf, shaders->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, BINKCONSTSIZE, consts);

  StateId state = textures->plane_present[ BINKPLANEA ] ? STATE_OPAQUEALPHA : STATE_OPAQUE; // default to opaque
  if(textures->draw_type != 2) 
  {
    if ( textures->plane_present[ BINKPLANEA ] )
      state = ( textures->draw_type == 1 ) ? STATE_TEXALPHA_PM : STATE_TEXALPHA;
    else if ( textures->alpha < 0.999f )
      state = ( textures->draw_type == 1 ) ? STATE_ONEALPHA_PM : STATE_ONEALPHA;
  }

  if ( textures->plane_present[ BINKPLANEH ] )
  {
    if(textures->tonemap == 2) {
      state = (StateId)((int)state + (int)(STATE_HDR_PQ_OPAQUE));
    } else if(textures->tonemap == 1) {
      state = (StateId)((int)state + (int)(STATE_HDR_TM_OPAQUE));
    } else {
      state = (StateId)((int)state + (int)(STATE_HDR_OPAQUE));
    }
  } else if(textures->draw_flags & 0x80000000) {
    // Decode sRGB to linear space (for when rendering to non-sRGB textures like HDR textures).
    state = (StateId)((int)state + (int)(STATE_SRGB_OPAQUE));
  }

  pvkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shaders->pipeline_layout, 0, 1, &textures->descriptor_set[frame_num], 0, NULL);
  pvkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shaders->pipeline_state[state][api_context->format_idx]);
  pvkCmdDraw(cmdBuf, 4, 1, 0, 0);
}

static void Start_texture_update( BINKTEXTURES * ptextures )
{
}

static void Finish_texture_update( BINKTEXTURES * ptextures )
{
  BINKTEXTURESVK * textures = (BINKTEXTURESVK*) ptextures;

  int frame_num = textures->bink_buffers.FrameNum;
  
  textures->pub.Ytexture = &textures->tex[frame_num][0];
  textures->pub.cRtexture = &textures->tex[frame_num][1];
  textures->pub.cBtexture = &textures->tex[frame_num][2];
  textures->pub.Atexture = &textures->tex[frame_num][3];
  textures->pub.Htexture = &textures->tex[frame_num][4];

  // Flush memory to ensure device readable
  for(int i = 0; i < 5; ++i) {
    if(!textures->tex[frame_num][i].image) {
      continue;
    }
		VkMappedMemoryRange mem_range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
		mem_range.memory = textures->tex[frame_num][i].device_mem;
		mem_range.size = textures->tex[frame_num][i].device_size;
		pvkFlushMappedMemoryRanges(textures->shaders->device, 1, &mem_range);
  }
}

static void Set_draw_position( BINKTEXTURES * ptextures,
                               F32 x0, F32 y0, F32 x1, F32 y1 )
{
  BINKTEXTURESVK * textures = (BINKTEXTURESVK*) ptextures;
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
  BINKTEXTURESVK * textures = (BINKTEXTURESVK*) ptextures;
  textures->Ax = Ax;
  textures->Ay = Ay;
  textures->Bx = Bx;
  textures->By = By;
  textures->Cx = Cx;
  textures->Cy = Cy;
}

static void Set_source_rect( BINKTEXTURES * ptextures, F32 u0, F32 v0, F32 u1, F32 v1 )
{
  BINKTEXTURESVK * textures = (BINKTEXTURESVK*) ptextures;
  textures->u0 = u0;
  textures->v0 = v0;
  textures->u1 = u1;
  textures->v1 = v1;
}

static void Set_alpha_settings( BINKTEXTURES * ptextures, F32 alpha_value, S32 draw_type )
{
  BINKTEXTURESVK * textures = (BINKTEXTURESVK*) ptextures;
  textures->alpha = alpha_value;
  textures->draw_type = draw_type & 0x0FFFFFFF;
  textures->draw_flags = draw_type & 0xF0000000;
}

static void Set_hdr_settings( BINKTEXTURES * ptextures, S32 tonemap, F32 exposure, S32 out_nits ) 
{
  BINKTEXTURESVK * textures = (BINKTEXTURESVK*) ptextures;
  textures->tonemap = tonemap;
  textures->exposure = exposure;
  textures->out_luma = out_nits/80.f;
}

