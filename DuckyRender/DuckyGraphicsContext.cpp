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
	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem creating command list: ", *LogFile)) return false;
	
	return true;
}

bool DuckyGraphicsContext::BeginFrame(UINT CurrentFrame, ID3D12Fence* Fence, ID3D12PipelineState* PipelineState, HANDLE event, std::wofstream* LogFile)
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

	hResult = context.allocator.Get()->Reset();
	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem resetting Command Allocator", *LogFile)) return false;

	hResult = mCommandList->Reset(context.allocator.Get(), PipelineState);
	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem resetting Command List", *LogFile)) return false;

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

bool DuckyGraphicsContext::WaitForGpu(ID3D12CommandQueue* queue, ID3D12Fence* fence, HANDLE event)
{
	const UINT64 fenceValue = ++mGlobalFenceValue;

	HRESULT hr = queue->Signal(fence, fenceValue);

	if (hr != S_OK) return false;

	if (fence->GetCompletedValue() < fenceValue)
	{
		hr = fence->SetEventOnCompletion(fenceValue, event);

		if (hr != S_OK) return false;

		if (WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0) return false;
	}

	return true;
}
