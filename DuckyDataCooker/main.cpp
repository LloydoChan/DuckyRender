#include <iostream>
#include <iomanip>
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

void DebugMatrix(XMMATRIX& nextTransform)
{
	XMFLOAT4X4 debugMatrix;
	XMStoreFloat4x4(
		&debugMatrix,
		nextTransform);

	std::cout
		<< std::setw(10) << debugMatrix._41 << ", "
		<< std::setw(10) << debugMatrix._42 << ", "
		<< std::setw(10) << debugMatrix._43 << ", "
		<< std::setw(10) << debugMatrix._44 << std::endl;
}

void FindPrimitiveData(const tinygltf::Primitive& Primitive, const tinygltf::Model& Model, const vector<string>& AttributeNames, stringstream& DataToFlushOut)
{
	vector<tinygltf::Accessor> accessors;
	vector<tinygltf::BufferView> bufferViews;
	vector<tinygltf::Buffer> buffers;
	vector<const unsigned char*> startPoints;
	vector<int> strides;

	unsigned int byteStride = 0;

	for (const auto& attribute : AttributeNames)
	{
		auto itr = Primitive.attributes.find(attribute);
		if (itr == Primitive.attributes.end())
		{
			cout << "couldn't find for attribute name " << attribute << endl;
			continue;
		}
		const tinygltf::Accessor& accessor = Model.accessors[itr->second];
		const tinygltf::BufferView& bufferView = Model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer& buffer = Model.buffers[bufferView.buffer];

		accessors.emplace_back(std::move(accessor));
		bufferViews.emplace_back(std::move(bufferView));
		buffers.emplace_back(std::move(buffer));

		const unsigned char* bufferStart = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
		startPoints.push_back(bufferStart);
		int stride = accessor.ByteStride(bufferView);
		strides.emplace_back(stride);
		byteStride += stride;
	}

	if (accessors.size() == 0) {
		cout << "no data?" << endl;
		return;
	}

	size_t overallDataSize = accessors[0].count * byteStride;
	DataToFlushOut.write((char*)&byteStride, sizeof(unsigned int));
	DataToFlushOut.write((char*)&overallDataSize, sizeof(size_t));
	for (int elem = 0; elem < accessors[0].count; elem++)
	{
		for (int accessor = 0; accessor < accessors.size(); accessor++)
		{
			DataToFlushOut.write(reinterpret_cast<const char*>(startPoints[accessor]), strides[accessor]);
			startPoints[accessor] += strides[accessor];
		}
	}
	
	size_t pos = DataToFlushOut.tellp();
}

void CountMeshNodes(const tinygltf::Node& Node, const tinygltf::Model& Model, size_t& numMeshNodes)
{
	if (Node.mesh != -1) numMeshNodes++;

	for (auto childNode : Node.children)
	{
		const tinygltf::Node& nextNode = Model.nodes[childNode];
		CountMeshNodes(nextNode, Model, numMeshNodes);
	}
}

void WriteOutNodeData(const tinygltf::Node& Node, const tinygltf::Model& Model, stringstream& DataToFlushOut, XMMATRIX transform)
{
	XMMATRIX nextTransform = XMMatrixIdentity();
	if (Node.matrix.size() != 0)
	{
		unsigned int i = 0;
		for (unsigned int i = 0; i < 16; i+=4)
		{
			XMVECTORF32 nextVector{ static_cast<float>(Node.matrix[i]), 
									 static_cast<float>(Node.matrix[i + 1]), 
									 static_cast<float>(Node.matrix[i + 2]), 
									 static_cast<float>(Node.matrix[i + 3])};

			nextTransform.r[i/4] = nextVector;
		}

		transform = nextTransform * transform;
	}
	else if (Node.translation.size() > 0 || Node.scale.size() > 0 || Node.rotation.size() > 0)
	{
		cout << "huh" << endl;
	}

	if (Node.light != -1)
	{
		cout << "light!" << endl;
	}

	streampos p = 0;
	if (Node.mesh != -1)
	{
		XMFLOAT4X4 storedWorld;
		XMStoreFloat4x4(&storedWorld,transform);
		tinygltf::Mesh mesh = Model.meshes[Node.mesh];
		int nodeMesh = Node.mesh;
		DataToFlushOut.write((char*)&nodeMesh, sizeof(int));
		DataToFlushOut.write((char*)&storedWorld, sizeof(XMFLOAT4X4));
	}

	for (auto childNode : Node.children)
	{
		const tinygltf::Node& nextNode = Model.nodes[childNode];
		WriteOutNodeData(nextNode, Model, DataToFlushOut, transform);
	}
}

