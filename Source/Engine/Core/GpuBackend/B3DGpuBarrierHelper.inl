//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "GpuBackend/B3DGpuBarrierHelper.h"
#include "GpuBackend/B3DGpuBackendUtility.h"

#include <algorithm>

namespace b3d::render
{
	template<class TDerived>
	TGpuBarrierHelper<TDerived>::TGpuBarrierHelper(TGpuResourceTracker<TDerived>* resourceTracker)
		: mResourceTracker(resourceTracker)
	{ }

	template<class TDerived>
	void TGpuBarrierHelper<TDerived>::AddBufferBarrier(IGpuBufferResource* buffer, GpuResourceUseFlags sourceUsage, GpuAccessFlags sourceAccess, GpuResourceUseFlags destinationUsage, GpuAccessFlags destinationAccess)
	{
		if(buffer == nullptr)
			return;

		const GpuStageFlags sourceAccessStageFlags = GpuBackendUtility::GetStageFlags(sourceUsage);
		const GpuStageFlags destinationAccessStageFlags = GpuBackendUtility::GetStageFlags(destinationUsage);

		AddBufferBarrier(buffer, GpuBarrierScope(sourceAccessStageFlags, sourceAccess, destinationAccessStageFlags, destinationAccess));
	}

	template<class TDerived>
	void TGpuBarrierHelper<TDerived>::AddBufferBarrier(IGpuBufferResource* buffer, GpuResourceUseFlags destinationUsage, GpuAccessFlags destinationAccess)
	{
		if(buffer == nullptr)
			return;

		const GpuBufferTrackingState* bufferTrackingState = mResourceTracker->FindBufferTrackingState(buffer);
		if(bufferTrackingState == nullptr)
			return;

		AddBufferBarrier(buffer, *bufferTrackingState, destinationUsage, destinationAccess);
	}

	template<class TDerived>
	void TGpuBarrierHelper<TDerived>::AddBufferBarrier(IGpuBufferResource* buffer, const GpuBufferTrackingState& bufferTrackingState, GpuResourceUseFlags destinationUsage, GpuAccessFlags destinationAccess)
	{
		if(buffer == nullptr)
			return;

		const GpuStageFlags destinationAccessStageFlags = GpuBackendUtility::GetStageFlags(destinationUsage);
		const GpuBarrierScope requiredBarrier = bufferTrackingState.HazardState->GetRequiredBarrier(destinationAccessStageFlags, destinationAccess);
		if(!requiredBarrier.IsValid())
			return;

		AddBufferBarrier(buffer, requiredBarrier);
	}

	template<class TDerived>
	void TGpuBarrierHelper<TDerived>::AddBufferBarrier(IGpuBufferResource* buffer, const GpuBarrierScope& barrier)
	{
		if(buffer == nullptr)
			return;

		static_cast<TDerived*>(this)->RecordBufferBarrier(buffer, barrier);

		BarrierTrackingInfo trackingInfo;
		trackingInfo.Buffer = buffer;
		trackingInfo.Barrier = barrier;
		mBarrierTracking.Add(trackingInfo);
	}

	template<class TDerived>
	void TGpuBarrierHelper<TDerived>::AddImageBarrier(IGpuImageResource* image, const GpuTextureSubresourceRange& subresourceRange, GpuResourceUseFlags sourceUsage, GpuAccessFlags sourceAccessFlags, GpuResourceUseFlags destinationUsage, GpuAccessFlags destinationAccessFlags, GpuImageLayout oldLayout, GpuImageLayout newLayout)
	{
		const GpuStageFlags sourceAccessStageFlags = GpuBackendUtility::GetStageFlags(sourceUsage);
		const GpuStageFlags destinationAccessStageFlags = GpuBackendUtility::GetStageFlags(destinationUsage);

		AddSubresourceBarrier(image, subresourceRange, GpuBarrierScope(sourceAccessStageFlags, sourceAccessFlags, destinationAccessStageFlags, destinationAccessFlags), oldLayout, newLayout);
	}

	template<class TDerived>
	void TGpuBarrierHelper<TDerived>::AddImageBarrier(IGpuImageResource* image, const GpuTextureSubresourceRange& subresourceRange, GpuResourceUseFlags destinationUsage, GpuAccessFlags destinationAccess, GpuImageLayout newLayout)
	{
		if(image == nullptr)
			return;

		const GpuImageTrackingState* imageTrackingState = mResourceTracker->FindImageTrackingState(image);
		if(imageTrackingState == nullptr)
			return;

		// The provided range may straddle several tracked subresource blocks; subdivide so each fully-overlapping block
		// is barriered with its own current layout.
		struct CallbackParameters
		{
			TGpuBarrierHelper* BarrierHelper;
			TGpuResourceTracker<TDerived>* ResourceTracker;
			IGpuImageResource* Image;
			GpuResourceUseFlags DestinationUsage;
			GpuAccessFlags DestinationAccess;
			GpuImageLayout NewLayout;
		};

		CallbackParameters callbackParameters { this, mResourceTracker, image, destinationUsage, destinationAccess, newLayout };
		mResourceTracker->IterateAndCreateOverlappingImageSubresourceTrackingState(image, subresourceRange, [](u32 globalSubresourceIndex, void* userData)
		{
			CallbackParameters* const callbackParameters = static_cast<CallbackParameters*>(userData);

			TGpuResourceTracker<TDerived>& resourceTracker = *callbackParameters->ResourceTracker;
			const GpuImageSubresourceTrackingState& subresourceTrackingState = resourceTracker.GetSubresourceTrackingStateAtIndex(globalSubresourceIndex);

			TGpuBarrierHelper& barrierHelper = *callbackParameters->BarrierHelper;
			barrierHelper.AddSubresourceBarrier(callbackParameters->Image, subresourceTrackingState, callbackParameters->DestinationUsage, callbackParameters->DestinationAccess, callbackParameters->NewLayout);
		}, &callbackParameters);
	}

