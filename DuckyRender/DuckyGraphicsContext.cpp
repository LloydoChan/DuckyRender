#include "pch.h"
#include "DuckyGraphicsContext.h"
#include "D3DDeviceManager.h"
#include "DuckyTools.h"

bool DuckyGraphicsContext::Init(D3DDeviceManager* DeviceManager, UINT64 CBVCapacity, UINT64 NumPossibleDraws)
{
	const size_t indirectBufferSize = NumPossibleDraws * sizeof(IndirectCommand);

	for (auto& frame : mFrames)
	{
		frame.mCmdAllocator = DeviceManager->CreateCommandAllocator();
		if (frame.mCmdAllocator == nullptr) return false;

		frame.mBufferAllocator.Init(DeviceManager->GetDevice(), CBVCapacity);
		if (frame.mBufferAllocator.mResourceBuffer.Get() == nullptr) return false;

		frame.mIndirectBuffer = DeviceManager->CreateDefaultBuffer(indirectBufferSize, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		if (!frame.mIndirectBuffer) return false;

		frame.mMappedIndirectCommand = nullptr;

		const UINT64 countBufferSize = sizeof(uint32_t) * static_cast<uint32_t>(PipelineType::DEBUG);

		frame.mVisibleDrawCounts = DeviceManager->CreateDefaultBuffer(countBufferSize, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	HRESULT hResult = DeviceManager->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mFrames[0].mCmdAllocator.Get(), nullptr, IID_PPV_ARGS(&mCommandList));
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem creating command list resource"))) return false;
	
	hResult = mCommandList->Close();
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't close on command list"))) return false;

	return true;
}

bool DuckyGraphicsContext::BeginFrame(UINT CurrentFrame, ID3D12Fence* Fence, ID3D12PipelineState* PipelineState, HANDLE event)
{
	HRESULT hResult;
	FrameContext& context = mFrames[CurrentFrame];
	if (context.fenceValue != 0 &&
		Fence->GetCompletedValue() < context.fenceValue)
	{
		hResult = Fence->SetEventOnCompletion(context.fenceValue,event);
		if (FAILED(hResult)) return false;
		if(WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0) return false;
	}

	hResult = context.mCmdAllocator.Get()->Reset();
	if(!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem resetting command allocator"))) return false;

	hResult = mCommandList->Reset(context.mCmdAllocator.Get(), PipelineState);
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem resetting command list"))) return false;

	context.mBufferAllocator.Reset();

	return true;
}

bool DuckyGraphicsContext::EndFrame(UINT CurrentFrame, ID3D12CommandQueue* Queue, ID3D12Fence* Fence)
{
	UINT64 submittedFenceVal = ++mGlobalFenceValue;
	HRESULT hResult = Queue->Signal(Fence, submittedFenceVal);
	if (FAILED(hResult)) return false;
	FrameContext& context = mFrames[CurrentFrame];
	context.fenceValue = submittedFenceVal;
	return true;
}

bool DuckyGraphicsContext::WaitForGpu(ID3D12CommandQueue* queue, ID3D12Fence* fence, HANDLE event)
{
	const UINT64 fenceValue = ++mGlobalFenceValue;

	HRESULT hResult = queue->Signal(fence, fenceValue);

	if (FAILED(hResult)) return false;

	if (fence->GetCompletedValue() < fenceValue)
	{
		hResult = fence->SetEventOnCompletion(fenceValue, event);

		if (FAILED(hResult)) return false;

		if (WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0) return false;
	}

	return true;
}
