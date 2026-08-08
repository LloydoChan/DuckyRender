#include "pch.h"
#include "SimplestApp.h"
#include "DuckyGraphicsContext.h"
#include "D3DDeviceManager.h"
#include "DuckyPipelineStates.h"
#include "DuckyTools.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hwnd,
	UINT msg,
	WPARAM wparam,
	LPARAM lparam);


using Clock = std::chrono::steady_clock;

// this isn't const because it needs to change based on scale
float MOVEMENT_SPEED = 20.f;
const float ROTATIONAL_SPEED_YAW = 2.f * 3.141f;
const float ROTATIONAL_SPEED_PITCH = 2.f * 3.141f;

namespace RootParameter
{
	constexpr UINT PerFrame = 0;
	constexpr UINT PerInstance = 1;
	constexpr UINT PerMaterial = 2;
	constexpr UINT BaseColorTexture = 3;
	constexpr UINT NormalTexture = 4;
	constexpr UINT MetallicRoughnessTexture = 5;
	constexpr UINT EmissiveTexture = 6;
	constexpr UINT Count = 7;
}

SimplestApp::~SimplestApp()
{
	if(mDuckyContext != nullptr)
		delete mDuckyContext;
};

bool SimplestApp::Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName)
{
	if (!DuckyApp::Init(WindowWidth, WindowHeight, WindowName)) return false;

	std::ifstream DuckyFile;
	DuckyFile.open(mInputFilePath, std::ios::binary);
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

	for (size_t instanceIndex = 0; instanceIndex < numInstances; ++instanceIndex)
	{
		DuckyMeshInstance instance;

		DuckyFile.read( reinterpret_cast<char*>(&instance.mMeshDataIndex), sizeof(instance.mMeshDataIndex));
		DuckyFile.read( reinterpret_cast<char*>(&instance.mTransform), sizeof(instance.mTransform));

		if (!DuckyFile) return false;

		mInstances.emplace_back(std::move(instance));
	}


	InitMaterials(DuckyFile);
	InitTextures(DuckyFile);

	// init fallback textures
	DescriptorHeapResource newResource = mDeviceManager->CreateFallbackTexture(L"BaseColorFallbackTexture", BaseColorFallback);
	if (newResource.buffer == nullptr) return false;
	mTextures.emplace_back(newResource);
	mBaseColorFallbackHandle = mTextures.size() - 1;

	newResource = mDeviceManager->CreateFallbackTexture(L"NormalFallbackTexture", NormalFallback);
	if (newResource.buffer == nullptr) return false;
	mTextures.emplace_back(newResource);
	mNormalColorFallbackHandle = mTextures.size() - 1;


	InitMeshes(DuckyFile);
	InitVertexAndIndexMegaBuffer(DuckyFile);
	InitDebugDrawsVBAndIB();
	CreateDrawRecords();

	if (mMeshes.empty()) return false;

	WorkOutGlobalBoundingBoxCenter();

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

	mDuckyContext = new DuckyGraphicsContext;
	if(!mDuckyContext->Init(mDeviceManager.get(), neededCapacity, &mLogFile)) return false;

	RootSignatureDesc drawSig = {};

	D3D12_ROOT_PARAMETER rootParams[RootParameter::Count]{};

	// Root parameter 0:
	// Direct root CBV at b0 for the vertex shader.
	rootParams[RootParameter::PerFrame].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[RootParameter::PerFrame].Descriptor.ShaderRegister = 0;
	rootParams[RootParameter::PerFrame].Descriptor.RegisterSpace = 0;
	rootParams[RootParameter::PerFrame].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// Root parameter 1:
	// Direct root CBV at b0 for the vertex shader.
	rootParams[RootParameter::PerInstance].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[RootParameter::PerInstance].Descriptor.ShaderRegister = 1;
	rootParams[RootParameter::PerInstance].Descriptor.RegisterSpace = 0;
	rootParams[RootParameter::PerInstance].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	// Root parameter 2:
	// SRV descriptor table at t0 for the pixel shader.
	D3D12_DESCRIPTOR_RANGE srvRange = {};

	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0;
	srvRange.RegisterSpace = 0;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParams[RootParameter::PerMaterial].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[RootParameter::PerMaterial].Descriptor.ShaderRegister = 2;
	rootParams[RootParameter::PerMaterial].Descriptor.RegisterSpace = 0;
	rootParams[RootParameter::PerMaterial].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_DESCRIPTOR_RANGE textureRanges[4]{};

	for (UINT index = 0; index < 4; ++index)
	{
		textureRanges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureRanges[index].NumDescriptors = 1;
		textureRanges[index].BaseShaderRegister = index;
		textureRanges[index].RegisterSpace = 0;
		textureRanges[index].OffsetInDescriptorsFromTableStart =D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

	for (UINT index = 0; index < 4; ++index)
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
	samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
	samplerDesc.MaxAnisotropy = 8;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MinLOD = 0.f;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	drawSig.staticSamplers.emplace_back(samplerDesc);

	auto opaqueState = MakeOpaquePipelineState();
	auto transparantState = MakeTransparentPipelineState();
	auto maskedState = MakeMaskedPipelineState();
	auto dblOpaqueState = MakeDoubleSidedOpaquePipelineState();
	auto dblBlendState = MakeDoubleSidedTransparentPipelineState();
	auto dblMaskedState = MakeDoubleSidedMaskedPipelineState();
	auto debugState = MakeDebugDrawPipelineState();

	mOpaquePipeline = mDeviceManager->CreatePSO(L"BasicVertTransformation.hlsl", L"main", L"BasicColorPixelShader.hlsl", L"main", drawSig, opaqueState);
	if (mOpaquePipeline.rootSig == nullptr || mOpaquePipeline.pipeLineState == nullptr) return false;

	mTransparentPipeline = mDeviceManager->CreatePSO(L"BasicVertTransformation.hlsl", L"main", L"BasicColorPixelShader.hlsl", L"main", drawSig, transparantState);
	if (mTransparentPipeline.rootSig == nullptr || mTransparentPipeline.pipeLineState == nullptr) return false;

	mMaskedPipeline = mDeviceManager->CreatePSO(L"BasicVertTransformation.hlsl", L"main", L"BasicColorPixelShader.hlsl", L"main", drawSig, maskedState);
	if (mMaskedPipeline.rootSig == nullptr || mMaskedPipeline.pipeLineState == nullptr) return false;

	mOpaqueDblPipeline = mDeviceManager->CreatePSO(L"BasicVertTransformation.hlsl", L"main", L"BasicColorPixelShader.hlsl", L"main", drawSig, dblOpaqueState);
	if (mOpaqueDblPipeline.rootSig == nullptr || mOpaqueDblPipeline.pipeLineState == nullptr) return false;

	mTransparentDblPipeline = mDeviceManager->CreatePSO(L"BasicVertTransformation.hlsl", L"main", L"BasicColorPixelShader.hlsl", L"main", drawSig, dblBlendState);
	if (mTransparentDblPipeline.rootSig == nullptr || mTransparentDblPipeline.pipeLineState == nullptr) return false;

	mMaskedDblPipeline = mDeviceManager->CreatePSO(L"BasicVertTransformation.hlsl", L"main", L"BasicColorPixelShader.hlsl", L"main", drawSig, dblMaskedState);
	if (mMaskedDblPipeline.rootSig == nullptr || mMaskedDblPipeline.pipeLineState == nullptr) return false;

	// DEBUG DRAW PSO START----------------------------------------------------------------------------------------------------------------
	RootSignatureDesc debugSig = {};

	D3D12_ROOT_PARAMETER debugParams[2]{};

	// Root parameter 1:
	debugParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	debugParams[0].Descriptor.ShaderRegister = 0;
	debugParams[0].Descriptor.RegisterSpace = 0;
	debugParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	// Root parameter 1:
	debugParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	debugParams[1].Descriptor.ShaderRegister = 0;
	debugParams[1].Descriptor.RegisterSpace = 0;
	debugParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	for (const auto& debugParam : debugParams)
	{
		debugSig.parameters.emplace_back(debugParam);
	}

	mDebugPipeline = mDeviceManager->CreatePSO(L"AABBVertexShader.hlsl", L"main", L"AABBPixelShader.hlsl", L"main", debugSig, debugState);
	if (mDebugPipeline.rootSig == nullptr || mDebugPipeline.pipeLineState == nullptr) return false;

	// DEBUG DRAW PSO END-------------------------------------------------------------------------------------------------------------------

	mWholeScreenViewPortScissor.scissor.left = 0;
	mWholeScreenViewPortScissor.scissor.right = WindowWidth;
	mWholeScreenViewPortScissor.scissor.top = 0;
	mWholeScreenViewPortScissor.scissor.bottom = WindowHeight;

	UINT64 fenceVal = 0;
	mFence = mDeviceManager->CreateFence(fenceVal);
	if (mFence == nullptr) return false;

	mCommandQueue = mDeviceManager->GetCommandQueue();

	mFenceEvent = CreateEvent(nullptr, false, false, nullptr);

	if (mFenceEvent == nullptr) return false;

	for (int i = 0; i < 2; i++)
	{
		mMatrixBuffer[i] = mDeviceManager->CreateConstantBuffer(sizeof(XMMATRIX));
		if (mMatrixBuffer[i].buffer == nullptr) return false;
		HRESULT hResult = mMatrixBuffer[i].buffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedTransform[i]));

		if (FAILED(hResult))return false;

		mStructuredBufferOBBs[i] = mDeviceManager->CreateStructuredBuffer(totalPrimitiveDraws * sizeof(GPUOBB), sizeof(GPUOBB));
	}

	if (!mImGui.Init(
		GetWindowHandle(),
		mDeviceManager.get(),
		mDeviceManager->GetDescriptorHeapHandleInt(),
		FrameCount,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))
	{
		return false;
	}
	
	if (!InitGPUTimeStamps()) return false;
	if (!InitGPUStats()) return false;

	return true;
}

