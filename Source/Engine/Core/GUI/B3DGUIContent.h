//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#pragma once

#include "B3DPrerequisites.h"
#include "B3DGUIUnits.h"

namespace b3d
{
	/** @addtogroup GUI
	 *  @{
	 */

	/**	Possible positions used for positioning content image within a GUI element. */
	enum class B3D_SCRIPT_EXPORT(DocumentationGroup(GUI)) GUIImagePosition
	{
		Left,
		Right
	};

	/**
	 * States a GUI element may be in. Multiple states can be active at the same time (e.g. an element can be hovered and
	 * checked), which is why they are combined into GUIElementStates rather than used individually.
	 */
	enum class B3D_SCRIPT_EXPORT(DocumentationGroup(GUI)) GUIElementState
	{
		Normal = 0, /**< No state is active, meaning the element is not being interacted with. */
		Hover = 1 << 0, /**< Pointer is hovering over the element. */
		Active = 1 << 1, /**< Element is being pressed. */
		Focus = 1 << 2, /**< Element has keyboard/gamepad input focus. */
		Disabled = 1 << 3, /**< Element is disabled and does not react to input. */
		Checked = 1 << 4, /**< Element is toggled on. */
	};

	using GUIElementStates = Flags<GUIElementState>;
	B3D_FLAGS_OPERATORS(GUIElementState)

	/** States that only describe an interaction that is in progress, and that therefore cannot apply to a disabled element. */
	constexpr GUIElementState kGUIInteractionStates = (GUIElementState)((u32)GUIElementState::Hover | (u32)GUIElementState::Active | (u32)GUIElementState::Focus);

	/**	Contains separate GUI content images for every possible GUI element state. */
	struct B3D_EXPORT B3D_SCRIPT_EXPORT(ExportAsStruct(true), DocumentationGroup(GUI)) GUIContentImages
	{
		GUIContentImages() = default;

		GUIContentImages(const HSpriteImage& image)
			: Normal(image), Hover(image), Active(image), Focused(image), NormalOn(image), HoverOn(image), ActiveOn(image), FocusedOn(image)
		{}

		B3D_NO_RREF HSpriteImage Normal;
		B3D_NO_RREF HSpriteImage Hover;
		B3D_NO_RREF HSpriteImage Active;
		B3D_NO_RREF HSpriteImage Focused;
		B3D_NO_RREF HSpriteImage NormalOn;
		B3D_NO_RREF HSpriteImage HoverOn;
		B3D_NO_RREF HSpriteImage ActiveOn;
		B3D_NO_RREF HSpriteImage FocusedOn;
	};

	/**
	 * Holds data used for displaying content in a GUIElement. Content can consist of a string, image, a tooltip or none
	 * of those.
	 */
	class B3D_EXPORT B3D_SCRIPT_EXPORT(ExportAsStruct(true), DocumentationGroup(GUI)) GUIContent
	{
	public:
		/**	Constructs an empty content. */
		GUIContent() = default;

		/**	Constructs content with just a string. */
		GUIContent(const HString& text)
			: Text(text)
		{}

		/**	Constructs content with a string and a tooltip. */
		GUIContent(const HString& text, const HString& tooltip)
			: Text(text), Tooltip(tooltip)
		{}

		/**	Constructs content with just an image. */
		GUIContent(const GUIContentImages& image)
			: Images(image)
		{}

		/**	Constructs content with an image and a tooltip. */
		GUIContent(const GUIContentImages& image, const HString& tooltip)
			: Images(image), Tooltip(tooltip)
		{}

		/**	Constructs content with a string and an image. */
		GUIContent(const HString& text, const GUIContentImages& image)
			: Text(text), Images(image)
		{}

		/**	Constructs content with a string, an image and a tooltip. */
		GUIContent(const HString& text, const GUIContentImages& image, const HString& tooltip)
			: Text(text), Images(image), Tooltip(tooltip)
		{}

		/**	Returns image content (if any) for the provided combination of element states. */
		B3D_NO_RREF const HSpriteImage& GetImage(GUIElementStates state = GUIElementState::Normal) const;

		/**	Determines the spacing between text and image content in pixels. */
		static const GUILogicalUnit kImageTextSpacing;

		HString Text;
		GUIContentImages Images;
		HString Tooltip;
	};

	/** @} */
} // namespace b3d
