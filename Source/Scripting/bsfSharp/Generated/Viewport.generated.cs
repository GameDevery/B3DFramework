//************************************ B3D Framework - Copyright 2025 Marko Pintera **************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace b3d
{
	/** @addtogroup Rendering
	 *  @{
	 */

	/// <summary>
	/// Viewport determines to which RenderTarget should rendering be performed. It allows you to render to a sub-region of 
	/// the target by specifying the area rectangle, and determines which of the target&apos;s surfaces get cleared before 
	/// rendering to that region.
	/// </summary>
	[ShowInInspector]
	public partial class Viewport : ScriptObject
	{
		private Viewport(bool __dummy0) { }
		protected Viewport() { }

		/// <summary>Creates a new viewport.</summary>
		public Viewport(RenderTarget target, float x = 0f, float y = 0f, float width = 1f, float height = 1f)
		{
			Internal_Create(this, target, x, y, width, height);
		}

		/// <summary>Determines the render target the viewport is associated with.</summary>
		[ShowInInspector]
		[NativeWrapper]
		public RenderTarget Target
		{
			get { return Internal_GetTarget(mCachedPtr); }
			set { Internal_SetTarget(mCachedPtr, value); }
		}

		/// <summary>Determines the area that the viewport covers. Coordinates are in normalized [0, 1] range.</summary>
		[ShowInInspector]
		[NativeWrapper]
		public TArea2<float,float> Area
		{
			get
			{
				TArea2<float,float> temp;
				Internal_GetArea(mCachedPtr, out temp);
				return temp;
			}
			set { Internal_SetArea(mCachedPtr, ref value); }
		}

		/// <summary>Returns the area of the render target covered by the viewport, in pixels.</summary>
		[NativeWrapper]
		public TArea2<int,int> PixelArea
		{
			get
			{
				TArea2<int,int> temp;
				Internal_GetPixelArea(mCachedPtr, out temp);
				return temp;
			}
		}

		/// <summary>
		/// Determines which portions of the render target should be cleared before rendering to this viewport is performed.
		/// </summary>
		[ShowInInspector]
		[NativeWrapper]
		public ClearFlags ClearFlags
		{
			get { return Internal_GetClearFlags(mCachedPtr); }
			set { Internal_SetClearFlags(mCachedPtr, value); }
		}

		/// <summary>Determines the background color drawn where nothing else covers the view.</summary>
		[ShowInInspector]
		[NativeWrapper]
		public Color BackgroundColor
		{
			get
			{
				Color temp;
				Internal_GetBackgroundColor(mCachedPtr, out temp);
				return temp;
			}
			set { Internal_SetBackgroundColor(mCachedPtr, ref value); }
		}

		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_SetTarget(IntPtr thisPtr, RenderTarget target);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern RenderTarget Internal_GetTarget(IntPtr thisPtr);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_SetArea(IntPtr thisPtr, ref TArea2<float,float> area);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_GetArea(IntPtr thisPtr, out TArea2<float,float> __output);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_GetPixelArea(IntPtr thisPtr, out TArea2<int,int> __output);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_SetClearFlags(IntPtr thisPtr, ClearFlags flags);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern ClearFlags Internal_GetClearFlags(IntPtr thisPtr);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_SetBackgroundColor(IntPtr thisPtr, ref Color color);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_GetBackgroundColor(IntPtr thisPtr, out Color __output);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_Create(Viewport managedInstance, RenderTarget target, float x, float y, float width, float height);
	}

	/** @} */
}
