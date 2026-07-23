#include "D3DDeviceManager.h"
#include <DirectXTex.h>
#include <dxgi1_6.h>
#include <vector>
#include <winrt/base.h>
#include <d3d12shader.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

struct TexRGBA
{
	unsigned char r, g, b, a;
};

std::vector<TexRGBA> textureData(256 * 256);

bool OutputErrorFromHResult(HRESULT hResult, const char* message, std::wofstream& logFile)
{
	if (hResult != S_OK)
	{
		const winrt::hstring hResultMessage = winrt::hresult_error(hResult).message().c_str();
		logFile << message << hResultMessage << std::endl;
		return false;
	}

	return true;
}

bool D3DDeviceManager::Init(HWND hWnd, UINT WindowWidth, UINT WindowHeight)
{
	logFile.open("log.txt");

	HRESULT hResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&mDevice));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating device: ", logFile)) return 1;

	IDXGIFactory6* factory = nullptr;
	hResult = CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating factory: ", logFile)) return 1;

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
		logFile << "found adapter " << desc.Description << std::endl;
	}

	if (adapters.size() == 0)
	{
		logFile << "no adapters found " << std::endl;
		return 1;
	}

	// enumerated by high perf so take first adapter in list
	IDXGIAdapter* chosenAdapter = adapters[0];

	// create compilers
	hResult = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(mUtils.ReleaseAndGetAddressOf()));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating utils: ", logFile)) return 1;

	hResult = mUtils->CreateDefaultIncludeHandler(mIncludeHandler.ReleaseAndGetAddressOf());
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating include handler: ", logFile)) return 1;

	hResult = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mCompiler));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating compiler: ", logFile)) return 1;

	hResult = mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdAllocator));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating command allocator: ", logFile)) return 1;

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hResult = mDevice->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&mCommandQueue));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating command queue: ", logFile)) return 1;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	swapChainDesc.Width = WindowWidth;
	swapChainDesc.Height = WindowHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;

	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	hResult = factory->CreateSwapChainForHwnd(mCommandQueue, hWnd, &swapChainDesc, nullptr, nullptr, (IDXGISwapChain1**)&mSwapChain);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating swapchain: ", logFile)) return false;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	rtvHeaps = nullptr;

	hResult = mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating desc heap: ", logFile)) return false;

	
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();

	for (UINT idx = 0; idx < swapChainDesc.BufferCount; idx++)
	{
		ID3D12Resource* ptr;
		backBuffers.push_back(ptr);
		hResult = mSwapChain->GetBuffer(idx, IID_PPV_ARGS(&backBuffers[idx]));

		if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem getting back buffer: ", logFile)) return false;

		mDevice->CreateRenderTargetView(backBuffers[idx], nullptr, handle);
		handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	factory->Release();

	// init directxtex file reading
	hResult = CoInitializeEx(0, COINITBASE_MULTITHREADED);
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem initializing com: ", logFile)) return false;

	return true;
}

ID3D12GraphicsCommandList* D3DDeviceManager::CreateAndReturnCommandList()
{
	ID3D12GraphicsCommandList* cmdList = nullptr;
	HRESULT hResult = mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocator, nullptr, IID_PPV_ARGS(&cmdList));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating command list: ", logFile)) return nullptr;
	return cmdList;
}

