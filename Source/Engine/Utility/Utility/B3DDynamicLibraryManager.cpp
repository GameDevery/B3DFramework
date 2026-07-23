//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "Utility/B3DDynamicLibraryManager.h"
#include "Utility/B3DDynamicLibrary.h"

namespace b3d
{
static bool operator<(const TUnique<DynamicLibrary>& lhs, const String& rhs)
{
	return lhs->GetName() < rhs;
}

static bool operator<(const String& lhs, const TUnique<DynamicLibrary>& rhs)
{
	return lhs < rhs->GetName();
}

static bool operator<(const TUnique<DynamicLibrary>& lhs, const TUnique<DynamicLibrary>& rhs)
{
	return lhs->GetName() < rhs->GetName();
}
} // namespace b3d

using namespace b3d;

DynamicLibrary* DynamicLibraryManager::Load(String filename)
{
	// Normalize the caller-supplied name to the platform's prefixed/extension form so lookups key consistently.
	filename = DynamicLibrary::EnsureFileName(filename);

	const auto& iterFind = mLoadedLibraries.lower_bound(filename);
	if(iterFind != mLoadedLibraries.end() && (*iterFind)->GetName() == filename)
	{
		return iterFind->get();
	}
	else
	{
		DynamicLibrary* newLib = B3DNew<DynamicLibrary>(std::move(filename));
		mLoadedLibraries.emplace_hint(iterFind, newLib);

		return newLib;
	}
}

void DynamicLibraryManager::Unload(DynamicLibrary* lib)
{
	const auto& iterFind = mLoadedLibraries.find(lib->GetName());
	if(iterFind != mLoadedLibraries.end())
		mLoadedLibraries.erase(iterFind);
	else
		B3DDelete(lib);
}

namespace b3d
{
DynamicLibraryManager& GetDynamicLibraryManager()
{
	return DynamicLibraryManager::Instance();
}
} // namespace b3d
