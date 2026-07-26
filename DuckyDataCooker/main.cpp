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

void FindPrimitiveData(const tinygltf::Primitive& Primitive, const tinygltf::Model& Model, stringstream& DataToFlushOut, const char* Attribute)
{
	if (Primitive.attributes.find(Attribute) != Primitive.attributes.end())
	{
		const tinygltf::Accessor& accessor = Model.accessors[Primitive.attributes.find(Attribute)->second];
		const tinygltf::BufferView& bufferView = Model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer& buffer = Model.buffers[bufferView.buffer];

		const unsigned char* start = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
		const unsigned char* end = start + accessor.count * (sizeof(float) * 3);

		size_t size = end - start;

		std::cout << "found " << Attribute << " " << accessor.count << " vertices " << std::endl;
		DataToFlushOut.write((char*)&size, sizeof(size_t));
		DataToFlushOut.write(reinterpret_cast<const char*>(start), size);
		size_t pos = DataToFlushOut.tellp();
	}
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
			size_t pos_t = DataToFlushOut.tellp();
			FindPrimitiveData(primitive, Model, DataToFlushOut, "POSITION");

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
			transform = transform * mat;
		}

		WriteOutSceneData(nextNode, Model, DataToFlushOut, NewString + "---", transform);
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