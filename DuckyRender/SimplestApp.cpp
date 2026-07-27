#include "SimplestApp.h"
#include "D3DDeviceManager.h"
#include "DuckyGraphicsContext.h"
#include <vector>
#include <errno.h>

#include <chrono>
using Clock = std::chrono::steady_clock;

const float MOVEMENT_SPEED = 15.f;
const float ROTATIONAL_SPEED_YAW = 4.f * 3.141f;
const float ROTATIONAL_SPEED_PITCH = 2.f * 3.141f;

bool SimplestApp::Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName)
{
	if (!DuckyApp::Init(WindowWidth, WindowHeight, WindowName)) return false;
	mCbvSrvUavHandle = mDeviceManager->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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
		DuckyMeshData nextMesh;
		nextMesh.Init(mDeviceManager, DuckyFile, mCbvSrvUavHandle);
		mMeshes.emplace_back(std::move(nextMesh));
	}

	mDuckyContext = new DuckyGraphicsContext;
	mDuckyContext->Init(mDeviceManager, &mLogFile);

	RootSignatureDesc DrawSig = {};

	D3D12_DESCRIPTOR_RANGE descTables[2]{};
	D3D12_ROOT_PARAMETER rootParams[2]{};
	
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descTables[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTables[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	for (UINT inputCounter = 0; inputCounter < 2; inputCounter++)
	{
		descTables[inputCounter].NumDescriptors = 1;
		descTables[inputCounter].BaseShaderRegister = 0;
		descTables[inputCounter].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		rootParams[inputCounter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		
		rootParams[inputCounter].DescriptorTable.pDescriptorRanges = &descTables[inputCounter];
		rootParams[inputCounter].DescriptorTable.NumDescriptorRanges = 1;
		DrawSig.parameters.emplace_back(rootParams[inputCounter]);
	}

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

	DrawSig.staticSamplers.emplace_back(samplerDesc);

	mPipeline = mDeviceManager->CreatePSO(L"BasicVertTransformation.hlsl", L"main", L"BasicColorPixelShader.hlsl", L"main", DrawSig);
	if (mPipeline.rootSig == nullptr || mPipeline.pipeLineState == nullptr) return false;

	mWholeScreenViewPortScissor.scissor.left = 0;
	mWholeScreenViewPortScissor.scissor.right = WindowWidth;
	mWholeScreenViewPortScissor.scissor.top = 0;
	mWholeScreenViewPortScissor.scissor.bottom = WindowHeight;

	UINT64 fenceVal = 0;
	mFence = mDeviceManager->CreateFence(fenceVal);
	if (mFence == nullptr) return false;

	mQueue = mDeviceManager->GetCommandQueue();

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

	if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && wParam == VK_ESCAPE)
	{
		PostQuitMessage(0);
		return;
	}

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

	if (msg == WM_SIZE)
	{
		// handle this later
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
		XMVECTOR quat = XMQuaternionRotationAxis(rightVector, -angle);
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
		movement = XMVectorSubtract(movement, rightVector);
	}

	if (mKeys['D'])
	{
		movement = XMVectorAdd(movement, rightVector);
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

	ID3D12DescriptorHeap* heapPtr = mDeviceManager->GetDescriptorHeapHandle(mCbvSrvUavHandle);

	ID3D12GraphicsCommandList* list = mDuckyContext->GetCommandList();

	if (list == nullptr) return;

	if (FAILED(list->Close()))
	{
		return;
	}
	
	auto event = CreateEvent(nullptr, false, false, nullptr);

	bool bRunning = true;

	while (bRunning)
	{
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				bRunning = false;
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!bRunning) break;

		UINT currentFrame = mDeviceManager->GetCurrentFrameIndex();

		
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

		mDuckyContext->BeginFrame(currentFrame, mFence, mPipeline.pipeLineState, event);
		D3D12_RESOURCE_BARRIER barrier = mDeviceManager->GetBarrier();
		list->ResourceBarrier(1, &barrier);
		list->SetPipelineState(mPipeline.pipeLineState);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHeap = mDeviceManager->IncrementAndReturnRTVHeaps();
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDeviceManager->GetDepthStencilBufferHeap()->GetCPUDescriptorHandleForHeapStart();

		list->OMSetRenderTargets(1, &rtvHeap, true, &dsvHandle);
		list->ClearRenderTargetView(rtvHeap, clearColor, 0, nullptr);
		list->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		list->RSSetViewports(1, &mWholeScreenViewPortScissor.viewport);
		list->RSSetScissorRects(1, &mWholeScreenViewPortScissor.scissor);

		list->SetGraphicsRootSignature(mPipeline.rootSig);
		list->SetDescriptorHeaps(1, &heapPtr);

		XMMATRIX world = XMMatrixIdentity();
		XMMATRIX wvp = XMMatrixTranspose(world * view * projection);
		//memcpy(frameInfo.mMappedMatrixData, &wvp, sizeof(XMFLOAT4X4));

		//list->SetGraphicsRootDescriptorTable(0, frameInfo.TransformUpdateBuffer.descHandle);

		for (auto& mesh : mMeshes)
		{
			mesh.DrawMesh(list, mDeviceManager);
		}

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		list->ResourceBarrier(1, &barrier);
		list->Close();

		ID3D12CommandList* cmdLists[] = {list};
		mQueue->ExecuteCommandLists(1, cmdLists);
		mDeviceManager->Present();

		mDuckyContext->EndFrame(currentFrame, mQueue, mFence);
	}

	CloseHandle(event);

	UnregisterClass(mLpszClassName, mHInstance);
}