#pragma once
#include "DuckyApp.h"
#include "D3DDeviceManager.h"
#include "DuckyMesh.h"
#include "DuckyGraphicsContext.h"


class SimplestApp : public DuckyApp
{
	public:
		SimplestApp() : mWholeScreenViewPortScissor(1920, 1080) {};
		virtual bool Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName) override;
		virtual LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
		virtual void HandleInput(UINT msg, WPARAM wParam, LPARAM lParam);
		void UpdateMovementAndRotation(XMVECTOR& ViewVector, XMVECTOR& ScaledMovement, float DeltaTime);
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
		std::vector<DuckyMeshInstance> mInstances;

		int mCbvSrvUavHandle = -1;

		std::unique_ptr<DuckyGraphicsContext> mDuckyContext;
};