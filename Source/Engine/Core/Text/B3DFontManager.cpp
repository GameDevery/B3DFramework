//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "Text/B3DFontManager.h"
#include "Resources/B3DPackage.h"
#include "Resources/B3DPackageManager.h"
#include "Resources/B3DResources.h"

using namespace b3d;

HFontFamily FontManager::TryGetFamily(const String& name)
{
	Lock lock(mMutex);

	const auto found = mFamiliesByKey.find(GetFamilyKey(name));
	return found != mFamiliesByKey.end() ? found->second.Family : nullptr;
}

void FontManager::RegisterFontFolder(const Path& virtualFolderPath)
{
	Lock lock(mMutex);

	GetPackageManager().EnumerateResourceMetaData(virtualFolderPath,
		[this](const Path& virtualPath, const PackageResourceMetaData& metaData)
		{
			String familyName;
			FontFaceStyle style;
			GetFaceIdentity(virtualPath, metaData, familyName, style);

			// A handle to a resource that has not been loaded costs nothing to create, so the whole folder can be indexed
			// without reading any font data. The face is loaded the first time the family is asked for that style.
			const HFont font = B3DStaticResourceCast<Font>(GetResources().GetOrCreateResourceHandle(metaData.Id));
			RegisterFaceInternal(familyName, style, virtualPath, font);
		}, TID_Font);
}

bool FontManager::RegisterFace(const String& familyName, const HFont& font, const FontFaceStyle& style)
{
	if(font == nullptr)
		return false;

	Lock lock(mMutex);
	return RegisterFaceInternal(familyName, style, Path::kBlank, font);
}

bool FontManager::RegisterFaceInternal(const String& familyName, const FontFaceStyle& style, const Path& virtualPath, const HFont& font)
{
	if(familyName.empty() || font == nullptr)
		return false;

	IndexedFamily& indexedFamily = mFamiliesByKey[GetFamilyKey(familyName)];
	if(indexedFamily.IsExplicitlyRegistered)
		return false;

	if(indexedFamily.Family == nullptr)
	{
		FontFamilyCreateInformation createInformation;
		createInformation.Name = familyName;

		indexedFamily.Family = FontFamily::Create(createInformation);
	}

	if(const FontFamilyFace* existingFace = indexedFamily.Family->FindExactFace(style); existingFace != nullptr)
	{
		// Two fonts claiming the same face of the same family cannot both be used, and picking between them would be
		// arbitrary, so the one that was registered first wins. Registering the same font again changes nothing.
		if(existingFace->Font != font)
		{
			B3D_LOG(Warning, LogFont, "Font \"{0}\" provides a face of family \"{1}\" that is already provided by another font. Ignoring the duplicate.", virtualPath, familyName);
		}

		return false;
	}

	// Faces are added to the family in place, so a family already handed out picks the new face up as well.
	return indexedFamily.Family->AddFace(font, style);
}

bool FontManager::RegisterFamily(const HFontFamily& family)
{
	if(family == nullptr || !family.IsLoaded(false) || family->GetFamilyName().empty())
		return false;

	Lock lock(mMutex);

	IndexedFamily& indexedFamily = mFamiliesByKey[GetFamilyKey(family->GetFamilyName())];
	indexedFamily.Family = family;
	indexedFamily.IsExplicitlyRegistered = true;

	return true;
}

void FontManager::UnregisterFamily(const String& name)
{
	Lock lock(mMutex);
	mFamiliesByKey.erase(GetFamilyKey(name));
}

Vector<String> FontManager::GetFamilyNames() const
{
	Lock lock(mMutex);

	Vector<String> names;
	names.reserve(mFamiliesByKey.size());

	for(const auto& entry : mFamiliesByKey)
	{
		if(entry.second.Family != nullptr)
			names.push_back(entry.second.Family->GetFamilyName());
	}

	return names;
}

void FontManager::GetFaceIdentity(const Path& virtualPath, const PackageResourceMetaData& metaData, String& outFamilyName, FontFaceStyle& outStyle)
{
	if(const TShared<FontMetaData> fontMetaData = B3DRTTICast<FontMetaData>(metaData.ResourceMetaData); fontMetaData != nullptr && !fontMetaData->FamilyName.empty())
	{
		outFamilyName = fontMetaData->FamilyName;
		outStyle = fontMetaData->Style;

		return;
	}

	// Fonts imported before face meta-data was recorded carry no identity, leaving the file name as the only thing to go on.
	// This names the family after the file and assumes a regular face, so such a font remains reachable by its file name.
	outFamilyName = virtualPath.GetFilename(false);
	outStyle = FontFaceStyle();
}

String FontManager::GetFamilyKey(const String& name)
{
	String key = name;
	StringUtility::ToLowerCase(key);

	return key;
}

namespace b3d
{
B3D_EXPORT FontManager& GetFontManager()
{
	return FontManager::Instance();
}
} // namespace b3d
