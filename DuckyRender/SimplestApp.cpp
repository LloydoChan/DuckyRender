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

	mScene.Init(DuckyFile, mDeviceManager.get());
	InitDebugDrawsVBAndIB();
	CreateDrawRecords();

	WorkOutGlobalBoundingBoxCenter();

	size_t totalPrimitiveDraws = 0;

	for (const DuckyMeshInstance& instance : mScene.GetInstances())
	{
		if (instance.mMeshDataIndex < 0 || instance.mMeshDataIndex >= static_cast<int>(mScene.GetMeshes().size())) continue;

		totalPrimitiveDraws += mScene.GetMeshes()[instance.mMeshDataIndex].GetPrimitiveCount();
	}

	UINT64 neededCapacity = AlignConstantBufferSize(sizeof(PerFrameConstants));

	mDuckyContext = new DuckyGraphicsContext;
	if(!mDuckyContext->Init(mDeviceManager.get(), neededCapacity, totalPrimitiveDraws, &mLogFile)) return false;

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
	XMVECTOR eye = XMVectorSet(5.f, viewLength, 5.f, 1.f);

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

		MovementStruct movement{};

		UpdateMovement(movement, deltaTime);

		sceneCamera->Update(deltaTime, movement, viewLength, mMouseDeltaX, mMouseDeltaY, mLeftButtonDown);

		mMouseDeltaX = mMouseDeltaY = 0;
		mScrollAmount = 0;

		XMFLOAT4X4 viewProj;
		XMStoreFloat4x4(&viewProj, sceneCamera->GetViewProjection());
		DuckyFrustum frameFrustum = ExtractFrustumFromViewProjection(viewProj);

		unsigned int numDrawableMeshes = 0;

		std::vector<SortRecord> sortedRecords[static_cast<int>(PipelineType::DEBUG)];
	
		for (int type = static_cast<int>(PipelineType::OPAQUE); type < static_cast<int>(PipelineType::DEBUG); type++)
		{
			sortedRecords[type] = SortAndCull(mDrawTypes[type], frameFrustum, sceneCamera->GetViewProjection(), currentFrame);
			numDrawableMeshes += sortedRecords[type].size();
		}

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
		ImGui::Text("Number Meshes Pre-cull: %d", mNumMeshes);
		ImGui::Text("Number Drawable Meshes: %d", numDrawableMeshes);

		ImGui::End();

		ID3D12PipelineState* opaqueState = mRenderer.GetPipelineSig(PipelineType::OPAQUE).pipeLineState.Get();
		if (!mDuckyContext->BeginFrame(currentFrame, mFence.Get(), opaqueState, mFenceEvent, &mLogFile)) break;
		ConstantBufferAllocator* cbvAllocator = mDuckyContext->GetBufferAllocator(currentFrame);

		IndirectCommand* indirectDst = mDuckyContext->GetIndirectCommandPtr(currentFrame);
		uint32_t commandOffset = 0;

		uint32_t Offsets[static_cast<int>(PipelineType::COUNT)];

		for (int type = static_cast<int>(PipelineType::OPAQUE); type < static_cast<int>(PipelineType::DEBUG); type++)
		{
			Offsets[type] = commandOffset;
			commandOffset += BuildIndirectCommands(sortedRecords[type], indirectDst + commandOffset);
		}

		gpuTime = GetGPUFrameMilliSeconds(currentFrame);

		StartGPUTimeStamp(list, currentFrame);

		D3D12_RESOURCE_BARRIER barrier = mDeviceManager->GetBarrier();
		list->ResourceBarrier(1, &barrier);
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
		asFrameConstants->mLightColor = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
		asFrameConstants->mLightDirection = XMFLOAT4(-0.4f, -1.0f, 0.2f, 0.f);
		asFrameConstants->mVisualisationMode = mVisualizationMode;
		asFrameConstants->mMaterialBufferIndex = mScene.GetMaterialsHeapBuffer().heapOffset;
		asFrameConstants->mInstanceBufferIndex = mScene.GetInstancesHeapBuffer().heapOffset;
		asFrameConstants->mDrawBufferIndex = mDrawsBuffer.heapOffset;
		list->SetGraphicsRootConstantBufferView(0, constantAllocation.mGpuAddress);

		list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		list->IASetVertexBuffers(0, 1, &mScene.GetVertexBufferView());
		list->IASetIndexBuffer(&mScene.GetIndexBufferView());

		PIXBeginEvent(list, PIX_COLOR(0, 255, 0), "DRAW");
		PIXScopedEvent(PIX_COLOR(0, 255, 255), "DRAW");

		StartGpuStats(list, currentFrame);

		ComPtr<ID3D12Resource> indirectBuffer = mDuckyContext->GetIndirectBuffer(currentFrame);
		
		for (int type = static_cast<int>(PipelineType::OPAQUE); type < static_cast<int>(PipelineType::DEBUG); type++)
		{
			mRenderer.ExecuteDraws(list,
				static_cast<PipelineType>(type),
				indirectBuffer.Get(),
				static_cast<UINT>(sortedRecords[type].size()),
				static_cast<UINT64>(Offsets[type]) * sizeof(IndirectCommand));
		}


		if (bDrawDebug)
		{
			XMFLOAT4X4 debugProjection;
			XMStoreFloat4x4(&debugProjection, XMMatrixTranspose(sceneCamera->GetViewProjection()));
			memcpy(mMappedTransform[currentFrame], &debugProjection, sizeof(debugProjection));

			list->SetGraphicsRootSignature(mRenderer.GetPipelineSig(PipelineType::DEBUG).rootSig.Get());
			list->SetPipelineState(mRenderer.GetPipelineSig(PipelineType::DEBUG).pipeLineState.Get());
			list->SetGraphicsRootConstantBufferView(0,mMatrixBuffer[currentFrame].buffer->GetGPUVirtualAddress());
			list->SetGraphicsRootShaderResourceView(1,mStructuredBufferOBBs[currentFrame].buffer->GetGPUVirtualAddress());
			list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			list->IASetVertexBuffers(0,1,&mVbDebugView);
			list->IASetIndexBuffer(&mIbDebugView);
			list->DrawIndexedInstanced(24, sortedRecords[static_cast<int>(PipelineType::OPAQUE_DBL)].size(), 0, 0, 0);
		}


		EndGPUStats(list, currentFrame);
		PIXEndEvent(list);

		gpuStats = WriteOutGPUStats(currentFrame);

		EndGPUTimeStamp(list, currentFrame);

		// don't want imGUI stats to contribute
		mImGui.Render(list);

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

	mDrawsBuffer = mDeviceManager->CreateStructuredBuffer(sizeof(GPUDrawData) * numDraws, sizeof(GPUDrawData));
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
	PIXScopedEvent(PIX_COLOR(255, 0, 0), "TrasnformAABBs");

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

