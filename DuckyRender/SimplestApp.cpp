#include "SimplestApp.h"
#include "D3DDeviceManager.h"
#include <vector>
#include <errno.h>

#include <chrono>
using Clock = std::chrono::steady_clock;

//temp for first tri
struct Vertex
	vertices[] =
{
	{{-0.5f, -0.5f, 0.f},{0.f , 0.f}},
	{{ 0.f,   0.5f, 0.f},{0.5f, 1.f}},
	{{ 0.5f, -0.5f, 0.f},{1.f , 0.f}},
};

unsigned short indices[] = { 0,1,2 };

const float MOVEMENT_SPEED = 15.f;
const float ROTATIONAL_SPEED_YAW = 4.f * 3.141f;
const float ROTATIONAL_SPEED_PITCH = 2.f * 3.141f;

bool SimplestApp::Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName)
{
	DuckyApp::Init(WindowWidth, WindowHeight, WindowName);

	std::ifstream DuckyFile;
	DuckyFile.open("CookedData.Ducky", std::ios::binary);
	if (!DuckyFile)
	{
		std::error_code ec(errno, std::generic_category());
		mLogFile << ec.message().c_str() << std::endl;
		return false;
	}

	size_t numMeshes = 0;
	DuckyFile.read((char*)&numMeshes, sizeof(size_t));

	for (int i = 0; i < numMeshes; i++)
	{
		DuckyMesh nextMesh;
		nextMesh.Init(mDeviceManager, DuckyFile);
		mMeshes.emplace_back(std::move(nextMesh));
	}

	mDescHeap = mDeviceManager->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mConstantBuffer = mDeviceManager->CreateConstantBuffer(sizeof(XMMATRIX), mDescHeap);
	if (mConstantBuffer.buffer == nullptr) return false;

	mTextureBuffer = mDeviceManager->CreateTexture(L"untitled.png", mDescHeap);
	if (mTextureBuffer.buffer == nullptr) return false;

	mPipeline = mDeviceManager->CreatePSO(L"BasicVertTransformation.hlsl", L"main", L"BasicColorPixelShader.hlsl", L"main", 1, 1);
	if (mPipeline.rootSig == nullptr || mPipeline.pipeLineState == nullptr) return 1;

	mWholeScreenViewPortScissor.scissor.left = 0;
	mWholeScreenViewPortScissor.scissor.right = WindowWidth;
	mWholeScreenViewPortScissor.scissor.top = 0;
	mWholeScreenViewPortScissor.scissor.bottom = WindowHeight;

	UINT64 fenceVal = 0;
	mFence = mDeviceManager->CreateFence(fenceVal);
	if (mFence == nullptr) return false;

	mCmdList = mDeviceManager->CreateAndReturnCommandList();
	if (mCmdList == nullptr) return false;

	mAllocator = mDeviceManager->GetCommandAllocator();
	mQueue = mDeviceManager->GetCommandQueue();

	mConstantBuffer.buffer->Map(0, nullptr, (void**)&mMappedMatrixData);

	return true;
}

LRESULT SimplestApp::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	HandleInput(msg, wparam, lparam);

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void SimplestApp::HandleInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
	UINT keyValue = static_cast<UINT>(wParam);
	if (keyValue >= 256) return;

	if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
	{
		mKeys[keyValue] = true;
	}

	if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
	{
		mKeys[keyValue] = false;
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
			mMouseDeltaX += rawInput->data.mouse.lLastX;
			mMouseDeltaY += rawInput->data.mouse.lLastY;
		}
	}
}

void SimplestApp::UpdateMovementAndRotation(XMVECTOR& ViewVector, XMVECTOR& ScaledMovement, float DeltaTime)
{
	LONG localMouseDeltaXCopy = mMouseDeltaX;
	LONG localMouseDeltaYCopy = mMouseDeltaY;

	static const XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	XMVECTOR rightVector = XMVector3Cross(ViewVector, up);

	XMVECTOR movement = XMVectorZero();

	if (localMouseDeltaXCopy != 0)
	{
		float angle = XMConvertToRadians(static_cast<float>(mMouseDeltaX) * ROTATIONAL_SPEED_YAW * DeltaTime);
		XMVECTOR quat = XMQuaternionRotationAxis(up, angle);
		ViewVector = XMVector3Rotate(ViewVector, quat);
		ViewVector = XMVector3Normalize(ViewVector);
	}

	XMVECTOR localDeltaYCopy = XMVector3Normalize(XMVector3Cross(up, ViewVector));

	if (localMouseDeltaYCopy != 0)
	{
		float angle = XMConvertToRadians(static_cast<float>(mMouseDeltaY) * ROTATIONAL_SPEED_PITCH * DeltaTime);
		XMVECTOR quat = XMQuaternionRotationAxis(rightVector, angle);
		XMVECTOR possibleViewVector = XMVector3Rotate(ViewVector, quat);
		possibleViewVector = XMVector3Normalize(possibleViewVector);

		const float verticalAlignment = XMVectorGetX(XMVector3Dot(possibleViewVector, up));

		constexpr float pitchLimit = 0.99f;

		if (std::abs(verticalAlignment) < pitchLimit)
		{
			ViewVector = possibleViewVector;
		}
	}

	rightVector = XMVector3Normalize(XMVector3Cross(up, ViewVector));

	if (mKeys['W'])
	{
		movement = XMVectorAdd(movement, ViewVector);
	}

	if (mKeys['S'])
	{
		movement = XMVectorSubtract(movement, ViewVector);
	}

	if (mKeys['A'])
	{
		movement = XMVectorAdd(movement, rightVector);
	}

	if (mKeys['D'])
	{
		movement = XMVectorSubtract(movement, rightVector);
	}

	ScaledMovement = XMVectorScale(movement, MOVEMENT_SPEED * DeltaTime);
}

