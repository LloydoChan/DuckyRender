#include "SimplestApp.h"
#include "D3DDeviceManager.h"
#include <vector>
#include <errno.h>
#include "DuckyTools.h"

#include <chrono>
using Clock = std::chrono::steady_clock;

const float MOVEMENT_SPEED = 15.f;
const float ROTATIONAL_SPEED_YAW = 4.f * 3.141f;
const float ROTATIONAL_SPEED_PITCH = 2.f * 3.141f;

namespace RootParameter
{
	constexpr UINT PerFrame = 0;
	constexpr UINT PerInstance = 1;
	constexpr UINT PerMaterial = 2;
	constexpr UINT BaseColorTexture = 3;
	constexpr UINT NormalTexture = 4;
	constexpr UINT MetallicRoughnessTexture = 5;
	constexpr UINT Count = 6;

}

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

	size_t numInstances = 0;

	DuckyFile.read(
		reinterpret_cast<char*>(&numInstances),
		sizeof(numInstances));

	if (!DuckyFile)
	{
		return false;
	}

	mInstances.reserve(numInstances);

	for (size_t instanceIndex = 0; instanceIndex < numInstances; ++instanceIndex)
	{
		DuckyMeshInstance instance;

		DuckyFile.read( reinterpret_cast<char*>(&instance.mMeshDataIndex), sizeof(instance.mMeshDataIndex));
		DuckyFile.read( reinterpret_cast<char*>(&instance.mTransform), sizeof(instance.mTransform));

		if (!DuckyFile) return false;

		mInstances.emplace_back(std::move(instance));
	}

	size_t numMeshes = 0;
	DuckyFile.read((char*)&numMeshes, sizeof(size_t));
	std::streampos p = 0;
	for (int i = 0; i < numMeshes; i++)
	{
		DuckyMeshData nextMesh;
		if(!nextMesh.Init(mDeviceManager.get(), DuckyFile, mCbvSrvUavHandle)) return false;
		mMeshes.emplace_back(std::move(nextMesh));
	}

	size_t totalPrimitiveDraws = 0;

	for (const DuckyMeshInstance& instance :mInstances)
	{
		if (instance.mMeshDataIndex < 0 || instance.mMeshDataIndex >= static_cast<int>(mMeshes.size())) continue;

		totalPrimitiveDraws += mMeshes[instance.mMeshDataIndex].GetPrimitiveCount();
	}

	// work out number of bytes for matrices needed - instances plus the world proj matrix
	const UINT64 neededCapacity = AlignConstantBufferSize(sizeof(PerFrameConstants)) +
								  numInstances * AlignConstantBufferSize(sizeof(PerInstanceConstants)) +
								  totalPrimitiveDraws * AlignConstantBufferSize(sizeof(MaterialConstants));

	mDuckyContext = std::make_unique<DuckyGraphicsContext>();
	if(!mDuckyContext->Init(mDeviceManager.get(), neededCapacity, &mLogFile)) return false;

	RootSignatureDesc drawSig = {};

	D3D12_ROOT_PARAMETER rootParams[RootParameter::Count]{};

	// Root parameter 0:
	// Direct root CBV at b0 for the vertex shader.
	rootParams[RootParameter::PerFrame].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[RootParameter::PerFrame].Descriptor.ShaderRegister = 0;
	rootParams[RootParameter::PerFrame].Descriptor.RegisterSpace = 0;
	rootParams[RootParameter::PerFrame].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	// Root parameter 1:
	// Direct root CBV at b0 for the vertex shader.
	rootParams[RootParameter::PerInstance].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[RootParameter::PerInstance].Descriptor.ShaderRegister = 1;
	rootParams[RootParameter::PerInstance].Descriptor.RegisterSpace = 0;
	rootParams[RootParameter::PerInstance].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	// Root parameter 1:
	// SRV descriptor table at t0 for the pixel shader.
	D3D12_DESCRIPTOR_RANGE srvRange = {};

	srvRange.RangeType =D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0;
	srvRange.RegisterSpace = 0;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParams[RootParameter::PerMaterial].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[RootParameter::PerMaterial].Descriptor.ShaderRegister = 2;
	rootParams[RootParameter::PerMaterial].Descriptor.RegisterSpace = 0;
	rootParams[RootParameter::PerMaterial].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_DESCRIPTOR_RANGE textureRanges[3]{};

	for (UINT index = 0; index < 3; ++index)
	{
		textureRanges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureRanges[index].NumDescriptors = 1;
		textureRanges[index].BaseShaderRegister = index;
		textureRanges[index].RegisterSpace = 0;
		textureRanges[index].OffsetInDescriptorsFromTableStart =D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

	for (UINT index = 0; index < 3; ++index)
	{
		const UINT rootIndex = RootParameter::BaseColorTexture + index;
		rootParams[rootIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[rootIndex].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[rootIndex].DescriptorTable.pDescriptorRanges = &textureRanges[index];
		rootParams[rootIndex].ShaderVisibility =D3D12_SHADER_VISIBILITY_PIXEL;
	}

	for (const auto& param : rootParams)
	{
		drawSig.parameters.emplace_back(param);
	}

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};

	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxAnisotropy = 8;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MinLOD = 0.f;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	drawSig.staticSamplers.emplace_back(samplerDesc);

	mPipeline = mDeviceManager->CreatePSO(L"BasicVertTransformation.hlsl", L"main", L"BasicColorPixelShader.hlsl", L"main", drawSig);
	if (mPipeline.rootSig == nullptr || mPipeline.pipeLineState == nullptr) return false;

	mWholeScreenViewPortScissor.scissor.left = 0;
	mWholeScreenViewPortScissor.scissor.right = WindowWidth;
	mWholeScreenViewPortScissor.scissor.top = 0;
	mWholeScreenViewPortScissor.scissor.bottom = WindowHeight;

	UINT64 fenceVal = 0;
	mFence = mDeviceManager->CreateFence(fenceVal);
	if (mFence == nullptr) return false;

	mQueue = mDeviceManager->GetCommandQueue();

	mFenceEvent = CreateEvent(nullptr, false, false, nullptr);

	if (mFenceEvent == nullptr) return false;

	for (int i = 0; i < 2; i++)
	{
		mMatrixBuffer[i] = mDeviceManager->CreateConstantBuffer(sizeof(XMMATRIX), mCbvSrvUavHandle);
		if (mMatrixBuffer[i].buffer == nullptr) return false;
		HRESULT hResult = mMatrixBuffer[i].buffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedTransform[i]));

		if (FAILED(hResult))return false;
	}
	
	return true;
}

