// Copyright Epic Games, Inc. All Rights Reserved.
#include "egttypes.h"
#include "rrCore.h"

#include "radmem.h"

#include <stdlib.h>

static void* ourmalloc( U64 bytes )
{
  #if !defined(__RAD64__)
    if ( bytes > 0xffffffff )
      return 0;
    return malloc( (U32) bytes );
  #else
    return malloc( bytes );
  #endif
}

#define radmalrad ourmalloc
#define radfrrad free

#include "../radmem.inl"

