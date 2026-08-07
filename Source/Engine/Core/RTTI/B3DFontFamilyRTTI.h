//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#pragma once

#include "B3DPrerequisites.h"
#include "Reflection/B3DRTTIType.h"
#include "RTTI/B3DStdRTTI.h"
#include "RTTI/B3DFontRTTI.h"
#include "Text/B3DFontFamily.h"

namespace b3d
{
	/** @cond RTTI */
	/** @addtogroup RTTI-Impl-Engine
	 *  @{
	 */

	class B3D_EXPORT FontFamilyFaceRTTI : public TRTTIType<FontFamilyFace, IReflectable, FontFamilyFaceRTTI>
	{
	private:
		B3D_RTTI_BEGIN_MEMBERS
			B3D_RTTI_MEMBER(Font, 0)
			B3D_RTTI_MEMBER(Style, 1)
		B3D_RTTI_END_MEMBERS

	public:
		const String& GetRttiName() override
		{
			static String name = "FontFamilyFace";
			return name;
		}

		u32 GetRttiId() const override
		{
			return TID_FontFamilyFace;
		}

		TShared<IReflectable> NewRttiObject() override
		{
			return B3DMakeShared<FontFamilyFace>();
		}
	};

	class B3D_EXPORT FontFamilyRTTI : public TRTTIType<FontFamily, Resource, FontFamilyRTTI>
	{
	private:
		B3D_RTTI_BEGIN_MEMBERS
			B3D_RTTI_MEMBER(mFamilyName, 0)
			B3D_RTTI_MEMBER_CONTAINER(mFaces, 1)
		B3D_RTTI_END_MEMBERS

	public:
		const String& GetRttiName() override
		{
			static String name = "FontFamily";
			return name;
		}

		u32 GetRttiId() const override
		{
			return TID_FontFamily;
		}

		TShared<IReflectable> NewRttiObject() override
		{
			return FontFamily::CreateEmpty();
		}
	};

	/** @} */
	/** @endcond */
} // namespace b3d