LRESULT SimplestApp::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{

	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

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
	if (msg == WM_MOUSEWHEEL)
	{
		// Extract the scroll delta amount
		int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);

		// Map the value to the number of notches clicked
		int scrollNotches = wheelDelta / WHEEL_DELTA;

		mScrollAmount = scrollNotches;
	}

	if (msg == WM_RBUTTONDOWN) mRightButtonDown = true;

	if (msg == WM_RBUTTONUP) mRightButtonDown = false;

	if (msg == WM_LBUTTONDOWN) mLeftButtonDown = true;

	if (msg == WM_LBUTTONUP) mLeftButtonDown = false;

	if (msg == WM_MOUSELEAVE) mRightButtonDown = false;

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

		
		if(rawInput->header.dwType == RIM_TYPEMOUSE)
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

			if (mKeys['v'] || mKeys['V'])
			{
				mVisualizationMode = (mVisualizationMode + 1) % MaterialVisualization::VIS_MAX;
			}

			if (mKeys['U'] || mKeys['u'])
			{
				bDrawDebug = !bDrawDebug;
			}
		}
	}
}

void SimplestApp::UpdateMovementAndRotation(XMVECTOR& ViewVector, MovementStruct& movement, float DeltaTime)
{
	LONG localMouseDeltaXCopy = mMouseDeltaX;
	LONG localMouseDeltaYCopy = mMouseDeltaY;

	static const XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	ViewVector = XMVector3Normalize(ViewVector);
	
	if (mLeftButtonDown && localMouseDeltaXCopy != 0)
	{
		float yaw = XMConvertToRadians(static_cast<float>(mMouseDeltaX) * ROTATIONAL_SPEED_YAW * DeltaTime);
		XMVECTOR quat = XMQuaternionRotationAxis(up, yaw);
		ViewVector = XMVector3Rotate(ViewVector, quat);
		ViewVector = XMVector3Normalize(ViewVector);
	}

	XMVECTOR localDeltaYCopy = XMVector3Normalize(XMVector3Cross(up, ViewVector));
	XMVECTOR rightVector = XMVector3Cross(ViewVector, up);
	rightVector = XMVector3Normalize(rightVector);

	if (mLeftButtonDown && localMouseDeltaYCopy != 0)
	{
		float pitch = XMConvertToRadians(static_cast<float>(mMouseDeltaY) * ROTATIONAL_SPEED_PITCH * DeltaTime);
		XMVECTOR quat = XMQuaternionRotationAxis(rightVector, -pitch);
		XMVECTOR possibleViewVector = XMVector3Rotate(ViewVector, quat);
		possibleViewVector = XMVector3Normalize(possibleViewVector);

		const float verticalAlignment = XMVectorGetX(XMVector3Dot(possibleViewVector, up));

		constexpr float pitchLimit = 0.99f;

		if (std::abs(verticalAlignment) < pitchLimit)
		{
			ViewVector = possibleViewVector;
		}
	}

	if (mKeys['W'] || mScrollAmount > 0)
	{
		movement.zMovement -= MOVEMENT_SPEED * DeltaTime;
	}

	if (mKeys['S'] || mScrollAmount < 0)
	{
		movement.zMovement += MOVEMENT_SPEED * DeltaTime;
	}
	
	if (mKeys['a'] || mKeys['A'])
	{
		movement.xMovement += MOVEMENT_SPEED * DeltaTime;
	}

	if (mKeys['d'] || mKeys['D'])
	{
		movement.xMovement -= MOVEMENT_SPEED * DeltaTime;
	}

	if (mKeys['q'] || mKeys['Q'])
	{
		movement.yMovement += MOVEMENT_SPEED * DeltaTime;
	}

	if (mKeys['e'] || mKeys['E'])
	{
		movement.yMovement -= MOVEMENT_SPEED * DeltaTime;
	}

	if (mRightButtonDown && localMouseDeltaYCopy != 0)
	{
		movement.yMovement = MOVEMENT_SPEED * DeltaTime * localMouseDeltaYCopy;
	}

	if (mRightButtonDown && localMouseDeltaXCopy != 0)
	{
		movement.xMovement = MOVEMENT_SPEED * DeltaTime * localMouseDeltaXCopy;
	}
}

