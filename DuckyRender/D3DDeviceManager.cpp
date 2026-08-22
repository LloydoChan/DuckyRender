#include "pch.h"
#include "D3DDeviceManager.h"
#include "DuckyTools.h"
#include "DuckyMaterial.h"


bool D3DDeviceManager::Init(HWND hWnd, UINT WindowWidth, UINT WindowHeight)
{
	if (GetModuleHandle(L"WinPixGpuCapturer.dll") == 0)
	{
		LoadLibrary(GetLatestWinPixGpuCapturerPath_Cpp17().c_str());
	}
	matIdent = XMMatrixIdentity();

	ComPtr<IDXGIFactory6> factory;
	HRESULT hResult = CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem creating factory"))) return false;

	std::vector<ComPtr<IDXGIAdapter>> adapters;
	

	for (int i = 0; ; i++)
	{
		ComPtr<IDXGIAdapter> tmpAdapter;

		hResult = factory->EnumAdapterByGpuPreference(i,
			DXGI_GPU_PREFERENCE::DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(tmpAdapter.ReleaseAndGetAddressOf()));

		if (hResult == DXGI_ERROR_NOT_FOUND) break;

		if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem enumerating adapters"))) return false;

		DXGI_ADAPTER_DESC desc{};
		tmpAdapter->GetDesc(&desc);
		DuckyLog::Info(std::wstring(L"Found adapter: ") + desc.Description);
		adapters.emplace_back(std::move(tmpAdapter));
	}

	if (adapters.size() == 0)
	{
		DuckyLog::Info(L"no adapters found ");
		return 1;
	}

	// enumerated by high perf so take first adapter in list
	IDXGIAdapter* chosenAdapter = adapters[0].Get();

	hResult = D3D12CreateDevice(chosenAdapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&mDevice));

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem creating device"))) return false;

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hResult = mDevice->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&mCommandQueue));

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem creating cmd queue"))) return false;

	mSwapChain =  std::make_unique<DuckySwapChain>();
	if (!mSwapChain->Init(this, factory.Get(), hWnd, WindowWidth, WindowHeight)) return false;

	if (!CreateDepthBufferHeap()) return false;
	if(!CreateDepthBuffer(WindowWidth, WindowHeight)) return false;

	// init directxtex file reading
	hResult = CoInitializeEx(0, COINITBASE_MULTITHREADED);
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem initializing com"))) return false;

	mCbvUavSrvDescriptorHandle = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024);

	return true;
}

DescriptorHeapResource D3DDeviceManager::CreateDefaultStructuredBuffer(size_t bufferSize, size_t elementSize)
{
	DescriptorHeapResource result;

	result.buffer = CreateDefaultBuffer(bufferSize, D3D12_RESOURCE_STATE_COPY_DEST);

	if (!result.buffer) return {};

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = static_cast<UINT>(bufferSize / elementSize);
	srvDesc.Buffer.StructureByteStride = static_cast<UINT>(elementSize);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	DescriptorAllocation allocation = AllocateCbvSrvUavDescriptor(mCbvUavSrvDescriptorHandle);

	if (!allocation.IsValid()) return {};

	mDevice->CreateShaderResourceView(result.buffer.Get(),&srvDesc, allocation.cpu);

	result.heapOffset = allocation.descriptorIndex;
	result.descHandle = allocation.gpu;

	return result;
}

MappedDescriptorHeapResource D3DDeviceManager::CreateStructuredBuffer(size_t BufferSize,size_t ElementSize)
{
	D3D12_HEAP_PROPERTIES heapprop = {};

	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty =
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference =
		D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};

	resdesc.Dimension =
		D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = BufferSize;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout =
		D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	MappedDescriptorHeapResource newBuff;

	HRESULT hResult =
		mDevice->CreateCommittedResource(
			&heapprop,
			D3D12_HEAP_FLAG_NONE,
			&resdesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&newBuff.buffer));

	
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem commiting resource"))) return {};
	
	D3D12_RANGE readRange = { 0, 0 };

	HRESULT hr = newBuff.buffer->Map(0, &readRange, &newBuff.mapped);

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't map read range"))) return {};

	// Create SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = static_cast<UINT>(BufferSize / ElementSize);
	srvDesc.Buffer.StructureByteStride = static_cast<UINT>(ElementSize);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	DescriptorAllocation allocation = AllocateCbvSrvUavDescriptor(mCbvUavSrvDescriptorHandle);

	mDevice->CreateShaderResourceView(
		newBuff.buffer.Get(),
		&srvDesc,
		allocation.cpu);

	newBuff.heapOffset = allocation.descriptorIndex;
	newBuff.descHandle = allocation.gpu;

	return newBuff;
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

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem creating committed resource"))) return {};

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

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem create const buffer"))) return {};

	DescriptorAllocation descriptor = AllocateCbvSrvUavDescriptor(mCbvUavSrvDescriptorHandle);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constantBuffer.buffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constantBuffer.buffer->GetDesc().Width;

	mDevice->CreateConstantBufferView(&cbvDesc, descriptor.cpu);

	constantBuffer.descHandle = descriptor.gpu;
	constantBuffer.heapOffset = mDescriptorHandleIndex++;

	return constantBuffer;
}

