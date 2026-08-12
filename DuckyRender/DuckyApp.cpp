#include "pch.h"
#include "DuckyApp.h"
#include "D3DDeviceManager.h"
#include "DuckyRenderTypes.h"

DebugVertex boxVertices[8] =
{
	{{-1,-1,-1}},
	{{ 1,-1,-1}},
	{{ 1, 1,-1}},
	{{-1, 1,-1}},

	{{-1,-1, 1}},
	{{ 1,-1, 1}},
	{{ 1, 1, 1}},
	{{-1, 1, 1}},
};


uint16_t boxIndices[24] =
{
	0,1, 1,2, 2,3, 3,0,
	4,5, 5,6, 6,7, 7,4,
	0,4, 1,5, 2,6, 3,7
};


DuckyApp::~DuckyApp() = default;

bool DuckyApp::Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName)
{
	mLogFile.open("log.txt");

	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 0; i < argc; i++)
	{
		if (wcscmp(argv[i], L"-debug") == 0) {
			ID3D12Debug* debugLayer = nullptr;
			D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
			debugLayer->EnableDebugLayer();
			debugLayer->Release();
		}

		if (wcscmp(argv[i], L"-input") == 0) {
			// get the folder name
			std::wstring path = L"..//Assets//CookedAssets//";
			std::wstring asset(argv[i + 1]);
			std::wstring suffix = L"//CookedData.Ducky";

			mInputFilePath = path + asset + suffix;
		}
	}
	LocalFree(argv);

	RECT wrc = { 0, 0, WindowWidth, WindowHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	WNDCLASSEX w = {};

	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)StaticWindowProcedure;
	mLpszClassName = w.lpszClassName = _T("DuckyApp");
	mHInstance     = w.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&w);

	mWindowHandle = CreateWindow(w.lpszClassName,
		WindowName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		w.hInstance,
		this); // need for wndproc

	RAWINPUTDEVICE rawInputDevice{};

	rawInputDevice.usUsagePage = 0x01; // Generic desktop controls
	rawInputDevice.usUsage = 0x02;     // Mouse
	rawInputDevice.dwFlags = 0;
	rawInputDevice.hwndTarget = mWindowHandle;

	if (!RegisterRawInputDevices(
		&rawInputDevice,
		1,
		sizeof(rawInputDevice)))
	{
		throw std::runtime_error("Failed to register raw mouse input.");
	}

	mDeviceManager = std::make_unique<D3DDeviceManager>();
	if (!mDeviceManager->Init(mWindowHandle, WindowWidth, WindowHeight, &mLogFile)) return false;

	if (!mPipelineManager.Init(&mLogFile, mDeviceManager->GetDevice())) return false;

	SetWindowLongPtr(
		mWindowHandle,
		GWL_STYLE,
		WS_POPUP | WS_VISIBLE);

	HMONITOR monitor = MonitorFromWindow(mWindowHandle, MONITOR_DEFAULTTOPRIMARY);

	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(monitor, &mi);

	SetWindowPos(
		mWindowHandle,
		HWND_TOP,
		mi.rcMonitor.left,
		mi.rcMonitor.top,
		mi.rcMonitor.right - mi.rcMonitor.left,
		mi.rcMonitor.bottom - mi.rcMonitor.top,
		SWP_FRAMECHANGED);

	ShowWindow(mWindowHandle, SW_SHOW);
	return true;
}

