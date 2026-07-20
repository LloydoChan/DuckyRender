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

//temp for first tri
XMFLOAT3 vertices[] = 
{
	{-0.5f, -0.7f, 0.f},
	{ 0.f,   0.7f, 1.f},
	{ 0.5f, -0.7f, 0.f},
};

unsigned short indices[] = { 0,1,2 };

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

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	std::wofstream logFile("log.txt");

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

	// new for creating triangle data
	D3D12_HEAP_PROPERTIES heapprop = {};

	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};

	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = sizeof(vertices);
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* vertBuff = nullptr;

	hResult = device->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);
	
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating committed resource : ", logFile)) return 1;

	XMFLOAT3* vertMap = nullptr;
	hResult = vertBuff->Map(0, nullptr, (void**)&vertMap);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem mapping buffer : ", logFile)) return 1;

	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(vertices);
	vbView.StrideInBytes = sizeof(vertices[0]);

	ID3D12Resource* idxBuff = nullptr;

	resdesc.Width = sizeof(indices);

	hResult = device->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff)
	);
	
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem creating committed index : ", logFile)) return 1;

	unsigned short* idxMap = nullptr;
	hResult = idxBuff->Map(0, nullptr, (void**)&idxMap);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "problem mapping buffer : ", logFile)) return 1;

	std::copy(std::begin(indices), std::end(indices), idxMap);
	idxBuff->Unmap(0, nullptr);

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indices);

	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	hResult = D3DCompileFromFile(
		L"BasicVertTransformation.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS", 
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&vsBlob,
		&errorBlob
	);

	if(hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't compile vert shader basic ", logFile)) return 1;

	hResult = D3DCompileFromFile(
		L"BasicColorPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&psBlob,
		&errorBlob
	);
	
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't compile pixel shader basic ", logFile)) return 1;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* rootSigBlob = nullptr;

	hResult = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		nullptr
	);

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't serialize root sig ", logFile)) return 1;

	ID3D12RootSignature* rootSig = nullptr;
	hResult = device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
	rootSigBlob->Release();
	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't serialize root sig ", logFile)) return 1;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipeline = {};

	gPipeline.pRootSignature = rootSig;

	gPipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	gPipeline.VS.BytecodeLength  = vsBlob->GetBufferSize();
	gPipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();
	gPipeline.PS.BytecodeLength  = psBlob->GetBufferSize();

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
												0} };

	gPipeline.InputLayout.pInputElementDescs = elemLayout;
	gPipeline.InputLayout.NumElements = 1;
	gPipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	gPipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gPipeline.NumRenderTargets = 1;
	gPipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipeline.SampleDesc.Count = 1;
	gPipeline.SampleDesc.Quality = 0;

	ID3D12PipelineState* pipeline = nullptr;

	hResult = device->CreateGraphicsPipelineState(&gPipeline, IID_PPV_ARGS(&pipeline));

	if (hResult != S_OK && !OutputErrorFromHResult(hResult, "couldn't create graphics pipeline ", logFile)) return 1;

	D3D12_VIEWPORT viewport = {};
	viewport.Height = WINDOW_HEIGHT;
	viewport.Width = WINDOW_WIDTH;
	viewport.TopLeftX = viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.f;
	viewport.MinDepth = 0.f;

	D3D12_RECT scissor = {};

	scissor.left = scissor.top = 0;
	scissor.bottom = WINDOW_HEIGHT;
	scissor.right = WINDOW_WIDTH;

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
		cmdList->SetPipelineState(pipeline);
		cmdList->SetGraphicsRootSignature(rootSig);
		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissor);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->IASetVertexBuffers(0, 1, &vbView);
		cmdList->IASetIndexBuffer(&ibView);
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