	template<class TDerived>
	void TGpuBarrierHelper<TDerived>::AddSubresourceBarrier(IGpuImageResource* image, const GpuImageSubresourceTrackingState& subresourceTrackingState, GpuResourceUseFlags destinationUsage, GpuAccessFlags destinationAccess, GpuImageLayout newLayout)
	{
		if(image == nullptr)
			return;

		// No layout transition if destination layout is undefined
		if(newLayout == GpuImageLayout::Undefined)
			newLayout = subresourceTrackingState.CurrentLayout;

		const GpuStageFlags destinationAccessStageFlags = GpuBackendUtility::GetStageFlags(destinationUsage);
		const bool needsLayoutTransition = subresourceTrackingState.CurrentLayout != newLayout;

		// A layout transition is potentially a write operation, so it must be ordered after both earlier reads and
		// writes even when the upcoming resource access itself is read-only.
		GpuAccessFlags hazardAccess = destinationAccess;
		if(needsLayoutTransition)
			hazardAccess |= GpuAccessFlag::Write;

		const GpuBarrierScope requiredBarrier =
			subresourceTrackingState.HazardState->GetRequiredBarrier(destinationAccessStageFlags, hazardAccess);
		const bool hasSourceDependency = requiredBarrier.IsValid();
		if(!hasSourceDependency && !needsLayoutTransition)
			return;

		GpuBarrierScope barrier = requiredBarrier;
		if(needsLayoutTransition)
		{
			// The synthetic write above is only used to find operations that must precede the layout transition. The
			// native destination scope describes the real access that will consume the image in its new layout.
			barrier.DestinationStages = destinationAccessStageFlags;
			barrier.DestinationAccess = destinationAccess;
		}

		AddSubresourceBarrier(image, subresourceTrackingState.Range, barrier, subresourceTrackingState.CurrentLayout, newLayout);
	}

	template<class TDerived>
	void TGpuBarrierHelper<TDerived>::AddSubresourceBarrier(IGpuImageResource* image, const GpuTextureSubresourceRange& subresourceRange, const GpuBarrierScope& barrier, GpuImageLayout oldLayout, GpuImageLayout newLayout)
	{
		if(image == nullptr)
			return;

		// Accumulate the native barrier. The backend may reconcile oldLayout from an already-merged barrier (e.g. Vulkan),
		// in which case the layout-tracking bookkeeping below must observe the reconciled value.
		static_cast<TDerived*>(this)->RecordSubresourceBarrier(image, subresourceRange, barrier, oldLayout, newLayout);

		if(oldLayout != newLayout)
		{
			auto foundTracking = std::find_if(mImageLayoutTracking.begin(), mImageLayoutTracking.end(), [image, &subresourceRange](const LayoutTrackingInfo& layoutTrackingInfo)
			{
				return layoutTrackingInfo.Image == image && GpuBackendUtility::RangeEquals(layoutTrackingInfo.SubresourceRange, subresourceRange);
			});

			if(foundTracking == mImageLayoutTracking.end())
			{
				LayoutTrackingInfo layoutTrackingInfo;
				layoutTrackingInfo.Image = image;
				layoutTrackingInfo.SubresourceRange = subresourceRange;
				layoutTrackingInfo.OldLayout = oldLayout;
				layoutTrackingInfo.NewLayout = newLayout;
				mImageLayoutTracking.Add(layoutTrackingInfo);
			}
			else
			{
				B3D_ASSERT(foundTracking->OldLayout == oldLayout);
				foundTracking->NewLayout = newLayout;
			}
		}

		BarrierTrackingInfo barrierTrackingInfo;
		barrierTrackingInfo.Image = image;
		barrierTrackingInfo.ImageSubresourceRange = subresourceRange;
		barrierTrackingInfo.Barrier = barrier;
		mBarrierTracking.Add(barrierTrackingInfo);
	}

	template<class TDerived>
	void TGpuBarrierHelper<TDerived>::ApplyPostBarrierTracking()
	{
		// Update layout for all image barriers
		for(const auto& trackingInfo : mImageLayoutTracking)
		{
			if(trackingInfo.Image == nullptr)
				continue;

			mResourceTracker->UpdateImageLayoutTrackingAfterBarrier(trackingInfo.Image, trackingInfo.SubresourceRange, trackingInfo.OldLayout, trackingInfo.NewLayout);
		}

		// Update hazard summaries for all barriers
		for(const auto& trackingInfo : mBarrierTracking)
		{
			if(trackingInfo.Buffer != nullptr)
				mResourceTracker->UpdateHazardStateAfterBarrier(trackingInfo.Buffer, trackingInfo.Barrier);
			else if(trackingInfo.Image != nullptr)
				mResourceTracker->UpdateHazardStateAfterBarrier(trackingInfo.Image, trackingInfo.ImageSubresourceRange, trackingInfo.Barrier);
		}
	}

	template<class TDerived>
	void TGpuBarrierHelper<TDerived>::Clear()
	{
		mImageLayoutTracking.Clear();
		mBarrierTracking.Clear();
	}
}
