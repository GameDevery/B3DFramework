//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "GUI/B3DGUIClickable.h"

#include "B3DGUIUtility.h"
#include "Image/B3DSpriteTexture.h"
#include "GUI/B3DGUISizeConstraints.h"
#include "GUI/B3DGUIMouseEvent.h"
#include "GUI/B3DGUICommandEvent.h"
#include "StyleSheet/B3DGUIStyleSheet.h"
#include "Utility/B3DTime.h"

using namespace b3d;

GUIClickable::GUIClickable(const String& styleName, const GUIContent& content, const GUISizeConstraints& sizeConstraints, GUIElementOptions options)
	: GUIInteractable(styleName, sizeConstraints, options), mContent(content)
{
	mBackgroundSprite.SetAnimationStartTime(GetTime().GetRealTimeInSeconds());
	mContentSprites.SetAnimationStartTime(GetTime().GetRealTimeInSeconds());
}

void GUIClickable::SetContent(const GUIContent& content)
{
	GUILogicalSize originalSize = mSizeConstraints.CalculateConstrainedOptimalSize(CalculateUnconstrainedOptimalSize());
	mContent = content;

	mContentSprites.SetAnimationStartTime(GetTime().GetRealTimeInSeconds());

	GUILogicalSize newSize = mSizeConstraints.CalculateConstrainedOptimalSize(CalculateUnconstrainedOptimalSize());

	if(originalSize != newSize)
		MarkLayoutAsDirty();
	else
		MarkContentAsDirty();
}

void GUIClickable::SetOnInternal(bool on)
{
	if(on)
		AddStateFlags(GUIElementState::Checked);
	else
		RemoveStateFlags(GUIElementState::Checked);
}

bool GUIClickable::IsOnInternal() const
{
	return mStateFlags.IsSet(GUIElementState::Checked);
}

void GUIClickable::UpdateRenderElements()
{
	mRenderElements.clear();
	GUISpriteHelper::BuildSpriteRenderElements(*this, mBackgroundSprite);
	GUISpriteHelper::BuildSpriteRenderElements(*this, mContent, mContentSprites);

	GUIInteractable::UpdateRenderElements();
}

GUILogicalSize GUIClickable::CalculateUnconstrainedOptimalSize() const
{
	if(mStyleSheetRuleInformation.CurrentStateRuleset == nullptr)
		return GUILogicalSize::kZero;

	const GUIStyleSheetRules& styleSheetRules = mStyleSheetRuleInformation.CurrentStateRuleset->Rules;
	return GUIUtility::CalculateOptimalContentSizeWithPaddingAndBorder(mContent, styleSheetRules, GetSizeConstraints().MaximumWidth);
}

u32 GUIClickable::GetRenderElementDepthRange() const
{
	return 2;
}

bool GUIClickable::DoOnMouseEvent(const GUIMouseEvent& event)
{
	if(mOptionFlags.IsSet(GUIElementOption::IgnorePointerEvents))
		return false;

	if(event.GetType() == GUIMouseEventType::MouseOver)
	{
		if(!IsDisabled())
		{
			AddStateFlags(GUIElementState::Hover);
			OnHover();
		}

		return !mOptionFlags.IsSet(GUIElementOption::ClickThrough);
	}
	else if(event.GetType() == GUIMouseEventType::MouseOut)
	{
		if(!IsDisabled())
		{
			RemoveStateFlags(GUIElementState::Hover | GUIElementState::Active);
			OnOut();
		}

		return !mOptionFlags.IsSet(GUIElementOption::ClickThrough);
	}
	else if(event.GetType() == GUIMouseEventType::MouseDown)
	{
		if(!IsDisabled())
			AddStateFlags(GUIElementState::Active);

		return !mOptionFlags.IsSet(GUIElementOption::ClickThrough);
	}
	else if(event.GetType() == GUIMouseEventType::MouseUp)
	{
		if(!IsDisabled())
		{
			RemoveStateFlags(GUIElementState::Active);
			OnClick();
		}

		return !mOptionFlags.IsSet(GUIElementOption::ClickThrough);
	}
	else if(event.GetType() == GUIMouseEventType::MouseDoubleClick)
	{
		if(!IsDisabled())
			OnDoubleClick();

		return !mOptionFlags.IsSet(GUIElementOption::ClickThrough);
	}

	return false;
}

bool GUIClickable::DoOnCommandEvent(const GUICommandEvent& event)
{
	const bool baseReturnValue = GUIInteractable::DoOnCommandEvent(event);

	if(event.GetType() == GUICommandEventType::FocusGained)
	{
		if(!IsDisabled())
			AddStateFlags(GUIElementState::Focus);

		return true;
	}
	else if(event.GetType() == GUICommandEventType::FocusLost)
	{
		RemoveStateFlags(GUIElementState::Focus);
		return true;
	}

	return baseReturnValue;
}

String GUIClickable::GetTooltip() const
{
	return (String)mContent.Tooltip;
}

void GUIClickable::NotifyStyleChanged()
{
	mBackgroundSprite.SetAnimationStartTime(GetTime().GetRealTimeInSeconds());
}

void GUIClickable::NotifyStateFlagsChanged()
{
	GUIInteractable::NotifyStateFlagsChanged();

	mBackgroundSprite.SetAnimationStartTime(GetTime().GetRealTimeInSeconds());
}
