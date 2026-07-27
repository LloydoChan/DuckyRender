#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <fstream>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

using namespace DirectX;

class D3DDeviceManager;
struct ID3D12Resource;

struct GpuBuffer
{
	ComPtr<ID3D12Resource> resource;

	uint64_t size = 0;
	uint32_t stride = 0;

	D3D12_RESOURCE_STATES state =
		D3D12_RESOURCE_STATE_COMMON;
};

struct BufferInfo
{
	size_t BufferSize = 0;
	UINT Stride = 0;
	void* Data = nullptr;
};

class DuckyMeshData;

class DuckyMeshData
{
	public:
		DuckyMeshData() {};
		bool Init(D3DDeviceManager* DeviceManager, std::vector<BufferInfo>& VertexInfos, std::vector<BufferInfo>& IndexInfos, std::vector<size_t>& TextureHashes);
		bool Init(D3DDeviceManager* DeviceManager, std::ifstream& InFile, UINT DescriptorHeapHandle);
		void DrawMesh(ID3D12GraphicsCommandList* CmdList, D3DDeviceManager* DeviceManager);

	protected:

		class DuckyPrimitive
		{
			public:
				DuckyPrimitive() {};
				bool InitPrimitive(D3DDeviceManager* DeviceManager,
									BufferInfo& VertexBufferInfo,
									BufferInfo& IndexBufferInfo,
									size_t TextureHash);
			private:
				GpuBuffer mVertices;
				GpuBuffer mIndices;

				D3D12_VERTEX_BUFFER_VIEW vertexView{};
				D3D12_INDEX_BUFFER_VIEW indexView{};

				UINT mHashedTextureName = 0;
				UINT mNumVertices = 0;
				UINT mNumIndices = 0;

				bool InitBuffer(D3DDeviceManager* DeviceManager, BufferInfo& BufferInfo, ID3D12Resource** Data);

				friend DuckyMeshData;
		};

		std::vector<DuckyPrimitive> mPrimitives;

		void AddPrimitive(DuckyPrimitive&& NewPrimitive) { mPrimitives.emplace_back(NewPrimitive); }
};

struct DuckyMeshInstance
{
	XMMATRIX mTransform = XMMatrixIdentity();
	DuckyMeshData* mMeshData;
};