LRESULT SimplestApp::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	if (msg == WM_SIZE)
	{
		if (wparam == SIZE_MINIMIZED)
		{
			mMinimized = true;
			return 0;
		}

		mMinimized = false;

		const UINT width = static_cast<UINT>(LOWORD(lparam));

		const UINT height = static_cast<UINT>(HIWORD(lparam));

		if (mDuckyContext != nullptr &&  mDeviceManager != nullptr && width > 0 && height > 0)
		{
			if(!Resize(width, height)) PostQuitMessage(1);
		}

		return 0;
	}

	HandleInput(msg, wparam, lparam);

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void SimplestApp::HandleInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
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

	if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN  || msg == WM_KEYUP || msg == WM_SYSKEYUP)
	{
		UINT keyValue = static_cast<UINT>(wParam);
		if (keyValue >= 256) return;

		if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
			return;
		}

		if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
		{
			mKeys[keyValue] = false;
		}

		if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
		{
			mKeys[keyValue] = true;
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

bool SimplestApp::BindMaterial( ID3D12GraphicsCommandList* commandList, ConstantBufferAllocator& allocator, const DuckyMaterial& material)
{
	ConstantBufferAllocation allocation = allocator.AllocateConstantBuffer(sizeof(MaterialConstants));

	if (allocation.mCpuAddress == nullptr) return false;

	std::memcpy(allocation.mCpuAddress, &material.constants, sizeof(MaterialConstants));

	commandList->SetGraphicsRootConstantBufferView(RootParameter::PerMaterial, allocation.mGpuAddress);

	BindTexture( commandList, RootParameter::BaseColorTexture, material.mBaseColorTexture);
	BindTexture(commandList, RootParameter::NormalTexture, material.mNormalTexture);
	BindTexture(commandList, RootParameter::MetallicRoughnessTexture, material.mMetallicRoughnessTexture);

	return true;
}

void SimplestApp::BindTexture(ID3D12GraphicsCommandList* commandList, UINT rootParameter, size_t textureHandle)
{
	DescriptorHeapResource* texture = mDeviceManager->GetTexture(textureHandle);

	if (texture == nullptr)
	{
		// TODO
		// Bind an appropriate fallback descriptor here.
		return;
	}

	commandList->SetGraphicsRootDescriptorTable(rootParameter,texture->descHandle);
}

bool SimplestApp::BindInstanceConstants( ID3D12GraphicsCommandList* commandList, ConstantBufferAllocator& allocator, const DuckyMeshInstance& instance)
{
	ConstantBufferAllocation allocation = allocator.AllocateConstantBuffer( sizeof(PerInstanceConstants));

	if (allocation.mCpuAddress == nullptr) return false;

	auto* constants = static_cast<PerInstanceConstants*>(allocation.mCpuAddress);

	XMStoreFloat4x4(&constants->world,XMMatrixTranspose(instance.mTransform));

	commandList->SetGraphicsRootConstantBufferView(RootParameter::PerInstance, allocation.mGpuAddress);

	return true;
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

	XMVECTOR eye = XMVectorSet(0.f, 50.f, -50.f, 1.f);
	XMVECTOR at = XMVectorSet(0.f, 0.f, 0.f, 1.f);
	const XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	auto previousTime = Clock::now();

	MSG msg = {};
	UINT64 fenceVal = 0;

	ID3D12DescriptorHeap* heapPtr = mDeviceManager->GetDescriptorHeapHandle(mCbvSrvUavHandle);

	ID3D12GraphicsCommandList* list = mDuckyContext->GetCommandList();

	if (list == nullptr) return;

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

		if (mMinimized)
		{
			WaitMessage();
			continue;
		}

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

		if (!mDuckyContext->BeginFrame(currentFrame, mFence.Get(), mPipeline.pipeLineState.Get(), mFenceEvent, &mLogFile)) break;
		ConstantBufferAllocator* cbvAllocator = mDuckyContext->GetBufferAllocator(currentFrame);
		D3D12_RESOURCE_BARRIER barrier = mDeviceManager->GetBarrier();
		list->ResourceBarrier(1, &barrier);
		list->SetPipelineState(mPipeline.pipeLineState.Get());

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHeap = mDeviceManager->IncrementAndReturnRTVHeaps();
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDeviceManager->GetDepthStencilBufferHeap()->GetCPUDescriptorHandleForHeapStart();

		list->OMSetRenderTargets(1, &rtvHeap, true, &dsvHandle);
		list->ClearRenderTargetView(rtvHeap, clearColor, 0, nullptr);
		list->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		list->RSSetViewports(1, &mWholeScreenViewPortScissor.viewport);
		list->RSSetScissorRects(1, &mWholeScreenViewPortScissor.scissor);

		list->SetGraphicsRootSignature(mPipeline.rootSig.Get());
		list->SetDescriptorHeaps(1, &heapPtr);

		size_t numMeshes = mMeshes.size();
		static int currentInstance = 0;
		static int frameCnt = 0;
		
		// per frame matrix buffer setting
		XMMATRIX vp = view * projection;
		auto vpAllocation = cbvAllocator->AllocateConstantBuffer(sizeof(XMFLOAT4X4));
		XMStoreFloat4x4(static_cast<XMFLOAT4X4*>(vpAllocation.mCpuAddress), XMMatrixTranspose(vp));
		list->SetGraphicsRootConstantBufferView(0, vpAllocation.mGpuAddress);

		list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (const DuckyMeshInstance& instance : mInstances)
		{
			if (instance.mMeshDataIndex < 0 || instance.mMeshDataIndex >= static_cast<int>(mMeshes.size())) continue;
			
			const DuckyMeshData& mesh = mMeshes[instance.mMeshDataIndex];

			BindInstanceConstants(list, *cbvAllocator,instance);

			for (const DuckyPrimitive& primitive : mesh.GetPrimitives())
			{
				const DuckyMaterial& material = mesh.GetMaterial(primitive.GetMaterialIndex());

				BindMaterial(list,*cbvAllocator,material);

				primitive.BindGeometry(list);
				primitive.Draw(list);
			}
		}

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		list->ResourceBarrier(1, &barrier);
		HRESULT hResult = list->Close();
		if (FAILED(hResult)) break;

		ID3D12CommandList* cmdLists[] = {list};
		mQueue->ExecuteCommandLists(1, cmdLists);
		mDeviceManager->Present();

		if (!mDuckyContext->EndFrame(currentFrame, mQueue, mFence.Get())) break;
		
	}

	if (mDuckyContext && mQueue && mFence && mFenceEvent)
	{
		mDuckyContext->WaitForGpu(mQueue, mFence.Get(), mFenceEvent);
	}

	CloseHandle(mFenceEvent);
	mFenceEvent = nullptr;
	UnregisterClass(mLpszClassName, mHInstance);
}

