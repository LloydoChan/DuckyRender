#include "DuckyGraphicsContext.h"
#include "D3DDeviceManager.h"
#include "DuckyTools.h"

bool DuckyGraphicsContext::Init(D3DDeviceManager* DeviceManager, std::wofstream* LogFile)
{
	for (auto& frame : mFrames)
	{
		frame.allocator = DeviceManager->CreateCommandAllocator();
		if (frame.allocator == nullptr) return false;
	}

	HRESULT hResult = DeviceManager->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mFrames[0].allocator.Get(), nullptr, IID_PPV_ARGS(&mCommandList));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating command list: ", *LogFile)) return false;
	
	return true;
}

bool DuckyGraphicsContext::BeginFrame(UINT CurrentFrame, ID3D12Fence* Fence, ID3D12PipelineState* PipelineState, HANDLE event)
{
	FrameContext& context = mFrames[CurrentFrame];
	if (context.fenceValue != 0 &&
		Fence->GetCompletedValue() < context.fenceValue)
	{
		Fence->SetEventOnCompletion(
			context.fenceValue,
			event);

		WaitForSingleObject(event, INFINITE);
	}

	context.allocator.Get()->Reset();
	mCommandList->Reset(context.allocator.Get(), PipelineState);
	return true;
}

bool DuckyGraphicsContext::EndFrame(UINT CurrentFrame, ID3D12CommandQueue* Queue, ID3D12Fence* Fence)
{
	UINT64 submittedFenceVal = ++mGlobalFenceValue;
	HRESULT hr = Queue->Signal(Fence, submittedFenceVal);
	if (hr != S_OK) return false;
	FrameContext& context = mFrames[CurrentFrame];
	context.fenceValue = submittedFenceVal;
	return true;
}

bool DuckyGraphicsContext::WaitForGpu(ID3D12CommandQueue* queue, ID3D12Fence* fence, HANDLE eventHandle)
{
	const UINT64 fenceValue = ++mGlobalFenceValue;

	HRESULT hr = queue->Signal(fence, fenceValue);

	if (hr != S_OK) return false;

	if (fence->GetCompletedValue() < fenceValue)
	{
		hr = fence->SetEventOnCompletion(fenceValue, eventHandle);

		if (hr != S_OK) return false;

		WaitForSingleObject(eventHandle, INFINITE);
	}

	return true;
}