bool SimplestApp::BindMaterial( ID3D12GraphicsCommandList* commandList, ConstantBufferAllocator* allocator, unsigned int material)
{
	ConstantBufferAllocation allocation = allocator->AllocateConstantBuffer(sizeof(MaterialConstants));

	if (allocation.mCpuAddress == nullptr) return false;

	const DuckyMaterial& materialRef = mMaterials[material];

	std::memcpy(allocation.mCpuAddress, &materialRef.constants, sizeof(MaterialConstants));

	commandList->SetGraphicsRootConstantBufferView(RootParameter::PerMaterial, allocation.mGpuAddress);

	BindTexture( commandList, RootParameter::BaseColorTexture, materialRef.mBaseColorTexture, mBaseColorFallbackHandle);
	BindTexture(commandList, RootParameter::NormalTexture, materialRef.mNormalTexture, mNormalColorFallbackHandle);
	BindTexture(commandList, RootParameter::MetallicRoughnessTexture, materialRef.mMetallicRoughnessTexture, mBaseColorFallbackHandle);
	BindTexture(commandList, RootParameter::EmissiveTexture, materialRef.mEmissive, mBaseColorFallbackHandle);

	return true;
}

void SimplestApp::BindTexture(ID3D12GraphicsCommandList* commandList, UINT rootParameter, int textureHandle, unsigned int fallBackHandle)
{
	DescriptorHeapResource* texture = nullptr;
	if (textureHandle == -1)
	{
		texture = &mTextures[fallBackHandle];
	}
	else
	{
		texture = &mTextures[textureHandle];
	}
	
	if (texture == nullptr) return; // if still not found...

	commandList->SetGraphicsRootDescriptorTable(rootParameter,texture->descHandle);
}

