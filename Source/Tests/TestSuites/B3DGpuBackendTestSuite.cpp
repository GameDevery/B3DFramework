//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "B3DGpuBackendTestSuite.h"
#include "GpuBackend/B3DGpuHazards.h"
#include "GpuBackend/Allocators/B3DGpuResource.h"
#include "GpuBackend/B3DGpuBackendUtility.h"
#include "GpuBackend/B3DGpuResourceTracker.h"
#include "GpuBackend/B3DGpuResourceTracker.inl"

using namespace b3d;
using namespace b3d::render;

namespace
{
	struct SubmissionTestBarrierHelper { };

	class SubmissionTestBuffer : public IGpuBufferResource
	{
	public:
		SubmissionTestBuffer() = default;
	};

	class SubmissionTestVisitor : public GpuSubmissionTransitionVisitor
	{
	public:
		void VisitBuffer(const GpuSubmissionBufferTransition& transition) override
		{
			ParallelAccessWaitMask = transition.ParallelAccessWaitMask;
			ExclusiveAccessWaitMask = transition.ExclusiveAccessWaitMask;
			SameQueueTransitionRecipe = transition.SameQueueTransitionRecipe;
		}

		void VisitImage(const GpuSubmissionImageTransition&) override
		{
			B3D_ASSERT(false);
		}

		GpuQueueMask ParallelAccessWaitMask = GpuQueueMask::kNone;
		GpuQueueMask ExclusiveAccessWaitMask = GpuQueueMask::kNone;
		GpuHazardState::TransitionRecipe SameQueueTransitionRecipe;
	};

	struct SubmissionTestResult
	{
		GpuQueueMask ParallelAccessWaitMask;
		GpuQueueMask ExclusiveAccessWaitMask;
		GpuHazardState::TransitionRecipe SameQueueTransitionRecipe;
	};

	SubmissionTestResult ResolveTestSubmission(SubmissionTestBuffer& buffer, GpuQueueId queueId, GpuStageFlags stages, GpuAccessFlags access)
	{
		GpuHazardStateWithHistory hazards;
		hazards.Access = access;
		hazards.RegisterStageAccess(stages, access);

		TGpuResourceTracker<SubmissionTestBarrierHelper> tracker;
		GpuBufferTrackingState trackingState = {};
		trackingState.WriteHazardTracking = &hazards;
		tracker.GetBuffers().insert(std::make_pair(&buffer, trackingState));

		SubmissionTestVisitor visitor;
		tracker.ResolveSubmissionTransitions(queueId, visitor);
		return { visitor.ParallelAccessWaitMask, visitor.ExclusiveAccessWaitMask, visitor.SameQueueTransitionRecipe };
	}

	void BeginTestRead(SubmissionTestBuffer& buffer, GpuQueueId queueId)
	{
		buffer.NotifyBound();
		buffer.NotifyUsed(queueId, GpuAccessFlag::Read);
	}

	void EndTestRead(SubmissionTestBuffer& buffer, GpuQueueId queueId)
	{
		buffer.NotifyDone(queueId, GpuAccessFlag::Read);
	}
}

GpuBackendTestSuite::GpuBackendTestSuite()
	: TestSuite("GpuBackendTestSuite")
{
	B3D_ADD_TEST(GpuBackendTestSuite::TestHazardHistoryEpochs)
	B3D_ADD_TEST(GpuBackendTestSuite::TestTransitionRecipe)
	B3D_ADD_TEST(GpuBackendTestSuite::TestSubmissionTransitionPlanning)
}

