//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "Text/B3DFontFamily.h"
#include "Resources/B3DResources.h"
#include "RTTI/B3DFontFamilyRTTI.h"

using namespace b3d;

/** Number of distinct weights, used to separate faces that substitute equally well but in a less preferred direction. */
constexpr u32 kWeightBandSize = (u32)kMaximumFontWeight - (u32)kMinimumFontWeight + 1;

/** Penalty of a face in the wrong slant. Exceeds every weight penalty, so slant is always matched ahead of weight. */
constexpr u32 kSlantMismatchPenalty = 3 * kWeightBandSize;

/**
 * Returns how poorly a face of weight @p available substitutes for a request for weight @p requested, where zero is an
 * exact match and lower is better.
 *
 * Faces are ordered the same way CSS orders them: the search commits to one direction and exhausts it before doubling
 * back, so a request for a bold face never lands on a light one while a semi-bold face is available. Directions searched
 * later are pushed into a higher band, which keeps every face in them behind every face in the direction before it.
 */
static u32 GetWeightPenalty(FontWeight requested, FontWeight available)
{
	const u32 requestedWeight = (u32)requested;
	const u32 availableWeight = (u32)available;
	const u32 distance = requestedWeight > availableWeight ? requestedWeight - availableWeight : availableWeight - requestedWeight;

	u32 band;
	if(requested >= FontWeight::Normal && requested <= FontWeight::Medium)
	{
		// A request within the regular-to-medium band steps across the band before searching outwards, as those two weights
		// substitute for each other far better than either substitutes for a face outside the band.
		if(availableWeight >= requestedWeight && available <= FontWeight::Medium)
			band = 0;
		else if(availableWeight < requestedWeight)
			band = 1;
		else
			band = 2;
	}
	else
	{
		// A light face is best replaced by a lighter one and a heavy face by a heavier one, so the direction searched first
		// is the one leading away from the regular-to-medium band.
		const bool preferHeavier = requested > FontWeight::Medium;
		band = (availableWeight >= requestedWeight) == preferHeavier ? 0 : 1;
	}

	return band * kWeightBandSize + distance;
}

/** Ensures the font providing the face is loaded. Returns false if it is missing or could not be loaded. */
static bool TryLoadFace(FontFamilyFace& face)
{
	if(face.Font.IsLoaded(false))
		return true;

	if(face.Font == nullptr)
		return false;

	face.Font = GetResources().Load<Font>(face.Font.GetId(), ResourceLoadOptions(false));
	return face.Font.IsLoaded(false);
}

FontFamily::FontFamily(const FontFamilyCreateInformation& createInformation)
	: Resource(false, createInformation.Name), mFamilyName(createInformation.Name)
{
	for(const FontFamilyFace& face : createInformation.Faces)
		AddFace(face.Font, face.Style);
}

const HFont& FontFamily::GetFace(const FontFaceStyle& style)
{
	static const HFont kNoFace;

	// A face that cannot be loaded is dropped and the next best match tried, as it would fail on every later request too.
	while(FontFamilyFace* face = FindBestFace(style))
	{
		if(TryLoadFace(*face))
			return face->Font;

		const FontFaceStyle faceStyle = face->Style;
		B3D_LOG(Warning, LogFont, "Unable to load the weight {0} face of font family \"{1}\". Dropping it from the family.", (u32)faceStyle.Weight, mFamilyName);

		RemoveFace(faceStyle);
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
	AddResourceDependency(font);

	return true;
}

bool FontFamily::RemoveFace(const FontFaceStyle& style)
{
	const auto found = std::find_if(mFaces.begin(), mFaces.end(),
		[&style](const FontFamilyFace& face) { return face.Style == style; });

	if(found == mFaces.end())
		return false;

	RemoveResourceDependency(found->Font);
	mFaces.erase(found);

	return true;
}

const FontFamilyFace* FontFamily::FindExactFace(const FontFaceStyle& style) const
{
	const auto found = std::find_if(mFaces.begin(), mFaces.end(),
		[&style](const FontFamilyFace& face) { return face.Style == style; });

	return found != mFaces.end() ? &(*found) : nullptr;
}

FontFamilyFace* FontFamily::FindBestFace(const FontFaceStyle& style)
{
	FontFamilyFace* bestFace = nullptr;
	u32 bestPenalty = 0;

	for(FontFamilyFace& face : mFaces)
	{
		// Slant is matched ahead of weight, the same way CSS does it: an upright face at the wrong weight reads better than
		// a slanted face at the right one.
		const u32 slantPenalty = face.Style.Slant != style.Slant ? kSlantMismatchPenalty : 0;
		const u32 penalty = slantPenalty + GetWeightPenalty(style.Weight, face.Style.Weight);

		if(bestFace == nullptr || penalty < bestPenalty)
		{
			bestFace = &face;
			bestPenalty = penalty;
		}
	}

	return bestFace;
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