void SimplestApp::AppMainLoop()
{
	float fov = XMConvertToRadians(60.0f);
	float aspect = static_cast<float>(mWholeScreenViewPortScissor.scissor.right) / static_cast<float>(mWholeScreenViewPortScissor.scissor.bottom);

	XMMATRIX projection =
		XMMatrixPerspectiveFovLH(
			fov,
			aspect,
			0.1f,
			1000.0f);

	float angle = 0.f;

	XMVECTOR eye = XMVectorSet(0.f, 0.f, -3.f, 1.f);
	XMVECTOR at = XMVectorSet(0.f, 0.f, -2.f, 1.f);
	const XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	auto previousTime = Clock::now();

	MSG msg = {};
	UINT64 fenceVal = 0;

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

		XMVECTOR movement = XMVectorZero();
		XMVECTOR viewVector = XMVectorSubtract(at, eye);

		UpdateMovementAndRotation(viewVector, movement, deltaTime);

		eye = XMVectorAdd(eye, movement);
		at = XMVectorAdd(eye, viewVector);

		XMMATRIX view = XMMatrixLookAtLH(eye, at, up);

		mMouseDeltaX = mMouseDeltaY = 0;

		float clearColor[] = { 0.f, 0.f, 0.f, 1.f };
		D3D12_RESOURCE_BARRIER barrier = mDeviceManager->GetBarrier();
		mCmdList->ResourceBarrier(1, &barrier);
		mCmdList->SetPipelineState(mPipeline.pipeLineState);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHeap = mDeviceManager->IncrementAndReturnRTVHeaps();

		mCmdList->OMSetRenderTargets(1, &rtvHeap, true, nullptr);
		mCmdList->ClearRenderTargetView(rtvHeap, clearColor, 0, nullptr);

		mCmdList->RSSetViewports(1, &mWholeScreenViewPortScissor.viewport);
		mCmdList->RSSetScissorRects(1, &mWholeScreenViewPortScissor.scissor);

		mCmdList->SetGraphicsRootSignature(mPipeline.rootSig);
		mCmdList->SetDescriptorHeaps(1, &mDescHeap);
		D3D12_GPU_DESCRIPTOR_HANDLE descHandle = mDescHeap->GetGPUDescriptorHandleForHeapStart();
		mCmdList->SetGraphicsRootDescriptorTable(mConstantBuffer.heapOffset, descHandle);
		descHandle.ptr += mDeviceManager->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		mCmdList->SetGraphicsRootDescriptorTable(mTextureBuffer.heapOffset, descHandle);

		XMMATRIX world = mMeshes[0].mTransform;
		XMMATRIX wvp = XMMatrixTranspose(world * view * projection);
		memcpy(mMappedMatrixData, &wvp, sizeof(XMFLOAT4X4));

		for (auto& mesh : mMeshes)
		{
			mesh.DrawMesh(mCmdList);
		}

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		mCmdList->ResourceBarrier(1, &barrier);
		mCmdList->Close();

		ID3D12CommandList* cmdLists[] = { mCmdList };
		mQueue->ExecuteCommandLists(1, cmdLists);
		mQueue->Signal(mFence, ++fenceVal);

		if (mFence->GetCompletedValue() != fenceVal)
		{
			auto event = CreateEvent(nullptr, false, false, nullptr);
			mFence->SetEventOnCompletion(fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}

		mAllocator->Reset();
		mCmdList->Reset(mAllocator, nullptr);

		mDeviceManager->Present();
	}

	UnregisterClass(mLpszClassName, mHInstance);
}