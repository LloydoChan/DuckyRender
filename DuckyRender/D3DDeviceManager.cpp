#include "pch.h"
#include "D3DDeviceManager.h"
#include "DuckyTools.h"
#include "DuckyMaterial.h"


bool D3DDeviceManager::Init(HWND hWnd, UINT WindowWidth, UINT WindowHeight, std::wofstream* LogFile)
{
	mLogFilePtr = LogFile;
	if (GetModuleHandle(L"WinPixGpuCapturer.dll") == 0)
	{
		LoadLibrary(GetLatestWinPixGpuCapturerPath_Cpp17().c_str());
	}
	matIdent = XMMatrixIdentity();

	ComPtr<IDXGIFactory6> factory;
	HRESULT hResult = CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem creating factory: ", *mLogFilePtr)) return false;

	std::vector<ComPtr<IDXGIAdapter>> adapters;
	

	for (int i = 0; ;
		i++)
	{
		ComPtr<IDXGIAdapter> tmpAdapter;

		hResult = factory->EnumAdapterByGpuPreference(i,
			DXGI_GPU_PREFERENCE::DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(tmpAdapter.ReleaseAndGetAddressOf()));

		if (hResult == DXGI_ERROR_NOT_FOUND) break;

		if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem enumerating adapters: ", *mLogFilePtr)) return false;

		DXGI_ADAPTER_DESC desc{};
		tmpAdapter->GetDesc(&desc);
		*mLogFilePtr << "found adapter " << desc.Description << std::endl;
		adapters.emplace_back(std::move(tmpAdapter));
	}

	if (adapters.size() == 0)
	{
		*mLogFilePtr << "no adapters found " << std::endl;
		return 1;
	}

	// enumerated by high perf so take first adapter in list
	IDXGIAdapter* chosenAdapter = adapters[0].Get();

	hResult = D3D12CreateDevice(chosenAdapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&mDevice));

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem creating device: ", *mLogFilePtr)) return false;

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hResult = mDevice->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&mCommandQueue));

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem creating command queue: ", *mLogFilePtr)) return false;

	mSwapChain =  std::make_unique<DuckySwapChain>();
	if (!mSwapChain->Init(this, factory.Get(), hWnd, WindowWidth, WindowHeight, LogFile)) return false;

	if (!CreateDepthBufferHeap()) return false;
	if(!CreateDepthBuffer(WindowWidth, WindowHeight)) return false;

	// init directxtex file reading
	hResult = CoInitializeEx(0, COINITBASE_MULTITHREADED);
	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem initializing com: ", *mLogFilePtr)) return false;

	mCompiler = std::make_unique<DuckyCompiler>();
	if(!mCompiler->Init(mLogFilePtr)) return false;

	mCbvUavSrvDescriptorHandle = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024);

	return true;
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
		else if (std::strcmp(paramDesc.SemanticName, "NORMAL") == 0)
		{
			currentDesc.SemanticName = "NORMAL";
			currentDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		}
		else if (std::strcmp(paramDesc.SemanticName, "TANGENT") == 0)
		{
			currentDesc.SemanticName = "TANGENT";
			currentDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		}
		else if (std::strcmp(paramDesc.SemanticName, "COLOR") == 0)
		{
			currentDesc.SemanticName = "COLOR";
			currentDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		}

		Elems.push_back(currentDesc);
	}

	return Elems;
}

size_t D3DDeviceManager::InitTexture(const wchar_t* Filepath)
{
	// Alternative: Inline instantiation and call
	std::size_t quickHash = std::hash<std::wstring>{}(Filepath);

	/*if(!mTextures.contains(quickHash))
	{ 
		DescriptorHeapResource newResource = CreateTexture(Filepath);
		if (newResource.buffer == nullptr) return INVALID_HANDLE;
		mTextures[quickHash] = newResource;
		return quickHash;
	}
	*/
	return quickHash;
}

size_t D3DDeviceManager::InitFallbackTexture(const wchar_t* Name, const XMFLOAT4& InputColor)
{
	std::size_t quickHash = std::hash<std::wstring>{}(Name);

	/*if (!mTextures.contains(quickHash))
	{
		DescriptorHeapResource newResource = CreateFallbackTexture(Name, InputColor);
		if (newResource.buffer == nullptr) return INVALID_HANDLE;
		mTextures[quickHash] = newResource;
		return quickHash;
	}*/

	return quickHash;
}

DescriptorHeapResource* D3DDeviceManager::GetTexture(size_t HashedInput)
{
	/*auto it = mTextures.find(HashedInput);

	if (it == mTextures.end()) return nullptr;

	return &it->second;*/
	return 0;
}

ComPtr<ID3D12Resource> D3DDeviceManager::CreateBuffer(size_t bufferSize)
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

	ComPtr<ID3D12Resource> newBuff;

	HRESULT hResult;

	hResult = mDevice->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&newBuff)
	);

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem creating committed resource : ", *mLogFilePtr)) return {};

	return newBuff;
}

