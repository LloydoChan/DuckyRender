#include "pch.h"
#include "DuckyPipelineManager.h"
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

namespace RootParameter
{
	constexpr UINT PerFrame = 0;
	constexpr UINT DrawConstants = 1;
	constexpr UINT Count = 2;
};

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

	if (!mUploadContext.Init( mDeviceManager->GetDevice(), mDeviceManager->GetCommandQueue())) return false;

	mScene.Init(DuckyFile, mDeviceManager.get(), mUploadContext);
	mNumMeshes = mScene.GetInstances().size();

	InitDebugDrawsVBAndIB();
	CreateDrawRecords();
	WorkOutGlobalBoundingBoxCenter();

	mTotalPrimitiveDraws = 0;

	for (const DuckyMeshInstance& instance : mScene.GetInstances())
	{
		if (instance.mMeshDataIndex < 0 || instance.mMeshDataIndex >= static_cast<int>(mScene.GetMeshes().size())) continue;

		mTotalPrimitiveDraws += mScene.GetMeshes()[instance.mMeshDataIndex].GetPrimitiveCount();
	}

	std::vector<GPUCullDraw> culldraws;
	culldraws.reserve(mTotalPrimitiveDraws);
	uint32_t drawOffset = 0;
	size_t obbOffset = 0;

	mStructuredBufferOBBs = mDeviceManager->CreateStructuredBuffer(mTotalPrimitiveDraws * sizeof(GPUOBB), sizeof(GPUOBB));

	for (int type = static_cast<int>(PipelineType::OPAQUE); type < static_cast<int>(PipelineType::DEBUG); type++)
	{
		mTransformedDrawTypes[type] = TransformAABBsToOBBs(mDrawTypes[type]);

		// Keep this for debug rendering
		CopyOBBsToGPU(mTransformedDrawTypes[type],obbOffset);

		obbOffset += mTransformedDrawTypes[type].size();

		mDrawRanges[type].Offset = drawOffset;
		mDrawRanges[type].Count = static_cast<uint32_t>(mTransformedDrawTypes[type].size());

		for (const TransformedDrawRecord& record : mTransformedDrawTypes[type])
		{
			const DrawRecord& drawRecord = record.mDrawRecord;

			const DuckyPrimitive& primitive = mScene.GetMeshes()[drawRecord.mMeshIndex].GetPrimitive(drawRecord.mPrimitiveIndex);

			GPUCullDraw cullDraw;

			cullDraw.mCenter		= record.mOBB.Center;
			cullDraw.mExtents		= record.mOBB.Extents;
			cullDraw.mOrientation   = record.mOBB.Orientation;

			cullDraw.mBaseVertex = static_cast<int32_t>(primitive.GetVertexOffset());
			cullDraw.mIndexCount = static_cast<int32_t>(primitive.GetNumIndices());
			cullDraw.mStartIndex = static_cast<int32_t>(primitive.GetIndexOffset());

			cullDraw.mDrawIndex = static_cast<uint32_t>(record.mDrawRecord.mGPUDrawIndex);

			culldraws.push_back(cullDraw);
		}

		drawOffset += mDrawRanges[type].Count;
	}

	mCullDrawBuffer = mDeviceManager->CreateDefaultBuffer(sizeof(GPUCullDraw) * mTotalPrimitiveDraws, D3D12_RESOURCE_STATE_COPY_DEST);

	mUploadContext.Begin();

	mUploadContext.UploadData(mDeviceManager.get(), mCullDrawBuffer.Get(), culldraws.data(), sizeof(GPUCullDraw) * mTotalPrimitiveDraws, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	mUploadContext.SubmitAndWait();

	RootSignatureDesc drawSig = {};

	D3D12_ROOT_PARAMETER rootParams[RootParameter::Count]{};

	// b0 - per frame
	rootParams[RootParameter::PerFrame].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[RootParameter::PerFrame].Descriptor.ShaderRegister = 0;
	rootParams[RootParameter::PerFrame].Descriptor.RegisterSpace = 0;
	rootParams[RootParameter::PerFrame].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// b2 - one uint material index
	rootParams[RootParameter::DrawConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParams[RootParameter::DrawConstants].Constants.ShaderRegister = 2;
	rootParams[RootParameter::DrawConstants].Constants.RegisterSpace = 0;
	rootParams[RootParameter::DrawConstants].Constants.Num32BitValues = 1;
	rootParams[RootParameter::DrawConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

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

	ShaderDesc pbrShaderVS, pbrShaderPS;
	pbrShaderVS.File = L"BasicVertTransformation.hlsl";
	pbrShaderPS.File = L"BasicColorPixelShader.hlsl";

	GraphicsPipelineDesc mainDesc;
	mainDesc.VSShader = pbrShaderVS;
	mainDesc.PSShader = pbrShaderPS;
	mainDesc.Params = rootParams;
	mainDesc.NumParams = RootParameter::Count;
	mainDesc.Samplers = &samplerDesc;
	mainDesc.NumSamplers = 1;

	GraphicsPipelineDesc descs[7] = { mainDesc ,mainDesc ,mainDesc ,mainDesc ,mainDesc ,mainDesc , mainDesc };

	descs[0].Type = PipelineType::OPAQUE;
	descs[1].Type = PipelineType::ALPHA;
	descs[2].Type = PipelineType::MASKED;
	descs[3].Type = PipelineType::OPAQUE_DBL;
	descs[4].Type = PipelineType::MASKED_DBL;
	descs[5].Type = PipelineType::ALPHA_DBL;


	// DEBUG DRAW PSO START----------------------------------------------------------------------------------------------------------------

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


	ShaderDesc debugShaderVS, debugShaderPS;
	debugShaderVS.File = L"AABBVertexShader.hlsl";
	debugShaderPS.File = L"AABBPixelShader.hlsl";

	GraphicsPipelineDesc debugDesc;
	debugDesc.VSShader = debugShaderVS;
	debugDesc.PSShader = debugShaderPS;
	debugDesc.Params = debugParams;
	debugDesc.NumParams = 2;
	debugDesc.Samplers = nullptr;
	debugDesc.NumSamplers = 0;
	debugDesc.Type = PipelineType::DEBUG;

	descs[6] = debugDesc;

	if(!mRenderer.Init(&mLogFile, mDeviceManager.get(), descs, 7)) return false;

	// DEBUG DRAW PSO END-------------------------------------------------------------------------------------------------------------------

	// CREATE COMPUTE PSO ------------------------------------------------------------------------------------------------------------------
	
	D3D12_ROOT_PARAMETER frustumCullParams[3]{};

	// b0
	frustumCullParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	frustumCullParams[0].Descriptor.ShaderRegister = 0;
	frustumCullParams[0].Descriptor.RegisterSpace = 0;
	frustumCullParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// t0
	frustumCullParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	frustumCullParams[1].Descriptor.ShaderRegister = 0;
	frustumCullParams[1].Descriptor.RegisterSpace = 0;
	frustumCullParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// u0
	frustumCullParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
	frustumCullParams[2].Descriptor.ShaderRegister = 0;
	frustumCullParams[2].Descriptor.RegisterSpace = 0;
	frustumCullParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	ShaderDesc ComputeShaderCS;
	ComputeShaderCS.File = L"FrustumCull.hlsl";

	ComputePipelineDesc computeDesc;
	computeDesc.CSShader = ComputeShaderCS;
	computeDesc.NumParams = 3;
	computeDesc.Params = frustumCullParams;

	mFrustumCullComputePipeline =  mRenderer.GetPipelineManager()->CreateComputePSO(computeDesc);

	if (!mFrustumCullComputePipeline.rootSig ||
		!mFrustumCullComputePipeline.pipeLineState)
	{
		return false;
	}

	mVisibilityBuffer = mDeviceManager->CreateStructuredBuffer(mTotalPrimitiveDraws * sizeof(uint32_t),sizeof(uint32_t));

	UINT64 neededCapacity = AlignConstantBufferSize(sizeof(PerFrameConstants) + sizeof(uint32_t) * mTotalPrimitiveDraws);

	mDuckyContext = new DuckyGraphicsContext;
	if (!mDuckyContext->Init(mDeviceManager.get(), neededCapacity, mTotalPrimitiveDraws, &mLogFile)) return false;

	// CREATE COMPUTE PSO END --------------------------------------------------------------------------------------------------------------
	
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

	for (int i = 0; i < FrameCount; i++)
	{
		mMatrixBuffer[i] = mDeviceManager->CreateConstantBuffer(sizeof(XMMATRIX));
		if (mMatrixBuffer[i].buffer == nullptr) return false;
		HRESULT hResult = mMatrixBuffer[i].buffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedTransform[i]));

		if (FAILED(hResult))return false;
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

void SimplestApp::UpdateMovement(MovementStruct& movement, float DeltaTime)
{
	LONG localMouseDeltaXCopy = mMouseDeltaX;
	LONG localMouseDeltaYCopy = mMouseDeltaY;


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
		movement.xMovement -= MOVEMENT_SPEED * DeltaTime;
	}

	if (mKeys['d'] || mKeys['D'])
	{
		movement.xMovement += MOVEMENT_SPEED * DeltaTime;
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

void SimplestApp::AppMainLoop()
{
	float angle = 0.f;

	XMFLOAT4 min = mGlobalAABB.GetMin();
	XMFLOAT4 max = mGlobalAABB.GetMax();

	float lengthX = max.x - min.x;
	float lengthY = max.y - min.y;
	float lengthZ = max.z - min.z;

	float sceneSize = (std::max)({ lengthX, lengthY, lengthZ });

	float viewLength =
		sceneSize * 1.5f;

	MOVEMENT_SPEED = sceneSize;

	float sceneMidPoint[3] = { min.x + lengthX * 0.5f,
							   min.y + lengthY * 0.5f,
							   min.z + lengthZ * 0.5f };



	XMVECTOR at = XMVectorSet(sceneMidPoint[0], sceneMidPoint[1], sceneMidPoint[2], 1.f);
	XMVECTOR initialDirection = XMVector3Normalize(XMVectorSet(1.0f, 0.6f, 1.0f, 0.0f));
	XMVECTOR eye =XMVectorAdd(at,XMVectorScale(initialDirection,viewLength));

	float fov = XMConvertToRadians(60.0f);
	float aspect = static_cast<float>(mWholeScreenViewPortScissor.scissor.right) / static_cast<float>(mWholeScreenViewPortScissor.scissor.bottom);

	std::unique_ptr<DuckyCamera> sceneCamera = std::make_unique<DuckyCamera>(at, eye, fov, aspect, 0.1f, 1000.0f);

	auto previousTime = Clock::now();
	MSG msg = {};
	UINT64 fenceVal = 0;

	ID3D12DescriptorHeap* heapPtr = mDeviceManager->GetDescriptorHeapHandle();

	ID3D12GraphicsCommandList* list = mDuckyContext->GetCommandList();

	if (list == nullptr) return;

	bool bRunning = true;
	double gpuTime = 0.0;
	float actualCPU = 0.f;

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

		float deltaFrameSeconds = std::chrono::duration<float>(currentTime - previousTime).count();

		previousTime = currentTime;

		MovementStruct movement{};

		UpdateMovement(movement, deltaFrameSeconds);

		sceneCamera->Update(deltaFrameSeconds, movement, mMouseDeltaX, mMouseDeltaY, mLeftButtonDown);

		mMouseDeltaX = mMouseDeltaY = 0;
		mScrollAmount = 0;

		DuckyFrustum frameFrustum = sceneCamera->GetViewFrustum();

		unsigned int numDrawableMeshes = 0;

		std::vector<SortRecord> sortedRecords[static_cast<int>(PipelineType::DEBUG)];
	
		for (int type = static_cast<int>(PipelineType::OPAQUE); type < static_cast<int>(PipelineType::DEBUG); type++)
		{
			char debugString[50];
			snprintf(debugString, 50, "Sort And Cull %d", type);
			PIXScopedEvent(PIX_COLOR(0, 255, 255), "Sort And Cull");

			const bool alphaPass =
				type == static_cast<int>(PipelineType::ALPHA) ||
				type == static_cast<int>(PipelineType::ALPHA_DBL);

			sortedRecords[type] = SortAndCull(mTransformedDrawTypes[type], frameFrustum, sceneCamera->GetViewProjection(), alphaPass);
			numDrawableMeshes += sortedRecords[type].size();
		}

		float clearColor[] = { 0.5f, 0.8f, 0.9f, 1.f };

		ImGui::SetNextWindowSize(
			ImVec2(320.f, 420.f),
			ImGuiCond_FirstUseEver);

		mImGui.BeginFrame();

		ImGui::Begin("DuckyRender");

		float deltaTime = gpuTime > actualCPU * 1000.f ? gpuTime : actualCPU * 1000.f;

		ImGui::Text("GPU Frame: %.3f ms", gpuTime);
		ImGui::Text("CPU Frame: %.3f ms", actualCPU * 1000.f);
		ImGui::Text("Frame time: %.3f ms", deltaTime);

		ImGui::Text("PS Invocations: %d ", gpuStats.PSInvocations);
		ImGui::Text("VS Invocations: %d ", gpuStats.VSInvocations);
		ImGui::Text("CS Invocations: %d ", gpuStats.CSInvocations);
		ImGui::Text("Primitives Sent To Rasterizer: %d ", gpuStats.CInvocations);
		ImGui::Text("Primitives Rasterized: %d ", gpuStats.CPrimitives);
		ImGui::Text("IA Vertices: %d", gpuStats.IAVertices);
		ImGui::Text("IA Primitives: %d", gpuStats.IAPrimitives);
		ImGui::Text("Number Meshes Pre-cull: %d", mNumMeshes);
		ImGui::Text("Number Drawable Meshes: %d", numDrawableMeshes);

		ImGui::End();


		ID3D12PipelineState* opaqueState = mRenderer.GetPipelineSig(PipelineType::OPAQUE).pipeLineState.Get();
		if (!mDuckyContext->BeginFrame(currentFrame, mFence.Get(), opaqueState, mFenceEvent, &mLogFile)) break;

		ConstantBufferAllocator* cbvAllocator = mDuckyContext->GetBufferAllocator(currentFrame);
		ConstantBufferAllocation cullAllocation = cbvAllocator->AllocateConstantBuffer(sizeof(GPUCullConstants));

		auto* cullConstants = reinterpret_cast<GPUCullConstants*>(cullAllocation.mCpuAddress);

		for (int i = 0; i < 6; ++i) cullConstants->mFrustumPlanes[i] = frameFrustum.mPlanes[i];

		cullConstants->mDrawCount = mTotalPrimitiveDraws;
		
		list->SetPipelineState(mFrustumCullComputePipeline.pipeLineState.Get());
		list->SetComputeRootSignature(mFrustumCullComputePipeline.rootSig.Get());
		list->SetComputeRootConstantBufferView(0, cullAllocation.mGpuAddress);


		const uint32_t groupCount = (mTotalPrimitiveDraws + 63) / 64;

		gpuTime = GetGPUFrameMilliSeconds(currentFrame);

		StartGPUTimeStamp(list, currentFrame);
		StartGpuStats(list, currentFrame);

		
		ComPtr<ID3D12Resource> indirectBuffer = mDuckyContext->GetIndirectBuffer(currentFrame);
		D3D12_RESOURCE_BARRIER indirectBarrier{};

		indirectBarrier.Type =
			D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

		indirectBarrier.Flags =
			D3D12_RESOURCE_BARRIER_FLAG_NONE;

		indirectBarrier.Transition.pResource =
			indirectBuffer.Get();

		indirectBarrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		indirectBarrier.Transition.StateBefore =
			D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

		indirectBarrier.Transition.StateAfter =
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		list->ResourceBarrier(1, &indirectBarrier);

		list->SetComputeRootShaderResourceView(1, mCullDrawBuffer->GetGPUVirtualAddress());


		list->SetComputeRootUnorderedAccessView(2, indirectBuffer->GetGPUVirtualAddress());
		list->Dispatch(groupCount, 1, 1);


		indirectBarrier.Transition = {
			mDuckyContext->GetIndirectBuffer(currentFrame).Get(),
			0,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT };

		list->ResourceBarrier(1, &indirectBarrier);
		list->SetPipelineState(opaqueState);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHeap = mDeviceManager->IncrementAndReturnRTVHeaps();
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDeviceManager->GetDepthStencilBufferHeap()->GetCPUDescriptorHandleForHeapStart();

		list->OMSetRenderTargets(1, &rtvHeap, true, &dsvHandle);
		list->ClearRenderTargetView(rtvHeap, clearColor, 0, nullptr);
		list->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		list->RSSetViewports(1, &mWholeScreenViewPortScissor.viewport);
		list->RSSetScissorRects(1, &mWholeScreenViewPortScissor.scissor);

		list->SetDescriptorHeaps(1, &heapPtr);
		list->SetGraphicsRootSignature(mRenderer.GetPipelineSig(PipelineType::OPAQUE).rootSig.Get());

		size_t numMeshes = mScene.GetMeshes().size();
		static int currentInstance = 0;
		static int frameCnt = 0;
		
		
		ConstantBufferAllocation constantAllocation = cbvAllocator->AllocateConstantBuffer(sizeof(PerFrameConstants));
		PerFrameConstants* asFrameConstants = reinterpret_cast<PerFrameConstants*>(constantAllocation.mCpuAddress);

		XMStoreFloat4x4(static_cast<XMFLOAT4X4*>(&asFrameConstants->mViewProjection), XMMatrixTranspose(sceneCamera->GetViewProjection()));
		XMStoreFloat4(static_cast<XMFLOAT4*>(&asFrameConstants->mCameraPosition), sceneCamera->GetEye());
		asFrameConstants->mLightColor = XMFLOAT4(1.f, 0.95f, 0.85f, 1.f);
		asFrameConstants->mLightDirection = XMFLOAT4(-0.4f, -1.0f, 0.2f, 0.f);
		asFrameConstants->mVisualisationMode = mVisualizationMode;
		asFrameConstants->mMaterialBufferIndex = mScene.GetMaterialsHeapBuffer().heapOffset;
		asFrameConstants->mInstanceBufferIndex = mScene.GetInstancesHeapBuffer().heapOffset;
		asFrameConstants->mDrawBufferIndex = mDrawsBuffer.heapOffset;

		list->SetGraphicsRootConstantBufferView(0, constantAllocation.mGpuAddress);
		

		list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		list->IASetVertexBuffers(0, 1, &mScene.GetVertexBufferView());
		list->IASetIndexBuffer(&mScene.GetIndexBufferView());

		
		for (int type = static_cast<int>(PipelineType::OPAQUE); type < static_cast<int>(PipelineType::DEBUG); type++)
		{
			const DrawRange& range = mDrawRanges[type];

			mRenderer.ExecuteDraws(list,
				static_cast<PipelineType>(type),
				indirectBuffer.Get(),
				static_cast<UINT>(range.Count),
				static_cast<UINT64>(range.Offset) * sizeof(IndirectCommand));
		}


		if (bDrawDebug)
		{
			XMFLOAT4X4 debugProjection;
			XMStoreFloat4x4(&debugProjection, XMMatrixTranspose(sceneCamera->GetViewProjection()));
			memcpy(mMappedTransform[currentFrame], &debugProjection, sizeof(debugProjection));

			list->SetGraphicsRootSignature(mRenderer.GetPipelineSig(PipelineType::DEBUG).rootSig.Get());
			list->SetPipelineState(mRenderer.GetPipelineSig(PipelineType::DEBUG).pipeLineState.Get());
			list->SetGraphicsRootConstantBufferView(0,mMatrixBuffer[currentFrame].buffer->GetGPUVirtualAddress());
			list->SetGraphicsRootShaderResourceView(1,mStructuredBufferOBBs.buffer->GetGPUVirtualAddress());
			list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			list->IASetVertexBuffers(0,1,&mVbDebugView);
			list->IASetIndexBuffer(&mIbDebugView);
			list->DrawIndexedInstanced(24, sortedRecords[static_cast<int>(PipelineType::OPAQUE_DBL)].size(), 0, 0, 0);
		}

		mImGui.Render(list);


		EndGPUStats(list, currentFrame);
		PIXEndEvent(list);

		gpuStats = WriteOutGPUStats(currentFrame);
		// don't want imGUI stats to contribute

		EndGPUTimeStamp(list, currentFrame);

		D3D12_RESOURCE_BARRIER backBufferBarrier = mDeviceManager->GetBarrier();

		backBufferBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		backBufferBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		list->ResourceBarrier(1, &backBufferBarrier);
		HRESULT hResult = list->Close();
		if (FAILED(hResult)) break;

		ID3D12CommandList* cmdLists[] = {list};
		mCommandQueue->ExecuteCommandLists(1, cmdLists);
		mDeviceManager->Present();

		if (!mDuckyContext->EndFrame(currentFrame, mCommandQueue, mFence.Get())) break;

		auto endTime = Clock::now();
		actualCPU = std::chrono::duration<float>(endTime - currentTime).count();
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
	for (const auto& instance : mScene.GetInstances())
	{
		const DuckyMeshData& data = mScene.GetMeshes()[instance.mMeshDataIndex];
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
						XMFLOAT4 newMax{  max(globalMax.x, transformedCorner.x) , max(globalMax.y, transformedCorner.y), max(globalMax.z, transformedCorner.z), 1.f };


						mGlobalAABB.SetNewMinMax(newMin, newMax);
					}
				}
			}
		}

	}
}