ComPtr<ID3D12Resource> D3DDeviceManager::CreateUploadBuffer(size_t BufferSize)
{
	// new for creating triangle data
	D3D12_HEAP_PROPERTIES heapprop = {};

	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};

	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = BufferSize;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> resource;

	HRESULT hResult;

	hResult = mDevice->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem creating committed resource"))) return {};

	return resource;
}

ComPtr<ID3D12Resource> D3DDeviceManager::CreateDefaultBuffer(size_t BufferSize, D3D12_RESOURCE_STATES InitialState, D3D12_RESOURCE_FLAGS ResourceFlags)
{
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = BufferSize;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = ResourceFlags;

	ComPtr<ID3D12Resource> resource;

	HRESULT hr = mDevice->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		InitialState,
		nullptr,
		IID_PPV_ARGS(&resource));

	if (FAILED(hr))
		return {};

	return resource;
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

	if (!mSwapChain->Resize( this, WindowWidth, WindowHeight)) return false;

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

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem initializing depth buffer"))) return false;

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
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"Problem initializing depth stencil heap"))) return false;

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

DescriptorHeapResource D3DDeviceManager::CreateTexture(const std::filesystem::path TexturePath)
{
	TexMetadata metaData = {};
	ScratchImage imageData = {};

	DescriptorHeapResource newTexture;

	HRESULT hResult;


	if (TexturePath.extension() == L".dds") hResult = LoadFromDDSFile(TexturePath.c_str(), DDS_FLAGS_NONE, &metaData, imageData);
	else hResult = LoadFromWICFile(TexturePath.c_str(), WIC_FLAGS_NONE, &metaData, imageData);

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't load texture"))) return newTexture;


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

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't create texture resource"))) return newTexture;
	
	newTexture.buffer->SetName(TexturePath.c_str());

	for (size_t subresourceIndex = 0; subresourceIndex < imgCount; ++subresourceIndex)
	{
		const Image& image = img[subresourceIndex];

		hResult = newTexture.buffer->WriteToSubresource(subresourceIndex,
			nullptr,
			img[subresourceIndex].pixels,
			img[subresourceIndex].rowPitch,
			img[subresourceIndex].slicePitch);
	}


	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't write to sub resource root sig"))) return newTexture;

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

DescriptorHeapResource D3DDeviceManager::CreateFallbackTexture(const wchar_t* Name, const XMFLOAT4& Color, DXGI_FORMAT Format)
{
	DescriptorHeapResource newTexture;

	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};

	resDesc.Format = Format;
	resDesc.Width = 1;
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

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't create texture"))) return newTexture;

	uint8_t pixel[4] =
	{
		static_cast<uint8_t>(Color.x * 255.0f),
		static_cast<uint8_t>(Color.y * 255.0f),
		static_cast<uint8_t>(Color.z * 255.0f),
		static_cast<uint8_t>(Color.w * 255.0f)
	};

	hResult = newTexture.buffer->WriteToSubresource(0, nullptr, pixel, 4, 4);
	newTexture.buffer->SetName(Name);

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't write to sub resource"))) return newTexture;

	// create the resource view
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = Format;
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
	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't create fence"))) return {};
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

	if (!DuckyLog::CheckHRESULT(hResult, std::wstring_view(L"couldn't create desc heap"))) return -1;

	mDescriptorHeaps.emplace_back(descHeap);

	// return index to look up heap later
	return mDescriptorHeaps.size() - 1;
}