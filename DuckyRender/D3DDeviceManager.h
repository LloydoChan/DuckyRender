#pragma once

#include <d3d12.h>
#include <fstream>
#include <dxgi1_6.h>
#include <vector>
#include <dxcapi.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <unordered_map>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct PipelineAndRootSig
{
	ID3D12RootSignature* rootSig = nullptr;
	ID3D12PipelineState* pipeLineState = nullptr;
};

struct ShaderCompilationOutput
{
	ComPtr<IDxcBlob> reflectionBlob;
	ComPtr<IDxcBlob> shaderBlob;
	ComPtr<IDxcResult> result;
};

struct DescriptorHeapResource
{
	ID3D12Resource* buffer = nullptr;
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
		ID3D12GraphicsCommandList* CreateAndReturnCommandList();
		PipelineAndRootSig CreatePSO(LPCWSTR vertexShader, LPCWSTR vertexEntry, LPCWSTR pixelShader, LPCWSTR pixelEntry, UINT numConstantBuffers, UINT numShaderResources);

		bool CompileShaderDXC(LPCWSTR ShaderFilePath, LPCWSTR entryPoint, LPCWSTR profile, ShaderCompilationOutput& newOutput);

		size_t InitTexture(const wchar_t* Filepath, UINT DescriptorHeapIndex);

		UINT GetCurrentSwapChainIndex() { return mSwapChain->GetCurrentBackBufferIndex(); }
		ID3D12CommandQueue* GetCommandQueue() { return mCommandQueue; }
		ID3D12CommandAllocator* GetCommandAllocator() { return mCmdAllocator; }

		DescriptorHeapResource GetTexture(UINT HashedInput) { return mTextures[HashedInput]; }

		D3D12_RESOURCE_BARRIER GetBarrier()
		{
			D3D12_RESOURCE_BARRIER BarrierDesc = {};
			UINT bbIdx = mSwapChain->GetCurrentBackBufferIndex();
			BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			BarrierDesc.Transition.pResource = backBuffers[bbIdx];
			BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			return BarrierDesc;
		}

		UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) { return mDevice->GetDescriptorHandleIncrementSize(type); }
		ID3D12DescriptorHeap* GetDescriptorHeapHandle(int Index) { return mDescriptorHeaps[Index]; };
		void Present();

		D3D12_CPU_DESCRIPTOR_HANDLE IncrementAndReturnRTVHeaps();
		ID3D12DescriptorHeap* GetDepthStencilBufferHeap() { return mDsvHeaps; }

		std::vector<D3D12_INPUT_ELEMENT_DESC> CreateInputLayout(ShaderCompilationOutput& shaderCompData);

	private:
		// only want to call this from the DeviceManager itself
		DescriptorHeapResource CreateTexture(const wchar_t* Filepath, ID3D12DescriptorHeap* descHeap);
		std::unordered_map<UINT,DescriptorHeapResource> mTextures;

		ID3D12Device* mDevice = nullptr;
		IDXGISwapChain3* mSwapChain = nullptr;
		ID3D12CommandAllocator* mCmdAllocator = nullptr;
		ID3D12CommandQueue* mCommandQueue = nullptr;

		ID3D12DescriptorHeap* mDsvHeaps = nullptr;
		ID3D12Resource* mDepthBuffer = nullptr;

		std::vector<ID3D12DescriptorHeap*> mDescriptorHeaps;

		ID3D12DescriptorHeap* rtvHeaps = nullptr;
		std::vector<ID3D12Resource*> backBuffers;

		// compiler
		ComPtr<IDxcUtils> mUtils;
		ComPtr<IDxcIncludeHandler> mIncludeHandler;
		ComPtr<IDxcCompiler3> mCompiler;

		XMMATRIX matIdent;

		UINT mDescriptorHandleIndex = 0;

		std::wofstream* mLogFilePtr = nullptr;

};