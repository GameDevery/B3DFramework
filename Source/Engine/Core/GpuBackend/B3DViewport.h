//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#pragma once

#include <utility>
#include "B3DPrerequisites.h"
#include "Reflection/B3DIReflectable.h"
#include "CoreObject/B3DCoreObject.h"
#include "Image/B3DColor.h"
#include "Math/B3DArea2.h"
#include "Math/B3DArea2.h"
#include "Script/B3DIScriptExportable.h"
#include "Utility/B3DEvent.h"

namespace b3d
{
	/** @addtogroup Rendering-Internal
	 *  @{
	 */

	/** Flags that determine which portion of the viewport to clear. */
	enum class B3D_SCRIPT_EXPORT(ExportName(ClearFlags)) ClearFlagBits
	{
		Empty,
		Color = 1 << 0,
		Depth = 1 << 1,
		Stencil = 1 << 2
	};

	typedef Flags<ClearFlagBits> ClearFlags;
	B3D_FLAGS_OPERATORS(ClearFlagBits)

	/** Common base type used for both main and render thread variants of Viewport. */
	class B3D_EXPORT ViewportBase
	{
	public:
		virtual ~ViewportBase() = default;

		/** Determines the area that the viewport covers. Coordinates are in normalized [0, 1] range. */
		B3D_SCRIPT_EXPORT(ExportName(Area), Property(Setter))
		void SetArea(const Area2& area);

		/** @copydoc SetArea() */
		B3D_SCRIPT_EXPORT(ExportName(Area), Property(Getter))
		Area2 GetArea() const { return mNormArea; }

		/**	Returns the area of the render target covered by the viewport, in pixels. */
		B3D_SCRIPT_EXPORT(ExportName(PixelArea), Property(Getter))
		Area2I GetPixelArea() const;

		/** Determines which portions of the render target should be cleared before rendering to this viewport is performed. */
		B3D_SCRIPT_EXPORT(ExportName(ClearFlags), Property(Setter))
		void SetClearFlags(ClearFlags flags);

		/** @copydoc SetClearFlags() */
		B3D_SCRIPT_EXPORT(ExportName(ClearFlags), Property(Getter))
		ClearFlags GetClearFlags() const { return mClearFlags; }

		/** Determines the background color drawn where nothing else covers the view. */
		B3D_SCRIPT_EXPORT(ExportName(BackgroundColor), Property(Setter))
		void SetBackgroundColor(const Color& color);

		/** @copydoc SetBackgroundColor() */
		B3D_SCRIPT_EXPORT(ExportName(BackgroundColor), Property(Getter))
		const Color& GetBackgroundColor() const { return mBackgroundColor; }

	protected:
		ViewportBase(float x = 0.0f, float y = 0.0f, float width = 1.0f, float height = 1.0f);

		/**
		 * Marks the render proxy data as dirty. This causes the data from the main thread object be synced with the render thread
		 * version of the object.
		 */
		virtual void MarkRenderProxyDataDirtyInternal() {}

		/** Gets the render target width. */
		virtual u32 GetTargetWidth() const = 0;

		/**	Gets the render target width. */
		virtual u32 GetTargetHeight() const = 0;

		Area2 mNormArea;

		ClearFlags mClearFlags;
		Color mBackgroundColor;

		static const Color kDefaultBackgroundColor;
	};

	/** Templated common base type used for both main and render thread variants of Viewport. */
	template <bool IsRenderProxy>
	class TViewport : public ViewportBase
	{
	public:
		using RenderTargetType = CoreVariantType<RenderTarget, IsRenderProxy>;

		TViewport(TShared<RenderTargetType> target = nullptr, float x = 0.0f, float y = 0.0f, float width = 1.0f, float height = 1.0f)
			: ViewportBase(x, y, width, height), mTarget(std::move(target))
		{}

		virtual ~TViewport() = default;

	protected:
		TShared<RenderTargetType> mTarget;
	};

	/** @} */

	/** @addtogroup Rendering
	 *  @{
	 */

	/**
	 * Viewport determines to which RenderTarget should rendering be performed. It allows you to render to a sub-region of the
	 * target by specifying the area rectangle, and determines which of the target's surfaces get cleared before rendering to
	 * that region.
	 */
	class B3D_EXPORT B3D_SCRIPT_EXPORT(DocumentationGroup(Rendering)) Viewport : public IReflectable, public IScriptExportable, public CoreObject, public TViewport<false>
	{
	public:
		/**	Determines the render target the viewport is associated with. */
		B3D_SCRIPT_EXPORT(ExportName(Target), Property(Setter))
		void SetTarget(const TShared<RenderTarget>& target);

		/** @copydoc setTarget() */
		B3D_SCRIPT_EXPORT(ExportName(Target), Property(Getter))

		TShared<RenderTarget> GetTarget() const { return mTarget; }

		/**
		 * Creates a new viewport.
		 *
		 * @note	Viewport coordinates are normalized in [0, 1] range.
		 */
		B3D_SCRIPT_EXPORT(ExtensionConstructorForType(Viewport))
		static TShared<Viewport> Create(const TShared<RenderTarget>& target, float x = 0.0f, float y = 0.0f, float width = 1.0f, float height = 1.0f);

	protected:
		friend class render::Viewport;
		struct SyncPacket;

		Viewport(const TShared<RenderTarget>& target, float x = 0.0f, float y = 0.0f, float width = 1.0f, float height = 1.0f);

		void MarkRenderProxyDataDirtyInternal() override;
		u32 GetTargetWidth() const override;
		u32 GetTargetHeight() const override;

		RenderProxySyncPacket* CreateRenderProxySyncPacket(FrameAllocator& allocator, u32 flags) override;
		void GetCoreDependencies(Vector<CoreObject*>& dependencies) override;

		TShared<render::RenderProxy> CreateRenderProxy() const override;

		/************************************************************************/
		/* 								RTTI		                     		*/
		/************************************************************************/
		Viewport() = default;

		/** Creates an empty viewport for serialization purposes. */
		static TShared<Viewport> CreateEmpty();

	public:
		friend class ViewportRTTI;
		static RTTIType* GetRttiStatic();
		RTTIType* GetRtti() const override;
	};

	/** @} */

	namespace render
	{
		/** @addtogroup Renderer
		 *  @{
		 */

		/** @copydoc b3d::Viewport */
		class B3D_EXPORT Viewport : public RenderProxy, public TViewport<true>
		{
		public:
			/**	Returns the render target the viewport is associated with. */
			TShared<RenderTarget> GetTarget() const { return mTarget; }

			/**	Sets the render target the viewport will be associated with. */
			void SetTarget(const TShared<RenderTarget>& target) { mTarget = target; }

			/** @copydoc b3d::Viewport::Create() */
			static TShared<Viewport> Create(const TShared<RenderTarget>& target, float x = 0.0f, float y = 0.0f, float width = 1.0f, float height = 1.0f);

		protected:
			friend class b3d::Viewport;

			Viewport(const TShared<RenderTarget>& target, float x = 0.0f, float y = 0.0f, float width = 1.0f, float height = 1.0f);

			u32 GetTargetWidth() const override;
			u32 GetTargetHeight() const override;

			void SyncFromCoreObject(const CoreSyncData& data, FrameAllocator& allocator) override;
		};

		/** @} */
	} // namespace render
} // namespace b3d
