//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "B3DVulkanGpuQueue.h"
#include "B3DVulkanGpuCommandBuffer.h"
#include "B3DVulkanGpuTimelineFence.h"
#include "B3DIVulkanRenderWindowSurface.h"
#include "GpuBackend/B3DGpuSubmitThread.h"
#include "B3DVulkanSwapChain.h"
#include "Profiling/B3DRenderStats.h"

using namespace b3d;
using namespace b3d::render;

void VulkanGpuQueue::SubmitWorkBuffer::AddWait(VkSemaphore semaphore, u64 value, VkPipelineStageFlags stages)
{
	WaitSemaphores.Add(semaphore);
	WaitValues.Add(value);
	WaitStages.Add(stages);
}

void VulkanGpuQueue::SubmitWorkBuffer::AddSignal(VkSemaphore semaphore, u64 value)
{
	SignalSemaphores.Add(semaphore);
	SignalValues.Add(value);
}

void VulkanGpuQueue::SubmitWorkBuffer::Clear()
{
	WaitSemaphores.Clear();
	WaitValues.Clear();
	WaitStages.Clear();
	SignalSemaphores.Clear();
	SignalValues.Clear();
	CommandBuffers.Clear();
}

VulkanGpuQueue::VulkanGpuQueue(VulkanGpuDevice& device, GpuQueueType type, u32 index, VkQueue vulkanQueue)
	: GpuQueue(device, type, index), mQueue(vulkanQueue)
{
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

void VulkanGpuQueue::WaitUntilIdle()
{
	mGpuDevice.GetSubmitThread().WaitUntilIdle(*this);
}

void VulkanGpuQueue::AppendSyncMaskWaits(GpuQueueMask syncMask, SubmitWorkBuffer& outWorkBuffer) const
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

			// Deliberately also waits on values that have already signaled: ordering against finished queues is still
			// required so their barriers are issued before this submission.
			if(sourceQueue->mLastSubmittedProgressValue != 0)
				outWorkBuffer.AddWait(sourceQueue->mProgressTimeline, sourceQueue->mLastSubmittedProgressValue);
		}
	}
}

void VulkanGpuQueue::RetainSemaphores(SubmissionRecord& outRecord, TArrayView<VulkanSemaphore* const> semaphores)
{
	AssertIfNotSubmitThread();

	for(VulkanSemaphore* semaphore : semaphores)
	{
		semaphore->NotifyBound();
		semaphore->NotifyUsed(GetId(), GpuAccessFlag::Read | GpuAccessFlag::Write);

		outRecord.RetainedSemaphores.Add(semaphore);
	}
}

VkSubmitInfo VulkanGpuQueue::BuildVkSubmitInfo(SubmitWorkBuffer& outWorkBuffer) const
{
	const u32 waitCount = (u32)outWorkBuffer.WaitSemaphores.Size();
	const u32 signalCount = (u32)outWorkBuffer.SignalSemaphores.Size();
	const u32 commandBufferCount = (u32)outWorkBuffer.CommandBuffers.Size();

	// Binary semaphores ignore their entries in the value arrays, so the timeline structure is chained unconditionally
	outWorkBuffer.TimelineSubmitInfo = {};
	outWorkBuffer.TimelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR;
	outWorkBuffer.TimelineSubmitInfo.waitSemaphoreValueCount = waitCount;
	outWorkBuffer.TimelineSubmitInfo.pWaitSemaphoreValues = waitCount > 0 ? outWorkBuffer.WaitValues.Data() : nullptr;
	outWorkBuffer.TimelineSubmitInfo.signalSemaphoreValueCount = signalCount;
	outWorkBuffer.TimelineSubmitInfo.pSignalSemaphoreValues = signalCount > 0 ? outWorkBuffer.SignalValues.Data() : nullptr;

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = &outWorkBuffer.TimelineSubmitInfo;
	submitInfo.waitSemaphoreCount = waitCount;
	submitInfo.pWaitSemaphores = waitCount > 0 ? outWorkBuffer.WaitSemaphores.Data() : nullptr;
	submitInfo.pWaitDstStageMask = waitCount > 0 ? outWorkBuffer.WaitStages.Data() : nullptr;
	submitInfo.commandBufferCount = commandBufferCount;
	submitInfo.pCommandBuffers = commandBufferCount > 0 ? outWorkBuffer.CommandBuffers.Data() : nullptr;
	submitInfo.signalSemaphoreCount = signalCount;
	submitInfo.pSignalSemaphores = signalCount > 0 ? outWorkBuffer.SignalSemaphores.Data() : nullptr;

	return submitInfo;
}

