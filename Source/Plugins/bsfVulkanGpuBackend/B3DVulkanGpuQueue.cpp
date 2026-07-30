//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "B3DVulkanGpuQueue.h"
#include "B3DVulkanGpuCommandBuffer.h"
#include "B3DVulkanGpuTimelineFence.h"
#include "B3DIVulkanRenderWindowSurface.h"
#include "B3DVulkanHeapBackend.h"
#include "GpuBackend/B3DGpuSubmitThread.h"
#include "B3DVulkanSwapChain.h"
#include "Profiling/B3DRenderStats.h"

using namespace b3d;
using namespace b3d::render;

VulkanBinaryQueueProgress::VulkanBinaryQueueProgress(VulkanGpuDevice& device, GpuQueueId sourceQueueId, u64 value)
	: mDevice(device.GetLogical()), mValue(value)
{
	for(u32 queueTypeIndex = 0; queueTypeIndex < GQT_COUNT; queueTypeIndex++)
	{
		const GpuQueueType queueType = (GpuQueueType)queueTypeIndex;
		const u32 queueCount = device.GetQueueCount(queueType);
		for(u32 queueIndex = 0; queueIndex < queueCount; queueIndex++)
		{
			const GpuQueueId destinationQueueId(queueType, queueIndex);
			if(destinationQueueId.Id == sourceQueueId.Id)
				continue;

			VkSemaphoreCreateInfo semaphoreCreateInformation = {};
			semaphoreCreateInformation.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			const VkResult result = vkCreateSemaphore(mDevice, &semaphoreCreateInformation, gVulkanAllocator, &mSemaphores[destinationQueueId.Id]);
			B3D_ASSERT(result == VK_SUCCESS);
		}
	}
}

VulkanBinaryQueueProgress::~VulkanBinaryQueueProgress()
{
	for(VkSemaphore semaphore : mSemaphores)
	{
		if(semaphore != VK_NULL_HANDLE)
			vkDestroySemaphore(mDevice, semaphore, gVulkanAllocator);
	}
}

void VulkanBinaryQueueProgress::AppendSignalSemaphores(TInlineArray<VkSemaphore, 8>& outSemaphores) const
{
	for(VkSemaphore semaphore : mSemaphores)
	{
		if(semaphore != VK_NULL_HANDLE)
			outSemaphores.Add(semaphore);
	}
}

VulkanGpuQueue::QueueSubmissionEntryInformation::QueueSubmissionEntryInformation(const TShared<VulkanGpuCommandBuffer>& commandBuffer, u32 semaphoreCount, TArrayView<const VulkanQueueSyncPoint> queueSyncPoints, const TShared<VulkanBinaryQueueProgress>& signaledBinaryProgress)
	: CommandBuffer(commandBuffer), SemaphoreCount(semaphoreCount), SignaledBinaryProgress(signaledBinaryProgress)
{
	for(const VulkanQueueSyncPoint& syncPoint : queueSyncPoints)
	{
		if(syncPoint.BinaryProgress != nullptr)
			BinaryProgressDependencies.Add(syncPoint.BinaryProgress);
	}
}

void VulkanGpuQueue::SubmitWorkBuffer::Clear()
{
	SignalSemaphores.Clear();
	SignalValues.Clear();
	WaitSemaphores.Clear();
	WaitValues.Clear();
	WaitStages.Clear();
	CommandBuffers.Clear();
}

VulkanGpuQueue::VulkanGpuQueue(VulkanGpuDevice& device, GpuQueueType type, u32 index, VkQueue vulkanQueue)
	: GpuQueue(device, type, index), mQueue(vulkanQueue)
{
	if(!device.SupportsTimelineSemaphores())
		return;

	VkSemaphoreTypeCreateInfoKHR timelineCreateInformation = {};
	timelineCreateInformation.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR;
	timelineCreateInformation.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR;
	timelineCreateInformation.initialValue = 0;

	VkSemaphoreCreateInfo semaphoreCreateInformation = {};
	semaphoreCreateInformation.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInformation.pNext = &timelineCreateInformation;

	const VkResult result = vkCreateSemaphore(device.GetLogical(), &semaphoreCreateInformation, gVulkanAllocator, &mProgressTimeline);
	B3D_ASSERT(result == VK_SUCCESS);
}

