// Copyright Epic Games, Inc. All Rights Reserved.
#include "egttypes.h"

RADDEFFUNC S32  RADLINK binkdefopen (UINTa * user_data, const char * fn);
RADDEFFUNC U64  RADLINK binkdefread (UINTa * user_data, void * dest, U64 bytes);
RADDEFFUNC void RADLINK binkdefseek (UINTa * user_data, U64 pos);
RADDEFFUNC void RADLINK binkdefclose(UINTa * user_data);

// @cdep pre $TakePlatformCPP

