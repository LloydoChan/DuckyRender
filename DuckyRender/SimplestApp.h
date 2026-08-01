#pragma once
#include "DuckyApp.h"
#include "D3DDeviceManager.h"
#include "DuckyMesh.h"
#include "DuckyGraphicsContext.h"

namespace MaterialVisualization
{
	static const unsigned int DEPTH = 1;
	static const unsigned int ROUGHNESS = 2;
	static const unsigned int METAL = 3;
	static const unsigned int NORMAL = 4;
	static const unsigned int VIS_MAX = 5;
};

enum class SortType
{
	AscendingOrder = 1,
	DescendingOrder = 2
};

struct DrawRecord
{
	const DuckyMeshInstance* mInstanceIndex;
	const DuckyPrimitive*    mPrimitiveIndex;
	const DuckyMaterial*     mMaterialIndex;
};

struct MovementStruct
{
	float xMovement = 0.f;
	float yMovement = 0.f;
	float zMovement = 0.f;
};

struct PerFrameConstants
{
	XMFLOAT4X4 mViewProjection;

	XMFLOAT4 mCameraPosition;
	XMFLOAT4 mLightDirection;
	XMFLOAT4 mLightColor;

	unsigned int mVisualisationMode = 0;

// put this in here to pad out entire struct to 128 bytes
private:
	XMFLOAT3 mDummyPadding;
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
		void UpdateMovementAndRotation(XMVECTOR& ViewVector, MovementStruct& movement, float DeltaTime);
		bool BindMaterial(ID3D12GraphicsCommandList* commandList, ConstantBufferAllocator& allocator, const DuckyMaterial& material);
		void BindTexture(ID3D12GraphicsCommandList* commandList, UINT rootParameter, size_t textureHandle, size_t fallBackHandle);
		bool BindInstanceConstants(ID3D12GraphicsCommandList* commandList, ConstantBufferAllocator& allocator, const DuckyMeshInstance& instance);
		virtual void AppMainLoop();

		void WorkOutGlobalBoundingBoxCenter();
		void CreateDrawRecords();

		bool Resize(UINT WindowWidth, UINT WindowHeight);

		void SortDrawRecords(const XMMATRIX& WorldView, SortType SortOrder, bool bAlphaPass = false);

	private:
		bool mKeys[256] = {};
		LONG mMouseDeltaX = 0;
		LONG mMouseDeltaY = 0;
		bool mRightButtonDown = false;
		bool mLeftButtonDown = false;
		int  mScrollAmount = 0;

		ID3D12CommandQueue* mQueue = nullptr;

		ViewportScissor mWholeScreenViewPortScissor;

		ComPtr<ID3D12Fence> mFence;

		DescriptorHeapResource mTextureBuffer;

		DescriptorHeapResource mMatrixBuffer[2];
		XMFLOAT4X4* mMappedTransform[2] = {};

		std::vector<DuckyMeshData> mMeshes;
		std::vector<DuckyMeshInstance> mInstances;

		PipelineAndRootSig mOpaquePipeline;
		std::vector<DrawRecord> mOpaqueDraws;

		PipelineAndRootSig mTransparentPipeline;
		std::vector<DrawRecord> mBlendedDraws;

		int mCbvSrvUavHandle = -1;

		std::unique_ptr<DuckyGraphicsContext> mDuckyContext;

		unsigned int mVisualizationMode = 0;

		AABB mGlobalAABB;
};