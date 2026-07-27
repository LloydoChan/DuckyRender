#include "D3DDeviceManager.h"
#include "DuckyTools.h"

#include <DirectXTex.h>
#include <dxgi1_6.h>
#include <vector>
#include <D3dx12.h>
#include <d3d12shader.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "DirectXTex.lib")


bool D3DDeviceManager::Init(HWND hWnd, UINT WindowWidth, UINT WindowHeight, std::wofstream* LogFile)
{
	mLogFilePtr = LogFile;

	matIdent = XMMatrixIdentity();

	HRESULT hResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&mDevice));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating device: ", *mLogFilePtr)) return 1;

	IDXGIFactory6* factory = nullptr;
	hResult = CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating factory: ", *mLogFilePtr)) return 1;

	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;

	for (int i = 0;
		factory->EnumAdapterByGpuPreference(i,
			DXGI_GPU_PREFERENCE::DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&tmpAdapter)) != DXGI_ERROR_NOT_FOUND;
		i++)
	{
		adapters.push_back(tmpAdapter);
		DXGI_ADAPTER_DESC desc;
		tmpAdapter->GetDesc(&desc);
		*mLogFilePtr << "found adapter " << desc.Description << std::endl;
	}

	if (adapters.size() == 0)
	{
		*mLogFilePtr << "no adapters found " << std::endl;
		return 1;
	}

	// enumerated by high perf so take first adapter in list
	IDXGIAdapter* chosenAdapter = adapters[0];

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hResult = mDevice->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&mCommandQueue));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating command queue: ", *mLogFilePtr)) return false;

	mSwapChain = new DuckySwapChain();
	if (!mSwapChain->Init(this, factory, hWnd, WindowWidth, WindowHeight, LogFile)) return false;

	if(!CreateDepthBuffer(WindowWidth, WindowHeight)) return false;

	// init directxtex file reading
	hResult = CoInitializeEx(0, COINITBASE_MULTITHREADED);
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem initializing com: ", *mLogFilePtr)) return false;

	mCompiler = new DuckyCompiler;
	if(!mCompiler->Init(mLogFilePtr)) return false;

	return true;
}

ID3D12GraphicsCommandList* D3DDeviceManager::CreateAndReturnCommandList(ID3D12CommandAllocator* Allocator)
{
	ID3D12GraphicsCommandList* cmdList = nullptr;
	HRESULT hResult = mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Allocator, nullptr, IID_PPV_ARGS(&cmdList));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating command list: ", *mLogFilePtr)) return nullptr;
	return cmdList;
}

std::vector<D3D12_INPUT_ELEMENT_DESC> D3DDeviceManager::CreateInputLayout(ShaderCompilationOutput& shaderCompData)
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> Elems;

	ComPtr<ID3D12ShaderReflection> vertReflectionData;

	DxcBuffer reflectionData = { shaderCompData.reflectionBlob->GetBufferPointer(),
								 shaderCompData.reflectionBlob->GetBufferSize(),
								 0U};

	if (!mCompiler->CreateReflectionData(&reflectionData, vertReflectionData.ReleaseAndGetAddressOf())) return Elems;

	D3D12_SHADER_DESC shaderDesc;
	vertReflectionData->GetDesc(&shaderDesc);

	for (UINT i = 0; i < shaderDesc.InputParameters; i++)
	{
		D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
		vertReflectionData->GetInputParameterDesc(i, &paramDesc);
		
		// assume is a position and just change where needed
		D3D12_INPUT_ELEMENT_DESC currentDesc = {
				"POSITION",
				 0,
				 DXGI_FORMAT_R32G32B32_FLOAT,
				 0,
				 D3D12_APPEND_ALIGNED_ELEMENT,
				 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				 0
		};
		
		if (std::strcmp(paramDesc.SemanticName, "TEXCOORD") == 0)
		{
			currentDesc.SemanticName = "TEXCOORD";
			currentDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		}

		Elems.push_back(currentDesc);
	}

	return Elems;
}

size_t D3DDeviceManager::InitTexture(const wchar_t* Filepath, UINT DescriptorHeapIndex)
{
	// Alternative: Inline instantiation and call
	std::size_t quickHash = std::hash<std::wstring>{}(Filepath);
	ID3D12DescriptorHeap* dscHeap = mDescriptorHeaps[DescriptorHeapIndex];

	if(!mTextures.contains(quickHash))
	{ 
		DescriptorHeapResource newResource = CreateTexture(Filepath, dscHeap);
		if (newResource.buffer == nullptr) return -1;
		mTextures[quickHash] = newResource;
		return quickHash;
	}
	
	return quickHash;
}

