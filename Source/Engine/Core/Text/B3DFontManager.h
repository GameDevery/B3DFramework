//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#pragma once

#include "B3DPrerequisites.h"
#include "Utility/B3DModule.h"
#include "Text/B3DFontFamily.h"

namespace b3d
{
	class PackageResourceMetaData;

	/** @addtogroup Text
	 *  @{
	 */

	/**
	 * Keeps an index of the font faces available to the application, grouped into families, so text can be requested by
	 * family name and style rather than by pointing at an individual font file.
	 *
	 * Families are normally discovered by scanning folders of font resources through RegisterFontFolder(). Scanning reads
	 * only resource meta-data, so indexing a folder does not load any font data; a family's faces are loaded the first time
	 * something asks for that family. Families may also be registered explicitly through RegisterFamily(), which takes
	 * precedence over anything discovery would produce under the same name.
	 *
	 * @note Thread safe.
	 */
	class B3D_EXPORT FontManager : public Module<FontManager>
	{
	public:
		/**
		 * Returns the family registered under the provided name, or null if no faces are registered under it. Name matching
		 * ignores case. Discovered families and their faces are loaded on the first request and cached afterwards.
		 */
		HFontFamily TryGetFamily(const String& name);

		/**
		 * Indexes every font resource within the provided virtual folder so its faces can be found through family lookups.
		 * Only resource meta-data is read; no font data is loaded. Safe to call repeatedly, and for overlapping folders.
		 *
		 * @param virtualFolderPath		Virtual path of the folder holding the font resources, e.g. '/B3D/EngineData/Fonts/'.
		 */
		void RegisterFontFolder(const Path& virtualFolderPath);

		/**
		 * Registers a face under the provided family name, creating the family if it does not exist yet. Intended for fonts
		 * that are created at runtime, or that live somewhere the discovery process does not scan.
		 *
		 * Returns false if @p font is null, or if the family already provides a face with the same style.
		 */
		bool RegisterFace(const String& familyName, const HFont& font, const FontFaceStyle& style);

		/**
		 * Registers an explicitly authored family, replacing anything previously registered under the same name. Use this to
		 * give a family a name of your choosing, or to assemble one out of faces that discovery would not group together.
		 *
		 * The family is registered under the name it was created with. Returns false if @p family is null or unnamed.
		 */
		bool RegisterFamily(const HFontFamily& family);

		/** Removes the family registered under the provided name, along with every face indexed for it. */
		void UnregisterFamily(const String& name);

		/** Returns the names of all registered families, in no particular order. */
		Vector<String> GetFamilyNames() const;

	private:
		/** A face known to the index, which may not have been loaded yet. */
		struct IndexedFace
		{
			FontFaceStyle Style;
			Path VirtualPath; /**< Path the face is loaded from. Blank for faces that were registered already loaded. */
			HFont Font; /**< Set once the face has been loaded, or immediately for faces that were registered already loaded. */
		};

		/** A family known to the index, whose FontFamily resource may not have been built yet. */
		struct IndexedFamily
		{
			String Name; /**< Name with its original casing, used when the family resource is built. */
			Vector<IndexedFace> Faces;
			HFontFamily Family; /**< Built on the first request, or provided up front for explicitly registered families. */
			bool IsExplicitlyRegistered = false; /**< True if the family came from RegisterFamily(), which discovery must not modify. */
		};

		/**
		 * Adds a face to the family with the provided name, creating the family if needed. Caller must hold the manager
		 * mutex. Returns false if the family already provides a face with the same style, or if it was explicitly registered.
		 */
		bool RegisterFaceInternal(const String& familyName, const FontFaceStyle& style, const Path& virtualPath, const HFont& font);

		/**
		 * Determines the family name and face style of an indexed font from its resource meta-data. Falls back to the file
		 * name when the font carries no meta-data, which is the case for fonts imported before it was recorded.
		 */
		static void GetFaceIdentity(const Path& virtualPath, const PackageResourceMetaData& metaData, String& outFamilyName, FontFaceStyle& outStyle);

		/** Returns the key the provided family name is indexed under. */
		static String GetFamilyKey(const String& name);

		UnorderedMap<String, IndexedFamily> mFamiliesByKey;
		mutable Mutex mMutex;
	};

	/** Provides easier access to FontManager. */
	B3D_EXPORT FontManager& GetFontManager();

	/** @} */
} // namespace b3d
