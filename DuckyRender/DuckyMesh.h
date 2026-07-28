#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <fstream>
#include <wrl/client.h>

#include "DuckyMaterial.h"

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
	unsigned int Stride = 0;
	std::vector<std::byte> Data;
};

struct PrimitiveLoadData
{
	BufferInfo vertexBuffer;
	BufferInfo indexBuffer;
	DuckyMaterial material;
};

class DuckyPrimitive
{
public:
	DuckyPrimitive() {};
	bool InitPrimitive(D3DDeviceManager* DeviceManager,
		BufferInfo& VertexBufferInfo,
		BufferInfo& IndexBufferInfo,
		UINT MaterialIndex);

	void BindGeometry(ID3D12GraphicsCommandList* commandList) const;
	void Draw(ID3D12GraphicsCommandList* commandList) const;

	uint32_t GetMaterialIndex() const { return mMaterialIndex; }

private:
	GpuBuffer mVertices;
	GpuBuffer mIndices;

	D3D12_VERTEX_BUFFER_VIEW mVertexView{};
	D3D12_INDEX_BUFFER_VIEW mIndexView{};
	UINT mNumVertices = 0;
	UINT mNumIndices = 0;

	UINT mMaterialIndex = 0;

	bool InitBuffer(D3DDeviceManager* DeviceManager, BufferInfo& BufferInfo, ComPtr<ID3D12Resource>& Data);
};

class DuckyMeshData
{
public:
	const std::vector<DuckyPrimitive>& GetPrimitives() const { return mPrimitives; }
	const DuckyMaterial& GetMaterial(uint32_t index) const { return mMaterials[index]; }
	bool Init(D3DDeviceManager* deviceManager, std::ifstream& inputFile, UINT descriptorHeapHandle);
	size_t GetPrimitiveCount() { return mPrimitives.size(); }

private:
	bool ReadPrimitive( D3DDeviceManager* deviceManager, std::ifstream& inputFile, UINT descriptorHeapHandle, PrimitiveLoadData& output);
	size_t ReadTexture( D3DDeviceManager* deviceManager, std::ifstream& inputFile, UINT descriptorHeapHandle);

	static bool ReadBufferInfo( std::ifstream& inputFile,BufferInfo& output);


	std::vector<DuckyPrimitive> mPrimitives;
	std::vector<DuckyMaterial> mMaterials;
};

struct DuckyMeshInstance
{
	int mMeshDataIndex = -1;
	XMMATRIX mTransform = XMMatrixIdentity();
};