#pragma once
#include "DuckyApp.h"
#include "D3DDeviceManager.h"
#include "DuckyMesh.h"
#include "DuckyImGui.h"

class DuckyGraphicsContext;
struct ConstantBufferAllocator;

namespace MaterialVisualization
{
	static const unsigned int DEPTH = 1;
	static const unsigned int ROUGHNESS = 2;
	static const unsigned int METAL = 3;
	static const unsigned int NORMAL = 4;
	static const unsigned int VIS_MAX = 5;
};

struct DrawRecord
{
	unsigned int mInstanceIndex;
	unsigned int mMeshIndex;
	unsigned int mPrimitiveIndex;
	unsigned int mMaterialIndex;
};

struct TransformedDrawRecord
{
	DrawRecord mDrawRecord;
	GPUOBB mOBB;
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
	XMFLOAT4X4 normal;
};

struct SortRecord
{
	DrawRecord record;
	float minZ = (std::numeric_limits<float>::max)();
};

class SimplestApp : public DuckyApp
{
	public:
		SimplestApp() : mWholeScreenViewPortScissor(1920, 1080) {};
		~SimplestApp();
		virtual bool Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName) override;
		virtual LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
		virtual void HandleInput(UINT msg, WPARAM wParam, LPARAM lParam);
		void UpdateMovementAndRotation(XMVECTOR& ViewVector, MovementStruct& movement, float DeltaTime);
		bool BindMaterial(ID3D12GraphicsCommandList* commandList, ConstantBufferAllocator* allocator, unsigned int material);
		void BindTexture(ID3D12GraphicsCommandList* commandList, UINT rootParameter, int textureHandle, unsigned int fallBackHandle);
		bool BindInstanceConstants(ID3D12GraphicsCommandList* commandList, ConstantBufferAllocator* allocator, unsigned int instance);
		virtual void AppMainLoop();

		void WorkOutGlobalBoundingBoxCenter();
		void CreateDrawRecords();
		void DrawRecords(const std::vector<SortRecord>& Draws, ConstantBufferAllocator* Allocator, ID3D12GraphicsCommandList* List);

		bool Resize(UINT WindowWidth, UINT WindowHeight);

		std::vector<TransformedDrawRecord> TransformAABBsToOBBs(std::vector<DrawRecord>& recordsToSort);
		std::vector<SortRecord> SortDrawRecords(const XMMATRIX& View, const std::vector<TransformedDrawRecord>& RecordsToSort, bool bAlphaPass = false);
		void CopyOBBsToGPU(const std::vector<TransformedDrawRecord>& TransformedAABBs, unsigned int CurrentFrame);

	private:
		bool mKeys[256] = {};
		LONG mMouseDeltaX = 0;
		LONG mMouseDeltaY = 0;
		bool mRightButtonDown = false;
		bool mLeftButtonDown = false;
		int  mScrollAmount = 0;
		bool bDrawDebug = false;

		ViewportScissor mWholeScreenViewPortScissor;

		ComPtr<ID3D12Fence> mFence;

		DescriptorHeapResource mTextureBuffer;

		DescriptorHeapResource mMatrixBuffer[2];
		XMFLOAT4X4* mMappedTransform[2] = {};
		MappedDescriptorHeapResource mStructuredBufferOBBs[2];

		PipelineAndRootSig mOpaquePipeline;
		std::vector<DrawRecord> mOpaqueDraws;

		PipelineAndRootSig mTransparentPipeline;
		std::vector<DrawRecord> mBlendedDraws;

		PipelineAndRootSig mMaskedPipeline;
		std::vector<DrawRecord> mMaskedDraws;

		PipelineAndRootSig mOpaqueDblPipeline;
		std::vector<DrawRecord> mOpaqueDblDraws;

		PipelineAndRootSig mTransparentDblPipeline;
		std::vector<DrawRecord> mBlendedDblDraws;

		PipelineAndRootSig mMaskedDblPipeline;
		std::vector<DrawRecord> mMaskedDblDraws;

		PipelineAndRootSig mDebugPipeline;

		DuckyGraphicsContext* mDuckyContext;

		unsigned int mVisualizationMode = 0;

		AABB mGlobalAABB;

		DuckyImGui mImGui;
};