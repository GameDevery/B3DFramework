//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#pragma once

#include "B3DPrerequisites.h"

/** @addtogroup Plugins
 *  @{
 */

/** @defgroup FBX bsfFBXImporter
 *	%Mesh importer for the FBX file format.
 */

/** @} */

#define FBXSDK_NEW_API

// The FBX SDK headers trigger a number of warnings we can't fix
#if B3D_COMPILER_GCC || B3D_COMPILER_CLANG
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#	pragma GCC diagnostic ignored "-Wdeprecated-copy"
#	if B3D_COMPILER_CLANG
#		pragma clang diagnostic ignored "-Wdeprecated-copy-with-user-provided-copy"
#		pragma clang diagnostic ignored "-Wnontrivial-memcall"
#		pragma clang diagnostic ignored "-Wuninitialized-const-reference"
#	endif
#endif

#include <fbxsdk.h>

#if B3D_COMPILER_GCC || B3D_COMPILER_CLANG
#	pragma GCC diagnostic pop
#endif

#define FBX_IMPORT_MAX_UV_LAYERS 2
#define FBX_IMPORT_MAX_BONE_INFLUENCES 4
