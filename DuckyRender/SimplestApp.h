#pragma once
#include "DuckyApp.h"
#include "D3DDeviceManager.h"
#include "DuckyMesh.h"

class DuckyGraphicsContext;


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

	ID3D12Fence* mFence = nullptr;

	DescriptorHeapResource mMatrixBuffer;
	DescriptorHeapResource mTextureBuffer;
	PipelineAndRootSig mPipeline;

	std::vector<DuckyMeshData> mMeshes;

	int mCbvSrvUavHandle = -1;

	DuckyGraphicsContext* mDuckyContext = nullptr;
};