DescriptorHeapResource D3DDeviceManager::CreateConstantBuffer(size_t bufferSize)
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

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't create const buffer ", *mLogFilePtr)) return constantBuffer;

	DescriptorAllocation descriptor = AllocateCbvSrvUavDescriptor(mCbvUavSrvDescriptorHandle);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constantBuffer.buffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constantBuffer.buffer->GetDesc().Width;

	mDevice->CreateConstantBufferView(&cbvDesc, descriptor.cpu);

	constantBuffer.descHandle = descriptor.gpu;
	constantBuffer.heapOffset = mDescriptorHandleIndex++;

	return constantBuffer;
}

PipelineAndRootSig D3DDeviceManager::CreatePSO(LPCWSTR vertexShader, 
												LPCWSTR vertexEntry, 
												LPCWSTR pixelShader, 
												LPCWSTR pixelEntry, 
												RootSignatureDesc& NewRootSigDesc, 
												D3D12_GRAPHICS_PIPELINE_STATE_DESC& PipelineDesc)
{
	PipelineAndRootSig newPipeline;
	
	ShaderCompilationOutput vertexShaderOutput;
	if(!mCompiler->CompileShaderDXC(vertexShader, vertexEntry, L"vs_6_0", vertexShaderOutput)) return newPipeline;
	ShaderCompilationOutput pixelShaderOutput;
	if (!mCompiler->CompileShaderDXC(pixelShader, pixelEntry, L"ps_6_0", pixelShaderOutput)) return newPipeline;
	
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSigDesc.NumParameters = static_cast<UINT>(NewRootSigDesc.parameters.size());
	rootSigDesc.pParameters = NewRootSigDesc.parameters.empty() ? nullptr : NewRootSigDesc.parameters.data();
	rootSigDesc.NumStaticSamplers = static_cast<UINT>(NewRootSigDesc.staticSamplers.size());
	rootSigDesc.pStaticSamplers = NewRootSigDesc.staticSamplers.empty() ? nullptr : NewRootSigDesc.staticSamplers.data();

	ID3DBlob* rootSigBlob = nullptr;

	HRESULT hResult = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		nullptr
	);

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't serialize root sig ", *mLogFilePtr)) return newPipeline;

	hResult = mDevice->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&newPipeline.rootSig));
	rootSigBlob->Release();
	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't create root sig ", *mLogFilePtr)) return newPipeline;

	PipelineDesc.pRootSignature = newPipeline.rootSig.Get();

	PipelineDesc.VS.pShaderBytecode = vertexShaderOutput.shaderBlob.Get()->GetBufferPointer();
	PipelineDesc.VS.BytecodeLength = vertexShaderOutput.shaderBlob.Get()->GetBufferSize();
	PipelineDesc.PS.pShaderBytecode = pixelShaderOutput.shaderBlob.Get()->GetBufferPointer();
	PipelineDesc.PS.BytecodeLength = pixelShaderOutput.shaderBlob.Get()->GetBufferSize();

	std::vector<D3D12_INPUT_ELEMENT_DESC> elems = CreateInputLayout(vertexShaderOutput);
	
	PipelineDesc.InputLayout.pInputElementDescs = elems.data();
	PipelineDesc.InputLayout.NumElements = elems.size();
	PipelineDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	PipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	PipelineDesc.NumRenderTargets = 1;
	PipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	PipelineDesc.SampleDesc.Count = 1;
	PipelineDesc.SampleDesc.Quality = 0;

	hResult = mDevice->CreateGraphicsPipelineState(&PipelineDesc, IID_PPV_ARGS(&newPipeline.pipeLineState));

	if (FAILED(hResult)) 
		OutputErrorFromHResult(hResult, "couldn't create graphics pipeline ", *mLogFilePtr);

	return newPipeline;
}

ComPtr<ID3D12CommandAllocator> D3DDeviceManager::CreateCommandAllocator()
{
	ComPtr<ID3D12CommandAllocator> newAllocator;
	HRESULT hResult = mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&newAllocator)); 
	if (FAILED(hResult)) return {};
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

	HRESULT hResult = mDevice->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&mDepthBuffer));

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem initializing depth buffer ", *mLogFilePtr)) return false;

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

bool D3DDeviceManager::CreateDepthBufferHeap()
{
	// create Depth Buffer
	D3D12_DESCRIPTOR_HEAP_DESC depthDesc = {};

	depthDesc.NumDescriptors = 1;
	depthDesc.NodeMask = 0;
	depthDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	depthDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hResult = mDevice->CreateDescriptorHeap(&depthDesc, IID_PPV_ARGS(&mDsvHeaps));
	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "problem initializing depth stencil heap ", *mLogFilePtr)) return false;
	return true;
}

