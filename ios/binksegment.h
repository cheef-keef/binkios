// Copyright Epic Games, Inc. All Rights Reserved.
#include "egttypes.h"

#if defined(__RADFINAL__) || defined(_DEBUG) || defined(__clang__) || !defined(_MSC_VER)

// nothing here

#else
#pragma code_seg("BINK_" SEGMENT_NAME)
#pragma data_seg("BINKDATA_" SEGMENT_NAME)
#pragma const_seg("BINKCONST_" SEGMENT_NAME)
#pragma bss_seg("BINKBSS_" SEGMENT_NAME)
#endif

#undef SEGMENT_NAME
