//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "Text/B3DFontFamily.h"
#include "Resources/B3DResources.h"
#include "RTTI/B3DFontFamilyRTTI.h"

using namespace b3d;

FontFamily::FontFamily(const FontFamilyCreateInformation& createInformation)
	: Resource(false, createInformation.Name), mFamilyName(createInformation.Name)
{
	for(const FontFamilyFace& face : createInformation.Faces)
		AddFace(face.Font, face.Style);
}

void FontFamily::Initialize()
{
	for(const FontFamilyFace& face : mFaces)
		AddResourceDependency(face.Font);

	Resource::Initialize();
}

const HFont& FontFamily::GetFace(const FontFaceStyle& style) const
{
	static const HFont kNoFace;

	// Slant is matched ahead of weight, the same way CSS does it: an upright face at the wrong weight reads better than a
	// slanted face at the right one.
	const FontSlant slantSearchOrder[] =
	{
		style.Slant,
		style.Slant == FontSlant::Italic ? FontSlant::Normal : FontSlant::Italic
	};

	for(FontSlant slant : slantSearchOrder)
	{
		if(const FontFamilyFace* face = FindExactFace({ style.Weight, slant }); face != nullptr)
			return face->Font;

		// A request within the regular-to-medium band takes a single step across the band before searching outwards, as
		// those two weights substitute for each other far better than either substitutes for a heavier or lighter face.
		if(style.Weight == FontWeight::Normal)
		{
			if(const FontFamilyFace* face = FindExactFace({ FontWeight::Medium, slant }); face != nullptr)
				return face->Font;
		}
		else if(style.Weight == FontWeight::Medium)
		{
			if(const FontFamilyFace* face = FindExactFace({ FontWeight::Normal, slant }); face != nullptr)
				return face->Font;
		}

		// Without an exact match the search commits to one direction and exhausts it before doubling back, so a request for
		// a bold face never lands on a light one while a semi-bold face is available.
		const bool searchHeavierFirst = style.Weight > FontWeight::Medium;

		if(const FontFamilyFace* face = FindClosestWeight(slant, style.Weight, searchHeavierFirst); face != nullptr)
			return face->Font;

		if(const FontFamilyFace* face = FindClosestWeight(slant, style.Weight, !searchHeavierFirst); face != nullptr)
			return face->Font;
	}

	return kNoFace;
}

bool FontFamily::AddFace(const HFont& font, const FontFaceStyle& style)
{
	if(font == nullptr)
		return false;

	if(FindExactFace(style) != nullptr)
		return false;

	mFaces.push_back(FontFamilyFace(font, style));
	return true;
}

bool FontFamily::RemoveFace(const FontFaceStyle& style)
{
	const auto found = std::find_if(mFaces.begin(), mFaces.end(),
		[&style](const FontFamilyFace& face) { return face.Style == style; });

	if(found == mFaces.end())
		return false;

	mFaces.erase(found);
	return true;
}

const FontFamilyFace* FontFamily::FindExactFace(const FontFaceStyle& style) const
{
	const auto found = std::find_if(mFaces.begin(), mFaces.end(),
		[&style](const FontFamilyFace& face) { return face.Style == style; });

	return found != mFaces.end() ? &(*found) : nullptr;
}

const FontFamilyFace* FontFamily::FindClosestWeight(FontSlant slant, FontWeight weight, bool searchHeavier) const
{
	const FontFamilyFace* closest = nullptr;

	for(const FontFamilyFace& face : mFaces)
	{
		if(face.Style.Slant != slant)
			continue;

		if(searchHeavier ? face.Style.Weight < weight : face.Style.Weight > weight)
			continue;

		const bool isCloser = closest == nullptr ||
			(searchHeavier ? face.Style.Weight < closest->Style.Weight : face.Style.Weight > closest->Style.Weight);

		if(isCloser)
			closest = &face;
	}

	return closest;
}

bool FontFamily::TryGetFaceIdentity(const HFont& font, String& outFamilyName, FontFaceStyle& outStyle)
{
	if(font == nullptr || !font.IsLoaded(false))
		return false;

	const TShared<FontMetaData> metaData = B3DRTTICast<FontMetaData>(font->GetMetaData());
	if(metaData == nullptr || metaData->FamilyName.empty())
		return false;

	outFamilyName = metaData->FamilyName;
	outStyle = metaData->Style;

	return true;
}

HFontFamily FontFamily::Create(const FontFamilyCreateInformation& createInformation)
{
	TShared<FontFamily> newFontFamily = CreateShared(createInformation);

	return B3DStaticResourceCast<FontFamily>(GetResources().CreateResourceHandle(newFontFamily));
}

HFontFamily FontFamily::CreateFromFace(const HFont& font)
{
	FontFamilyCreateInformation createInformation;

	FontFaceStyle style;
	if(!TryGetFaceIdentity(font, createInformation.Name, style))
	{
		// Fonts imported before face meta-data was recorded report no identity, in which case the font's own name is the
		// best available family name and the face is assumed to be a regular one.
		createInformation.Name = font != nullptr && font.IsLoaded(false) ? font->GetName() : String();
	}

	createInformation.Faces.push_back(FontFamilyFace(font, style));

	return Create(createInformation);
}

TShared<FontFamily> FontFamily::CreateShared(const FontFamilyCreateInformation& createInformation)
{
	TShared<FontFamily> newFontFamily = B3DMakeSharedFromExisting<FontFamily>(new(B3DAllocate<FontFamily>()) FontFamily(createInformation));
	newFontFamily->SetShared(newFontFamily);
	newFontFamily->Initialize();

	return newFontFamily;
}

TShared<FontFamily> FontFamily::CreateEmpty()
{
	TShared<FontFamily> newFontFamily = B3DMakeSharedFromExisting<FontFamily>(new(B3DAllocate<FontFamily>()) FontFamily(FontFamilyCreateInformation()));
	newFontFamily->SetShared(newFontFamily);

	return newFontFamily;
}

RTTIType* FontFamilyFace::GetRttiStatic()
{
	return FontFamilyFaceRTTI::Instance();
}

RTTIType* FontFamilyFace::GetRtti() const
{
	return FontFamilyFace::GetRttiStatic();
}

RTTIType* FontFamily::GetRttiStatic()
{
	return FontFamilyRTTI::Instance();
}

RTTIType* FontFamily::GetRtti() const
{
	return FontFamily::GetRttiStatic();
}