bool SimplestApp::BindInstanceConstants( ID3D12GraphicsCommandList* commandList, ConstantBufferAllocator* allocator, unsigned int instance)
{
	ConstantBufferAllocation allocation = allocator->AllocateConstantBuffer( sizeof(PerInstanceConstants));

	if (allocation.mCpuAddress == nullptr) return false;

	auto* constants = static_cast<PerInstanceConstants*>(allocation.mCpuAddress);

	XMMATRIX normalMatrix = XMMatrixInverse(nullptr, mInstances[instance].mTransform);

	XMStoreFloat4x4(&constants->normal,normalMatrix);
	XMStoreFloat4x4(&constants->world, XMMatrixTranspose(mInstances[instance].mTransform));
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

	XMFLOAT4 min;
	XMFLOAT4 max;

	//XMStoreFloat4(&min, mGlobalAABB.GetMin());
	//XMStoreFloat4(&max, mGlobalAABB.GetMax());

	float lengthX = 0.f;//max.x - min.x;
	float lengthY = 0.f;//max.y - min.y;
	float lengthZ = 0.f;//max.z - min.z;

	float xDiff = 0.f, zDiff = 0.f;
	float viewLength = 5.f; //0.f;

	/*if (lengthX < lengthZ)
	{
		xDiff = lengthX;
		viewLength = xDiff * 0.5f;
	}
	else
	{
		zDiff = lengthZ;
		viewLength = zDiff * 0.5f;
	}*/

	MOVEMENT_SPEED = viewLength;

	/*float sceneMidPoint[3] = { min.x + lengthX * 0.5f,
							   min.y + lengthY * 0.5f,
							   min.z + lengthZ * 0.5f };*/



	XMVECTOR at = XMVectorSet(0.f, 0.f, 0.0f, 1.f);//XMVectorSet(sceneMidPoint[0], sceneMidPoint[1], sceneMidPoint[2], 1.f);
	XMVECTOR eye = XMVectorSet(5.f, viewLength, 5.f, 1.f);
	const XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	auto previousTime = Clock::now();

	MSG msg = {};
	UINT64 fenceVal = 0;

	ID3D12DescriptorHeap* heapPtr = mDeviceManager->GetDescriptorHeapHandle();

	ID3D12GraphicsCommandList* list = mDuckyContext->GetCommandList();

	if (list == nullptr) return;

	bool bRunning = true;
	double gpuTime = 0.0;
	float deltaCPU = 0.f;

	D3D12_QUERY_DATA_PIPELINE_STATISTICS gpuStats{};

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

		XMVECTOR viewVector = XMVector4Normalize(XMVectorSubtract(eye, at));

		MovementStruct movement{};

		UpdateMovementAndRotation(viewVector, movement, deltaTime);

		XMVECTOR right = XMVector3Cross(viewVector, up);
		XMVECTOR relativeUp = XMVector3Cross(viewVector, right);
		XMVECTOR delta{ movement.xMovement, movement.yMovement, 0.f };

		XMVECTOR forward = XMVectorScale(viewVector, movement.zMovement);
		right = XMVectorScale(right, movement.xMovement);
		relativeUp = XMVectorScale(relativeUp, movement.yMovement);

		at = XMVectorAdd(at, forward);
		at = XMVectorAdd(at, right);
		at = XMVectorAdd(at, relativeUp);

		eye = XMVectorAdd(at, XMVectorScale(viewVector, viewLength));

		XMMATRIX view = XMMatrixLookAtLH(eye, at, up);

		mMouseDeltaX = mMouseDeltaY = 0;
		mScrollAmount = 0;

		std::vector<TransformedDrawRecord> transformedRecords = TransformAABBsToOBBs(mOpaqueDblDraws);
		std::vector<SortRecord> sortedRecordsOpaqueDbl = SortDrawRecords(view, transformedRecords);
		CopyOBBsToGPU(transformedRecords, currentFrame);

		//transformedRecords = TransformAABBs(mMaskedDblDraws);
		//std::vector<SortRecord> sortedRecordsMaskedDbl = SortDrawRecords(view, transformedRecords);
		////CopyAABBsToGPU(transformedRecords, currentFrame);

		//transformedRecords = TransformAABBs(mBlendedDblDraws);
		//std::vector<SortRecord> sortedRecordsBlendedDbl = SortDrawRecords(view, transformedRecords, true);
		////CopyAABBsToGPU(transformedRecords, currentFrame);

		//transformedRecords = TransformAABBs(mOpaqueDraws);
		//std::vector<SortRecord> sortedRecordsOpaque = SortDrawRecords(view, transformedRecords);
		////CopyAABBsToGPU(transformedRecords, currentFrame);

		//transformedRecords = TransformAABBs(mBlendedDraws);
		//std::vector<SortRecord> sortedRecordsBlended = SortDrawRecords(view, transformedRecords, true);
		////CopyAABBsToGPU(transformedRecords, currentFrame);

		//transformedRecords = TransformAABBs(mMaskedDraws);
		//std::vector<SortRecord> sortedRecordsMasked = SortDrawRecords(view, transformedRecords);
		////CopyAABBsToGPU(transformedRecords, currentFrame);
		

		float clearColor[] = { 0.5f, 0.8f, 0.9f, 1.f };

		ImGui::SetNextWindowSize(
			ImVec2(320.f, 420.f),
			ImGuiCond_FirstUseEver);

		mImGui.BeginFrame();

		ImGui::Begin("DuckyRender");

		ImGui::Text("GPU Frame: %.3f ms", gpuTime);
		ImGui::Text("CPU Frame: %.3f ms", deltaCPU);
		ImGui::Text("Frame time: %.3f ms", deltaTime * 1000.f);

		ImGui::Text("PS Invocations: %d ", gpuStats.PSInvocations);
		ImGui::Text("VS Invocations: %d ", gpuStats.VSInvocations);
		ImGui::Text("IA Vertices: %d", gpuStats.IAVertices);
		ImGui::Text("IA Primitives: %d", gpuStats.IAPrimitives);

		ImGui::End();

		if (!mDuckyContext->BeginFrame(currentFrame, mFence.Get(), mOpaquePipeline.pipeLineState.Get(), mFenceEvent, &mLogFile)) break;
		ConstantBufferAllocator* cbvAllocator = mDuckyContext->GetBufferAllocator(currentFrame);

		gpuTime = GetGPUFrameMilliSeconds(currentFrame);


		StartGPUTimeStamp(list, currentFrame);

		D3D12_RESOURCE_BARRIER barrier = mDeviceManager->GetBarrier();
		list->ResourceBarrier(1, &barrier);
		list->SetPipelineState(mOpaquePipeline.pipeLineState.Get());

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHeap = mDeviceManager->IncrementAndReturnRTVHeaps();
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDeviceManager->GetDepthStencilBufferHeap()->GetCPUDescriptorHandleForHeapStart();

		list->OMSetRenderTargets(1, &rtvHeap, true, &dsvHandle);
		list->ClearRenderTargetView(rtvHeap, clearColor, 0, nullptr);
		list->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		list->RSSetViewports(1, &mWholeScreenViewPortScissor.viewport);
		list->RSSetScissorRects(1, &mWholeScreenViewPortScissor.scissor);

		list->SetGraphicsRootSignature(mOpaquePipeline.rootSig.Get());
		list->SetDescriptorHeaps(1, &heapPtr);

		size_t numMeshes = mMeshes.size();
		static int currentInstance = 0;
		static int frameCnt = 0;
		
		// per frame matrix buffer setting
		XMMATRIX vp = view * projection;
		ConstantBufferAllocation constantAllocation = cbvAllocator->AllocateConstantBuffer(sizeof(PerFrameConstants));
		PerFrameConstants* asFrameConstants = reinterpret_cast<PerFrameConstants*>(constantAllocation.mCpuAddress);
		XMStoreFloat4x4(static_cast<XMFLOAT4X4*>(&asFrameConstants->mViewProjection), XMMatrixTranspose(vp));
		XMStoreFloat4(static_cast<XMFLOAT4*>(&asFrameConstants->mCameraPosition), eye);
		asFrameConstants->mLightColor = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
		asFrameConstants->mLightDirection = XMFLOAT4(-0.4f, -1.0f, 0.2f, 0.f);
		asFrameConstants->mVisualisationMode = mVisualizationMode;
		list->SetGraphicsRootConstantBufferView(0, constantAllocation.mGpuAddress);

		list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		list->IASetVertexBuffers(0, 1, &mVbView);
		list->IASetIndexBuffer(&mIbView);

		PIXBeginEvent(list, PIX_COLOR(0, 255, 0), "DRAW");
		PIXScopedEvent(PIX_COLOR(0, 255, 255), "DRAW");

		StartGpuStats(list, currentFrame);

		list->SetPipelineState(mOpaqueDblPipeline.pipeLineState.Get());
		DrawRecords(sortedRecordsOpaqueDbl, cbvAllocator, list);

		/*list->SetPipelineState(mMaskedDblPipeline.pipeLineState.Get());
		DrawRecords(sortedRecordsMaskedDbl, cbvAllocator, list);

		list->SetPipelineState(mOpaquePipeline.pipeLineState.Get());
		DrawRecords(sortedRecordsOpaque, cbvAllocator, list);

		list->SetPipelineState(mMaskedPipeline.pipeLineState.Get());
		DrawRecords(sortedRecordsMasked, cbvAllocator, list);

		list->SetPipelineState(mTransparentDblPipeline.pipeLineState.Get());
		DrawRecords(sortedRecordsBlendedDbl, cbvAllocator, list);

		list->SetPipelineState(mTransparentPipeline.pipeLineState.Get());
		DrawRecords(sortedRecordsBlended, cbvAllocator, list);*/

		if (bDrawDebug)
		{
			XMFLOAT4X4 debugProjection;
			XMStoreFloat4x4(&debugProjection, XMMatrixTranspose(vp));
			memcpy(mMappedTransform[currentFrame], &debugProjection, sizeof(debugProjection));

			list->SetGraphicsRootSignature(mDebugPipeline.rootSig.Get());
			list->SetPipelineState(mDebugPipeline.pipeLineState.Get());
			list->SetGraphicsRootConstantBufferView(0,mMatrixBuffer[currentFrame].buffer->GetGPUVirtualAddress());
			list->SetGraphicsRootShaderResourceView(1,mStructuredBufferOBBs[currentFrame].buffer->GetGPUVirtualAddress());
			list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			list->IASetVertexBuffers(0,1,&mVbDebugView);
			list->IASetIndexBuffer(&mIbDebugView);
			list->DrawIndexedInstanced(24, transformedRecords.size(), 0, 0, 0);
		}


		EndGPUStats(list, currentFrame);
		PIXEndEvent(list);

		gpuStats = WriteOutGPUStats(currentFrame);

		EndGPUTimeStamp(list, currentFrame);

		// don't want imGUI stats to contribute
		mImGui.Render(list);

		// now, add debug Draws, using the sorted AABB data from this frame


		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		list->ResourceBarrier(1, &barrier);
		HRESULT hResult = list->Close();
		if (FAILED(hResult)) break;

		ID3D12CommandList* cmdLists[] = {list};
		mCommandQueue->ExecuteCommandLists(1, cmdLists);
		mDeviceManager->Present();

		if (!mDuckyContext->EndFrame(currentFrame, mCommandQueue, mFence.Get())) break;
	}

	if (mDuckyContext && mCommandQueue && mFence && mFenceEvent)
	{
		mDuckyContext->WaitForGpu(mCommandQueue, mFence.Get(), mFenceEvent);
	}

	if (mDuckyContext &&
		mCommandQueue &&
		mFence &&
		mFenceEvent)
	{
		mDuckyContext->WaitForGpu(
			mCommandQueue,
			mFence.Get(),
			mFenceEvent);
	}

	mImGui.Shutdown();

	CloseHandle(mFenceEvent);
	mFenceEvent = nullptr;
	UnregisterClass(mLpszClassName, mHInstance);
}