bool DuckyApp::InitGPUTimeStamps()
{
	mFrameCount = 2;

	HRESULT result = mCommandQueue->GetTimestampFrequency(&mTimeStampFrequency);

	if (FAILED(result) || mTimeStampFrequency == 0) return false;

	const UINT totalQueryCount = mFrameCount * QueriesPerFrame;

	D3D12_QUERY_HEAP_DESC queryHeapDesc{};
	queryHeapDesc.Type =
		D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	queryHeapDesc.Count = totalQueryCount;
	queryHeapDesc.NodeMask = 0;

	ID3D12Device* device = mDeviceManager->GetDevice();

	result = device->CreateQueryHeap(
		&queryHeapDesc,
		IID_PPV_ARGS(
		mQueryHeap.ReleaseAndGetAddressOf()));

	if (FAILED(result)) return false;

	const UINT64 readbackSize = static_cast<UINT64>(totalQueryCount) * sizeof(UINT64);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = readbackSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	result = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(
		mReadbackBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result)) return false;

	D3D12_RANGE readRange{ 0, static_cast<SIZE_T>(readbackSize)};

	result = mReadbackBuffer->Map(
		0,
		&readRange,
		reinterpret_cast<void**>(
		&mMappedTimestamps));

	if (FAILED(result))
	{
		mMappedTimestamps = nullptr;
		mReadbackBuffer.Reset();
		mQueryHeap.Reset();
		return false;
	}

	return true;
}

void DuckyApp::StartGPUTimeStamp(ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	const UINT startQuery = frameIndex * QueriesPerFrame;

	// Timestamp queries use EndQuery for both
	// the start and end timestamp.
	commandList->EndQuery(
		mQueryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		startQuery);
}

void DuckyApp::EndGPUTimeStamp(ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	const UINT startQuery = frameIndex * QueriesPerFrame;

	const UINT endQuery = startQuery + 1;

	commandList->EndQuery(
		mQueryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		endQuery);

	const UINT64 destinationOffset = static_cast<UINT64>(startQuery) * sizeof(UINT64);

	commandList->ResolveQueryData(
		mQueryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		startQuery,
		QueriesPerFrame,
		mReadbackBuffer.Get(),
		destinationOffset);
}

double DuckyApp::GetGPUFrameMilliSeconds(UINT frameIndex)
{
	double outMilliseconds = 0.0;

	if (mMappedTimestamps == nullptr ||
		mTimeStampFrequency == 0 ||
		frameIndex >= mFrameCount)
	{
		return false;
	}

	const UINT startIndex = frameIndex * QueriesPerFrame;

	const UINT endIndex = startIndex + 1;

	const UINT64 startTicks = mMappedTimestamps[startIndex];

	const UINT64 endTicks = mMappedTimestamps[endIndex];

	if (endTicks < startTicks) return false;

	const UINT64 elapsedTicks = endTicks - startTicks;

	outMilliseconds = static_cast<double>(elapsedTicks) * 1000.0 / static_cast<double>(mTimeStampFrequency);

	return outMilliseconds;
}

bool DuckyApp::InitGPUStats()
{
	D3D12_QUERY_HEAP_DESC queryHeapDesc{};
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
	queryHeapDesc.Count = 2;
	queryHeapDesc.NodeMask = 0;

	ID3D12Device* device = mDeviceManager->GetDevice();

	HRESULT result = device->CreateQueryHeap(
		&queryHeapDesc,
		IID_PPV_ARGS(
			mPipelineStatsHeap.ReleaseAndGetAddressOf()));

	if (FAILED(result)) return false;

	const UINT64 bufferSize = sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS) * 2;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = bufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	result = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(mPipelineStatsReadback.ReleaseAndGetAddressOf()));

	if (FAILED(result)) return false;

	return true;
}

void DuckyApp::StartGpuStats(ID3D12GraphicsCommandList* CommandList, UINT FrameIndex)
{
	CommandList->BeginQuery(
		mPipelineStatsHeap.Get(),
		D3D12_QUERY_TYPE_PIPELINE_STATISTICS,
		FrameIndex);
}

void DuckyApp::EndGPUStats(ID3D12GraphicsCommandList* CommandList, UINT FrameIndex)
{
	CommandList->EndQuery(
		mPipelineStatsHeap.Get(),
		D3D12_QUERY_TYPE_PIPELINE_STATISTICS,
		FrameIndex);

	const UINT64 destinationOffset =
		static_cast<UINT64>(FrameIndex) *
		sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);

	CommandList->ResolveQueryData(
		mPipelineStatsHeap.Get(),
		D3D12_QUERY_TYPE_PIPELINE_STATISTICS,
		FrameIndex,
		1,
		mPipelineStatsReadback.Get(),
		destinationOffset);
}

