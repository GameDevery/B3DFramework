//********************************* B3D Framework - Copyright 2018-2019 Marko Pintera ************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
using System.Runtime.CompilerServices;

namespace b3d
{
    /** @addtogroup GUI_Engine
     *  @{
     */

    public partial struct GUIContent
    {
        /// <summary>
        /// Returns image content (if any).
        /// </summary>
        public SpriteImage GetImage(GUIElementState state = GUIElementState.Normal)
        {
            // Interaction states are more specific than the checked state, so they are matched first
            bool isChecked = (state & GUIElementState.Checked) != 0;

            if ((state & GUIElementState.Active) != 0)
                return isChecked ? Images.ActiveOn : Images.Active;

            if ((state & GUIElementState.Hover) != 0)
                return isChecked ? Images.HoverOn : Images.Hover;

            if ((state & GUIElementState.Focus) != 0)
                return isChecked ? Images.FocusedOn : Images.Focused;

            return isChecked ? Images.NormalOn : Images.Normal;
        }

        /// <summary>
        /// Implicitly converts a localized string into a GUI content containing only text.
        /// </summary>
        /// <param name="text">Localized string to initialize the GUI content with.</param>
        /// <returns>GUI content containing only a string.</returns>
        public static implicit operator GUIContent(LocString text)
        {
            return new GUIContent(text);
        }

        /// <summary>
        /// Implicitly converts a string into a GUI content containing only text.
        /// </summary>
        /// <param name="text">String to initialize the GUI content with.</param>
        /// <returns>GUI content containing only a string.</returns>
        public static implicit operator GUIContent(string text)
        {
            return new GUIContent(new LocString(text));
        }
    }

    public partial struct GUIContentImages
    {
        /// <summary>
        /// Creates a new object where content images for on and off states are different.
        /// </summary>
        /// <param name="imageOff">Image to assign to all off states.</param>
        /// <param name="imageOn">Image to assign to all on states.</param>
        public GUIContentImages(SpriteImage imageOff, SpriteImage imageOn)
        {
            Normal = imageOff;
            Hover = imageOff;
            Active = imageOff;
            Focused = imageOff;
            NormalOn = imageOn;
            HoverOn = imageOn;
            ActiveOn = imageOn;
            FocusedOn = imageOn;
        }

        /// <summary>
        /// Implicitly converts a sprite texture into a GUI content images object.
        /// </summary>
        /// <param name="image">Image to instantiate the GUI content images with.</param>
        /// <returns>GUI content images with all states set to the provided image.</returns>
        public static implicit operator GUIContentImages(SpriteImage image)
        {
            return new GUIContentImages(image);
        }
    }

    /** @} */
}