void D3DDeviceManager::Present()
{
	mSwapChain->Present(1, 0);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DDeviceManager::IncrementAndReturnRTVHeaps()
{
	// d3d12 code
	UINT bbIdx = mSwapChain->GetCurrentBackBufferIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeap = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvHeap.ptr += bbIdx * mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	return rtvHeap;
}

std::vector<D3D12_INPUT_ELEMENT_DESC> D3DDeviceManager::CreateInputLayout(ShaderCompilationOutput& shaderCompData)
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> Elems;

	ComPtr<ID3D12ShaderReflection> vertReflectionData;

	DxcBuffer reflectionData = { shaderCompData.reflectionBlob->GetBufferPointer(),
								 shaderCompData.reflectionBlob->GetBufferSize(),
								 0U};

	HRESULT hResult = mUtils->CreateReflection(&reflectionData, IID_PPV_ARGS(&vertReflectionData));

	D3D12_SHADER_DESC shaderDesc;
	vertReflectionData->GetDesc(&shaderDesc);

	for (int i = 0; i < shaderDesc.InputParameters; i++)
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


bool D3DDeviceManager::MapAndCreateVertexView(VBPair* newVertexBufferPair, Vertex* vertData, unsigned int numElems)
{
	Vertex* vertMap = nullptr;
	HRESULT hResult = newVertexBufferPair->vertBuffPointer->Map(0, nullptr, (void**)&vertMap);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem mapping buffer : ", logFile)) return false;

	std::copy(vertData, vertData + numElems, vertMap);
	newVertexBufferPair->vertBuffPointer->Unmap(0, nullptr);

	newVertexBufferPair->vbView.BufferLocation = newVertexBufferPair->vertBuffPointer->GetGPUVirtualAddress();
	newVertexBufferPair->vbView.SizeInBytes = sizeof(Vertex) * numElems;
	newVertexBufferPair->vbView.StrideInBytes = sizeof(Vertex);

	return true;
}

bool D3DDeviceManager::MapAndCreateIndexView(IBPair* newIndexBufferPair, unsigned short* indexData, unsigned int numElems)
{
	unsigned short* idxMap = nullptr;
	HRESULT hResult = newIndexBufferPair->idxBuffPointer->Map(0, nullptr, (void**)&idxMap);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem mapping buffer : ", logFile)) return false;

	std::copy(indexData, indexData + numElems, idxMap);
	newIndexBufferPair->idxBuffPointer->Unmap(0, nullptr);

	newIndexBufferPair->ibView.BufferLocation = newIndexBufferPair->idxBuffPointer->GetGPUVirtualAddress();
	newIndexBufferPair->ibView.Format = DXGI_FORMAT_R16_UINT;
	newIndexBufferPair->ibView.SizeInBytes = sizeof(unsigned short) * numElems;

	return true;
}

bool D3DDeviceManager::CompileShaderDXC(LPCWSTR ShaderFilePath, LPCWSTR entryPoint, LPCWSTR profile, ShaderCompilationOutput& newOutput)
{
	std::ifstream file(ShaderFilePath, std::ios::binary | std::ios::ate);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<char> buffer(size);
	file.read(buffer.data(), size);

	// 4. Populate DxcBuffer
	DxcBuffer dxcBuffer;
	dxcBuffer.Ptr = buffer.data();
	dxcBuffer.Size = buffer.size();
	dxcBuffer.Encoding = DXC_CP_UTF8; 

	std::vector<LPCWSTR> arguments;
	//entrypoint
	arguments.push_back(L"-E");
	arguments.push_back(entryPoint);

	//profile
	arguments.push_back(L"-T");
	arguments.push_back(profile);

	HRESULT hResult = mCompiler->Compile(&dxcBuffer, arguments.data(), (UINT32)arguments.size(), mIncludeHandler.Get(), IID_PPV_ARGS(&newOutput.result));
	if (hResult != S_OK && OutputErrorFromHResult(hResult, "couldn't compile shader", logFile)) return false;

	hResult = newOutput.result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&newOutput.reflectionBlob), nullptr);
	if (hResult != S_OK && OutputErrorFromHResult(hResult, "couldn't get reflection from result", logFile)) return false;

	hResult = newOutput.result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&newOutput.shaderBlob), nullptr);
	if (hResult != S_OK && OutputErrorFromHResult(hResult, "couldn't get shader info from result", logFile)) return false;

	return true;
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

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating committed resource : ", logFile)) return nullptr;

	return newBuff;
}

