// Copyright Epic Games, Inc. All Rights Reserved.
/* vim: set softtabstop=2 shiftwidth=2 expandtab : */
#include "rrvulkan.h"

#define ProcessProc(name) RR_STRING_JOIN( PFN_, name ) RR_STRING_JOIN( p, name );
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

typedef void voidfunc(void);

#ifdef __RADNT__

#define WIN32_LEAN_AND_MEAN
#include <windows.h> 
#include <malloc.h> 

static void * StartVKLoad( void )
{
  return (void *)LoadLibraryA("vulkan-1.dll");
}

static voidfunc * GetVKProcAddress( void * handle, char const * proc )
{
  voidfunc * res = 0;

  res = (voidfunc *)GetProcAddress((HMODULE)handle, proc);

  return res;
}

static void EndVKLoad( void * handle )
{
  FreeLibrary( (HMODULE)handle );
}

#elif defined( __RADLINUX__ ) || defined(__RADANDROID__)

#include <stddef.h>
#include <stdlib.h>
#include <dlfcn.h>

static void *StartVKLoad(void) {
#if defined(__RADLINUX__)
  return dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_GLOBAL);
#elif defined(__RADANDROID__)
  return dlopen("libvulkan.so", RTLD_LAZY | RTLD_GLOBAL);
#endif
}

static voidfunc *GetVKProcAddress(void *handle, char const *proc) {
  voidfunc *res = 0;

  res = (voidfunc *)dlsym(handle, proc);

  return res;
}

static void EndVKLoad(void *handle) {
  dlclose(handle);
}

#endif

static void *VKhandle;

int rr_setup_vulkan( void ) 
{
  if ( VKhandle )
    return 0;

  VKhandle = StartVKLoad();
  if(!VKhandle) 
    return __LINE__;

#define STRINGDELAY( v ) #v
#define STRINGIZE( v ) STRINGDELAY( v )

  #define ProcessProc(name) RR_STRING_JOIN( p, name ) = (RR_STRING_JOIN(PFN_,name))GetVKProcAddress( VKhandle, STRINGIZE( name ) );
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

  return 0;
}

void rr_shutdown_vulkan( void )
{
  EndVKLoad( VKhandle );
  VKhandle = 0;
}