ID3D12Resource* D3DDeviceManager::CreateBuffer(size_t bufferSize)
{
	// new for creating triangle data
	D3D12_HEAP_PROPERTIES heapprop = {};

	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};

	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = bufferSize;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* newBuff = nullptr;

	HRESULT hResult;

	hResult = mDevice->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&newBuff)
	);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating committed resource : ", *mLogFilePtr)) return nullptr;

	return newBuff;
}

DescriptorHeapResource D3DDeviceManager::CreateConstantBuffer(size_t bufferSize, int DescriptorHeapHandle)
{
	DescriptorHeapResource constantBuffer;

	CD3DX12_HEAP_PROPERTIES prop(D3D12_HEAP_TYPE_UPLOAD);
	auto buff = CD3DX12_RESOURCE_DESC::Buffer((bufferSize + 0xFF) & ~0xFF);

	HRESULT hResult = mDevice->CreateCommittedResource(
		&prop,
		D3D12_HEAP_FLAG_NONE,
		&buff,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constantBuffer.buffer));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create const buffer ", *mLogFilePtr)) return constantBuffer;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constantBuffer.buffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constantBuffer.buffer->GetDesc().Width;

	ID3D12DescriptorHeap* heap = mDescriptorHeaps[DescriptorHeapHandle];
	UINT offset = mDescriptorHandleIndex * mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
	cpuHandle.ptr += offset;
	mDevice->CreateConstantBufferView(&cbvDesc, cpuHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = heap->GetGPUDescriptorHandleForHeapStart();
	gpuHandle.ptr += offset;
	constantBuffer.descHandle = gpuHandle;

	constantBuffer.heapOffset = mDescriptorHandleIndex++;
	return constantBuffer;
}

PipelineAndRootSig D3DDeviceManager::CreatePSO(LPCWSTR vertexShader, LPCWSTR vertexEntry, LPCWSTR pixelShader, LPCWSTR pixelEntry, RootSignatureDesc& NewRootSigDesc)
{
	PipelineAndRootSig newPipeline;
	
	ShaderCompilationOutput vertexShaderOutput;
	if(!mCompiler->CompileShaderDXC(vertexShader, vertexEntry, L"vs_6_0", vertexShaderOutput)) return newPipeline;
	ShaderCompilationOutput pixelShaderOutput;
	if (!mCompiler->CompileShaderDXC(pixelShader, pixelEntry, L"ps_6_0", pixelShaderOutput)) return newPipeline;

	std::vector<D3D12_DESCRIPTOR_RANGE> descTables(NewRootSigDesc.parameters.size());
	
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSigDesc.pParameters = NewRootSigDesc.parameters.data();
	rootSigDesc.NumParameters = NewRootSigDesc.parameters.size();
	rootSigDesc.pStaticSamplers = NewRootSigDesc.staticSamplers.data();
	rootSigDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;

	HRESULT hResult = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		nullptr
	);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't serialize root sig ", *mLogFilePtr)) return newPipeline;

	hResult = mDevice->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&newPipeline.rootSig));
	rootSigBlob->Release();
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create root sig ", *mLogFilePtr)) return newPipeline;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipeline = {};

	gPipeline.DepthStencilState = depthStencilDesc;
	gPipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	gPipeline.pRootSignature = newPipeline.rootSig;

	gPipeline.VS.pShaderBytecode = vertexShaderOutput.shaderBlob.Get()->GetBufferPointer();
	gPipeline.VS.BytecodeLength = vertexShaderOutput.shaderBlob.Get()->GetBufferSize();
	gPipeline.PS.pShaderBytecode = pixelShaderOutput.shaderBlob.Get()->GetBufferPointer();
	gPipeline.PS.BytecodeLength = pixelShaderOutput.shaderBlob.Get()->GetBufferSize();

	gPipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gPipeline.RasterizerState.MultisampleEnable = false;

	gPipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gPipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipeline.RasterizerState.DepthClipEnable = true;

	gPipeline.BlendState.AlphaToCoverageEnable = false;
	gPipeline.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	gPipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	std::vector<D3D12_INPUT_ELEMENT_DESC> elems = CreateInputLayout(vertexShaderOutput);
	
	gPipeline.InputLayout.pInputElementDescs = elems.data();
	gPipeline.InputLayout.NumElements = elems.size();
	gPipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	gPipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gPipeline.NumRenderTargets = 1;
	gPipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipeline.SampleDesc.Count = 1;
	gPipeline.SampleDesc.Quality = 0;

	hResult = mDevice->CreateGraphicsPipelineState(&gPipeline, IID_PPV_ARGS(&newPipeline.pipeLineState));

	if (hResult != S_OK) 
		OutputErrorFromHResult(hResult, "couldn't create graphics pipeline ", *mLogFilePtr);

	return newPipeline;
}

