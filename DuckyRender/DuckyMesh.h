#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>

class D3DDeviceManager;

struct VBPair
{
	ID3D12Resource* vertBuffPointer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
};

struct IBPair
{
	ID3D12Resource* idxBuffPointer = nullptr;
	D3D12_INDEX_BUFFER_VIEW ibView = {};
};

struct BufferInfo
{
	UINT BufferSize = 0;
	UINT Stride = 0;
	void* Data = nullptr;
};

class DuckyMesh;


class DuckyMesh
{
	public:
		DuckyMesh() {};
		bool Init(D3DDeviceManager* DeviceManager, std::vector<BufferInfo>& VertexInfos, std::vector<BufferInfo>& IndexInfos);
		void DrawMesh(ID3D12GraphicsCommandList* mCmdList);
	protected:

		class DuckyPrimitive
		{
			public:
				DuckyPrimitive() {};
				bool InitPrimitive(D3DDeviceManager* DeviceManager,
									BufferInfo& VertexBufferInfo,
									BufferInfo& IndexBufferInfo);
			private:
				VBPair mVertices;
				IBPair mIndices;
				UINT mNumVertices = 0;
				UINT mNumIndices = 0;

				bool InitBuffer(D3DDeviceManager* DeviceManager, BufferInfo& BufferInfo, ID3D12Resource** Data);

				friend DuckyMesh;
		};

		std::vector<DuckyPrimitive> mPrimitives;

		void AddPrimitive(DuckyPrimitive&& NewPrimitive) { mPrimitives.emplace_back(NewPrimitive); }
};