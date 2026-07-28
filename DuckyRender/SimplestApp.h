#pragma once
#include "DuckyApp.h"
#include "D3DDeviceManager.h"
#include "DuckyMesh.h"
#include "DuckyGraphicsContext.h"

struct PerFrameConstants
{
	XMFLOAT4X4 mViewProjection;

	XMFLOAT4 mCameraPosition;
	XMFLOAT4 mLightDirection;
	XMFLOAT4 mLightColor;

// put this in here to pad out entire struct to 32 bytes
private:
	XMFLOAT4 mDummyPadding;
};

struct PerInstanceConstants
{
	XMFLOAT4X4 world;
};

class SimplestApp : public DuckyApp
{
	public:
		SimplestApp() : mWholeScreenViewPortScissor(1920, 1080) {};
		virtual bool Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName) override;
		virtual LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
		virtual void HandleInput(UINT msg, WPARAM wParam, LPARAM lParam);
		void UpdateMovementAndRotation(XMVECTOR& ViewVector, XMVECTOR& ScaledMovement, float DeltaTime);
		bool BindMaterial(ID3D12GraphicsCommandList* commandList, ConstantBufferAllocator& allocator, const DuckyMaterial& material);
		void BindTexture(ID3D12GraphicsCommandList* commandList, UINT rootParameter, size_t textureHandle);
		bool BindInstanceConstants(ID3D12GraphicsCommandList* commandList, ConstantBufferAllocator& allocator, const DuckyMeshInstance& instance);
		virtual void AppMainLoop();

		bool Resize(UINT WindowWidth, UINT WindowHeight);

	private:
		bool mKeys[256] = {};
		LONG mMouseDeltaX = 0;
		LONG mMouseDeltaY = 0;

		ID3D12CommandQueue* mQueue = nullptr;

		ViewportScissor mWholeScreenViewPortScissor;

		ComPtr<ID3D12Fence> mFence;

		DescriptorHeapResource mTextureBuffer;
		PipelineAndRootSig mPipeline;

		DescriptorHeapResource mMatrixBuffer[2];
		XMFLOAT4X4* mMappedTransform[2] = {};

		std::vector<DuckyMeshData> mMeshes;

		int mCbvSrvUavHandle = -1;

		std::unique_ptr<DuckyGraphicsContext> mDuckyContext;

		std::vector<DuckyMeshInstance> mInstances;
};