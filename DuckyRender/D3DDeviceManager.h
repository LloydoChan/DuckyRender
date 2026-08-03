#pragma once


#include "DuckyCompiler.h"
#include "DuckySwapChain.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;
const size_t NO_TEXTURE     = 6666;

struct RootSignatureDesc
{
	std::vector<D3D12_ROOT_PARAMETER> parameters;
	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
};

struct PipelineAndRootSig
{
	ComPtr<ID3D12RootSignature> rootSig;
	ComPtr<ID3D12PipelineState> pipeLineState;
};


struct DescriptorHeapResource
{
	ComPtr<ID3D12Resource> buffer;
	UINT heapOffset = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE descHandle;
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

const UINT MAX_NUM_DESCRIPTORS_PER_HEAP = 100;

class D3DDeviceManager
{
	public:
		bool Init(HWND hWnd, UINT WindowWidth, UINT WindowHeight, std::wofstream* LogFile);

		ComPtr<ID3D12Fence> CreateFence(UINT64 FenceVal);
		ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();

		int CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_FLAGS flags, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors = MAX_NUM_DESCRIPTORS_PER_HEAP);

		ComPtr<ID3D12Resource> CreateBuffer(size_t bufferSize);
		DescriptorHeapResource CreateConstantBuffer(size_t bufferSize, int DescriptorHeapHandle);
		PipelineAndRootSig CreatePSO(LPCWSTR vertexShader, LPCWSTR vertexEntry, LPCWSTR pixelShader, LPCWSTR pixelEntry, RootSignatureDesc& NewRootSigDesc, D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
		std::vector<D3D12_INPUT_ELEMENT_DESC> CreateInputLayout(ShaderCompilationOutput& shaderCompData);

		size_t InitTexture(const wchar_t* Filepath, UINT DescriptorHeapIndex);
		size_t InitFallbackTexture(const wchar_t* Name, const XMFLOAT4& InputColor, UINT DescriptorHeapIndex);

		ID3D12CommandQueue* GetCommandQueue() { return mCommandQueue.Get(); }
		DescriptorHeapResource* GetTexture(size_t HashedInput);
		ID3D12DescriptorHeap* GetDepthStencilBufferHeap() { return mDsvHeaps.Get(); }
		UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) { return mDevice->GetDescriptorHandleIncrementSize(type); }
		ID3D12DescriptorHeap* GetDescriptorHeapHandle(int Index) { return mDescriptorHeaps[Index].Get(); };
		ID3D12Device* GetDevice() { return mDevice.Get(); }
		UINT32 GetCurrentFrameIndex() { return mSwapChain->GetCurrentBackBufferIndex(); };
		bool CreateDepthBuffer(UINT width, UINT height);
		bool CreateDepthBufferHeap();

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


	private:
		// only want to call this from the DeviceManager itself
		DescriptorHeapResource CreateTexture(const wchar_t* Filepath, ID3D12DescriptorHeap* descHeap);
		DescriptorHeapResource CreateFallbackTexture(const wchar_t* Name, const XMFLOAT4& Color, ID3D12DescriptorHeap* descHeap);
		std::unordered_map<size_t,DescriptorHeapResource> mTextures;

		ComPtr<ID3D12Device> mDevice;
		ComPtr<ID3D12CommandQueue> mCommandQueue;

		ComPtr<ID3D12DescriptorHeap> mDsvHeaps;
		ComPtr<ID3D12Resource> mDepthBuffer;

		std::vector<ComPtr<ID3D12DescriptorHeap>> mDescriptorHeaps;
		
		XMMATRIX matIdent;

		UINT mDescriptorHandleIndex = 0;

		std::wofstream* mLogFilePtr = nullptr;
		std::unique_ptr<DuckyCompiler> mCompiler;
		std::unique_ptr<DuckySwapChain> mSwapChain;
};