void VulkanGpuQueue::ExecuteSubmitOnSubmitThread(const VulkanGpuCommandBufferSubmitInformation& submitInformation, GpuQueueMask syncMask, TArrayView<const GpuTimelineFenceAndValue> signalFences)
{
	AssertIfNotSubmitThread();

	if (!B3D_ENSURE(submitInformation.PrimaryCommandBuffer))
		return;

	VulkanGpuDevice& device = GetDevice();

	syncMask |= submitInformation.RequiredWaitMask;

	// Same-queue resource dependencies are recorded in the destination transition command buffer
	syncMask &= ~GpuQueueMask(GetId());

	for(const VulkanSourceQueueTransition& sourceQueueTransition : submitInformation.SourceQueueTransitions)
	{
		if(sourceQueueTransition.CommandBuffer == nullptr)
			continue;

		TShared<VulkanGpuQueue> sourceQueue = std::static_pointer_cast<VulkanGpuQueue>(device.GetQueue(sourceQueueTransition.QueueId.GetType(), sourceQueueTransition.QueueId.GetIndex()));
		if(!B3D_ENSURE(sourceQueue != nullptr))
			continue;

		syncMask |= sourceQueueTransition.QueueId;

		VulkanGpuCommandBufferSubmitInformation sourceQueueTransitionSubmitInformation;
		sourceQueueTransitionSubmitInformation.PrimaryCommandBuffer = sourceQueueTransition.CommandBuffer;

		sourceQueue->ExecuteSubmitOnSubmitThread(sourceQueueTransitionSubmitInformation, sourceQueueTransition.WaitMask, {});
	}

	SubmissionRecord record;
	RetainSemaphores(record, submitInformation.WaitSemaphores);

	SubmitWorkBuffer& transitionWorkBuffer = mSubmitWorkBuffers[0];
	SubmitWorkBuffer& primaryWorkBuffer = mSubmitWorkBuffers[1];
	transitionWorkBuffer.Clear();
	primaryWorkBuffer.Clear();

	// All waits attach to the first submitted command buffer, all signals to the last one
	const bool hasTransition = submitInformation.DestinationQueueTransitionCommandBuffer != nullptr;
	SubmitWorkBuffer& waitWorkBuffer = hasTransition ? transitionWorkBuffer : primaryWorkBuffer;

	for(VulkanSemaphore* semaphore : submitInformation.WaitSemaphores)
		waitWorkBuffer.AddWait(semaphore->GetHandle());

	AppendSyncMaskWaits(syncMask, waitWorkBuffer);

	for(VulkanSemaphore* semaphore : submitInformation.SignalSemaphores)
	{
		if(B3D_ENSURE(semaphore != nullptr))
			primaryWorkBuffer.AddSignal(semaphore->GetHandle());
	}

	for(const GpuTimelineFenceAndValue& signalFence : signalFences)
	{
		if(!B3D_ENSURE(signalFence.Fence != nullptr))
			continue;

		VulkanGpuTimelineFence* vulkanFence = static_cast<VulkanGpuTimelineFence*>(signalFence.Fence.get());
		primaryWorkBuffer.AddSignal(vulkanFence->GetTimelineSemaphore(), signalFence.Value);

		record.RetainedFences.Add(signalFence.Fence);
	}

	primaryWorkBuffer.AddSignal(mProgressTimeline, mNextProgressValue);

	TInlineArray<VkSubmitInfo, 2> submitInfos;

	if(hasTransition)
	{
		const TShared<VulkanGpuCommandBuffer>& transitionCommandBuffer = submitInformation.DestinationQueueTransitionCommandBuffer;
		transitionCommandBuffer->SetIsSubmitted();
		transitionWorkBuffer.CommandBuffers.Add(transitionCommandBuffer->GetVulkanHandle());
		record.CommandBuffers.Add(transitionCommandBuffer);

		submitInfos.Add(BuildVkSubmitInfo(transitionWorkBuffer));
	}

	const TShared<VulkanGpuCommandBuffer>& primaryCommandBuffer = submitInformation.PrimaryCommandBuffer;
	primaryCommandBuffer->SetIsSubmitted();
	primaryWorkBuffer.CommandBuffers.Add(primaryCommandBuffer->GetVulkanHandle());
	record.CommandBuffers.Add(primaryCommandBuffer);

	submitInfos.Add(BuildVkSubmitInfo(primaryWorkBuffer));

	record.SubmitIndex = mNextSubmitIndex++;
	mActiveSubmissions.push_back(std::move(record));

	const VkResult result = vkQueueSubmit(mQueue, (u32)submitInfos.Size(), submitInfos.Data(), primaryCommandBuffer->GetFence());

	// A failed queue submit is unrecoverable: the submitted command buffer's fence will never signal, so any wait on
	// it (and any dependent read-back / frame-completion wait) would deadlock forever. The most common cause is
	// VK_ERROR_DEVICE_LOST from a GPU hang/TDR or page fault. Fail fast with a fatal error (which brings the
	// application down) rather than silently hanging.
	if(result != VK_SUCCESS)
	{
		B3D_LOG(Fatal, LogRenderBackend, "vkQueueSubmit failed with VkResult {0}{1}. The GPU device is in an unrecoverable state; aborting.",
			(i32)result, (result == VK_ERROR_DEVICE_LOST) ? " (VK_ERROR_DEVICE_LOST)" : "");
		return;
	}

	mLastSubmittedProgressValue = mNextProgressValue++;
	primaryCommandBuffer->NotifyWasSubmitted(GetId());
}

