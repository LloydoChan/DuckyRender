#pragma once

#include <d3d12.h>
#include <fstream>
#include <dxgi1_6.h>
#include <vector>
#include <dxcapi.h>
#include <wrl/client.h>
#include <DirectXMath.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct VBPair
{
	ID3D12Resource* vertBuffPointer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
};

struct IBPair
{
	ID3D12Resource* idxBuffPointer = nullptr;
	D3D12_INDEX_BUFFER_VIEW ibView = {};
};

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
		bool Init(HWND hWnd, UINT WindowWidth, UINT WindowHeight);
		bool MapAndCreateVertexView(VBPair* newVertexBufferPair, Vertex* vertData, unsigned int numElems);
		bool MapAndCreateIndexView(IBPair* newIndexBufferPair, unsigned short* indexData, unsigned int numElems);

		ID3D12Fence* CreateFence(UINT64 FenceVal);

		ID3D12DescriptorHeap* CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_FLAGS flags, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors = MAX_NUM_DESCRIPTORS_PER_HEAP);
		ID3D12Resource* CreateBuffer(size_t bufferSize);
		DescriptorHeapResource CreateConstantBuffer(size_t bufferSize, ID3D12DescriptorHeap* descHeap);
		DescriptorHeapResource CreateTexture(const wchar_t* Filepath, ID3D12DescriptorHeap* descHeap);
		ID3D12GraphicsCommandList* CreateAndReturnCommandList();
		PipelineAndRootSig CreatePSO(LPCWSTR vertexShader, LPCWSTR vertexEntry, LPCWSTR pixelShader, LPCWSTR pixelEntry, UINT numConstantBuffers, UINT numShaderResources);

		bool CompileShaderDXC(LPCWSTR ShaderFilePath, LPCWSTR entryPoint, LPCWSTR profile, ShaderCompilationOutput& newOutput);


		UINT GetCurrentSwapChainIndex() { return mSwapChain->GetCurrentBackBufferIndex(); }
		ID3D12CommandQueue* GetCommandQueue() { return mCommandQueue; }
		ID3D12CommandAllocator* GetCommandAllocator() { return mCmdAllocator; }

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

		void Present();

		D3D12_CPU_DESCRIPTOR_HANDLE IncrementAndReturnRTVHeaps();

		std::vector<D3D12_INPUT_ELEMENT_DESC> CreateInputLayout(ShaderCompilationOutput& shaderCompData);

	private:
		ID3D12Device* mDevice = nullptr;
		IDXGISwapChain3* mSwapChain = nullptr;
		ID3D12CommandAllocator* mCmdAllocator = nullptr;
		ID3D12CommandQueue* mCommandQueue = nullptr;
		ID3D12DescriptorHeap* rtvHeaps = nullptr;

		std::wofstream logFile;

		std::vector<ID3D12Resource*> backBuffers;

		// compiler
		ComPtr<IDxcUtils> mUtils;
		ComPtr<IDxcIncludeHandler> mIncludeHandler;
		ComPtr<IDxcCompiler3> mCompiler;

		XMMATRIX matIdent;

		UINT mDescriptorHandleIndex = 0;
};