void SimplestApp::WorkOutGlobalBoundingBoxCenter()
{
	for (const auto& instance : mInstances)
	{
		const DuckyMeshData& data = mMeshes[instance.mMeshDataIndex];
		for (const auto& primitive : data.GetPrimitives())
		{
			const AABB& box = primitive.GetBoundingBox();

			for (int x = 0; x < 2; ++x)
			{
				for (int y = 0; y < 2; ++y)
				{
					for (int z = 0; z < 2; ++z)
					{
						const XMFLOAT4& min = box.GetMin();
						const XMFLOAT4& max = box.GetMax();

						const XMVECTOR localCorner = XMVectorSet(
							x ? max.x : min.x,
							y ? max.y : min.y,
							z ? max.z : min.z,
							1.0f);

						const XMVECTOR worldCorner = XMVector3TransformCoord( localCorner, instance.mTransform);

						XMFLOAT4 transformedCorner;
						XMStoreFloat4(&transformedCorner, worldCorner);
						XMFLOAT4 globalMin = mGlobalAABB.GetMin();
						XMFLOAT4 globalMax = mGlobalAABB.GetMax();


						XMFLOAT4 newMin { min(globalMin.x, transformedCorner.x) , min(globalMin.y, transformedCorner.y), min(globalMin.z, transformedCorner.z), 1.f};
						XMFLOAT4 newMax{ min(globalMax.x, transformedCorner.x) , min(globalMax.y, transformedCorner.y), min(globalMax.z, transformedCorner.z), 1.f };


						mGlobalAABB.SetNewMinMax(newMin, newMax);
					}
				}
			}
		}

	}
}