void SimplestApp::CreateDrawRecords()
{
	size_t numDraws = 0;
	std::vector<DrawRecord> tempRecords;
	for (int instance = 0; instance < mScene.GetInstances().size(); instance++)
	{
		const DuckyMeshInstance* instancePtr = &mScene.GetInstances()[instance];
		const DuckyMeshData& meshData = mScene.GetMeshes()[instancePtr->mMeshDataIndex];

		const std::vector<DuckyPrimitive>& primitives = meshData.GetPrimitives();

		for (int primitive = 0; primitive < primitives.size(); primitive++)
		{
			const DuckyPrimitive* primPtr = &primitives[primitive];
			const DuckyMaterial* matPtr = &mScene.GetMaterials()[primPtr->GetMaterialIndex()];

			DrawRecord newRecord{ instance, instancePtr->mMeshDataIndex, primitive, primPtr->GetMaterialIndex(), numDraws };
			numDraws++;
			tempRecords.push_back(newRecord);

			AlphaMode mode = matPtr->constants.alphaMode;

			if (matPtr->constants.doubleSided)
			{
				switch (mode)
				{
					case AlphaMode::Blend:
						mDrawTypes[static_cast<int>(PipelineType::ALPHA_DBL)].push_back(newRecord);
						break;
					case AlphaMode::Mask:
						mDrawTypes[static_cast<int>(PipelineType::MASKED_DBL)].push_back(newRecord);
						break;
					default:
						mDrawTypes[static_cast<int>(PipelineType::OPAQUE_DBL)].push_back(newRecord);
				}
			}
			else
			{
				switch (mode)
				{
					case AlphaMode::Blend:
						mDrawTypes[static_cast<int>(PipelineType::ALPHA)].push_back(newRecord);
						break;
					case AlphaMode::Mask:
						mDrawTypes[static_cast<int>(PipelineType::MASKED)].push_back(newRecord);
						break;
					default:
						mDrawTypes[static_cast<int>(PipelineType::OPAQUE)].push_back(newRecord);
				}
			}
		}
	}

	mDrawsBuffer =  mDeviceManager->CreateStructuredBuffer(sizeof(GPUDrawData) * numDraws, sizeof(GPUDrawData));
	mDrawsBuffer.buffer->Map(0, nullptr, &mDrawsBuffer.mapped);

	GPUDrawData* dst = (GPUDrawData*)mDrawsBuffer.mapped;
	for (const DrawRecord& record : tempRecords)
	{
		GPUDrawData newData;
		newData.mInstanceIndex = record.mInstanceIndex;
		newData.mMaterialIndex = record.mMaterialIndex;
		*dst = newData;
		dst++;
	}
}