DescriptorAllocation D3DDeviceManager::AllocateCbvSrvUavDescriptor(UINT descriptorHeapHandle)
{
	DescriptorAllocation allocation{};

	if (descriptorHeapHandle >= mDescriptorHeaps.size()) return allocation;

	ID3D12DescriptorHeap* heap = mDescriptorHeaps[descriptorHeapHandle].Get();

	if (heap == nullptr) return allocation;

	const UINT descriptorIndex = mDescriptorHandleIndex++;

	if (descriptorIndex >= MAX_NUM_DESCRIPTORS_PER_HEAP) return allocation;

	const UINT descriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	allocation.cpu = heap->GetCPUDescriptorHandleForHeapStart();
	allocation.gpu = heap->GetGPUDescriptorHandleForHeapStart();
	allocation.cpu.ptr += static_cast<SIZE_T>(descriptorIndex) * descriptorSize;
	allocation.gpu.ptr += static_cast<UINT64>(descriptorIndex) * descriptorSize;

	allocation.heapIndex = descriptorHeapHandle;
	allocation.descriptorIndex = descriptorIndex;

	return allocation;
}

DescriptorHeapResource D3DDeviceManager::CreateTexture(const wchar_t* Filepath)
{
	TexMetadata metaData = {};
	ScratchImage imageData = {};

	DescriptorHeapResource newTexture;

	HRESULT hResult;

	const std::filesystem::path path(Filepath);

	if (path.extension() == L".dds") hResult = LoadFromDDSFile(Filepath, DDS_FLAGS_NONE, &metaData, imageData);
	else hResult = LoadFromWICFile( Filepath, WIC_FLAGS_NONE, &metaData,imageData);

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't load Texture ", *mLogFilePtr)) return newTexture;


	auto img = imageData.GetImages();
	size_t imgCount = imageData.GetImageCount();
	
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

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't create texture ", *mLogFilePtr)) return newTexture;
	
	newTexture.buffer->SetName(Filepath);

	for (size_t subresourceIndex = 0; subresourceIndex < imgCount; ++subresourceIndex)
	{
		const Image& image = img[subresourceIndex];

		hResult = newTexture.buffer->WriteToSubresource(subresourceIndex,
			nullptr,
			img[subresourceIndex].pixels,
			img[subresourceIndex].rowPitch,
			img[subresourceIndex].slicePitch);
	}


	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't write to subresource ", *mLogFilePtr)) return newTexture;

	// create the resource view
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metaData.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = metaData.mipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;

	DescriptorAllocation allocation = AllocateCbvSrvUavDescriptor(mCbvUavSrvDescriptorHandle);

	mDevice->CreateShaderResourceView(newTexture.buffer.Get(), &srvDesc, allocation.cpu);
	newTexture.heapOffset = allocation.descriptorIndex;
	newTexture.descHandle = allocation.gpu;
	return newTexture;
}

DescriptorHeapResource D3DDeviceManager::CreateFallbackTexture(const wchar_t* Name, const XMFLOAT4& Color)
{
	DescriptorHeapResource newTexture;

	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};

	resDesc.Format = DXGI_FORMAT_BC7_UNORM_SRGB;
	resDesc.Width = 4;
	resDesc.Height =1;
	resDesc.DepthOrArraySize = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


	HRESULT hResult = mDevice->CreateCommittedResource(&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&newTexture.buffer));

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't create texture ", *mLogFilePtr)) return newTexture;

	hResult = newTexture.buffer->WriteToSubresource(0, nullptr, &Color, 1, 1);
	newTexture.buffer->SetName(Name);

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't write to subresource ", *mLogFilePtr)) return newTexture;

	// create the resource view
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_BC7_UNORM_SRGB;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;

	DescriptorAllocation descriptor = AllocateCbvSrvUavDescriptor(mCbvUavSrvDescriptorHandle);

	mDevice->CreateShaderResourceView(newTexture.buffer.Get(), &srvDesc, descriptor.cpu);

	newTexture.heapOffset = descriptor.descriptorIndex;
	newTexture.descHandle = descriptor.gpu;

	return newTexture;
}

ComPtr<ID3D12Fence> D3DDeviceManager::CreateFence(UINT64 FenceVal)
{
	ComPtr<ID3D12Fence> fence = nullptr;
	HRESULT hResult = mDevice->CreateFence(FenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf()));
	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't create fence ", *mLogFilePtr)) return {};
	return fence;
}

int D3DDeviceManager::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_FLAGS flags, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors)
{
	ComPtr<ID3D12DescriptorHeap> descHeap;

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};

	descHeapDesc.Flags = flags;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = numDescriptors;
	descHeapDesc.Type = type;

	HRESULT hResult = mDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(descHeap.ReleaseAndGetAddressOf()));

	if (FAILED(hResult) && !OutputErrorFromHResult(hResult, "couldn't create desc heap ", *mLogFilePtr)) return -1;

	mDescriptorHeaps.emplace_back(descHeap);

	// return index to look up heap later
	return mDescriptorHeaps.size() - 1;
}