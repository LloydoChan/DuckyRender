#include <Windows.h>

#include "D3DDeviceManager.h"

#include <tchar.h>
#include <fstream>
#include <cmath>
#include <vector>

#include <chrono>
using Clock = std::chrono::steady_clock;

#pragma comment(lib, "WindowsApp.lib")

const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;

const float MOVEMENT_SPEED = 5.0f;
const float ROTATIONAL_SPEED_YAW = 2.0f * 3.141f;
const float ROTATIONAL_SPEED_PITCH = 3.141f;

//temp for first tri
struct Vertex
	vertices[] =
{
	{{-0.5f, -0.5f, 0.f},{0.f ,0.f}},
	{{ 0.f,   0.5f, 0.f},{0.5f,1.f}},
	{{ 0.5f, -0.5f, 0.f},{1.f ,0.f}},
};

unsigned short indices[] = { 0,1,2 };

bool keys[256];

LONG mouseDeltaX = 0;
LONG mouseDeltaY = 0;


void HandleInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
	UINT keyValue = static_cast<UINT>(wParam);
	if (keyValue >= 256) return;

	if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
	{
		keys[keyValue] = true;
	}

	if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
	{
		keys[keyValue] = false;
	}

	if (msg == WM_INPUT)
	{
		UINT dataSize = 0;

		GetRawInputData(
			reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT,
			nullptr,
			&dataSize,
			sizeof(RAWINPUTHEADER));

		std::vector<std::byte> data(dataSize);

		if (GetRawInputData(
			reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT,
			data.data(),
			&dataSize,
			sizeof(RAWINPUTHEADER)) != dataSize)
		{
			return;
		}

		const RAWINPUT* rawInput =
			reinterpret_cast<const RAWINPUT*>(data.data());

		if (rawInput->header.dwType == RIM_TYPEMOUSE)
		{
			mouseDeltaX += rawInput->data.mouse.lLastX;
			mouseDeltaY += rawInput->data.mouse.lLastY;
		}
	}
}

void UpdateCameraTransform()
{

}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_INPUT:
		HandleInput(msg, wparam, lparam);
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

	RAWINPUTDEVICE rawInputDevice{};

	rawInputDevice.usUsagePage = 0x01; // Generic desktop controls
	rawInputDevice.usUsage = 0x02;     // Mouse
	rawInputDevice.dwFlags = 0;
	rawInputDevice.hwndTarget = hwnd;

	if (!RegisterRawInputDevices(
		&rawInputDevice,
		1,
		sizeof(rawInputDevice)))
	{
		throw std::runtime_error("Failed to register raw mouse input.");
	}

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
	
	XMVECTOR eye = XMVectorSet(0.f, 0.f, -3.f, 1.f);
	XMVECTOR at  = XMVectorSet(0.f, 0.f, -2.f, 1.f);
	XMVECTOR up  = XMVectorSet(0.f, 1.f,  0.f, 0.f);

	auto previousTime = Clock::now();

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		auto currentTime = Clock::now();

		float deltaTime = std::chrono::duration<float>(
			currentTime - previousTime).count();

		previousTime = currentTime;

		XMVECTOR movement    = XMVectorZero();
		XMVECTOR viewVector  = XMVectorSubtract(at, eye);
		XMVECTOR rightVector = XMVector3Cross(viewVector, up);

		LONG localMouseDeltaXCopy = mouseDeltaX;
		LONG localMouseDeltaYCopy = mouseDeltaY;

		if (localMouseDeltaXCopy != 0)
		{
			float angle = XMConvertToRadians(static_cast<float>(mouseDeltaX) * ROTATIONAL_SPEED_YAW * deltaTime);
			XMVECTOR quat = XMQuaternionRotationAxis(up, angle);
			viewVector = XMVector3Rotate(viewVector, quat);
			viewVector = XMVector3Normalize(viewVector);
		}

		XMVECTOR localDeltaYCopy = XMVector3Normalize(XMVector3Cross(up, viewVector));

		if (mouseDeltaY != 0)
		{
			float angle = XMConvertToRadians(static_cast<float>(mouseDeltaY) * ROTATIONAL_SPEED_PITCH * deltaTime);
			XMVECTOR quat = XMQuaternionRotationAxis(rightVector, angle);
			XMVECTOR possibleViewVector = XMVector3Rotate(viewVector, quat);
			possibleViewVector = XMVector3Normalize(possibleViewVector);

			const float verticalAlignment = XMVectorGetX(XMVector3Dot(possibleViewVector, up));

			constexpr float pitchLimit = 0.99f;

			if (std::abs(verticalAlignment) < pitchLimit)
			{
				viewVector = possibleViewVector;
			}
		}

		rightVector = XMVector3Normalize(XMVector3Cross(up, viewVector));

		if (keys['W'])
		{
			movement = XMVectorAdd(movement,viewVector);
		}

		if (keys['S'])
		{
			movement = XMVectorSubtract(movement, viewVector);
		}

		if (keys['A'])
		{
			movement = XMVectorAdd(movement, rightVector);
		}

		if (keys['D'])
		{
			movement = XMVectorSubtract(movement, rightVector);
		}

		if (!XMVector3Equal(movement, XMVectorZero()))
		{
			movement = XMVector3Normalize(movement);

			const XMVECTOR scaledMovement = XMVectorScale(
				movement,
				MOVEMENT_SPEED * deltaTime);

			eye = XMVectorAdd(eye, scaledMovement);
		}

		at = XMVectorAdd(eye, viewVector);

		XMMATRIX view = XMMatrixLookAtLH(eye, at, up);

		XMMATRIX world = XMMatrixRotationY(angle);
		XMMATRIX wvp = XMMatrixTranspose(world * view * projection);
		angle += 0.01f;

		mouseDeltaX = mouseDeltaY = 0;

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