void SimplestApp::DrawRecords(const std::vector<SortRecord>& Draws, ConstantBufferAllocator* Allocator, ID3D12GraphicsCommandList* List)
{
	for (const SortRecord& draw : Draws)
	{
		List->SetGraphicsRoot32BitConstant( RootParameter::DrawConstants, draw.record.mGPUDrawIndex, 0);
		const DuckyPrimitive& prim = mScene.GetMeshes()[draw.record.mMeshIndex].GetPrimitive(draw.record.mPrimitiveIndex);

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


std::vector<TransformedDrawRecord> SimplestApp::TransformAABBsToOBBs(const std::vector<DrawRecord>& recordsToSort)
{
	PIXScopedEvent(PIX_COLOR(255, 0, 0), "TransformAABBs");

	std::vector<TransformedDrawRecord> transformedRecords;
	transformedRecords.reserve(recordsToSort.size());

	for (const DrawRecord& record : recordsToSort)
	{
		const DuckyMeshInstance& inst = mScene.GetInstances()[record.mInstanceIndex];
		XMMATRIX transform = inst.mTransform;

		const DuckyMeshData& meshData = mScene.GetMeshes()[record.mMeshIndex];
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

void SimplestApp::CopyOBBsToGPU(const std::vector<TransformedDrawRecord>& TransformedOBBs, size_t Offset)
{
	mStructuredBufferOBBs.buffer->Map(0, nullptr, &mStructuredBufferOBBs.mapped);
	auto* dst = static_cast<GPUOBB*>(mStructuredBufferOBBs.mapped);

	dst += Offset;

	for (size_t i = 0; i < TransformedOBBs.size(); ++i)
	{
		const GPUOBB& box = TransformedOBBs[i].mOBB;
		dst[i] = box;
	}
}

std::vector<SortRecord> SimplestApp::SortAndCull(const std::vector<TransformedDrawRecord>& records,const DuckyFrustum& frustum,const XMMATRIX& view,bool alphaPass)
{
	return SortDrawRecords(view, records, alphaPass);
}

uint32_t SimplestApp::BuildIndirectCommands(const std::vector<SortRecord>& draws, IndirectCommand* destination)
{
	for (uint32_t i = 0; i < draws.size(); ++i)
	{
		const SortRecord& record = draws[i];

		const DuckyPrimitive& primitive = mScene.GetMeshes()[record.record.mMeshIndex].GetPrimitive(record.record.mPrimitiveIndex);

		IndirectCommand& cmd = destination[i];

		cmd.mDrawIndex = record.record.mGPUDrawIndex;

		cmd.mDraw.IndexCountPerInstance = static_cast<UINT>(primitive.GetNumIndices());
		cmd.mDraw.InstanceCount = 1;
		cmd.mDraw.StartIndexLocation = static_cast<UINT>(primitive.GetIndexOffset());
		cmd.mDraw.BaseVertexLocation =static_cast<INT>(primitive.GetVertexOffset());
		cmd.mDraw.StartInstanceLocation = 0;
	}

	return static_cast<uint32_t>(draws.size());
}