D3D12_QUERY_DATA_PIPELINE_STATISTICS DuckyApp::WriteOutGPUStats(UINT FrameIndex)
{
	D3D12_QUERY_DATA_PIPELINE_STATISTICS stats{};

	const SIZE_T offset = static_cast<SIZE_T>(FrameIndex) * sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);

	D3D12_RANGE readRange{offset, offset + sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS)};

	void* mappedData = nullptr;

	HRESULT result = mPipelineStatsReadback->Map(0, &readRange, &mappedData);

	if (SUCCEEDED(result))
	{
		const auto* frameStats = reinterpret_cast<const D3D12_QUERY_DATA_PIPELINE_STATISTICS*>(static_cast<const std::byte*>(mappedData) +offset);

		stats = *frameStats;
		D3D12_RANGE writtenRange{ 0, 0 };

		mPipelineStatsReadback->Unmap(0, &writtenRange);
	}

	return stats;
}

bool DuckyApp::InitInstanceData(std::ifstream& ModelFile)
{
	size_t numInstances = 0;

	ModelFile.read(reinterpret_cast<char*>(&numInstances), sizeof(numInstances));

	if (!ModelFile) return false;

	for (size_t instanceIndex = 0; instanceIndex < numInstances; ++instanceIndex)
	{
		DuckyMeshInstance instance;

		ModelFile.read(reinterpret_cast<char*>(&instance.mMeshDataIndex), sizeof(instance.mMeshDataIndex));
		ModelFile.read(reinterpret_cast<char*>(&instance.mTransform), sizeof(instance.mTransform));

		if (!ModelFile) return false;

		mInstances.emplace_back(std::move(instance));
	}

	const size_t bufferSize = mInstances.size() * sizeof(GPUInstance);
	mInstanceBuffer = mDeviceManager->CreateStructuredBuffer(bufferSize, sizeof(GPUInstance));

	HRESULT result = mInstanceBuffer.buffer->Map(0, nullptr, &mInstanceBuffer.mapped);

	if (FAILED(result)) return false;

	GPUInstance* dst = static_cast<GPUInstance*>(mInstanceBuffer.mapped);

	for (const DuckyMeshInstance& instance : mInstances)
	{
		XMMATRIX normal = XMMatrixInverse(nullptr, instance.mTransform);
		XMMATRIX world  = XMMatrixTranspose(instance.mTransform);
		XMStoreFloat4x4(&dst->mWorld, world);
		XMStoreFloat4x4(&dst->mNormal, normal);
		++dst;
	}

	return true;
}