void WriteOutTextureData(int index, tinygltf::Model Model, stringstream& DataToFlushOut)
{
	if (index != -1)
	{
		const tinygltf::Texture tex = Model.textures[index];
		const tinygltf::Image img = Model.images[tex.source];
		std::string albedoName = img.uri;
		size_t albedoNameLength = albedoName.length();
		DataToFlushOut.write((char*)&albedoNameLength, sizeof(size_t));
		DataToFlushOut.write((char*)&albedoName[0], albedoNameLength);
	}
}

void WriteOutMeshData(const tinygltf::Node& Node, const tinygltf::Model& Model, stringstream& DataToFlushOut)
{
	// first write out Node info, get transforms and mesh indices of all nodes...
	//top node

	// now write out Mesh Data
	for (auto& mesh : Model.meshes)
	{
		size_t numPrims = mesh.primitives.size();
		DataToFlushOut.write((char*)&numPrims, sizeof(size_t));

		for (const auto& primitive : mesh.primitives)
		{
			// get texture info for this primitive
			const tinygltf::Material primMaterial = Model.materials[primitive.material];
			int primTextureIndex = primMaterial.pbrMetallicRoughness.baseColorTexture.index;

			DataToFlushOut.write((char*)&primTextureIndex, sizeof(int));
			WriteOutTextureData(primTextureIndex, Model, DataToFlushOut);

			int normTextureIndex = primMaterial.normalTexture.index;
			DataToFlushOut.write((char*)&normTextureIndex, sizeof(int));
			WriteOutTextureData(normTextureIndex, Model, DataToFlushOut);

			int metallicIndex = primMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
			DataToFlushOut.write((char*)&metallicIndex, sizeof(int));
			WriteOutTextureData(metallicIndex, Model, DataToFlushOut);

			const tinygltf::PbrMetallicRoughness& pbrValues = primMaterial.pbrMetallicRoughness;
			float roughness = static_cast<float>(pbrValues.roughnessFactor);
			DataToFlushOut.write((char*)&roughness, sizeof(float));
			float metal = static_cast<float>(pbrValues.metallicFactor);
			DataToFlushOut.write((char*)&metal, sizeof(float));

			
			std::vector<std::string> names{ "POSITION", "TEXCOORD_0", "TEXCOORD_1", "NORMAL", "TANGENT", "COLOR_0"};
			FindPrimitiveData(primitive, Model,names, DataToFlushOut);
			
			if (primitive.indices >= 0)
			{
				const tinygltf::Accessor& accessor = Model.accessors[primitive.indices];
				const tinygltf::BufferView& bufferView = Model.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = Model.buffers[bufferView.buffer];

				unsigned int stride = 0;
				std::cout << "found index data " << accessor.count << " indices " << std::endl;

				if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) stride = 1;
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) stride = 2;
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)   stride = 4;

				DataToFlushOut.write((char*)&stride, sizeof(unsigned int));

				const unsigned char* start = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				const unsigned char* end = start + accessor.count * stride;

				size_t size = end - start;
				DataToFlushOut.write((char*)&size, sizeof(size_t));
				DataToFlushOut.write((char*)start, size);

			}
		}
	}
}

int main(int argc, char** argv)
{
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string error;
	std::string warning;

	if (argc < 2)
	{
		cout << "please give a input model to the command line" << endl;
		return 1;
	}

	bool success = loader.LoadASCIIFromFile(&model, &error, &warning, argv[1]);

	if (!success)
	{
		std::cout << "Error: " << error.c_str();
		return 1;
	}
	
	ofstream outputFile("models//CookedData.Ducky", std::ios::binary);
	stringstream dataToFlushOut;

	const tinygltf::Scene& scene = model.scenes[model.defaultScene];
	const tinygltf::Node& rootNode = model.nodes[scene.nodes[0]];

	size_t numMeshNodes = 0;
	CountMeshNodes(rootNode, model, numMeshNodes);
	dataToFlushOut.write((char*)&numMeshNodes, sizeof(size_t));

	XMMATRIX Transform = XMMatrixIdentity();
	WriteOutNodeData(rootNode, model, dataToFlushOut, Transform);

	size_t numMeshes = model.meshes.size();
	dataToFlushOut.write((char*)&numMeshes, sizeof(size_t));
	WriteOutMeshData(model.nodes[scene.nodes[0]], model, dataToFlushOut);

	outputFile << dataToFlushOut.rdbuf();
	size_t s = outputFile.tellp();
	outputFile.close();

	return 0;
}