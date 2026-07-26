#include <iostream>
#include <fstream>
#include <sstream>
#include <DirectXMath.h>
#include "tiny_gltf.h"

using namespace std;
using namespace DirectX;

struct MeshOutput
{
	unsigned int mNumPrimitives;
};

struct PrimitiveOutput
{
	unsigned char* mVertexData = nullptr;
	unsigned char* mIndexData = nullptr;
};

size_t totalMeshes = 0;

void FindPrimitiveDataPositionUVs(const tinygltf::Primitive& Primitive, const tinygltf::Model& Model, stringstream& DataToFlushOut)
{
	const tinygltf::Accessor& vertexAccessor = Model.accessors[Primitive.attributes.find("POSITION")->second];
	const tinygltf::BufferView& vertexBufferView = Model.bufferViews[vertexAccessor.bufferView];
	const tinygltf::Buffer& vertexBuffer = Model.buffers[vertexBufferView.buffer];

	const tinygltf::Accessor& uvAccessor = Model.accessors[Primitive.attributes.find("TEXCOORD_0")->second];
	const tinygltf::BufferView& uvBufferView = Model.bufferViews[uvAccessor.bufferView];
	const tinygltf::Buffer& uvBuffer = Model.buffers[uvBufferView.buffer];

	const unsigned char* vertexStart = &vertexBuffer.data[vertexAccessor.byteOffset + vertexBufferView.byteOffset];
	const unsigned char* uvStart     = &uvBuffer.data[uvAccessor.byteOffset + uvBufferView.byteOffset];

	size_t size = vertexAccessor.count * sizeof(float) * 5;
	DataToFlushOut.write((char*)&size, sizeof(size_t));
	size_t vertSize = sizeof(float) * 3;
	size_t uvSize = sizeof(float) * 2;
	for (int elem = 0; elem < vertexAccessor.count; elem++)
	{
		DataToFlushOut.write(reinterpret_cast<const char*>(vertexStart), vertSize);
		vertexStart += vertSize;
		DataToFlushOut.write(reinterpret_cast<const char*>(uvStart), uvSize);
		uvStart += uvSize;
	}
	
	size_t pos = DataToFlushOut.tellp();
}

void WriteOutSceneData(const tinygltf::Node& Node, const tinygltf::Model& Model, stringstream& DataToFlushOut, string NewString, XMMATRIX transform)
{
	// show recursion in output
	cout << NewString << endl;

	int meshIndex = Node.mesh;
	if (meshIndex != -1)
	{
		totalMeshes++;
		const tinygltf::Mesh nextMesh = Model.meshes[meshIndex];

		//write out num primitives
		size_t numPrims = nextMesh.primitives.size();
		DataToFlushOut.write((const char*)&numPrims, sizeof(size_t));
		//write out transform data first
		DataToFlushOut.write((const char*)&transform, sizeof(float) * 16);

		for (const auto& primitive : nextMesh.primitives)
		{
			// get texture info for this primitive
			const tinygltf::Material primMaterial = Model.materials[primitive.material];
			int primTextureIndex = primMaterial.pbrMetallicRoughness.baseColorTexture.index;
			const tinygltf::Image img = Model.images[primTextureIndex];
			std::string albedoName = img.uri;
			size_t albedoNameLength = albedoName.length();
			DataToFlushOut.write((char*)&albedoNameLength, sizeof(size_t));
			DataToFlushOut.write((char*)&albedoName[0], albedoNameLength);

			size_t pos_t = DataToFlushOut.tellp();
			FindPrimitiveDataPositionUVs(primitive, Model, DataToFlushOut);
		
			pos_t = DataToFlushOut.tellp();
			if (primitive.indices >= 0)
			{
				const tinygltf::Accessor& accessor = Model.accessors[primitive.indices];
				const tinygltf::BufferView& bufferView = Model.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = Model.buffers[bufferView.buffer];

				unsigned int stride = 0;
				pos_t = DataToFlushOut.tellp();
				std::cout << "found index data " << accessor.count << " indices " << std::endl;

				if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) stride = 1;
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) stride = 2;
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)   stride = 4;

				const unsigned char* start = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				const unsigned char* end = start + accessor.count * stride;
				size_t size = end - start;
				DataToFlushOut.write((char*)&size, sizeof(size_t));
				DataToFlushOut.write((char*)start, size);

			}
		}
		
	}

	for (const auto& childNode : Node.children)
	{
		const tinygltf::Node nextNode = Model.nodes[childNode];
		XMMATRIX newMat = transform;
		if (nextNode.matrix.size() > 0)
		{
			XMFLOAT4X4 temp;

			float* dst = &temp._11;

			for (size_t i = 0; i < 16; ++i)
			{
				dst[i] = static_cast<float>(nextNode.matrix[i]);
			}

			XMMATRIX mat = XMLoadFloat4x4(&temp);
			//mat = XMMatrixTranspose(mat);
			newMat = transform * mat;
		}

		WriteOutSceneData(nextNode, Model, DataToFlushOut, NewString + "---", newMat);
	}
}

int main()
{
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string error;
	std::string warning;

	bool success = loader.LoadASCIIFromFile(&model, &error, &warning, "models//scene.gltf");

	if (!success)
	{
		std::cout << "Error: " << error.c_str();
		return 1;
	}
	
	ofstream outputFile("models//CookedData.Ducky", std::ios::binary);
	stringstream dataToFlushOut;

	const tinygltf::Scene& scene = model.scenes[model.defaultScene];
	size_t numMeshes = model.meshes.size();
	dataToFlushOut.write((char*)&numMeshes, sizeof(size_t));
	WriteOutSceneData(model.nodes[scene.nodes[0]], model, dataToFlushOut, "", XMMatrixIdentity());

	outputFile << dataToFlushOut.rdbuf();
	size_t s = outputFile.tellp();
	outputFile.close();

	return 0;
}