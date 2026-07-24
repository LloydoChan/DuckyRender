#include <Windows.h>

#include "D3DDeviceManager.h"

#include <tchar.h>
#include <fstream>
#include <cmath>
#include <vector>


#pragma comment(lib, "WindowsApp.lib")

const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;

//temp for first tri
struct Vertex
	vertices[] =
{
	{{-0.5f, -0.5f, 0.f},{0.f ,0.f}},
	{{ 0.f,   0.5f, 0.f},{0.5f,1.f}},
	{{ 0.5f, -0.5f, 0.f},{1.f ,0.f}},
};

unsigned short indices[] = { 0,1,2 };

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
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 0; i < argc; i++)
	{
		if (wcscmp(argv[i],L"-debug")) {
			ID3D12Debug* debugLayer = nullptr;
			D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
			debugLayer->EnableDebugLayer();
			debugLayer->Release();
		}
	}
	LocalFree(argv);

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

	D3DDeviceManager devManager;
	if (!devManager.Init(hwnd, WINDOW_WIDTH, WINDOW_HEIGHT)) return 0;
	

	VBPair trianglePair;
	trianglePair.vertBuffPointer = devManager.CreateBuffer(sizeof(Vertex) * 3);
	if (trianglePair.vertBuffPointer == nullptr) return 1;
	if (!devManager.MapAndCreateVertexView(&trianglePair, vertices, 3)) return 1;

	IBPair triangleIndexPair;
	triangleIndexPair.idxBuffPointer = devManager.CreateBuffer(sizeof(unsigned short) * 3);
	if (triangleIndexPair.idxBuffPointer == nullptr) return 1;

	if(!devManager.MapAndCreateIndexView(&triangleIndexPair, indices, 3)) return 1;

	ID3D12DescriptorHeap* descHeap = devManager.CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	DescriptorHeapResource constantBuffer = devManager.CreateConstantBuffer(sizeof(XMMATRIX), descHeap);
	if (constantBuffer.buffer == nullptr) return 1;

	DescriptorHeapResource textureBuffer = devManager.CreateTexture(L"untitled.png", descHeap);
	if (textureBuffer.buffer == nullptr) return 1;


	PipelineAndRootSig pipeline = devManager.CreatePSO(L"BasicVertTransformation.hlsl", L"main", L"BasicColorPixelShader.hlsl", L"main", 1, 1);
	if (pipeline.rootSig == nullptr || pipeline.pipeLineState == nullptr) return 1;

	ViewportScissor wholeScreenViewPortScissor(WINDOW_WIDTH, WINDOW_HEIGHT);

	ShowWindow(hwnd, SW_SHOW);

	MSG msg = {};

	UINT64 fenceVal = 0;

	ID3D12Fence* fence = devManager.CreateFence(fenceVal);

	ID3D12GraphicsCommandList* cmdList = devManager.CreateAndReturnCommandList();
	if (cmdList == nullptr) return 1;

	ID3D12CommandAllocator* allocator = devManager.GetCommandAllocator();
	ID3D12CommandQueue* queue = devManager.GetCommandQueue();

	XMVECTOR eye = XMVectorSet(0.0f, 0.0f, -3.0f, 1.0f); 
	XMVECTOR at  = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);  
	XMVECTOR up  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);  

	XMMATRIX view = XMMatrixLookAtLH(eye, at, up);

	float fov = XMConvertToRadians(60.0f);
	float aspect = WINDOW_WIDTH / static_cast<float>(WINDOW_HEIGHT);

	XMMATRIX projection =
		XMMatrixPerspectiveFovLH(
			fov,
			aspect,
			0.1f,
			1000.0f);

	float angle = 0.f;

	void* mappedData = nullptr;
	constantBuffer.buffer->Map(0, nullptr, (void**)&mappedData);
	

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

		XMMATRIX world = XMMatrixRotationY(angle);
		XMMATRIX wvp = XMMatrixTranspose(world * view * projection);
		angle += 0.01f;

		if (angle > 2.f * 3.141f) angle = 0.f;

		memcpy(mappedData, &wvp, sizeof(XMFLOAT4X4));

		float clearColor[] = {0.f, 0.f, 0.f, 1.f};
		D3D12_RESOURCE_BARRIER barrier =  devManager.GetBarrier();
		cmdList->ResourceBarrier(1, &barrier);
		cmdList->SetPipelineState(pipeline.pipeLineState);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHeap = devManager.IncrementAndReturnRTVHeaps();
		
		cmdList->OMSetRenderTargets(1, &rtvHeap, true, nullptr);
		cmdList->ClearRenderTargetView(rtvHeap, clearColor, 0, nullptr);

		cmdList->RSSetViewports(1, &wholeScreenViewPortScissor.viewport);
		cmdList->RSSetScissorRects(1, &wholeScreenViewPortScissor.scissor);

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->IASetVertexBuffers(0, 1, &trianglePair.vbView);
		cmdList->IASetIndexBuffer(&triangleIndexPair.ibView);

		cmdList->SetGraphicsRootSignature(pipeline.rootSig);

		cmdList->SetDescriptorHeaps(1, &descHeap);
		D3D12_GPU_DESCRIPTOR_HANDLE descHandle = descHeap->GetGPUDescriptorHandleForHeapStart();
		cmdList->SetGraphicsRootDescriptorTable(constantBuffer.heapOffset, descHandle);
		descHandle.ptr += devManager.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmdList->SetGraphicsRootDescriptorTable(textureBuffer.heapOffset, descHandle);
		cmdList->DrawIndexedInstanced(3, 1, 0, 0, 0);

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		cmdList->ResourceBarrier(1, &barrier);
		cmdList->Close();

		ID3D12CommandList* cmdLists[] = { cmdList};
		queue->ExecuteCommandLists(1, cmdLists);
		queue->Signal(fence, ++fenceVal);

		if (fence->GetCompletedValue() != fenceVal)
		{
			auto event = CreateEvent(nullptr, false, false, nullptr);
			fence->SetEventOnCompletion(fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}

		allocator->Reset();
		cmdList->Reset(allocator, nullptr);

		devManager.Present();
	}

	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}