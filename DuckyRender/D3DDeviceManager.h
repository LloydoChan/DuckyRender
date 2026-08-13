#pragma once


#include "DuckyCompiler.h"
#include "DuckySwapChain.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;
const size_t NO_TEXTURE     = 6666;


struct DescriptorHeapResource
{
	ComPtr<ID3D12Resource> buffer;
	UINT heapOffset = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE descHandle;
};

struct MappedDescriptorHeapResource
{
	ComPtr<ID3D12Resource> buffer;
	UINT heapOffset = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE descHandle;
	void* mapped = nullptr;
};

struct ViewportScissor
{
	// default - wholescreen
	ViewportScissor(FLOAT Width, FLOAT Height)
	{
		viewport.Height = Height;
		viewport.Width = Width;
		viewport.TopLeftX = viewport.TopLeftY = 0;
		viewport.MaxDepth = 1.f;
		viewport.MinDepth = 0.f;

		scissor.left = scissor.top = 0;
		scissor.bottom = static_cast<LONG>(Height);
		scissor.right = static_cast<LONG>(Width);
	};


	D3D12_VIEWPORT viewport = {};
	D3D12_RECT scissor = {};
};

struct Vertex
{
	XMFLOAT3 pos;
	XMFLOAT2 uv;
};

struct DescriptorAllocation
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
	UINT heapIndex = UINT_MAX;
	UINT descriptorIndex = UINT_MAX;

	bool IsValid() const
	{
		return heapIndex != UINT_MAX &&
			descriptorIndex != UINT_MAX;
	}
};

const UINT MAX_NUM_DESCRIPTORS_PER_HEAP = 100;

class D3DDeviceManager
{
	public:
		bool Init(HWND hWnd, UINT WindowWidth, UINT WindowHeight, std::wofstream* LogFile);

		ComPtr<ID3D12Fence> CreateFence(UINT64 FenceVal);
		ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();

		int CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_FLAGS flags, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors = MAX_NUM_DESCRIPTORS_PER_HEAP);

		DescriptorHeapResource CreateDefaultStructuredBuffer(size_t bufferSize, size_t elementSize);
		MappedDescriptorHeapResource CreateStructuredBuffer(size_t BufferSize, size_t ElementSize);
		ComPtr<ID3D12Resource> CreateBuffer(size_t bufferSize);
		DescriptorHeapResource CreateConstantBuffer(size_t bufferSize);
		ComPtr<ID3D12Resource> CreateUploadBuffer(size_t BufferSize);
		ComPtr<ID3D12Resource> CreateDefaultBuffer(size_t BufferSize, D3D12_RESOURCE_STATES initialState);

		ID3D12CommandQueue* GetCommandQueue() { return mCommandQueue.Get(); }
		ID3D12DescriptorHeap* GetDepthStencilBufferHeap() { return mDsvHeaps.Get(); }
		ID3D12DescriptorHeap* GetDescriptorHeapHandle() { return mDescriptorHeaps[mCbvUavSrvDescriptorHandle].Get(); };
		UINT GetDescriptorHeapHandleInt() { return mCbvUavSrvDescriptorHandle; };
		ID3D12Device* GetDevice() { return mDevice.Get(); }
		UINT32 GetCurrentFrameIndex() { return mSwapChain->GetCurrentBackBufferIndex(); };
		bool CreateDepthBuffer(UINT width, UINT height);
		bool CreateDepthBufferHeap();

		DescriptorAllocation AllocateCbvSrvUavDescriptor(UINT descriptorHeapHandle);

		D3D12_RESOURCE_BARRIER GetBarrier()
		{
			D3D12_RESOURCE_BARRIER BarrierDesc = {};
			UINT bbIdx = mSwapChain->GetCurrentBackBufferIndex();
			BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			BarrierDesc.Transition.pResource = mSwapChain->GetCurrentBackBuffer();
			BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			return BarrierDesc;
		}

		void Present() { mSwapChain->Present(); }
		D3D12_CPU_DESCRIPTOR_HANDLE IncrementAndReturnRTVHeaps() { return mSwapChain->GetCurrentRtv(this); };

		bool Resize(UINT WindowWidth, UINT WindowHeight);

		DescriptorHeapResource CreateTexture(const wchar_t* Filepath);
		DescriptorHeapResource CreateFallbackTexture(const wchar_t* Name, const XMFLOAT4& Color, DXGI_FORMAT Format);

	private:

		ComPtr<ID3D12Device> mDevice;
		ComPtr<ID3D12CommandQueue> mCommandQueue;

		ComPtr<ID3D12DescriptorHeap> mDsvHeaps;
		ComPtr<ID3D12Resource> mDepthBuffer;

		std::vector<ComPtr<ID3D12DescriptorHeap>> mDescriptorHeaps;
		
		XMMATRIX matIdent;

		UINT mDescriptorHandleIndex = 0;
		UINT mCbvUavSrvDescriptorHandle = 0;

		std::wofstream* mLogFilePtr = nullptr;
		std::unique_ptr<DuckySwapChain> mSwapChain;
};