void GpuBackendTestSuite::TestHazardHistoryEpochs()
{
	GpuHazardStateWithHistory hazardHistory;
	for(u32 accessIndex = 0; accessIndex < 1000; ++accessIndex)
	{
		if((accessIndex & 1) == 0)
			hazardHistory.RegisterStageAccess(GpuStageFlag::Transfer, GpuAccessFlag::Read);
		else
			hazardHistory.RegisterStageAccess(GpuStageFlag::ComputeShaderNonUniform, GpuAccessFlag::Write);
	}

	B3D_TEST_ASSERT(hazardHistory.GetCompletedEpochs().Size() == 0)

	const GpuAccessScope& currentAccessScope = hazardHistory.GetCurrentAccessScope();
	B3D_TEST_ASSERT(currentAccessScope.GetAccess(GpuStageFlag::Transfer) == GpuAccessFlag::Read)
	B3D_TEST_ASSERT(currentAccessScope.GetAccess(GpuStageFlag::ComputeShaderNonUniform) == GpuAccessFlag::Write)

	hazardHistory.RegisterBarrier(GpuHazardStageAndAccess(GpuStageFlag::Transfer, GpuAccessFlag::Read, GpuStageFlag::ComputeShaderNonUniform, GpuAccessFlag::Write));
	B3D_TEST_ASSERT(hazardHistory.GetCompletedEpochs().Size() == 1)
	B3D_TEST_ASSERT(!hazardHistory.GetCurrentAccessScope().IsValid())

	const GpuHazardStateWithHistory::Epoch& completedEpoch = hazardHistory.GetCompletedEpochs()[0];
	B3D_TEST_ASSERT(completedEpoch.AccessScope.GetAccess(GpuStageFlag::Transfer) == GpuAccessFlag::Read)
	B3D_TEST_ASSERT(completedEpoch.AccessScope.GetAccess(GpuStageFlag::ComputeShaderNonUniform) == GpuAccessFlag::Write)
	B3D_TEST_ASSERT(completedEpoch.IssuedBarrier.SourceStages == GpuStageFlag::Transfer)
	B3D_TEST_ASSERT(completedEpoch.IssuedBarrier.SourceAccess == GpuAccessFlag::Read)
	B3D_TEST_ASSERT(completedEpoch.IssuedBarrier.DestinationStages == GpuStageFlag::ComputeShaderNonUniform)
	B3D_TEST_ASSERT(completedEpoch.IssuedBarrier.DestinationAccess == GpuAccessFlag::Write)

	for(u32 accessIndex = 0; accessIndex < 1000; ++accessIndex)
		hazardHistory.RegisterStageAccess(GpuStageFlag::ColorAttachment, GpuAccessFlag::Write);

	B3D_TEST_ASSERT(hazardHistory.GetCompletedEpochs().Size() == 1)
	B3D_TEST_ASSERT(hazardHistory.GetCurrentAccessScope().GetAccess(GpuStageFlag::ColorAttachment) == GpuAccessFlag::Write)

	const GpuAccessScope accumulatedAccessScope = hazardHistory.GetAccumulatedAccessScope();
	B3D_TEST_ASSERT(accumulatedAccessScope.GetStages().IsSet(GpuStageFlag::Transfer))
	B3D_TEST_ASSERT(accumulatedAccessScope.GetStages().IsSet(GpuStageFlag::ComputeShaderNonUniform))
	B3D_TEST_ASSERT(accumulatedAccessScope.GetStages().IsSet(GpuStageFlag::ColorAttachment))

	GpuHazardStateWithHistory leadingBarrierHistory;
	leadingBarrierHistory.RegisterBarrier(GpuHazardStageAndAccess(GpuStageFlag::Transfer, GpuAccessFlag::Write, GpuStageFlag::FragmentShaderNonUniform, GpuAccessFlag::Read));
	leadingBarrierHistory.RegisterStageAccess(GpuStageFlag::FragmentShaderNonUniform, GpuAccessFlag::Read);

	B3D_TEST_ASSERT(leadingBarrierHistory.GetCompletedEpochs().Size() == 1)
	B3D_TEST_ASSERT(!leadingBarrierHistory.GetCompletedEpochs()[0].AccessScope.IsValid())
	B3D_TEST_ASSERT(leadingBarrierHistory.GetCurrentAccessScope().GetAccess(GpuStageFlag::FragmentShaderNonUniform) == GpuAccessFlag::Read)
}

