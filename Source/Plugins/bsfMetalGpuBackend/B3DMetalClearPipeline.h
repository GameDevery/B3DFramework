//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#pragma once

#include "B3DMetalPrerequisites.h"
#include "Utility/B3DUtil.h"

namespace b3d
{
	namespace render
	{
		class MetalGpuDevice;

		/** @addtogroup MetalGpuBackend
		 *  @{
		 */

		/**
		 * Fragment-stage buffer slot the clear shader reads its parameters from.
		 *
		 * Parameter-set argument buffers occupy the low fragment-stage slots, so the clear parameters
		 * live at the top of the table where nothing else binds. The clear pipeline declares no vertex
		 * inputs and no argument buffers, so this is the only slot it touches.
		 */
		constexpr u32 kMetalClearParametersBufferSlot = 30;

		/**
		 * Owns the backend's internal clear pipeline: the shader library plus the pipeline and
		 * depth-stencil state caches used by @c MetalGpuCommandBuffer::ClearViewport.
		 *
		 * Metal's only native clear is the render pass' @c MTLLoadActionClear, which always covers the
		 * whole attachment. Clearing a sub-rect (the engine's viewport clear, used for shadow-map atlas
		 * slots) therefore has to be a draw: a single oversized triangle covering NDC, rasterized
		 * through the current viewport and scissor so only the requested region is touched. Color comes
		 * from the fragment shader's return value, depth from its @c [[depth(any)]] output, and stencil
		 * from the depth-stencil state's replace-on-pass operation against the encoder's reference value.
		 *
		 * One instance is owned by the device and shared by every command buffer, so the caches are
		 * guarded by an internal mutex. Both are keyed only by state that varies across render passes,
		 * and in practice hold a handful of entries.
		 */
		class MetalClearPipeline
		{
		public:
			/** Parameters the clear shader reads; must match the @c B3DClearParameters struct in the MSL source. */
			struct Parameters
			{
				/**
				 * Color written to each attachment, indexed by attachment index. Clear colors are a per-texture
				 * property, so attachments cleared by the same command can carry different values. Entries for
				 * attachments the clear does not write are ignored - Key::ColorWriteMask masks them off.
				 */
				float Color[B3D_MAXIMUM_RENDER_TARGET_COUNT][4] = {};
				float Depth = 0.0f;
				float Padding[3] = { 0.0f, 0.0f, 0.0f };
			};

			/**
			 * Identifies one compiled clear pipeline. Metal bakes the attachment layout into the
			 * pipeline object, and which attachments the clear may write is expressed through per-
			 * attachment write masks, so both participate in the key.
			 */
			struct Key
			{
				u16 ColorFormats[B3D_MAXIMUM_RENDER_TARGET_COUNT] = {}; /**< MTLPixelFormat per attachment, 0 when absent. */
				u32 DepthFormat = 0; /**< MTLPixelFormat of the depth attachment, or 0 if none. */
				u32 StencilFormat = 0; /**< MTLPixelFormat of the stencil attachment, or 0 if none. */
				u16 SampleCount = 1;
				/** Bit per color attachment: set when the clear writes it, clear when it must be preserved. */
				u8 ColorWriteMask = 0;
				/** True when the fragment shader outputs depth; requires a depth attachment to be present. */
				bool WritesDepth = false;

				bool operator==(const Key& rhs) const
				{
					for (u32 attachmentIndex = 0; attachmentIndex < B3D_MAXIMUM_RENDER_TARGET_COUNT; attachmentIndex++)
					{
						if (ColorFormats[attachmentIndex] != rhs.ColorFormats[attachmentIndex])
							return false;
					}
					return DepthFormat == rhs.DepthFormat
						&& StencilFormat == rhs.StencilFormat
						&& SampleCount == rhs.SampleCount
						&& ColorWriteMask == rhs.ColorWriteMask
						&& WritesDepth == rhs.WritesDepth;
				}
			};

			struct KeyHash
			{
				size_t operator()(const Key& key) const
				{
					size_t hash = 0;
					for (u32 attachmentIndex = 0; attachmentIndex < B3D_MAXIMUM_RENDER_TARGET_COUNT; attachmentIndex++)
						B3DCombineHash(hash, key.ColorFormats[attachmentIndex]);
					B3DCombineHash(hash, key.DepthFormat);
					B3DCombineHash(hash, key.StencilFormat);
					B3DCombineHash(hash, key.SampleCount);
					B3DCombineHash(hash, key.ColorWriteMask);
					B3DCombineHash(hash, key.WritesDepth);
					return hash;
				}
			};

			explicit MetalClearPipeline(MetalGpuDevice& gpuDevice);
			~MetalClearPipeline();

#ifdef __OBJC__
			/**
			 * Returns a cached (or freshly compiled) pipeline state for @p key, or nil when the shader
			 * library is unavailable or the pipeline failed to compile. Failures are cached as nil so a
			 * pipeline that cannot be built is only reported once.
			 */
			id<MTLRenderPipelineState> GetOrCreatePipelineState(const Key& key);

			/**
			 * Returns a cached (or freshly created) depth-stencil state that writes depth and/or stencil
			 * unconditionally. Stencil writes replace with the encoder's reference value, so the caller
			 * must set it to the requested clear value before drawing.
			 */
			id<MTLDepthStencilState> GetOrCreateDepthStencilState(bool writeDepth, bool writeStencil);
#endif

		private:
#ifdef __OBJC__
			/**
			 * Compiles the clear shader library on first use. Returns false when compilation failed;
			 * the failure is latched so the (expensive) compile is not retried on every clear.
			 */
			bool EnsureLibrary();
#endif

			MetalGpuDevice& mGpuDevice;

			/** Pimpl holding the Obj-C library / cache state so plain C++ translation units can include this header. */
			struct Impl;
			TUnique<Impl> mImpl;
		};

		/** @} */
	} // namespace render
} // namespace b3d
