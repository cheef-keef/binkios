// Copyright Epic Games, Inc. All Rights Reserved.
#include "egttypes.h"

#include "defread.h"
#ifdef __RADLINUX__
  #define _LARGEFILE64_SOURCE
#endif

#ifdef __RADIPHONE__
// NOTE(fg): when building from Xcode, SEEK_SET doesn't seem to get defined in unistd.h. No idea.
#include <stdio.h>
#endif

#include <fcntl.h>
#include <unistd.h>

S32  RADLINK binkdefopen(UINTa * user_data, const char * fn)
{
  int h;

  h = open( fn, O_RDONLY );
  if ( h == -1 )
    return 0;

  *user_data = (UINTa)h;
  return 1;
}

U64  RADLINK binkdefread(UINTa * user_data, void * dest,U64 bytes)
{
  U64 amt64;

  amt64 = 0;
  while( bytes )
  {
    size_t b;
    size_t amt;
    
    b = ( bytes > ( 512*1024*1024 ) ) ? (512*1024*1024) : ((U32)bytes);
    amt = read( (int)user_data[0], dest, b );
    bytes -= amt;
    dest = (U8*)dest + amt;
    amt64 += amt;

    if (amt != b )
      break;
  }

  return amt64;
}

void RADLINK binkdefseek(UINTa * user_data, U64 pos)
{
  int h;
  h = (int)user_data[0];

  #if defined(__RADMAC__) || defined(__RADIPHONE__) || defined(__RADEMSCRIPTEN__)
    lseek( h,pos,SEEK_SET );
  #else
    lseek64( h,pos,SEEK_SET );
  #endif
}

void RADLINK binkdefclose(UINTa * user_data)
{
  int h;
  h = (int)user_data[0];
  close( h );
  user_data[0] = 0;
}
