//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#pragma once

#include "B3DPrerequisites.h"

namespace b3d
{
	/** @addtogroup Text
	 *  @{
	 */

	/**
	 * Weight of a font face, on the same 100-900 scale used by the OS/2 table of a font file and by CSS. Values other than
	 * the named ones are valid, as some families ship intermediate weights.
	 */
	enum class B3D_SCRIPT_EXPORT(DocumentationGroup(Text)) FontWeight : u16
	{
		Thin = 100,
		ExtraLight = 200,
		Light = 300,
		Normal = 400,
		Medium = 500,
		SemiBold = 600,
		Bold = 700,
		ExtraBold = 800,
		Black = 900
	};

	/** Smallest and largest weight a font face may report. */
	constexpr FontWeight kMinimumFontWeight = FontWeight::Thin;
	constexpr FontWeight kMaximumFontWeight = FontWeight::Black;

	/** Slant of a font face. */
	enum class B3D_SCRIPT_EXPORT(DocumentationGroup(Text)) FontSlant
	{
		Normal, /**< Upright face. */
		Italic /**< Slanted face, whether a true italic or an oblique. */
	};

	/** Identifies a single face within a font family. */
	struct B3D_SCRIPT_EXPORT(ExportAsStruct(true), DocumentationGroup(Text)) FontFaceStyle
	{
		FontFaceStyle() = default;
		FontFaceStyle(FontWeight weight, FontSlant slant = FontSlant::Normal)
			: Weight(weight), Slant(slant)
		{ }

		B3D_SCRIPT_EXPORT()
		FontWeight Weight = FontWeight::Normal;

		B3D_SCRIPT_EXPORT()
		FontSlant Slant = FontSlant::Normal;

		bool operator==(const FontFaceStyle& other) const { return Weight == other.Weight && Slant == other.Slant; }
		bool operator!=(const FontFaceStyle& other) const { return !(*this == other); }
	};

	/** @} */
} // namespace b3d