bool DuckyApp::InitMaterials(std::ifstream& ModelFile)
{
	size_t numMaterials = 0;
	ModelFile.read((char*)&numMaterials, sizeof(size_t));

	// get a buffer for the materials and copy into mapped dest
	size_t bufferSize = numMaterials * sizeof(GPUMaterial);
	mMaterialBuffer = mDeviceManager->CreateStructuredBuffer(bufferSize, sizeof(GPUMaterial));

	HRESULT result = mMaterialBuffer.buffer->Map(0, nullptr, &mMaterialBuffer.mapped);
	GPUMaterial* dst = (GPUMaterial*)mMaterialBuffer.mapped;

	if (!FAILED(result))
	{
		for (int i = 0; i < numMaterials; i++)
		{
			DuckyMaterial nextMaterial{};
			ModelFile.read((char*)&nextMaterial, sizeof(DuckyMaterial));

			GPUMaterial gpuFriendly{};

			gpuFriendly.alphaCutoff		= nextMaterial.constants.alphaCutoff;
			gpuFriendly.alphaMode		= static_cast<uint32_t>(nextMaterial.constants.alphaMode);
			gpuFriendly.doubleSided		= nextMaterial.constants.doubleSided;
			gpuFriendly.RoughnessFactor = nextMaterial.constants.mRoughnessFactor;
			gpuFriendly.MetallicFactor	= nextMaterial.constants.mMetallicFactor;
			gpuFriendly.NormalScale		= nextMaterial.constants.mNormalScale;
			gpuFriendly.BaseColorFactor = nextMaterial.constants.mBaseColorFactor;

			if (nextMaterial.mBaseColorTexture != -1)
			{
				gpuFriendly.BaseColorTexture = mTextures[nextMaterial.mBaseColorTexture].heapOffset;
			}
			else
			{
				gpuFriendly.BaseColorTexture = mTextures[mBaseColorFallbackHandle].heapOffset;
			}

			if (nextMaterial.mNormalTexture != -1)
			{
				gpuFriendly.NormalTexture = mTextures[nextMaterial.mNormalTexture].heapOffset;
			}
			else
			{
				gpuFriendly.NormalTexture = mTextures[mNormalColorFallbackHandle].heapOffset;
			}

			if (nextMaterial.mMetallicRoughnessTexture != -1)
			{
				gpuFriendly.MetallicRoughnessTexture = mTextures[nextMaterial.mMetallicRoughnessTexture].heapOffset;
			}
			else
			{
				gpuFriendly.MetallicRoughnessTexture = mTextures[mMetallicRougnessFallbackHandle].heapOffset;
			}

			if (nextMaterial.mEmissive != -1)
			{
				gpuFriendly.EmissiveTexture = mTextures[nextMaterial.mEmissive].heapOffset;
			}
			else
			{
				gpuFriendly.EmissiveTexture = mTextures[mEmissiveColorFallbackHandle].heapOffset;
			}

			memcpy(dst, &gpuFriendly, sizeof(GPUMaterial));

			dst++;

			mMaterialsCPU.push_back(nextMaterial);
		}
	}
	else
	{
		return false;
	}

	return true;
}

bool DuckyApp::InitTextures(std::ifstream& ModelFile)
{
	size_t numTextures = 0;
	ModelFile.read((char*)&numTextures, sizeof(size_t));

	for (size_t i = 0; i < numTextures; i++)
	{
		size_t textureNameLength = 0;
		ModelFile.read((char*)&textureNameLength, sizeof(size_t));
		std::string fileName(textureNameLength, '\0');

		if (textureNameLength == 0)
		{
			DescriptorHeapResource dummyResource;
			mTextures.emplace_back(dummyResource);
			continue;
		}

		ModelFile.read((char*)&fileName[0], textureNameLength);

		if (!ModelFile) return false;

		const std::wstring wideTextureName(fileName.begin(), fileName.end());

		DescriptorHeapResource newResource = mDeviceManager->CreateTexture(wideTextureName.c_str());
		if (newResource.buffer == nullptr) return false;
		mTextures.emplace_back(newResource);
	}

	return true;
}

bool DuckyApp::InitMeshes(std::ifstream& ModelFile)
{
	int numMeshes = 0;
	ModelFile.read((char*)&numMeshes, sizeof(int));

	for (int i = 0; i < numMeshes; i++)
	{
		DuckyMeshData newMesh;
		int numPrims = 0;
		ModelFile.read((char*)&numPrims, sizeof(int));

		for (int j = 0; j < numPrims; j++)
		{
			int materialIndex = 0;
			size_t numberVertices = 0;
			size_t vertexOffset = 0;
			size_t numberIndices = 0;
			size_t indexOffset = 0;

			ModelFile.read((char*)&materialIndex, sizeof(int));
		
			ModelFile.read((char*)&numberVertices, sizeof(size_t));
			ModelFile.read((char*)&vertexOffset, sizeof(size_t));
			ModelFile.read((char*)&numberIndices, sizeof(size_t));
			ModelFile.read((char*)&indexOffset, sizeof(size_t));

			XMFLOAT4 min, max;
			ModelFile.read((char*)&min, sizeof(XMFLOAT4));
			ModelFile.read((char*)&max, sizeof(XMFLOAT4));

			AABB newBB(min, max);

			DuckyPrimitive newPrimitive(numberIndices, numberVertices, indexOffset, vertexOffset, materialIndex, newBB);
			newMesh.AddPrimitive(newPrimitive);
		}

		mMeshes.emplace_back(newMesh);
	}

	return true;
}