VkResult VulkanGpuQueue::Present(VulkanSwapChain* swapChain, u32 swapChainImageIndex, GpuQueueMask syncMask, TArrayView<VulkanSemaphore* const> waitSemaphores)
{
	AssertIfNotSubmitThread();

	// vkQueuePresentKHR can only wait on binary semaphores, so all present dependencies (acquire semaphores and
	// inter-queue timeline waits) are routed through an empty bridge submission that signals a single binary
	// semaphore. The bridge's fence also makes those dependencies CPU-waitable through regular submission retirement.
	VulkanSemaphore* presentSemaphore = GetDevice().GetResourceManager().Create<VulkanSemaphore>("PresentReady");

	GpuCommandBufferPool& commandBufferPool = GetDevice().GetSubmitThread().GetCommandBufferPool(GetType());
	const TShared<VulkanGpuCommandBuffer> bridgeCommandBuffer = std::static_pointer_cast<VulkanGpuCommandBuffer>(commandBufferPool.Create(GpuCommandBufferCreateInformation::Create("Present synchronization")));
	bridgeCommandBuffer->End();

	VulkanGpuCommandBufferSubmitInformation bridgeSubmitInformation;
	bridgeSubmitInformation.PrimaryCommandBuffer = bridgeCommandBuffer;
	bridgeSubmitInformation.WaitSemaphores.Append(waitSemaphores.begin(), waitSemaphores.end());
	bridgeSubmitInformation.SignalSemaphores.Add(presentSemaphore);

	ExecuteSubmitOnSubmitThread(bridgeSubmitInformation, syncMask, {});

	SubmissionRecord record;
	record.PresentSwapChain = swapChain;
	RetainSemaphores(record, TArrayView<VulkanSemaphore* const>(&presentSemaphore, 1));

	SubmitWorkBuffer& workBuffer = mSubmitWorkBuffers[0];
	workBuffer.Clear();
	workBuffer.AddWait(presentSemaphore->GetHandle());

	VkSwapchainKHR vulkanSwapChain = swapChain->GetHandle();

	VkPresentInfoKHR presentInfo;
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &vulkanSwapChain;
	presentInfo.pImageIndices = &swapChainImageIndex;
	presentInfo.pResults = nullptr;
	presentInfo.waitSemaphoreCount = (u32)workBuffer.WaitSemaphores.Size();
	presentInfo.pWaitSemaphores = workBuffer.WaitSemaphores.Empty() ? nullptr : workBuffer.WaitSemaphores.Data();

	const VkResult result = vkQueuePresentKHR(mQueue, &presentInfo);
	B3D_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR);

	record.SubmitIndex = mNextSubmitIndex++;
	mActiveSubmissions.push_back(std::move(record));

	presentSemaphore->Destroy(); // Deferred until the present retires and releases its use
	return result;
}

