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
	if(found == mFamiliesByKey.end())
		return nullptr;

	IndexedFamily& indexedFamily = found->second;
	if(indexedFamily.Family != nullptr)
		return indexedFamily.Family;

	// Faces are only loaded once something actually asks for the family, which keeps indexing a font folder free.
	FontFamilyCreateInformation createInformation;
	createInformation.Name = indexedFamily.Name;
	createInformation.Faces.reserve(indexedFamily.Faces.size());

	// TODO - We load all faces at once, when usually the caller only wants one. It would be better to allow loading of faces on demand as well
	for(IndexedFace& indexedFace : indexedFamily.Faces)
	{
		if(indexedFace.Font == nullptr)
			indexedFace.Font = GetResources().Load<Font>(indexedFace.VirtualPath, ResourceLoadOptions(false));

		if(!indexedFace.Font.IsLoaded(false))
		{
			B3D_LOG(Warning, LogFont, "Unable to load font face \"{0}\" of family \"{1}\".", indexedFace.VirtualPath, indexedFamily.Name);
			continue;
		}

		createInformation.Faces.push_back(FontFamilyFace(indexedFace.Font, indexedFace.Style));
	}

	if(createInformation.Faces.empty())
		return nullptr;

	indexedFamily.Family = FontFamily::Create(createInformation);
	return indexedFamily.Family;
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

			RegisterFaceInternal(familyName, style, virtualPath, nullptr);
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
	if(familyName.empty())
		return false;

	IndexedFamily& indexedFamily = mFamiliesByKey[GetFamilyKey(familyName)];
	if(indexedFamily.IsExplicitlyRegistered)
		return false;

	if(indexedFamily.Name.empty())
		indexedFamily.Name = familyName;

	const auto found = std::find_if(indexedFamily.Faces.begin(), indexedFamily.Faces.end(),
		[&style](const IndexedFace& face) { return face.Style == style; });

	if(found != indexedFamily.Faces.end())
	{
		// Two font files claiming the same face of the same family cannot both be used, and picking between them would be
		// arbitrary, so the one that was indexed first wins.
		if(found->VirtualPath != virtualPath)
		{
			B3D_LOG(Warning, LogFont, "Font \"{0}\" provides a face of family \"{1}\" that is already provided by \"{2}\". Ignoring the duplicate.",
				virtualPath, indexedFamily.Name, found->VirtualPath);
		}

		return false;
	}

	indexedFamily.Faces.push_back(IndexedFace{ style, virtualPath, font });

	// Any family already handed out is missing the new face, so it is rebuilt on the next request.
	// TODO - We should be able to just update the family, not create a new resource
	indexedFamily.Family = nullptr;

	return true;
}

bool FontManager::RegisterFamily(const HFontFamily& family)
{
	if(family == nullptr || !family.IsLoaded(false) || family->GetFamilyName().empty())
		return false;

	Lock lock(mMutex);

	IndexedFamily& indexedFamily = mFamiliesByKey[GetFamilyKey(family->GetFamilyName())];
	indexedFamily.Name = family->GetFamilyName();
	indexedFamily.Faces.clear();
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
		names.push_back(entry.second.Name);

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