void GpuBackendTestSuite::TestTransitionRecipe()
{
	const GpuQueueId sourceQueueId(GQT_GRAPHICS, 0);

	GpuHazardState sourceHazardState;
	sourceHazardState.ClearSafeAccess(GpuStageFlag::Transfer, GpuAccessFlag::Write);

	GpuHazardStateWithHistory fragmentReadHistory;
	fragmentReadHistory.RegisterStageAccess(GpuStageFlag::FragmentShaderNonUniform, GpuAccessFlag::Read);

	const GpuHazardState::TransitionRecipe fragmentReadRecipe = sourceHazardState.BuildTransitionRecipe(sourceQueueId, fragmentReadHistory);
	B3D_TEST_ASSERT(fragmentReadRecipe.SourceQueueId.Id == sourceQueueId.Id)
	B3D_TEST_ASSERT(fragmentReadRecipe.MemoryDependency.SourceStages == GpuStageFlag::Transfer)
	B3D_TEST_ASSERT(fragmentReadRecipe.MemoryDependency.SourceAccess == GpuAccessFlag::Write)
	B3D_TEST_ASSERT(fragmentReadRecipe.MemoryDependency.DestinationStages == GpuStageFlag::FragmentShaderNonUniform)
	B3D_TEST_ASSERT(fragmentReadRecipe.MemoryDependency.DestinationAccess == GpuAccessFlag::Read)
	B3D_TEST_ASSERT(!fragmentReadRecipe.ExecutionDependency.IsValid())
	B3D_TEST_ASSERT(fragmentReadRecipe.RemainingHazardState.MemoryBarrierTracking.GetUnsafeAccessStages(GpuStageFlag::FragmentShaderNonUniform) == GpuStageFlag::None)
	B3D_TEST_ASSERT(fragmentReadRecipe.RemainingHazardState.MemoryBarrierTracking.GetUnsafeAccessStages(GpuStageFlag::ComputeShaderNonUniform).IsSet(GpuStageFlag::Transfer))

	GpuHazardStateWithHistory computeReadHistory;
	computeReadHistory.RegisterStageAccess(GpuStageFlag::ComputeShaderNonUniform, GpuAccessFlag::Read);

	const GpuHazardState::TransitionRecipe computeReadRecipe = fragmentReadRecipe.RemainingHazardState.BuildTransitionRecipe(sourceQueueId, computeReadHistory);
	B3D_TEST_ASSERT(computeReadRecipe.MemoryDependency.SourceStages == GpuStageFlag::Transfer)
	B3D_TEST_ASSERT(computeReadRecipe.MemoryDependency.DestinationStages == GpuStageFlag::ComputeShaderNonUniform)

	GpuHazardStateWithHistory multiEpochHistory;
	multiEpochHistory.RegisterStageAccess(GpuStageFlag::FragmentShaderNonUniform, GpuAccessFlag::Read);
	multiEpochHistory.RegisterBarrier(GpuHazardStageAndAccess(GpuStageFlag::FragmentShaderNonUniform, GpuAccessFlag::Read, GpuStageFlag::ComputeShaderNonUniform, GpuAccessFlag::Read));
	multiEpochHistory.RegisterStageAccess(GpuStageFlag::ComputeShaderNonUniform, GpuAccessFlag::Read);

	const GpuHazardState::TransitionRecipe multiEpochRecipe = sourceHazardState.BuildTransitionRecipe(sourceQueueId, multiEpochHistory);
	B3D_TEST_ASSERT(multiEpochHistory.GetFirstAccessScope().GetStages() == GpuStageFlag::FragmentShaderNonUniform)
	B3D_TEST_ASSERT(multiEpochRecipe.MemoryDependency.DestinationStages == (GpuStageFlag::FragmentShaderNonUniform | GpuStageFlag::ComputeShaderNonUniform))

	const GpuAccessScope destinationAccessScope = multiEpochRecipe.GetDestinationAccessScope();
	B3D_TEST_ASSERT(destinationAccessScope.GetAccess(GpuStageFlag::FragmentShaderNonUniform) == GpuAccessFlag::Read)
	B3D_TEST_ASSERT(destinationAccessScope.GetAccess(GpuStageFlag::ComputeShaderNonUniform) == GpuAccessFlag::Read)

	GpuHazardStateWithHistory leadingBarrierHistory;
	leadingBarrierHistory.RegisterBarrier(GpuHazardStageAndAccess(GpuStageFlag::Transfer, GpuAccessFlag::Write, GpuStageFlag::FragmentShaderNonUniform, GpuAccessFlag::Read));
	leadingBarrierHistory.RegisterStageAccess(GpuStageFlag::FragmentShaderNonUniform, GpuAccessFlag::Read);

	const GpuHazardState::TransitionRecipe leadingBarrierRecipe = sourceHazardState.BuildTransitionRecipe(sourceQueueId, leadingBarrierHistory);
	B3D_TEST_ASSERT(!leadingBarrierRecipe.MemoryDependency.IsValid())
	B3D_TEST_ASSERT(leadingBarrierRecipe.RequiresCrossQueueDependency)
	B3D_TEST_ASSERT(leadingBarrierRecipe.RemainingHazardState.MemoryBarrierTracking.GetUnsafeAccessStages(GpuStageFlag::FragmentShaderNonUniform) == GpuStageFlag::None)
}

