//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#pragma once

#include "B3DPrerequisites.h"
#include "Resources/B3DResource.h"
#include "Text/B3DFont.h"

namespace b3d
{
	/** @addtogroup Text
	 *  @{
	 */

	/** A single face belonging to a font family. */
	struct B3D_EXPORT B3D_SCRIPT_EXPORT(ExportAsStruct(true), DocumentationGroup(Text)) FontFamilyFace : IReflectable
	{
		FontFamilyFace() = default;
		FontFamilyFace(HFont font, const FontFaceStyle& style)
			: Font(std::move(font)), Style(style)
		{ }

		B3D_SCRIPT_EXPORT()
		HFont Font;

		B3D_SCRIPT_EXPORT()
		FontFaceStyle Style;

		/************************************************************************/
		/* 								SERIALIZATION                      		*/
		/************************************************************************/
	public:
		friend class FontFamilyFaceRTTI;
		static RTTIType* GetRttiStatic();
		RTTIType* GetRtti() const override;
	};

	/** Descriptor structure used for initialization of a FontFamily. */
	struct B3D_SCRIPT_EXPORT(ExportAsStruct(true), DocumentationGroup(Text)) FontFamilyCreateInformation
	{
		/** Name the family is identified by. Family lookups match this name, ignoring case. */
		B3D_SCRIPT_EXPORT()
		String Name;

		/** Faces the family provides. Faces with a style already provided by an earlier face are discarded. */
		B3D_SCRIPT_EXPORT()
		Vector<FontFamilyFace> Faces;
	};

	/**
	 * A named group of Font faces that differ only in weight and slant. Provides the lookup from a requested style to the
	 * face that best provides it, so callers can ask for bold or italic text without knowing which faces the family ships.
	 */
	class B3D_EXPORT B3D_SCRIPT_EXPORT(DocumentationGroup(Text)) FontFamily : public Resource
	{
	public:
		/** Returns the name the family is identified by, fixed when the family is created. Family lookups match this name ignoring case. */
		B3D_SCRIPT_EXPORT()
		B3D_NO_RREF const String& GetFamilyName() const { return mFamilyName; }

		/**
		 * Returns the face that best matches the requested style. For a family that provides at least one face this never
		 * returns null: when no face provides the requested style exactly, the closest available face is substituted. Use
		 * HasFace() when an exact match is required.
		 */
		B3D_SCRIPT_EXPORT()
		B3D_NO_RREF const HFont& GetFace(const FontFaceStyle& style) const;

		/** Returns true if the family provides a face with exactly the requested style. */
		B3D_SCRIPT_EXPORT()
		bool HasFace(const FontFaceStyle& style) const { return FindExactFace(style) != nullptr; }

		/** Returns all faces the family provides. */
		B3D_NO_RREF const Vector<FontFamilyFace>& GetFaces() const { return mFaces; }

		/**
		 * Adds a face to the family. Returns false without modifying the family if @p font is null, or if the family already
		 * provides a face with the same style.
		 */
		B3D_SCRIPT_EXPORT()
		bool AddFace(const HFont& font, const FontFaceStyle& style);

		/** Removes the face with exactly the provided style. Returns false if the family provides no such face. */
		B3D_SCRIPT_EXPORT()
		bool RemoveFace(const FontFaceStyle& style);

		/** Creates a new font family from the provided faces. */
		static HFontFamily Create(const FontFamilyCreateInformation& createInformation);

		/**
		 * Creates a new font family out of a single face, naming it after the family the face reports belonging to. Intended
		 * for fonts referenced directly by path, where no other face of the family is known.
		 */
		static HFontFamily CreateFromFace(const HFont& font);

		/** @name Internal
		 *  @{
		 */

		/** Creates a new font family as a pointer instead of a resource handle. */
		static TShared<FontFamily> CreateShared(const FontFamilyCreateInformation& createInformation);

		/** Creates a font family without initializing it. */
		static TShared<FontFamily> CreateEmpty();

		/** Returns the family name and face style the provided font reports through its resource meta-data. */
		static bool TryGetFaceIdentity(const HFont& font, String& outFamilyName, FontFaceStyle& outStyle);

		void Initialize() override;

		/** @} */

	protected:
		friend class FontManager;

		FontFamily(const FontFamilyCreateInformation& createInformation);

		/** Returns the face with exactly the provided style, or null if the family provides no such face. */
		const FontFamilyFace* FindExactFace(const FontFaceStyle& style) const;

		/**
		 * Returns the face of the provided slant whose weight is closest to @p weight, searching only in a single direction.
		 * Returns null if the family provides no face of that slant in that direction.
		 *
		 * @param slant			Slant the face must have. Faces of any other slant are ignored.
		 * @param weight		Weight to search around.
		 * @param searchHeavier	If true only faces at or above @p weight are considered, otherwise only those at or below it.
		 */
		const FontFamilyFace* FindClosestWeight(FontSlant slant, FontWeight weight, bool searchHeavier) const;

		String mFamilyName;
		Vector<FontFamilyFace> mFaces;

		/************************************************************************/
		/* 								SERIALIZATION                      		*/
		/************************************************************************/
	public:
		friend class FontFamilyRTTI;
		static RTTIType* GetRttiStatic();
		RTTIType* GetRtti() const override;
	};

	/** @} */
} // namespace b3d
