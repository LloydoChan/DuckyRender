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

struct PBRValues
{
	float roughness = 0.f;
	float metal = 0.f;
};

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
	std::vector<std::byte> Data;
};

class DuckyMeshData;

class DuckyMeshData
{
	public:
		DuckyMeshData() {};
		bool Init(D3DDeviceManager* DeviceManager, std::vector<BufferInfo>& VertexInfos, std::vector<BufferInfo>& IndexInfos, std::vector<size_t>& TextureHashes, std::vector<PBRValues>& pbrValues);
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
									size_t AlbedoHash,
									size_t NormalHash,
									size_t MetallicHash,
									PBRValues& pbrValues);
			private:
				GpuBuffer mVertices;
				GpuBuffer mIndices;

				D3D12_VERTEX_BUFFER_VIEW vertexView{};
				D3D12_INDEX_BUFFER_VIEW indexView{};

				size_t mHashedTextureName = 0;
				size_t mHashedNormalMapName = 0;
				size_t mHashedMetallicRoughnessName = 0;
				UINT mNumVertices = 0;
				UINT mNumIndices = 0;

				PBRValues mMaterialValues{};

				bool InitBuffer(D3DDeviceManager* DeviceManager, BufferInfo& BufferInfo, ComPtr<ID3D12Resource>& Data);

				friend DuckyMeshData;
		};

		std::vector<DuckyPrimitive> mPrimitives;

		void AddPrimitive(DuckyPrimitive&& NewPrimitive) { mPrimitives.emplace_back(std::move(NewPrimitive)); }
};

struct DuckyMeshInstance
{
	XMMATRIX mTransform = XMMatrixIdentity();
	int mMeshDataIndex;
};
