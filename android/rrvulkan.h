// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#ifndef RRVULKAN_H
#define RRVULKAN_H

#include "egttypes.h"

RADDEFSTART

#if defined(__RADWIN__) 
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__RADSTADIA__) 
#ifndef VK_USE_PLATFORM_GGP
#define VK_USE_PLATFORM_GGP
#endif
#elif defined(__RADLINUX__) 
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(__RADANDROID__) 
#define VK_USE_PLATFORM_ANDROID_KHR
#endif

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define DO_VK_PROCS_INSTANCE() \
	ProcessProc(vkDestroyInstance) \
	ProcessProc(vkEnumeratePhysicalDevices) \
	ProcessProc(vkGetPhysicalDeviceFeatures) \
	ProcessProc(vkGetPhysicalDeviceFormatProperties) \
	ProcessProc(vkGetPhysicalDeviceImageFormatProperties) \
	ProcessProc(vkGetPhysicalDeviceProperties) \
	ProcessProc(vkGetPhysicalDeviceQueueFamilyProperties) \
	ProcessProc(vkGetPhysicalDeviceMemoryProperties) \
	ProcessProc(vkCreateDevice) \
	ProcessProc(vkDestroyDevice) \
	ProcessProc(vkEnumerateDeviceExtensionProperties) \
	ProcessProc(vkEnumerateDeviceLayerProperties) \
	ProcessProc(vkGetDeviceQueue) \
	ProcessProc(vkQueueSubmit) \
	ProcessProc(vkQueueWaitIdle) \
	ProcessProc(vkDeviceWaitIdle) \
	ProcessProc(vkAllocateMemory) \
	ProcessProc(vkFreeMemory) \
	ProcessProc(vkMapMemory) \
	ProcessProc(vkUnmapMemory) \
	ProcessProc(vkFlushMappedMemoryRanges) \
	ProcessProc(vkInvalidateMappedMemoryRanges) \
	ProcessProc(vkGetDeviceMemoryCommitment) \
	ProcessProc(vkBindBufferMemory) \
	ProcessProc(vkBindImageMemory) \
	ProcessProc(vkGetBufferMemoryRequirements) \
	ProcessProc(vkGetImageMemoryRequirements) \
	ProcessProc(vkGetImageSparseMemoryRequirements) \
	ProcessProc(vkGetPhysicalDeviceSparseImageFormatProperties) \
	ProcessProc(vkQueueBindSparse) \
	ProcessProc(vkCreateFence) \
	ProcessProc(vkDestroyFence) \
	ProcessProc(vkResetFences) \
	ProcessProc(vkGetFenceStatus) \
	ProcessProc(vkWaitForFences) \
	ProcessProc(vkCreateSemaphore) \
	ProcessProc(vkDestroySemaphore) \
	ProcessProc(vkCreateEvent) \
	ProcessProc(vkDestroyEvent) \
	ProcessProc(vkGetEventStatus) \
	ProcessProc(vkSetEvent) \
	ProcessProc(vkResetEvent) \
	ProcessProc(vkCreateQueryPool) \
	ProcessProc(vkDestroyQueryPool) \
	ProcessProc(vkGetQueryPoolResults) \
	ProcessProc(vkCreateBuffer) \
	ProcessProc(vkDestroyBuffer) \
	ProcessProc(vkCreateBufferView) \
	ProcessProc(vkDestroyBufferView) \
	ProcessProc(vkCreateImage) \
	ProcessProc(vkDestroyImage) \
	ProcessProc(vkGetImageSubresourceLayout) \
	ProcessProc(vkCreateImageView) \
	ProcessProc(vkDestroyImageView) \
	ProcessProc(vkCreateShaderModule) \
	ProcessProc(vkDestroyShaderModule) \
	ProcessProc(vkCreatePipelineCache) \
	ProcessProc(vkDestroyPipelineCache) \
	ProcessProc(vkGetPipelineCacheData) \
	ProcessProc(vkMergePipelineCaches) \
	ProcessProc(vkCreateGraphicsPipelines) \
	ProcessProc(vkCreateComputePipelines) \
	ProcessProc(vkDestroyPipeline) \
	ProcessProc(vkCreatePipelineLayout) \
	ProcessProc(vkDestroyPipelineLayout) \
	ProcessProc(vkCreateSampler) \
	ProcessProc(vkDestroySampler) \
	ProcessProc(vkCreateDescriptorSetLayout) \
	ProcessProc(vkDestroyDescriptorSetLayout) \
	ProcessProc(vkCreateDescriptorPool) \
	ProcessProc(vkDestroyDescriptorPool) \
	ProcessProc(vkResetDescriptorPool) \
	ProcessProc(vkAllocateDescriptorSets) \
	ProcessProc(vkFreeDescriptorSets) \
	ProcessProc(vkUpdateDescriptorSets) \
	ProcessProc(vkCreateFramebuffer) \
	ProcessProc(vkDestroyFramebuffer) \
	ProcessProc(vkCreateRenderPass) \
	ProcessProc(vkDestroyRenderPass) \
	ProcessProc(vkGetRenderAreaGranularity) \
	ProcessProc(vkCreateCommandPool) \
	ProcessProc(vkDestroyCommandPool) \
	ProcessProc(vkResetCommandPool) \
	ProcessProc(vkAllocateCommandBuffers) \
	ProcessProc(vkFreeCommandBuffers) \
	ProcessProc(vkBeginCommandBuffer) \
	ProcessProc(vkEndCommandBuffer) \
	ProcessProc(vkResetCommandBuffer) \
	ProcessProc(vkCmdBindPipeline) \
	ProcessProc(vkCmdSetViewport) \
	ProcessProc(vkCmdSetScissor) \
	ProcessProc(vkCmdSetLineWidth) \
	ProcessProc(vkCmdSetDepthBias) \
	ProcessProc(vkCmdSetBlendConstants) \
	ProcessProc(vkCmdSetDepthBounds) \
	ProcessProc(vkCmdSetStencilCompareMask) \
	ProcessProc(vkCmdSetStencilWriteMask) \
	ProcessProc(vkCmdSetStencilReference) \
	ProcessProc(vkCmdBindDescriptorSets) \
	ProcessProc(vkCmdBindIndexBuffer) \
	ProcessProc(vkCmdBindVertexBuffers) \
	ProcessProc(vkCmdDraw) \
	ProcessProc(vkCmdDrawIndexed) \
	ProcessProc(vkCmdDrawIndirect) \
	ProcessProc(vkCmdDrawIndexedIndirect) \
	ProcessProc(vkCmdDispatch) \
	ProcessProc(vkCmdDispatchIndirect) \
	ProcessProc(vkCmdCopyBuffer) \
	ProcessProc(vkCmdCopyImage) \
	ProcessProc(vkCmdBlitImage) \
	ProcessProc(vkCmdCopyBufferToImage) \
	ProcessProc(vkCmdCopyImageToBuffer) \
	ProcessProc(vkCmdUpdateBuffer) \
	ProcessProc(vkCmdFillBuffer) \
	ProcessProc(vkCmdClearColorImage) \
	ProcessProc(vkCmdClearDepthStencilImage) \
	ProcessProc(vkCmdClearAttachments) \
	ProcessProc(vkCmdResolveImage) \
	ProcessProc(vkCmdSetEvent) \
	ProcessProc(vkCmdResetEvent) \
	ProcessProc(vkCmdWaitEvents) \
	ProcessProc(vkCmdPipelineBarrier) \
	ProcessProc(vkCmdBeginQuery) \
	ProcessProc(vkCmdEndQuery) \
	ProcessProc(vkCmdResetQueryPool) \
	ProcessProc(vkCmdWriteTimestamp) \
	ProcessProc(vkCmdCopyQueryPoolResults) \
	ProcessProc(vkCmdPushConstants) \
	ProcessProc(vkCmdBeginRenderPass) \
	ProcessProc(vkCmdNextSubpass) \
	ProcessProc(vkCmdEndRenderPass) \
	ProcessProc(vkCmdExecuteCommands) \
	ProcessProc(vkCreateSwapchainKHR) \
	ProcessProc(vkDestroySwapchainKHR) \
	ProcessProc(vkGetSwapchainImagesKHR) \
	ProcessProc(vkAcquireNextImageKHR) \
	ProcessProc(vkQueuePresentKHR)

