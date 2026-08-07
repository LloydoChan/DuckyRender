#pragma once
#include "pch.h"
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

const float stdMax = (std::numeric_limits<float>::max)();
const float stdMin = std::numeric_limits<float>::lowest();


class AABB
{
	public:
		AABB();
		AABB(const XMFLOAT4& Min, const XMFLOAT4& Max) : mVertices { Min, Max } { RegenerateBox(); }

		const XMFLOAT4& GetMin() const { return mVertices[AABB_MIN]; }
		const XMFLOAT4& GetMax() const { return mVertices[AABB_MAX]; }

		void SetNewMinMax(const XMFLOAT4& newMin, const XMFLOAT4& newMax) {
			mVertices[AABB_MIN] = newMin;
			mVertices[AABB_MAX] = newMax;
			RegenerateBox();
		}

		XMFLOAT4 const * GetPointsAddress() const { return &mVertices[0]; }

	private:
		void RegenerateBox();
		XMFLOAT4 mVertices[8];
};


class DuckyPrimitive
{
	public:
		DuckyPrimitive(size_t NumIndices, 
			size_t NumVertices, 
			size_t IndexOffset, 
			size_t VertexOffset, 
			unsigned int MaterialIndex, 
			AABB& BoundingBox) : mNumIndices(NumIndices),
								 mNumVertices(NumVertices),
								 mVertexOffset(VertexOffset),
								 mIndexOffset(IndexOffset),
								 mMaterialIndex(MaterialIndex),
								 mPrimitiveAABB(BoundingBox) {};
	
		void Draw(ID3D12GraphicsCommandList* commandList) const;

		uint32_t GetMaterialIndex() const { return mMaterialIndex; }
		const AABB& GetBoundingBox() const { return mPrimitiveAABB; }
		void SetBoundingBox(const AABB& NewBB) { mPrimitiveAABB = NewBB; }

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