#pragma once

#include <d3d12.h>
#include <fstream>
#include <dxgi1_6.h>
#include <vector>

#include <wrl/client.h>
#include <DirectXMath.h>
#include <unordered_map>

#include "DuckyCompiler.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct PipelineAndRootSig
{
	ID3D12RootSignature* rootSig;
	ID3D12PipelineState* pipeLineState;
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

		ID3D12Fence* CreateFence(UINT64 FenceVal);

		int CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_FLAGS flags, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors = MAX_NUM_DESCRIPTORS_PER_HEAP);
		ID3D12Resource* CreateBuffer(size_t bufferSize);
		DescriptorHeapResource CreateConstantBuffer(size_t bufferSize, int DescriptorHeapHandle);
		ID3D12GraphicsCommandList* CreateAndReturnCommandList(ID3D12CommandAllocator* Allocator);
		PipelineAndRootSig CreatePSO(LPCWSTR vertexShader, LPCWSTR vertexEntry, LPCWSTR pixelShader, LPCWSTR pixelEntry, UINT numConstantBuffers, UINT numShaderResources);
		ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();


		size_t InitTexture(const wchar_t* Filepath, UINT DescriptorHeapIndex);

		UINT GetCurrentSwapChainIndex() { return mSwapChain->GetCurrentBackBufferIndex(); }
		ID3D12CommandQueue* GetCommandQueue() { return mCommandQueue.Get(); }

		DescriptorHeapResource GetTexture(UINT HashedInput) { return mTextures[HashedInput]; }

		D3D12_RESOURCE_BARRIER GetBarrier()
		{
			D3D12_RESOURCE_BARRIER BarrierDesc = {};
			UINT bbIdx = mSwapChain->GetCurrentBackBufferIndex();
			BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			BarrierDesc.Transition.pResource = backBuffers[bbIdx].Get();
			BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			return BarrierDesc;
		}

		UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) { return mDevice->GetDescriptorHandleIncrementSize(type); }
		ID3D12DescriptorHeap* GetDescriptorHeapHandle(int Index) { return mDescriptorHeaps[Index]; };
		void Present();

		D3D12_CPU_DESCRIPTOR_HANDLE IncrementAndReturnRTVHeaps();
		ID3D12DescriptorHeap* GetDepthStencilBufferHeap() { return mDsvHeaps.Get(); }

		std::vector<D3D12_INPUT_ELEMENT_DESC> CreateInputLayout(ShaderCompilationOutput& shaderCompData);

		UINT GetCurrentFrameIndex() { return  mSwapChain->GetCurrentBackBufferIndex(); }

	private:
		// only want to call this from the DeviceManager itself
		DescriptorHeapResource CreateTexture(const wchar_t* Filepath, ID3D12DescriptorHeap* descHeap);
		std::unordered_map<UINT,DescriptorHeapResource> mTextures;

		ComPtr<ID3D12Device> mDevice;
		IDXGISwapChain3* mSwapChain;
		ComPtr<ID3D12CommandQueue> mCommandQueue;

		ComPtr<ID3D12DescriptorHeap> mDsvHeaps;
		ComPtr<ID3D12Resource> mDepthBuffer;

		std::vector<ID3D12DescriptorHeap*> mDescriptorHeaps;

		ComPtr<ID3D12DescriptorHeap> rtvHeaps = nullptr;
		std::vector<ComPtr<ID3D12Resource>> backBuffers;
		
		XMMATRIX matIdent;

		UINT mDescriptorHandleIndex = 0;

		std::wofstream* mLogFilePtr = nullptr;
		DuckyCompiler* mCompiler = nullptr;
};