bool DuckyApp::InitVertexAndIndexMegaBuffer(std::ifstream& ModelFile)
{
	size_t numVertices = 0;
	size_t numIndices = 0;

	for (const DuckyMeshData& mesh : mMeshes)
	{
		for (const DuckyPrimitive& primitive : mesh.GetPrimitives())
		{
			numVertices += primitive.GetNumVertices();
			numIndices += primitive.GetNumIndices();
		}
	}

	size_t vertexBufferSize = numVertices * sizeof(CookedVertex);
	size_t indexBufferSize = numIndices * sizeof(unsigned int);
	//now create vertex and index buffers
	mVertices = mDeviceManager->CreateBuffer(vertexBufferSize);
	mIndices = mDeviceManager->CreateBuffer(indexBufferSize);

	void* mappedVertices = nullptr;
	HRESULT result = mVertices->Map(0, nullptr, &mappedVertices);

	if (FAILED(result)) return false;

	ModelFile.read(static_cast<char*>(mappedVertices), static_cast<std::streamsize>(vertexBufferSize));

	mVertices->Unmap(0, nullptr);

	if (!ModelFile) return false;

	void* mappedIndices = nullptr;

	result = mIndices->Map(0, nullptr,&mappedIndices);

	if (FAILED(result)) return false;

	ModelFile.read(static_cast<char*>(mappedIndices), static_cast<std::streamsize>(indexBufferSize));

	mIndices->Unmap(0, nullptr);

	if (!ModelFile) return false;

	mVbView.BufferLocation = mVertices->GetGPUVirtualAddress();
	mVbView.SizeInBytes = vertexBufferSize;
	mVbView.StrideInBytes = sizeof(CookedVertex);

	mIbView.BufferLocation = mIndices->GetGPUVirtualAddress();
	mIbView.SizeInBytes = indexBufferSize;
	mIbView.Format = DXGI_FORMAT_R32_UINT;

	return true;
}

bool DuckyApp::InitDebugDrawsVBAndIB()
{
	
	size_t vertexBufferSize = 8 * sizeof(DebugVertex);
	size_t indexBufferSize = 24 * sizeof(uint16_t);

	//now create vertex and index buffers
	mDebugVertices = mDeviceManager->CreateBuffer(vertexBufferSize);
	mDebugIndices = mDeviceManager->CreateBuffer(indexBufferSize);

	void* mappedVertices = nullptr;
	HRESULT result = mDebugVertices->Map(0, nullptr, &mappedVertices);

	if (FAILED(result)) return false;

	memcpy(mappedVertices, &boxVertices[0], sizeof(DebugVertex) * 8);

	mDebugVertices->Unmap(0, nullptr);


	void* mappedIndices = nullptr;

	result = mDebugIndices->Map(0, nullptr, &mappedIndices);

	if (FAILED(result)) return false;
	memcpy(mappedIndices, &boxIndices[0], sizeof(uint16_t) * 24);

	mDebugIndices->Unmap(0, nullptr);


	mVbDebugView.BufferLocation = mDebugVertices->GetGPUVirtualAddress();
	mVbDebugView.SizeInBytes = vertexBufferSize;
	mVbDebugView.StrideInBytes = sizeof(DebugVertex);

	mIbDebugView.BufferLocation = mDebugIndices->GetGPUVirtualAddress();
	mIbDebugView.SizeInBytes = indexBufferSize;
	mIbDebugView.Format = DXGI_FORMAT_R16_UINT;

	return true;
}

LRESULT DuckyApp::StaticWindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	DuckyApp* app = nullptr;

	if (msg == WM_NCCREATE)
	{
		auto createStruct = reinterpret_cast<CREATESTRUCT*>(lparam);

		app = static_cast<DuckyApp*>(createStruct->lpCreateParams);

		SetWindowLongPtr(
			hwnd,
			GWLP_USERDATA,
			reinterpret_cast<LONG_PTR>(app));

		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	app = reinterpret_cast<DuckyApp*>(
		GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (app)
	{
		return app->WindowProcedure(hwnd, msg, wparam, lparam);
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}