void SimplestApp::CreateDrawRecords()
{
	for (int instance = 0; instance < mInstances.size(); instance++)
	{
		const DuckyMeshInstance* instancePtr = &mInstances[instance];
		const DuckyMeshData&	 meshData = mMeshes[instancePtr->mMeshDataIndex];
		
		const std::vector<DuckyPrimitive>& primitives = meshData.GetPrimitives();

		for (int primitive = 0; primitive < primitives.size(); primitive++)
		{
			const DuckyPrimitive* primPtr = &primitives[primitive];
			const DuckyMaterial*  matPtr =  &mMaterials[primPtr->GetMaterialIndex()];

			DrawRecord newRecord{ instance, instancePtr->mMeshDataIndex, primitive, primPtr->GetMaterialIndex() };

			AlphaMode mode = matPtr->constants.alphaMode;

			if (matPtr->constants.doubleSided)
			{
				switch (mode)
				{
					case AlphaMode::Blend:
						mBlendedDblDraws.push_back(newRecord);
						break;
					case AlphaMode::Mask:
						mMaskedDblDraws.push_back(newRecord);
						break;
					default:
						mOpaqueDblDraws.push_back(newRecord);
				}
			}
			else
			{
				switch (mode)
				{
					case AlphaMode::Blend:
						mBlendedDraws.push_back(newRecord);
						break;
					case AlphaMode::Mask:
						mMaskedDraws.push_back(newRecord);
						break;
					default:
						mOpaqueDraws.push_back(newRecord);
				}
			}
		}

	}
}