VulkanGpuQueue::~VulkanGpuQueue()
{
	mLastBinaryProgress = nullptr;

	if(mProgressTimeline != VK_NULL_HANDLE)
		vkDestroySemaphore(GetDevice().GetLogical(), mProgressTimeline, gVulkanAllocator);
}

void VulkanGpuQueue::SubmitCommandBuffer(const GpuSubmissionInformation& information)
{
	if (!B3D_ENSURE(information.CommandBuffer))
		return;

	VulkanGpuCommandBuffer& vulkanCommandBuffer = static_cast<VulkanGpuCommandBuffer&>(*information.CommandBuffer);
	if (!B3D_ENSURE(vulkanCommandBuffer.GetQueueType() == mType))
		return;

	if (vulkanCommandBuffer.GetState() == GpuCommandBufferState::Executing)
	{
		B3D_LOG(Error, LogRenderBackend, "Cannot submit a command buffer that's still executing.");
		return;
	}

	if (!B3D_ENSURE(!vulkanCommandBuffer.IsInRenderPass()))
		vulkanCommandBuffer.EndRenderPass();

	if (vulkanCommandBuffer.IsRecording())
		vulkanCommandBuffer.End();

	vulkanCommandBuffer.SetIsSubmitted();
	mGpuDevice.GetSubmitThread().QueueSubmit(information.CommandBuffer, *this, information.SyncMask, information.SignalFences);
}

void VulkanGpuQueue::PresentRenderWindow(const TShared<RenderWindow>& renderWindow, GpuQueueMask syncMask)
{
	if(renderWindow == nullptr)
		return;

	IVulkanRenderWindowSurface* surface = static_cast<IVulkanRenderWindowSurface*>(renderWindow->GetRenderWindowSurface().get());
	if(surface == nullptr)
		return;

	renderWindow->NotifySwapBuffersRequested();
	surface->SwapBuffers(*this, syncMask);

	B3D_INCREMENT_RENDER_STATISTIC(NumPresents);
}

bool VulkanGpuQueue::IsExecuting() const
{
	AssertIfNotSubmitThread();

	if(mLastSubmittedCommandBuffer == nullptr)
		return false;

	return mLastSubmittedCommandBuffer->IsSubmitted() || mLastSubmittedCommandBuffer->IsDone();
}

VkResult VulkanGpuQueue::Present(VulkanSwapChain* swapChain, u32 swapChainImageIndex, TArrayView<VulkanSemaphore*> waitSemaphores)
{
	AssertIfNotSubmitThread();

	SubmitWorkBuffer& workBuffer = AcquireSubmitWorkBuffer();
	RegisterSemaphoresAndGetHandles(waitSemaphores, workBuffer.WaitSemaphores);

	VkSwapchainKHR vkSwapChain = swapChain->GetHandle();

	VkPresentInfoKHR presentInfo;
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &vkSwapChain;
	presentInfo.pImageIndices = &swapChainImageIndex;
	presentInfo.pResults = nullptr;

	// Wait before presenting, if required
	const u32 waitSemaphoreCount = (u32)workBuffer.WaitSemaphores.Size();
	if(waitSemaphoreCount > 0)
	{
		presentInfo.pWaitSemaphores = workBuffer.WaitSemaphores.Data();
		presentInfo.waitSemaphoreCount = waitSemaphoreCount;
	}
	else
	{
		presentInfo.pWaitSemaphores = nullptr;
		presentInfo.waitSemaphoreCount = 0;
	}

	VkResult result = vkQueuePresentKHR(mQueue, &presentInfo);
	B3D_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR);

	mActiveSubmissions.push_back(QueueSubmissionInformation(swapChain, mNextSubmitIndex++, 1));
	mActiveCommandBuffers.push(QueueSubmissionEntryInformation(nullptr, waitSemaphoreCount, {}, nullptr));

	ReleaseAllSubmitWorkBuffers();
	return result;
}