void GpuBackendTestSuite::TestSubmissionTransitionPlanning()
{
	B3D_TEST_ASSERT(GpuBackendUtility::GetStageFlags(GpuResourceUseFlag::Host) == GpuStageFlag::Host)
	B3D_TEST_ASSERT(GpuBackendUtility::GetStageFlags(GpuResourceUseFlag::ShaderAccess | GpuResourceUseFlag::StageVertexShader) == GpuStageFlag::VertexShaderNonUniform)

	const GpuQueueId writerQueue(GQT_GRAPHICS, 0);
	const GpuQueueId firstReaderQueue(GQT_COMPUTE, 0);
	const GpuQueueId secondReaderQueue(GQT_TRANSFER, 0);
	const GpuQueueId nextWriterQueue(GQT_GRAPHICS, 1);
	SubmissionTestBuffer buffer;

	const SubmissionTestResult initialWrite = ResolveTestSubmission(buffer, writerQueue, GpuStageFlag::ColorAttachment, GpuAccessFlag::Write);
	B3D_TEST_ASSERT(initialWrite.ParallelAccessWaitMask.IsEmpty())
	B3D_TEST_ASSERT(buffer.GetSubmissionState().HasWriter)
	B3D_TEST_ASSERT(buffer.GetSubmissionState().WriterQueueId.Id == writerQueue.Id)

	const SubmissionTestResult firstRead = ResolveTestSubmission(buffer, firstReaderQueue, GpuStageFlag::ComputeShaderNonUniform, GpuAccessFlag::Read);
	B3D_TEST_ASSERT(firstRead.ParallelAccessWaitMask == GpuQueueMask(writerQueue))
	BeginTestRead(buffer, firstReaderQueue);

	const SubmissionTestResult repeatedRead = ResolveTestSubmission(buffer, firstReaderQueue, GpuStageFlag::ComputeShaderNonUniform, GpuAccessFlag::Read);
	B3D_TEST_ASSERT(repeatedRead.ParallelAccessWaitMask.IsEmpty())
	BeginTestRead(buffer, firstReaderQueue);

	const SubmissionTestResult parallelRead = ResolveTestSubmission(buffer, secondReaderQueue, GpuStageFlag::Transfer, GpuAccessFlag::Read);
	B3D_TEST_ASSERT(parallelRead.ParallelAccessWaitMask == GpuQueueMask(writerQueue))
	B3D_TEST_ASSERT(!parallelRead.ParallelAccessWaitMask.IsSet(firstReaderQueue))
	BeginTestRead(buffer, secondReaderQueue);

	const SubmissionTestResult nextWrite = ResolveTestSubmission(buffer, nextWriterQueue, GpuStageFlag::Transfer, GpuAccessFlag::Write);
	const GpuQueueMask readerMask = GpuQueueMask(firstReaderQueue) | GpuQueueMask(secondReaderQueue);
	B3D_TEST_ASSERT(nextWrite.ParallelAccessWaitMask == readerMask)
	B3D_TEST_ASSERT(nextWrite.ExclusiveAccessWaitMask == readerMask)
	B3D_TEST_ASSERT(!nextWrite.ParallelAccessWaitMask.IsSet(writerQueue))

	EndTestRead(buffer, firstReaderQueue);
	EndTestRead(buffer, firstReaderQueue);
	EndTestRead(buffer, secondReaderQueue);

	const SubmissionTestResult readAfterNewWrite = ResolveTestSubmission(buffer, firstReaderQueue, GpuStageFlag::ComputeShaderNonUniform, GpuAccessFlag::Read);
	B3D_TEST_ASSERT(readAfterNewWrite.ParallelAccessWaitMask == GpuQueueMask(nextWriterQueue))

	SubmissionTestBuffer readOnlyBuffer;
	ResolveTestSubmission(readOnlyBuffer, firstReaderQueue, GpuStageFlag::ComputeShaderNonUniform, GpuAccessFlag::Read);
	BeginTestRead(readOnlyBuffer, firstReaderQueue);
	const SubmissionTestResult sameQueueWrite = ResolveTestSubmission(readOnlyBuffer, firstReaderQueue, GpuStageFlag::ComputeShaderNonUniform, GpuAccessFlag::Write);
	B3D_TEST_ASSERT(sameQueueWrite.ParallelAccessWaitMask.IsEmpty())
	B3D_TEST_ASSERT(sameQueueWrite.SameQueueTransitionRecipe.ExecutionDependency.IsValid())
	EndTestRead(readOnlyBuffer, firstReaderQueue);
}