void VulkanGpuQueue::RefreshCompletionState(bool forceWait, bool queueEmpty, u32 lastSubmitIndex)
{
	AssertIfNotSubmitThread();

	u32 lastFinishedSubmission = 0;
	for(const SubmissionRecord& record : mActiveSubmissions)
	{
		if(lastSubmitIndex != ~0u && record.SubmitIndex > lastSubmitIndex)
			break;

		// Present operations carry no fence; they retire alongside a later command buffer, or via queueEmpty
		if(record.CommandBuffers.Empty())
			continue;

		if(!record.CommandBuffers.Back()->UpdateExecutionStatus(forceWait))
		{
			B3D_ASSERT(!forceWait);
			break; // No chance of any later CBs of being done either
		}

		lastFinishedSubmission = record.SubmitIndex;
	}

	// If last submission was a Present() call, it won't be freed until a command buffer after it is done. However on
	// shutdown there might not be a CB following it. So we instead check this special flag and free everything when its
	// true.
	if(queueEmpty)
		lastFinishedSubmission = mNextSubmitIndex - 1;

	WaitGroup waitGroup;

	while(!mActiveSubmissions.empty())
	{
		SubmissionRecord& record = mActiveSubmissions.front();
		if(record.SubmitIndex > lastFinishedSubmission)
			break;

		const bool isPresentOperation = record.PresentSwapChain != nullptr;
		SingleConsumerQueue& messageBackQueue = isPresentOperation ? record.PresentSwapChain->GetMessageQueue() : record.CommandBuffers.Front()->GetPool().GetMessageQueue();

		if(!record.RetainedSemaphores.Empty())
		{
			messageBackQueue.PostCommand([semaphoresToRelease = std::move(record.RetainedSemaphores), queueId = GetId()]()
			{
				for(VulkanSemaphore* semaphore : semaphoresToRelease)
					semaphore->NotifyDone(queueId, GpuAccessFlag::Read | GpuAccessFlag::Write);
			});
		}

		if(isPresentOperation)
		{
			waitGroup.Increment();
			messageBackQueue.PostCommand([swapChain = record.PresentSwapChain, waitGroup = forceWait ? &waitGroup : nullptr]
			{
				swapChain->NotifyUnbound();

				if(waitGroup != nullptr)
					waitGroup->NotifyDone();
			}, "CommandBufferCompleteCallback");
		}
		else
		{
			for(const TShared<VulkanGpuCommandBuffer>& commandBuffer : record.CommandBuffers)
			{
				waitGroup.Increment();
				commandBuffer->GetPool().GetMessageQueue().PostCommand([commandBuffer, waitGroup = forceWait ? &waitGroup : nullptr]()
				{
					commandBuffer->mState = GpuCommandBufferState::Done;
					commandBuffer->OnDidComplete();
					commandBuffer->Reset();

					if(waitGroup != nullptr)
						waitGroup->NotifyDone();
				}, "CommandBufferCompleteCallback");
			}
		}

		mActiveSubmissions.pop_front();
	}

	// Ensure the message back callbacks also trigger in the force wait case
	if(forceWait)
		waitGroup.Wait();
}