void VulkanGpuQueue::WaitUntilIdle()
{
	mGpuDevice.GetSubmitThread().WaitUntilIdle(*this);
}

VkSubmitInfo VulkanGpuQueue::RegisterSubmissionAndGenerateSubmitInfo(const TShared<VulkanGpuCommandBuffer>& commandBuffer, TArrayView<VulkanSemaphore* const> waitSemaphores, TArrayView<const VulkanQueueSyncPoint> queueSyncPoints, TArrayView<VulkanSemaphore* const> signalSemaphores, TArrayView<const GpuTimelineFenceAndValue> signalFences, const TShared<VulkanBinaryQueueProgress>& signalBinaryProgress, u64 signalTimelineProgressValue)
{
	SubmitWorkBuffer& workBuffer = AcquireSubmitWorkBuffer();

	const u32 managedWaitSemaphoreCount = RegisterSemaphoresAndGetHandles(waitSemaphores, workBuffer.WaitSemaphores);
	for(u32 semaphoreIndex = 0; semaphoreIndex < managedWaitSemaphoreCount; semaphoreIndex++)
	{
		workBuffer.WaitValues.Add(0);
		workBuffer.WaitStages.Add(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}

	bool hasTimelineOperations = false;
	for(const VulkanQueueSyncPoint& syncPoint : queueSyncPoints)
	{
		if(!syncPoint.IsValid())
			continue;

		workBuffer.WaitSemaphores.Add(syncPoint.Semaphore);
		workBuffer.WaitValues.Add(syncPoint.SemaphoreValue);
		workBuffer.WaitStages.Add(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

		hasTimelineOperations |= syncPoint.SemaphoreValue != 0;
	}

	B3D_ASSERT(commandBuffer != nullptr);
	commandBuffer->SetIsSubmitted();

	workBuffer.CommandBuffers.Add(commandBuffer->GetVulkanHandle());
	mActiveCommandBuffers.push(QueueSubmissionEntryInformation(commandBuffer, managedWaitSemaphoreCount, queueSyncPoints, signalBinaryProgress));

	for(VulkanSemaphore* semaphore : signalSemaphores)
	{
		if(!B3D_ENSURE(semaphore != nullptr))
			continue;

		workBuffer.SignalSemaphores.Add(semaphore->GetHandle());
		workBuffer.SignalValues.Add(0);
	}

	for(const GpuTimelineFenceAndValue& entry : signalFences)
	{
		B3D_ASSERT(entry.Fence != nullptr);

		VulkanGpuTimelineFence* vulkanFence = static_cast<VulkanGpuTimelineFence*>(entry.Fence.get());
		const VkSemaphore semaphore = vulkanFence->GetTimelineSemaphore();

		if(semaphore == VK_NULL_HANDLE)
			continue;

		workBuffer.SignalSemaphores.Add(semaphore);
		workBuffer.SignalValues.Add(entry.Value);

		hasTimelineOperations = true;
	}

	if(signalBinaryProgress != nullptr)
	{
		const u32 firstBinaryProgressSemaphore = (u32)workBuffer.SignalSemaphores.Size();
		signalBinaryProgress->AppendSignalSemaphores(workBuffer.SignalSemaphores);
		for(u32 semaphoreIndex = firstBinaryProgressSemaphore; semaphoreIndex < (u32)workBuffer.SignalSemaphores.Size(); semaphoreIndex++)
			workBuffer.SignalValues.Add(0);
	}

	if(signalTimelineProgressValue != 0)
	{
		B3D_ASSERT(mProgressTimeline != VK_NULL_HANDLE);
		workBuffer.SignalSemaphores.Add(mProgressTimeline);
		workBuffer.SignalValues.Add(signalTimelineProgressValue);
		hasTimelineOperations = true;
	}

	B3D_ASSERT(workBuffer.WaitSemaphores.Size() == workBuffer.WaitValues.Size());
	B3D_ASSERT(workBuffer.WaitSemaphores.Size() == workBuffer.WaitStages.Size());
	B3D_ASSERT(workBuffer.SignalSemaphores.Size() == workBuffer.SignalValues.Size());

	const u32 commandBufferCount = (u32)workBuffer.CommandBuffers.Size();
	const u32 waitSemaphoreCount = (u32)workBuffer.WaitSemaphores.Size();
	const u32 totalSignalCount = (u32)workBuffer.SignalSemaphores.Size();

	VkSubmitInfo vkSubmitInfo;
	vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	vkSubmitInfo.pNext = nullptr;
	vkSubmitInfo.commandBufferCount = commandBufferCount;
	vkSubmitInfo.pCommandBuffers = commandBufferCount > 0 ? workBuffer.CommandBuffers.Data() : nullptr;
	vkSubmitInfo.waitSemaphoreCount = waitSemaphoreCount;

	if(waitSemaphoreCount != 0)
	{
		vkSubmitInfo.pWaitSemaphores = workBuffer.WaitSemaphores.Data();
		vkSubmitInfo.pWaitDstStageMask = workBuffer.WaitStages.Data();
	}
	else
	{
		vkSubmitInfo.pWaitSemaphores = nullptr;
		vkSubmitInfo.pWaitDstStageMask = nullptr;
	}

	vkSubmitInfo.signalSemaphoreCount = totalSignalCount;
	vkSubmitInfo.pSignalSemaphores = totalSignalCount > 0 ? workBuffer.SignalSemaphores.Data() : nullptr;

	if(hasTimelineOperations)
	{
		workBuffer.TimelineSubmitInfo = {};
		workBuffer.TimelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR;
		workBuffer.TimelineSubmitInfo.pNext = nullptr;
		workBuffer.TimelineSubmitInfo.waitSemaphoreValueCount = waitSemaphoreCount;
		workBuffer.TimelineSubmitInfo.pWaitSemaphoreValues = waitSemaphoreCount > 0 ?  workBuffer.WaitValues.Data() : nullptr;
		workBuffer.TimelineSubmitInfo.signalSemaphoreValueCount = totalSignalCount;
		workBuffer.TimelineSubmitInfo.pSignalSemaphoreValues = totalSignalCount > 0 ? workBuffer.SignalValues.Data() : nullptr;

		vkSubmitInfo.pNext = &workBuffer.TimelineSubmitInfo;
	}

	return vkSubmitInfo;
}

void VulkanGpuQueue::AppendQueueSyncPoints(GpuQueueMask syncMask, TInlineArray<VulkanQueueSyncPoint, 8>& outSyncPoints) const
{
	AssertIfNotSubmitThread();

	syncMask &= ~GpuQueueMask(GetId());
	if(syncMask.IsEmpty())
		return;

	VulkanGpuDevice& device = GetDevice();
	for(u32 queueTypeIndex = 0; queueTypeIndex < GQT_COUNT; queueTypeIndex++)
	{
		const GpuQueueType queueType = (GpuQueueType)queueTypeIndex;
		const u32 queueCount = device.GetQueueCount(queueType);
		for(u32 queueIndex = 0; queueIndex < queueCount; queueIndex++)
		{
			const GpuQueueId sourceQueueId(queueType, queueIndex);
			if(!syncMask.IsSet(sourceQueueId))
				continue;

			const TShared<VulkanGpuQueue> sourceQueue = std::static_pointer_cast<VulkanGpuQueue>(device.GetQueue(queueType, queueIndex));
			if(!B3D_ENSURE(sourceQueue != nullptr))
				continue;

			const u64 progressValue = sourceQueue->mLastSubmittedProgressValue;
			if(progressValue == 0)
				continue;

			if(sourceQueue->mProgressTimeline != VK_NULL_HANDLE)
			{
				outSyncPoints.Add(VulkanQueueSyncPoint(sourceQueueId, sourceQueue->mProgressTimeline, progressValue, progressValue, nullptr));
				continue;
			}

			if(mLastWaitedBinaryProgressValues[sourceQueueId.Id] >= progressValue)
				continue;

			const TShared<VulkanBinaryQueueProgress>& binaryProgress = sourceQueue->mLastBinaryProgress;
			if(!B3D_ENSURE(binaryProgress != nullptr && binaryProgress->GetValue() == progressValue))
				continue;

			const VkSemaphore semaphore = binaryProgress->GetSemaphore(GetId());
			if(!B3D_ENSURE(semaphore != VK_NULL_HANDLE))
				continue;

			outSyncPoints.Add(VulkanQueueSyncPoint(sourceQueueId, semaphore, 0, progressValue, binaryProgress));
		}
	}
}

VulkanSemaphore* VulkanGpuQueue::SubmitPresentBridge(GpuQueueMask syncMask, TArrayView<VulkanSemaphore* const> waitSemaphores)
{
	AssertIfNotSubmitThread();

	VulkanSemaphore* presentSemaphore = GetDevice().GetResourceManager().Create<VulkanSemaphore>("PresentReady");

	GpuCommandBufferPool& commandBufferPool = GetDevice().GetSubmitThread().GetCommandBufferPool(GetType());
	const TShared<VulkanGpuCommandBuffer> commandBuffer = std::static_pointer_cast<VulkanGpuCommandBuffer>(commandBufferPool.Create(GpuCommandBufferCreateInformation::Create("Present synchronization")));
	commandBuffer->End();

	VulkanGpuCommandBufferSubmitInformation submitInformation;
	submitInformation.PrimaryCommandBuffer = commandBuffer;
	submitInformation.Semaphores.Append(waitSemaphores.begin(), waitSemaphores.end());
	submitInformation.SignalSemaphores.Add(presentSemaphore);

	ExecuteSubmitOnSubmitThread(submitInformation, syncMask, {});
	return presentSemaphore;
}

void VulkanGpuQueue::ExecuteSubmitOnSubmitThread(const VulkanGpuCommandBufferSubmitInformation& submitInformation, GpuQueueMask syncMask, TArrayView<const GpuTimelineFenceAndValue> signalFences)
{
	AssertIfNotSubmitThread();

	if (!B3D_ENSURE(submitInformation.PrimaryCommandBuffer))
		return;

	VulkanGpuDevice& device = static_cast<VulkanGpuDevice&>(mGpuDevice);

	syncMask |= submitInformation.RequiredWaitMask;

	// Same-queue resource dependencies are recorded in the destination transition command buffer
	const GpuQueueMask queueMask(GetId());
	syncMask &= ~queueMask;

	for(const VulkanSourceQueueTransition& sourceQueueTransition : submitInformation.SourceQueueTransitions)
	{
		if(sourceQueueTransition.CommandBuffer == nullptr)
			continue;

		TShared<VulkanGpuQueue> sourceQueue = std::static_pointer_cast<VulkanGpuQueue>(
			device.GetQueue(sourceQueueTransition.QueueId.GetType(), sourceQueueTransition.QueueId.GetIndex()));
		if(!B3D_ENSURE(sourceQueue != nullptr))
			continue;

		syncMask |= sourceQueueTransition.QueueId;

		VulkanGpuCommandBufferSubmitInformation sourceQueueTransitionSubmitInformation;
		sourceQueueTransitionSubmitInformation.PrimaryCommandBuffer = sourceQueueTransition.CommandBuffer;

		sourceQueue->ExecuteSubmitOnSubmitThread(sourceQueueTransitionSubmitInformation, sourceQueueTransition.WaitMask, {});
	}

	B3D_ENSURE(mWaitSemaphoreBuffer.Empty());
	mWaitSemaphoreBuffer.Append(submitInformation.Semaphores.begin(), submitInformation.Semaphores.end());

	TInlineArray<VulkanQueueSyncPoint, 8> queueSyncPoints;
	AppendQueueSyncPoints(syncMask, queueSyncPoints);
	const TInlineArray<VulkanQueueSyncPoint, 8> submittedQueueSyncPoints = queueSyncPoints;

	const u64 progressValue = mNextProgressValue++;
	TShared<VulkanBinaryQueueProgress> binaryProgress;
	u64 timelineProgressValue = 0;
	if(mProgressTimeline != VK_NULL_HANDLE)
		timelineProgressValue = progressValue;
	else
		binaryProgress = B3DMakeShared<VulkanBinaryQueueProgress>(device, GetId(), progressValue);

	B3D_ASSERT(mActiveSubmitWorkBufferCount == 0);

	TInlineArray<VkSubmitInfo, 3> submitInfos;

	if (submitInformation.DestinationQueueTransitionCommandBuffer != nullptr)
	{
		submitInfos.Add(RegisterSubmissionAndGenerateSubmitInfo(submitInformation.DestinationQueueTransitionCommandBuffer, mWaitSemaphoreBuffer, queueSyncPoints, {}, {}, nullptr, 0));
		mWaitSemaphoreBuffer.Clear(); // No need to wait on these again with later submissions
		queueSyncPoints.Clear();
	}

	submitInfos.Add(RegisterSubmissionAndGenerateSubmitInfo(submitInformation.PrimaryCommandBuffer, mWaitSemaphoreBuffer, queueSyncPoints, submitInformation.SignalSemaphores, signalFences, binaryProgress, timelineProgressValue));
	mWaitSemaphoreBuffer.Clear();

	mLastSubmittedCommandBuffer = submitInformation.PrimaryCommandBuffer;
	mActiveSubmissions.push_back(QueueSubmissionInformation(mLastSubmittedCommandBuffer, mNextSubmitIndex++, (u32)submitInfos.Size()));

	VkResult result = vkQueueSubmit(mQueue, (u32)submitInfos.Size(), submitInfos.Data(), mLastSubmittedCommandBuffer->GetFence());

	// A failed queue submit is unrecoverable: the submitted command buffer's fence will never signal, so any wait on
	// it (and any dependent read-back / frame-completion wait) would deadlock forever. The most common cause is
	// VK_ERROR_DEVICE_LOST from a GPU hang/TDR or page fault. Fail fast with a fatal error (which brings the
	// application down) rather than silently hanging.
	if(result != VK_SUCCESS)
	{
		B3D_LOG(Fatal, LogRenderBackend, "vkQueueSubmit failed with VkResult {0}{1}. The GPU device is in an unrecoverable state; aborting.",
			(i32)result, (result == VK_ERROR_DEVICE_LOST) ? " (VK_ERROR_DEVICE_LOST)" : "");

		ReleaseAllSubmitWorkBuffers();
		return;
	}

	mLastSubmittedProgressValue = progressValue;
	mLastBinaryProgress = binaryProgress;
	for(const VulkanQueueSyncPoint& syncPoint : submittedQueueSyncPoints)
	{
		if(syncPoint.BinaryProgress != nullptr)
			mLastWaitedBinaryProgressValues[syncPoint.SourceQueueId.Id] = syncPoint.ProgressValue;
	}

	submitInformation.PrimaryCommandBuffer->NotifyWasSubmitted(GetId());

	ReleaseAllSubmitWorkBuffers();
}

void VulkanGpuQueue::RefreshCompletionState(bool forceWait, bool queueEmpty, u32 lastSubmitIndex)
{
	AssertIfNotSubmitThread();

	u32 lastFinishedSubmission = 0;

	auto it = mActiveSubmissions.begin();
	while(it != mActiveSubmissions.end())
	{
		const TShared<VulkanGpuCommandBuffer> cmdBuffer = it->LastSubmittedCommandBuffer;
		if(cmdBuffer == nullptr)
		{
			++it;
			continue;
		}

		if(lastSubmitIndex != ~0u && it->SubmitIndex > lastSubmitIndex)
			break;

		if(!cmdBuffer->UpdateExecutionStatus(forceWait))
		{
			B3D_ASSERT(!forceWait);
			break; // No chance of any later CBs of being done either
		}

		lastFinishedSubmission = it->SubmitIndex;
		++it;
	}

	// If last submission was a Present() call, it won't be freed until a command buffer after it is done. However on
	// shutdown there might not be a CB following it. So we instead check this special flag and free everything when its
	// true.
	if(queueEmpty)
		lastFinishedSubmission = mNextSubmitIndex - 1;

	WaitGroup waitGroup;

	{
		Lock lock(mMutex);
		it = mActiveSubmissions.begin();
		while(it != mActiveSubmissions.end())
		{
			if(it->SubmitIndex > lastFinishedSubmission)
				break;

			for(u32 commandBufferIndex = 0; commandBufferIndex < it->CommandBufferCount; commandBufferIndex++)
			{
				const QueueSubmissionEntryInformation queueSubmissionInformation = mActiveCommandBuffers.front();
				mActiveCommandBuffers.pop();

				const bool isPresentCall = queueSubmissionInformation.CommandBuffer == nullptr;
				SingleConsumerQueue& messageBackQueue = isPresentCall ? it->PresentOperationSwapChain->GetMessageQueue() : queueSubmissionInformation.CommandBuffer->GetPool().GetMessageQueue();

				TInlineArray<VulkanSemaphore*, 8> semaphoresToRelease;
				for (u32 semaphoreIndex = 0; semaphoreIndex < queueSubmissionInformation.SemaphoreCount; semaphoreIndex++)
				{
					VulkanSemaphore* const semaphore = mActiveSemaphores.front();
					mActiveSemaphores.pop();

					semaphoresToRelease.Add(semaphore);
				}

				messageBackQueue.PostCommand([semaphoresToRelease, queueId = GetId()]()
				{
					for (const auto& semaphore : semaphoresToRelease)
						semaphore->NotifyDone(queueId, GpuAccessFlag::Read | GpuAccessFlag::Write);
				});

				waitGroup.Increment();
				if (isPresentCall)
				{
					messageBackQueue.PostCommand([swapChain = it->PresentOperationSwapChain, waitGroup = forceWait ? &waitGroup : nullptr]
					{
						swapChain->NotifyUnbound();

						if(waitGroup != nullptr)
							waitGroup->NotifyDone();
					}, "CommandBufferCompleteCallback");
				}
				else
				{
					messageBackQueue.PostCommand([commandBuffer = queueSubmissionInformation.CommandBuffer, waitGroup = forceWait ? &waitGroup : nullptr]()
					{
						commandBuffer->mState = GpuCommandBufferState::Done;
						commandBuffer->OnDidComplete();
						commandBuffer->Reset();

						if(waitGroup != nullptr)
							waitGroup->NotifyDone();
					}, "CommandBufferCompleteCallback");
				}

				if(mLastSubmittedCommandBuffer == queueSubmissionInformation.CommandBuffer)
					mLastSubmittedCommandBuffer = nullptr;
			}

			it = mActiveSubmissions.erase(it);
		}
	}

	// Ensure the message back callbacks also trigger in the force wait case
	if(forceWait)
		waitGroup.Wait();
}

u32 VulkanGpuQueue::RegisterSemaphoresAndGetHandles(TArrayView<VulkanSemaphore* const> inSemaphores, TInlineArray<VkSemaphore, 8>& outSemaphores)
{
	AssertIfNotSubmitThread();

	u32 count = 0;
	for(const auto& semaphore : inSemaphores)
	{
		semaphore->NotifyBound();
		semaphore->NotifyUsed(GetId(), GpuAccessFlag::Read | GpuAccessFlag::Write);

		outSemaphores.Add(semaphore->GetHandle());
		count++;
		mActiveSemaphores.push(semaphore);
	}

	return count;
}

VulkanGpuQueue::SubmitWorkBuffer& VulkanGpuQueue::AcquireSubmitWorkBuffer()
{
	AssertIfNotSubmitThread();

	if (mActiveSubmitWorkBufferCount >= mSubmitWorkBufferPool.size())
		mSubmitWorkBufferPool.push_back(B3DMakeUnique<SubmitWorkBuffer>());

	SubmitWorkBuffer& buffer = *mSubmitWorkBufferPool[mActiveSubmitWorkBufferCount++];
	buffer.Clear();
	return buffer;
}

void VulkanGpuQueue::ReleaseAllSubmitWorkBuffers()
{
	AssertIfNotSubmitThread();
	mActiveSubmitWorkBufferCount = 0;
}
