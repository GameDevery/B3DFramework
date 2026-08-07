//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "GUI/B3DGUIContent.h"

using namespace b3d;

const GUILogicalUnit GUIContent::kImageTextSpacing = 3;

const HSpriteImage& GUIContent::GetImage(GUIElementStates state) const
{
	// Interaction states are more specific than the checked state, so they are matched first
	const bool isChecked = state.IsSet(GUIElementState::Checked);

	if(state.IsSet(GUIElementState::Active))
		return isChecked ? Images.ActiveOn : Images.Active;

	if(state.IsSet(GUIElementState::Hover))
		return isChecked ? Images.HoverOn : Images.Hover;

	if(state.IsSet(GUIElementState::Focus))
		return isChecked ? Images.FocusedOn : Images.Focused;

	return isChecked ? Images.NormalOn : Images.Normal;
}