void SimplestApp::DrawRecords(const std::vector<SortRecord>& Draws, ConstantBufferAllocator* Allocator, ID3D12GraphicsCommandList* List)
{
	for (const SortRecord& draw : Draws)
	{
		BindInstanceConstants(List, Allocator, draw.record.mInstanceIndex);
		BindMaterial(List, Allocator, draw.record.mMaterialIndex);
		const DuckyPrimitive& prim = mMeshes[draw.record.mMeshIndex].GetPrimitive(draw.record.mPrimitiveIndex);
		prim.Draw(List);
	}
}

bool SimplestApp::Resize(UINT WindowWidth, UINT WindowHeight)
{
	if (WindowWidth == 0 || WindowHeight == 0) return true;
	
	if (!mDuckyContext->WaitForGpu(mCommandQueue, mFence.Get(), mFenceEvent)) return false;

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


std::vector<TransformedDrawRecord> SimplestApp::TransformAABBsToOBBs(std::vector<DrawRecord>& recordsToSort)
{
	PIXScopedEvent(PIX_COLOR(255, 0, 0), "TrasnformAABBs");

	std::vector<TransformedDrawRecord> transformedRecords;
	transformedRecords.reserve(recordsToSort.size());

	for (const DrawRecord& record : recordsToSort)
	{
		DuckyMeshInstance& inst = mInstances[record.mInstanceIndex];
		XMMATRIX transform = inst.mTransform;

		DuckyMeshData& meshData = mMeshes[record.mMeshIndex];
		const AABB& boundingBox = meshData.GetPrimitives()[record.mPrimitiveIndex].GetBoundingBox();

		const XMFLOAT4& min = boundingBox.GetMin();
		const XMFLOAT4& max = boundingBox.GetMax();

		XMVECTOR localCenter = XMVectorSet(
			(min.x + max.x) * 0.5f,
			(min.y + max.y) * 0.5f,
			(min.z + max.z) * 0.5f,
			1.0f);

		XMVECTOR localExtents = XMVectorSet(
			(max.x - min.x) * 0.5f,
			(max.y - min.y) * 0.5f,
			(max.z - min.z) * 0.5f,
			0.0f);

		XMVECTOR scale;
		XMVECTOR rotation;
		XMVECTOR translation;

		XMMatrixDecompose(&scale, &rotation, &translation, transform);

		XMVECTOR worldCenter = XMVector3TransformCoord(localCenter, inst.mTransform);
		XMVECTOR worldExtents = XMVectorMultiply(localExtents, XMVectorAbs(scale));

		GPUOBB gpuOBB{};

		XMStoreFloat3(&gpuOBB.Center, worldCenter);
		XMStoreFloat3(&gpuOBB.Extents, worldExtents);
		XMStoreFloat4(&gpuOBB.Orientation, rotation);

		TransformedDrawRecord newRecord;
		newRecord.mDrawRecord = record;
		newRecord.mOBB = gpuOBB;
		transformedRecords.emplace_back(newRecord);
	}

	return transformedRecords;
}

std::vector<SortRecord> SimplestApp::SortDrawRecords(const XMMATRIX& View, const std::vector<TransformedDrawRecord>& RecordsToSort, bool bAlphaPass)
{
	PIXScopedEvent(PIX_COLOR(255, 0, 0), "SortDrawRecords");

	std::vector<SortRecord> sortedRecords;
	sortedRecords.reserve(RecordsToSort.size());

	for (const TransformedDrawRecord& record : RecordsToSort)
	{
		SortRecord newRecord;
		newRecord.record = record.mDrawRecord;
		const XMFLOAT3& center = record.mOBB.Center;
		const XMFLOAT3& extents = record.mOBB.Extents;
		XMFLOAT3 newPoints[8] = {

			{center.x + extents.x, center.y - extents.y, center.z - extents.z},
			{center.x + extents.x, center.y - extents.y, center.z + extents.z},
			{center.x + extents.x, center.y + extents.y, center.z - extents.z},
			{center.x + extents.x, center.y + extents.y, center.z + extents.z},

			{center.x - extents.x, center.y - extents.y, center.z - extents.z},
			{center.x - extents.x, center.y - extents.y, center.z + extents.z},
			{center.x - extents.x, center.y + extents.y, center.z - extents.z},
			{center.x - extents.x, center.y + extents.y, center.z + extents.z}

		};

		for (int i = 0; i < 8; ++i)
		{
			if(newPoints[i].z < newRecord.minZ) newRecord.minZ = newPoints[i].z;
		}

		sortedRecords.emplace_back(std::move(newRecord));
	}

	if (bAlphaPass) sort(sortedRecords.begin(), sortedRecords.end(), [](const SortRecord& a, const SortRecord& b) { return a.minZ > b.minZ;});
	else sort(sortedRecords.begin(), sortedRecords.end(), [](const SortRecord& a, const SortRecord& b) { return a.minZ < b.minZ;});

	return sortedRecords;
}

void SimplestApp::CopyOBBsToGPU(const std::vector<TransformedDrawRecord>& TransformedOBBs, unsigned int CurrentFrame)
{
	auto* dst = static_cast<GPUOBB*>(mStructuredBufferOBBs[CurrentFrame].mapped);

	for (size_t i = 0; i < TransformedOBBs.size(); ++i)
	{
		const GPUOBB& box = TransformedOBBs[i].mOBB;
		dst[i] = box;
	}
}