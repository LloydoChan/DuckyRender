#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#include <winrt/base.h>
#include <tchar.h>

#include <fstream>

#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "WindowsApp.lib")

const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;

using namespace DirectX;

struct Vertex
{
	XMFLOAT3 pos;
	XMFLOAT2 uv;
};

//temp for first tri
struct Vertex
	vertices[] =
{
	{{-0.5f, -0.7f, 0.f},{0.f ,0.f}},
	{{ 0.f,   0.7f, 1.f},{0.5f,1.f}},
	{{ 0.5f, -0.7f, 0.f},{0.f ,0.f}},
};

std::wofstream logFile("log.txt");

unsigned short indices[] = { 0,1,2 };

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

struct ViewportScissor
{
	// default - wholescreen
	ViewportScissor()
	{ 
		viewport.Height = WINDOW_HEIGHT;
		viewport.Width = WINDOW_WIDTH;
		viewport.TopLeftX = viewport.TopLeftY = 0;
		viewport.MaxDepth = 1.f;
		viewport.MinDepth = 0.f; 
	
		scissor.left = scissor.top = 0;
		scissor.bottom = WINDOW_HEIGHT;
		scissor.right = WINDOW_WIDTH;
	};


	D3D12_VIEWPORT viewport = {};
	D3D12_RECT scissor = {};
};

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

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

bool MapAndCreateVertexView(VBPair* newVertexBufferPair, Vertex* vertData, unsigned int numElems)
{
	Vertex* vertMap = nullptr;
	HRESULT hResult = newVertexBufferPair->vertBuffPointer->Map(0, nullptr, (void**)&vertMap);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem mapping buffer : ", logFile)) return false;

	std::copy(vertData, vertData + numElems, vertMap);
	newVertexBufferPair->vertBuffPointer->Unmap(0, nullptr);

	newVertexBufferPair->vbView.BufferLocation = newVertexBufferPair->vertBuffPointer->GetGPUVirtualAddress();
	newVertexBufferPair->vbView.SizeInBytes = sizeof(Vertex) * numElems;
	newVertexBufferPair->vbView.StrideInBytes = sizeof(Vertex);
}

bool MapAndCreateIndexView(IBPair* newIndexBufferPair, unsigned short* indexData, unsigned int numElems)
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

ID3DBlob* CompileShaderReturnBlob(LPCWSTR ShaderFilePath, LPCSTR entryPoint, LPCSTR profile)
{
	ID3DBlob* blob = nullptr;

	HRESULT hResult = D3DCompileFromFile(
		ShaderFilePath,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entryPoint,
		profile,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&blob,
		nullptr
	);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating vs shader : ", logFile)) return nullptr;

	return blob;
}

