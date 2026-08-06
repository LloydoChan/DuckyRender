#pragma once

#include "DuckyMaterial.h"

using Microsoft::WRL::ComPtr;

using namespace DirectX;

class D3DDeviceManager;
struct ID3D12Resource;

const unsigned int AABB_MIN = 0;
const unsigned int AABB_MAX = 1;

struct CookedVertex
{
	XMFLOAT3 position{ 0.f,0.f,0.f };
	XMFLOAT3 normal{ 0.f, 1.f, 0.f };
	XMFLOAT4 tangent{ 1.f, 0.f, 0.f, 1.f };
	XMFLOAT2 texcoord0{ 0.f, 0.f };
	XMFLOAT4 color0{ 1.f,1.f,1.f,1.f };
};

class AABB
{
	public:
		AABB();
		AABB(const XMVECTOR& Min, const XMVECTOR& Max);

		const XMVECTOR& GetMin() const { return mVertices[AABB_MIN]; }
		const XMVECTOR& GetMax() const { return mVertices[AABB_MAX]; }

		void SetNewMinMax(const XMVECTOR& newMin, const XMVECTOR& newMax) {
			mVertices[AABB_MIN] = newMin;
			mVertices[AABB_MAX] = newMax;
			RegenerateBox(newMin, newMax);
		}

		XMVECTOR const * GetPointsAddress() const { return &mVertices[0]; }

	private:
		void RegenerateBox(const XMVECTOR& newMin, const XMVECTOR& newMax);
		XMVECTOR mVertices[8] {};
};


class DuckyPrimitive
{
	public:
		DuckyPrimitive(size_t NumIndices, size_t NumVertices, size_t IndexOffset, size_t VertexOffset, unsigned int MaterialIndex) : mNumIndices(NumIndices),
																																	 mNumVertices(NumVertices),
																																     mVertexOffset(VertexOffset),
																																     mIndexOffset(IndexOffset),
																																     mMaterialIndex(MaterialIndex){};
	
		void Draw(ID3D12GraphicsCommandList* commandList) const;

		uint32_t GetMaterialIndex() const { return mMaterialIndex; }
		const AABB& GetBoundingBox() const { return mPrimitiveAABB; }

		size_t GetNumVertices() const { return mNumVertices; }
		size_t GetNumIndices() const { return mNumIndices; }
	private:
		size_t mNumIndices = 0;
		size_t mNumVertices = 0;
		size_t mVertexOffset = 0;
		size_t mIndexOffset = 0;

		UINT mMaterialIndex = 0;

		AABB mPrimitiveAABB;
};

class DuckyMeshData
{
	public:
		const std::vector<DuckyPrimitive>& GetPrimitives() const { return mPrimitives; }
		const DuckyPrimitive& GetPrimitive(unsigned int Index) const { return mPrimitives[Index]; }
		void AddPrimitive(const DuckyPrimitive& Primitive) { mPrimitives.emplace_back(Primitive); }
		size_t GetPrimitiveCount() const { return mPrimitives.size(); }

	private:
		std::vector<DuckyPrimitive> mPrimitives;
};

struct DuckyMeshInstance
{
	int mMeshDataIndex = -1;
	XMMATRIX mTransform = XMMatrixIdentity();
};