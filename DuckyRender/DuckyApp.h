#pragma once
#include "DuckyMaterial.h"
#include "DuckyMesh.h"
#include "DuckyPipelineManager.h"
#include "DuckyScene.h"

using namespace DirectX;

class DuckyApp
{
public:

	virtual ~DuckyApp();

	virtual bool Init(UINT WindowWidth, UINT WindowHeight, const wchar_t* WindowName);
	static LRESULT CALLBACK StaticWindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	virtual LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) = 0;
	virtual void HandleInput(UINT msg, WPARAM wParam, LPARAM lParam) = 0;
	virtual void AppMainLoop() = 0;

	HWND GetWindowHandle() { return mWindowHandle; }

	// GPU timing
	bool InitGPUTimeStamps();
	void StartGPUTimeStamp(ID3D12GraphicsCommandList* commandList, UINT frameIndex);
	void EndGPUTimeStamp(ID3D12GraphicsCommandList* commandList, UINT frameIndex);
	double GetGPUFrameMilliSeconds(UINT frameIndex);

	// GPU Stats
	bool InitGPUStats();
	void StartGpuStats(ID3D12GraphicsCommandList* commandList, UINT frameIndex);
	void EndGPUStats(ID3D12GraphicsCommandList* commandList, UINT frameIndex);
	D3D12_QUERY_DATA_PIPELINE_STATISTICS WriteOutGPUStats(UINT FrameIndex);

	bool InitDebugDrawsVBAndIB();

protected:
	
	std::unique_ptr<D3DDeviceManager> mDeviceManager;

	HINSTANCE mHInstance = nullptr;
	LPCWSTR mLpszClassName = nullptr;

	std::wofstream mLogFile;

	HANDLE mFenceEvent = nullptr;
	HWND mWindowHandle = 0;

	UINT mClientWidth  = 0;
	UINT mClientHeight = 0;

	bool mMinimized = false;

	std::wstring mInputFilePath;

	ID3D12CommandQueue* mCommandQueue = nullptr;

  //for gpu timestamps
	static constexpr UINT QueriesPerFrame = 2;

	Microsoft::WRL::ComPtr<ID3D12QueryHeap> mQueryHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource>  mReadbackBuffer;

	//for gpu stats
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> mPipelineStatsHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource>  mPipelineStatsReadback;

	UINT64* mMappedTimestamps = nullptr;

	UINT64 mFrameCount = 0;
	UINT64 mTimeStampFrequency = 0;

	MappedDescriptorHeapResource mDrawsBuffer;
	
	ComPtr<ID3D12CommandSignature> mDrawCommandSignature;

	Microsoft::WRL::ComPtr<ID3D12Resource> mDebugVertices;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDebugIndices;

	D3D12_VERTEX_BUFFER_VIEW mVbDebugView;
	D3D12_INDEX_BUFFER_VIEW mIbDebugView;

	DuckyPipelineManager mPipelineManager;
	DuckyScene mScene;
};