PipelineAndRootSig D3DDeviceManager::CreatePSO(LPCWSTR vertexShader, LPCWSTR vertexEntry, LPCWSTR pixelShader, LPCWSTR pixelEntry)
{
	PipelineAndRootSig newPipeline;
	
	ShaderCompilationOutput vertexShaderOutput;
	if(!CompileShaderDXC(vertexShader, vertexEntry, L"vs_6_0", vertexShaderOutput)) return newPipeline;
	ShaderCompilationOutput pixelShaderOutput;
	if (!CompileShaderDXC(pixelShader, pixelEntry, L"ps_6_0", pixelShaderOutput)) return newPipeline;

	D3D12_DESCRIPTOR_RANGE descTblRange = {};
	descTblRange.NumDescriptors = 1;
	descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange.BaseShaderRegister = 0;
	descTblRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParam = {};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam.DescriptorTable.pDescriptorRanges = &descTblRange;
	rootParam.DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSigDesc.pParameters = &rootParam;
	rootSigDesc.NumParameters = 1;

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};

	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MinLOD = 0.f;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	rootSigDesc.pStaticSamplers = &samplerDesc;
	rootSigDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;

	HRESULT hResult = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		nullptr
	);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't serialize root sig ", logFile)) return newPipeline;

	hResult = mDevice->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&newPipeline.rootSig));
	rootSigBlob->Release();
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create root sig ", logFile)) return newPipeline;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipeline = {};

	gPipeline.pRootSignature = newPipeline.rootSig;

	gPipeline.VS.pShaderBytecode = vertexShaderOutput.shaderBlob.Get()->GetBufferPointer();
	gPipeline.VS.BytecodeLength = vertexShaderOutput.shaderBlob.Get()->GetBufferSize();
	gPipeline.PS.pShaderBytecode = pixelShaderOutput.shaderBlob.Get()->GetBufferPointer();
	gPipeline.PS.BytecodeLength = pixelShaderOutput.shaderBlob.Get()->GetBufferSize();

	gPipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gPipeline.RasterizerState.MultisampleEnable = false;

	gPipeline.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
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
		OutputErrorFromHResult(hResult, "couldn't create graphics pipeline ", logFile);

	return newPipeline;
}

TextureBufferDescPair D3DDeviceManager::CreateTexture(const wchar_t* Filepath)
{
	TexMetadata metaData = {};
	ScratchImage imageData = {};
	TextureBufferDescPair newTexPair;

	HRESULT hResult = LoadFromWICFile(Filepath, WIC_FLAGS_NONE, &metaData, imageData);
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't load Texture ", logFile)) return newTexPair;

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
		IID_PPV_ARGS(&newTexPair.textureBuffer));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create texture ", logFile)) return newTexPair;

	hResult = newTexPair.textureBuffer->WriteToSubresource(0,
		nullptr,
		img->pixels,
		img->rowPitch,
		img->slicePitch);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't write to subresource ", logFile)) return newTexPair;

	ID3D12DescriptorHeap* texDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC texDescHeapDesc = {};

	texDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	texDescHeapDesc.NodeMask = 0;
	texDescHeapDesc.NumDescriptors = 1;
	texDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	hResult = mDevice->CreateDescriptorHeap(&texDescHeapDesc, IID_PPV_ARGS(&newTexPair.texDescHeap));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create texture desc heap ", logFile)) return newTexPair;

	// create the resource view
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metaData.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	mDevice->CreateShaderResourceView(newTexPair.textureBuffer, &srvDesc, newTexPair.texDescHeap->GetCPUDescriptorHandleForHeapStart());

	return newTexPair;
}

ID3D12Fence* D3DDeviceManager::CreateFence(UINT64 FenceVal)
{
	ID3D12Fence* fence = nullptr;
	HRESULT hResult = mDevice->CreateFence(FenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create fence ", logFile)) return nullptr;
	return fence;
}