#define DO_VK_PROCS_SURFACE() \
	ProcessProc(vkDestroySurfaceKHR) \
	ProcessProc(vkGetPhysicalDeviceSurfaceSupportKHR) \
	ProcessProc(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
	ProcessProc(vkGetPhysicalDeviceSurfaceFormatsKHR) \
	ProcessProc(vkGetPhysicalDeviceSurfacePresentModesKHR)

#define DO_VK_PROCS_BASE() \
	ProcessProc(vkCreateInstance) \
	ProcessProc(vkGetInstanceProcAddr) \
	ProcessProc(vkGetDeviceProcAddr) \
	ProcessProc(vkEnumerateInstanceExtensionProperties) \
	ProcessProc(vkEnumerateInstanceLayerProperties)

#define DO_VK_PROCS_OPT_BASE() \
	ProcessProc(vkGetPhysicalDeviceDisplayPropertiesKHR) \
	ProcessProc(vkGetPhysicalDeviceDisplayPlanePropertiesKHR) \
	ProcessProc(vkGetDisplayPlaneSupportedDisplaysKHR) \
	ProcessProc(vkGetDisplayModePropertiesKHR) \
	ProcessProc(vkCreateDisplayModeKHR) \
	ProcessProc(vkGetDisplayPlaneCapabilitiesKHR)

#define DO_VK_PROCS_OPT_INSTANCE() \
	ProcessProc(vkCreateSharedSwapchainsKHR)

#define DO_VK_PROCS_WIN() \
    ProcessProc(vkCreateWin32SurfaceKHR)

#define DO_VK_PROCS_LINUX() \
	ProcessProc(vkCreateXlibSurfaceKHR) \
	ProcessProc(vkGetPhysicalDeviceXlibPresentationSupportKHR)

#define DO_VK_PROCS_STADIA() \
	ProcessProc(vkCreateStreamDescriptorSurfaceGGP)

#define DO_VK_PROCS_ANDROID() \
    ProcessProc(vkCreateAndroidSurfaceKHR)

#define DO_VK_PROCS() \
    DO_VK_PROCS_INSTANCE() \
    DO_VK_PROCS_SURFACE() \
    DO_VK_PROCS_BASE() \
    DO_VK_PROCS_OPT_BASE() \
    DO_VK_PROCS_OPT_INSTANCE()



#define ProcessProc(name) extern RR_STRING_JOIN( PFN_, name ) RR_STRING_JOIN( p, name );
DO_VK_PROCS()
#if defined(__RADWIN__) 
DO_VK_PROCS_WIN()
#elif defined(__RADSTADIA__)
DO_VK_PROCS_STADIA()
#elif defined(__RADLINUX__) 
DO_VK_PROCS_LINUX()
#elif defined(__RADANDROID__) 
DO_VK_PROCS_ANDROID()
#endif
#undef ProcessProc

extern int rr_setup_vulkan( void );
extern void rr_shutdown_vulkan( void );

RADDEFEND

// @cdep pre $requires(rrvulkan.c)

#endif // RRVULKAN_H