void SimplestApp::CopyOBBsToGPU(const std::vector<TransformedDrawRecord>& TransformedOBBs, unsigned int CurrentFrame)
{
	auto* dst = static_cast<GPUOBB*>(mStructuredBufferOBBs[CurrentFrame].mapped);

	for (size_t i = 0; i < TransformedOBBs.size(); ++i)
	{
		const GPUOBB& box = TransformedOBBs[i].mOBB;
		dst[i] = box;
	}
}

std::vector<SortRecord> SimplestApp::SortAndCull(const std::vector<DrawRecord>& DrawRecords, const DuckyFrustum& Frustum, const XMMATRIX& View, int CurrentFrame)
{
	std::vector<TransformedDrawRecord> transformedRecords = TransformAABBsToOBBs(DrawRecords);
	CopyOBBsToGPU(transformedRecords, CurrentFrame);
	std::vector<TransformedDrawRecord> nonCulledRecords = FrustumCullUsingOBBs(transformedRecords, Frustum);
	std::vector<SortRecord> sortedRecords = SortDrawRecords(View, nonCulledRecords);
	return sortedRecords;
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

std::vector<TransformedDrawRecord> SimplestApp::FrustumCullUsingOBBs(const std::vector<TransformedDrawRecord>& TransformedOBBs, const DuckyFrustum& Frustum)
{
	std::vector<TransformedDrawRecord> nonCulledRecords;
	nonCulledRecords.reserve(TransformedOBBs.size());

	for (const auto& record : TransformedOBBs)
	{
		const GPUOBB& obb = record.mOBB;

		XMVECTOR q = XMLoadFloat4(&obb.Orientation);

		XMVECTOR axisX = XMVector3Rotate( XMVectorSet(1, 0, 0, 0), q);
		XMVECTOR axisY = XMVector3Rotate( XMVectorSet(0, 1, 0, 0), q);
		XMVECTOR axisZ = XMVector3Rotate( XMVectorSet(0, 0, 1, 0), q);

		bool bPass = true;

		for (int i = 0; i < 6; ++i)
		{
			XMVECTOR plane = XMLoadFloat4(&Frustum.mPlanes[i]);
			XMVECTOR normal = XMVectorSet(Frustum.mPlanes[i].x, Frustum.mPlanes[i].y, Frustum.mPlanes[i].z, 0.0f);
			XMVECTOR center = XMLoadFloat3(&obb.Center);

			float distance = XMVectorGetX(XMVector3Dot(center, normal)) + Frustum.mPlanes[i].w;

			float radius = obb.Extents.x * std::abs(XMVectorGetX( XMVector3Dot(axisX, normal))) +
							obb.Extents.y * std::abs(XMVectorGetX( XMVector3Dot(axisY, normal))) +
							obb.Extents.z * std::abs(XMVectorGetX( XMVector3Dot(axisZ, normal)));

			if (distance + radius < 0.0f) bPass = false;
		}

		if (bPass) nonCulledRecords.emplace_back(record);
	}

	return nonCulledRecords;
}

DuckyFrustum SimplestApp::ExtractFrustumFromViewProjection(const XMFLOAT4X4& ViewProjection)
{
	DuckyFrustum frustum;
	const XMFLOAT4X4& m = ViewProjection;

	// left
	frustum.mPlanes[0] ={ m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41};

	// right
	frustum.mPlanes[1] = { m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41};

	// bottom
	frustum.mPlanes[2] = { m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42};

	// top
	frustum.mPlanes[3] = { m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42};

	// near
	frustum.mPlanes[4] = { m._13, m._23, m._33, m._43 };

	// far
	frustum.mPlanes[5] = { m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43};

	for (int i = 0; i < 6; ++i)
	{
		XMVECTOR p = XMLoadFloat4(&frustum.mPlanes[i]);
		p = XMPlaneNormalize(p);
		XMStoreFloat4(&frustum.mPlanes[i], p);
	}

	return frustum;
}