ComPtr<ID3D12CommandAllocator> D3DDeviceManager::CreateCommandAllocator()
{
	ComPtr<ID3D12CommandAllocator> newAllocator;
	mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&newAllocator)); 
	return newAllocator;
}

bool D3DDeviceManager::Resize(UINT WindowWidth, UINT WindowHeight)
{
	if (mSwapChain == nullptr) return true;

	if (WindowWidth == 0 || WindowHeight == 0) return true;
	
	// Release the old size-dependent depth resource.
	mDepthBuffer.Reset();

	if (!mSwapChain->Resize( this, WindowWidth, WindowHeight, mLogFilePtr)) return false;

	if (!CreateDepthBuffer(WindowWidth, WindowHeight)) return false;

	return true;
}

bool D3DDeviceManager::CreateDepthBuffer(UINT WindowWidth, UINT WindowHeight)
{
	// create Depth Buffer
	D3D12_DESCRIPTOR_HEAP_DESC depthDesc = {};

	depthDesc.NumDescriptors = 1;
	depthDesc.NodeMask = 0;
	depthDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	depthDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hResult = mDevice->CreateDescriptorHeap(&depthDesc, IID_PPV_ARGS(&mDsvHeaps));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem initializing depth stencil heap ", *mLogFilePtr)) return false;

	DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.Format = depthFormat;
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC depthResDesc = {};

	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResDesc.Alignment = 0;
	depthResDesc.Width = WindowWidth;
	depthResDesc.Height = WindowHeight;
	depthResDesc.DepthOrArraySize = 1;
	depthResDesc.MipLevels = 1;
	depthResDesc.Format = depthFormat;
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.SampleDesc.Quality = 0;
	depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	mDevice->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&mDepthBuffer));

	mDepthBuffer->SetName(L"Depth Buffer");
	mDsvHeaps->SetName(L"DSV Heap");

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = depthFormat;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;

	mDevice->CreateDepthStencilView(
		mDepthBuffer.Get(),
		&dsvDesc,
		mDsvHeaps->GetCPUDescriptorHandleForHeapStart());

	return true;
}

DescriptorHeapResource D3DDeviceManager::CreateTexture(const wchar_t* Filepath, ID3D12DescriptorHeap* descHeap)
{
	TexMetadata metaData = {};
	ScratchImage imageData = {};

	DescriptorHeapResource newTexture;

	HRESULT hResult = LoadFromWICFile(Filepath, WIC_FLAGS_NONE, &metaData, imageData);
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't load Texture ", *mLogFilePtr)) return newTexture;

	auto img = imageData.GetImage(0, 0, 0);
	
	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};

	resDesc.Format = metaData.format;
	resDesc.Width = metaData.width;
	resDesc.Height = metaData.height;
	resDesc.DepthOrArraySize = metaData.arraySize;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = metaData.mipLevels;
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metaData.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


	hResult = mDevice->CreateCommittedResource(&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&newTexture.buffer));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create texture ", *mLogFilePtr)) return newTexture;

	hResult = newTexture.buffer->WriteToSubresource(0,
		nullptr,
		img->pixels,
		img->rowPitch,
		img->slicePitch);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't write to subresource ", *mLogFilePtr)) return newTexture;

	// create the resource view
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metaData.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	UINT IncrementOffset = mDescriptorHandleIndex * mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descHeap->GetCPUDescriptorHandleForHeapStart();
	cpuHandle.ptr += IncrementOffset;
	mDevice->CreateShaderResourceView(newTexture.buffer.Get(), &srvDesc, cpuHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = descHeap->GetGPUDescriptorHandleForHeapStart();
	gpuHandle.ptr += IncrementOffset;
	newTexture.heapOffset = mDescriptorHandleIndex++;
	newTexture.descHandle = gpuHandle;
	return newTexture;
}

ID3D12Fence* D3DDeviceManager::CreateFence(UINT64 FenceVal)
{
	ID3D12Fence* fence = nullptr;
	HRESULT hResult = mDevice->CreateFence(FenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create fence ", *mLogFilePtr)) return nullptr;
	return fence;
}

int D3DDeviceManager::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_FLAGS flags, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors)
{
	ID3D12DescriptorHeap* descHeap = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};

	descHeapDesc.Flags = flags;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = numDescriptors;
	descHeapDesc.Type = type;

	HRESULT hResult = mDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&descHeap));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create desc heap ", *mLogFilePtr)) return -1;

	mDescriptorHeaps.emplace_back(descHeap);

	// return index to look up heap later
	return mDescriptorHeaps.size() - 1;
}