bool SimplestApp::Resize(UINT WindowWidth, UINT WindowHeight)
{
	if (WindowWidth == 0 || WindowHeight == 0) return true;
	
	if (!mDuckyContext->WaitForGpu(mQueue, mFence.Get(), mFenceEvent)) return false;

	if (!mDeviceManager->Resize(WindowWidth,WindowHeight)) return false;

	mClientWidth = WindowWidth;
	mClientHeight = WindowHeight;

	mWholeScreenViewPortScissor.viewport.TopLeftX = 0.0f;
	mWholeScreenViewPortScissor.viewport.TopLeftY = 0.0f;
	mWholeScreenViewPortScissor.viewport.Width  = static_cast<float>(WindowWidth);
	mWholeScreenViewPortScissor.viewport.Height = static_cast<float>(WindowHeight);
	mWholeScreenViewPortScissor.viewport.MinDepth = 0.0f;
	mWholeScreenViewPortScissor.viewport.MaxDepth = 1.0f;

	mWholeScreenViewPortScissor.scissor.left = 0;
	mWholeScreenViewPortScissor.scissor.top = 0;
	mWholeScreenViewPortScissor.scissor.right  = static_cast<LONG>(WindowWidth);
	mWholeScreenViewPortScissor.scissor.bottom = static_cast<LONG>(WindowHeight);

	return true;
}