ID3D12Resource* CreateBuffer(ID3D12Device* device, void** sourceBuffer)
{
	// new for creating triangle data
	D3D12_HEAP_PROPERTIES heapprop = {};

	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};

	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = sizeof(*sourceBuffer);
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* newBuff = nullptr;

	HRESULT hResult;

	hResult = device->CreateCommittedResource(
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

PipelineAndRootSig CreatePSO(ID3D12Device* device, LPCWSTR vertexShader, LPCWSTR pixelShader)
{
	PipelineAndRootSig newPipeline;
	ID3DBlob* vsBlob = CompileShaderReturnBlob(vertexShader, "BasicVS", "vs_5_0");
	ID3DBlob* psBlob = CompileShaderReturnBlob(pixelShader, "BasicPS", "ps_5_0");

	if (vsBlob == nullptr || psBlob == nullptr) return newPipeline;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* rootSigBlob = nullptr;

	HRESULT hResult = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		nullptr
	);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't serialize root sig ", logFile)) return newPipeline;

	hResult = device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&newPipeline.rootSig));
	rootSigBlob->Release();
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create root sig ", logFile)) return newPipeline;;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipeline = {};

	gPipeline.pRootSignature = newPipeline.rootSig;

	gPipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	gPipeline.VS.BytecodeLength = vsBlob->GetBufferSize();
	gPipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();
	gPipeline.PS.BytecodeLength = psBlob->GetBufferSize();

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


	D3D12_INPUT_ELEMENT_DESC elemLayout[] = { {"POSITION",
												0,
												DXGI_FORMAT_R32G32B32_FLOAT,
												0,
												D3D12_APPEND_ALIGNED_ELEMENT,
												D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
												0},
											   {"TEXCOORD",
												0,
												DXGI_FORMAT_R32G32_FLOAT,
												0,
												D3D12_APPEND_ALIGNED_ELEMENT,
												D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
												0}
	};

	gPipeline.InputLayout.pInputElementDescs = elemLayout;
	gPipeline.InputLayout.NumElements = 2;
	gPipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	gPipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gPipeline.NumRenderTargets = 1;
	gPipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipeline.SampleDesc.Count = 1;
	gPipeline.SampleDesc.Quality = 0;

	hResult = device->CreateGraphicsPipelineState(&gPipeline, IID_PPV_ARGS(&newPipeline.pipeLineState));
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create graphics pipeline ", logFile));

	return newPipeline;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	RECT wrc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	WNDCLASSEX w = {};

	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;
	w.lpszClassName = _T("DuckyRender");
	w.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&w);

	HWND hwnd = CreateWindow(w.lpszClassName,
		_T("Ducky!!"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		w.hInstance,
		nullptr);

	ID3D12Device* device = nullptr;
	HRESULT hResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));

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

	// create lists and queues
	ID3D12CommandAllocator* cmdAllocator = nullptr;
	ID3D12GraphicsCommandList* cmdList = nullptr;

	hResult = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating command allocator: ", logFile)) return 1;

	hResult = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator, nullptr, IID_PPV_ARGS(&cmdList));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating command list: ", logFile)) return 1;

	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hResult = device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&commandQueue));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating command queue: ", logFile)) return 1;

	IDXGISwapChain1* swapChain = nullptr;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	swapChainDesc.Width = WINDOW_WIDTH;
	swapChainDesc.Height = WINDOW_HEIGHT;
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

	hResult = factory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, (IDXGISwapChain1**)&swapChain);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating swapchain: ", logFile)) return 1;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ID3D12DescriptorHeap* rtvHeaps = nullptr;

	hResult = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating desc heap: ", logFile)) return 1;

	std::vector<ID3D12Resource*> backBuffers(swapChainDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();

	for (UINT idx = 0; idx < swapChainDesc.BufferCount; idx++)
	{
		hResult = swapChain->GetBuffer(idx, IID_PPV_ARGS(&backBuffers[idx]));

		if (!OutputErrorFromHResult(hResult, "problem getting back buffer: ", logFile)) return 1;

		device->CreateRenderTargetView(backBuffers[idx], nullptr, handle);
		handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	VBPair trianglePair;
	trianglePair.vertBuffPointer = CreateBuffer(device, (void**)&vertices);
	if (trianglePair.vertBuffPointer == nullptr) return 1;
	if (!MapAndCreateVertexView(&trianglePair, vertices, 3)) return 1;

	IBPair triangleIndexPair;
	triangleIndexPair.idxBuffPointer = CreateBuffer(device, (void**)&indices);
	if (triangleIndexPair.idxBuffPointer == nullptr) return 1;

	if(!MapAndCreateIndexView(&triangleIndexPair, indices, 3)) return 1;

	PipelineAndRootSig pipeline = CreatePSO(device, L"BasicVertTransformation.hlsl", L"BasicColorPixelShader.hlsl");
	if (pipeline.rootSig == nullptr || pipeline.pipeLineState == nullptr) return 1;

	ViewportScissor wholeScreenViewPortScissor;

	ShowWindow(hwnd, SW_SHOW);

	MSG msg = {};

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
		{
			break;
		}

		static int bufferIndex = 0;

		// d3d12 code
		auto rtvHeap = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvHeap.ptr += bufferIndex * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		float clearColor[] = {0.f, 0.f, 0.f, 1.f};

		cmdList->OMSetRenderTargets(1, &rtvHeap, true, nullptr);
		cmdList->ClearRenderTargetView(rtvHeap, clearColor, 0, nullptr);
		cmdList->SetPipelineState(pipeline.pipeLineState);
		cmdList->SetGraphicsRootSignature(pipeline.rootSig);
		cmdList->RSSetViewports(1, &wholeScreenViewPortScissor.viewport);
		cmdList->RSSetScissorRects(1, &wholeScreenViewPortScissor.scissor);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->IASetVertexBuffers(0, 1, &trianglePair.vbView);
		cmdList->IASetIndexBuffer(&triangleIndexPair.ibView);
		cmdList->DrawIndexedInstanced(3, 1, 0, 0, 0);
		cmdList->Close();

		ID3D12CommandList* cmdLists[] = { cmdList };
		commandQueue->ExecuteCommandLists(1, cmdLists);

		cmdAllocator->Reset();
		cmdList->Reset(cmdAllocator, nullptr);

		swapChain->Present(1, 0);

		bufferIndex++;
		bufferIndex %= 2;
	}

	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}