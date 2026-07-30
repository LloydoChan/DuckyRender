#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <DirectXMath.h>
#include <set>
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

enum class TextureType
{
	BASE_COLOR = 0,
	NORMAL = 1,
	METALLIC_ROUGHNESS = 2,
};

using TexturePair = std::pair<TextureType, std::string>;
std::map<TextureType, std::string> CompressionTypes = { TexturePair {TextureType::BASE_COLOR, "BC7_UNORM_SRGB"},
														TexturePair {TextureType::NORMAL, "BC7_UNORM"}, //normals are linear
														TexturePair {TextureType::METALLIC_ROUGHNESS, "BC7_UNORM"}}; // so are roughness textures

const float stdMax = std::numeric_limits<float>::max();
const float stdMin = std::numeric_limits<float>::lowest();

struct AABB
{
	XMFLOAT4 mMin { stdMax, stdMax, stdMax, stdMax };
	XMFLOAT4 mMax{ stdMin, stdMin, stdMin, stdMin };
};

size_t totalMeshes = 0;

bool ConvertToDds(TextureType Type, const std::string& texconvPath, const std::string& inputJpg, const std::string& outputDir) {
	
	std::string conversionFormat = CompressionTypes[Type];
	// Example command: texconv.exe -f BC1_UNORM -y input.jpg -o C:/output
	std::string cmd = texconvPath + " -f " + conversionFormat +"  -y \"" + inputJpg + "\" -o \"" + outputDir + "\"";

	int result = std::system(cmd.c_str());

	if (result == 0) { std::cout << "Conversion successful!\n"; return true; }
	else { std::cerr << "Conversion failed with code: " << result << "\n"; return false; }
}

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

	// get min / max bounding box points for primitive, transformed

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
			const unsigned char* start = startPoints[accessor];
			int stride = strides[accessor];
			DataToFlushOut.write(reinterpret_cast<const char*>(startPoints[accessor]), strides[accessor]);
			startPoints[accessor] += strides[accessor];
		}
	}

	AABB localBoundingBox;
	localBoundingBox.mMin.x = static_cast<float>(accessors[0].minValues[0]);
	localBoundingBox.mMin.y = static_cast<float>(accessors[0].minValues[1]);
	localBoundingBox.mMin.z = static_cast<float>(accessors[0].minValues[2]);
	localBoundingBox.mMin.w = 1.f;

	localBoundingBox.mMax.x = static_cast<float>(accessors[0].maxValues[0]);
	localBoundingBox.mMax.y = static_cast<float>(accessors[0].maxValues[1]);
	localBoundingBox.mMax.z = static_cast<float>(accessors[0].maxValues[2]);
	localBoundingBox.mMax.w = 1.f;
	
	// write out localBoundingBox
	DataToFlushOut.write(reinterpret_cast<const char*>(&localBoundingBox), sizeof(AABB));
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

void WriteOutTextureData(int index, 
						 tinygltf::Model Model, 
						 stringstream& DataToFlushOut, 
						 TextureType Type, 
						 const std::filesystem::path& InputPath, 
						 const std::filesystem::path& OutputPath)
{
	static std::set<string> alreadySeen;

	if (index != -1)
	{
		const tinygltf::Texture tex = Model.textures[index];
		const tinygltf::Image img = Model.images[tex.source];

		std::string textureName = img.uri;


		const std::filesystem::path expectedDdsPath =
			OutputPath /
			std::filesystem::path(textureName).filename()
			.replace_extension(".dds");

		string outputDirStr = OutputPath.string();
		string expectedDDSPathStr = expectedDdsPath.string();
		size_t expectedDDSPathLength = expectedDDSPathStr.length();

		DataToFlushOut.write((char*)&expectedDDSPathLength, sizeof(size_t));
		DataToFlushOut.write((char*)&expectedDDSPathStr[0], expectedDDSPathLength);

		if (alreadySeen.contains(textureName)) return;
		alreadySeen.insert(textureName);

		const std::filesystem::path expectedInputPath =
			InputPath / textureName;

		ConvertToDds(Type, "texConv.exe", expectedInputPath.string(), outputDirStr);

		cout << expectedDDSPathStr << endl;
	}
}

void WriteOutMeshData(const tinygltf::Node& Node, 
					  const tinygltf::Model& Model, 
					  stringstream& DataToFlushOut, 
					  AABB& GlobalBoundingBox, 
					  const std::filesystem::path& InputPath, 
					  const std::filesystem::path& OutputPath)
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
			WriteOutTextureData(primTextureIndex, Model, DataToFlushOut, TextureType::BASE_COLOR, InputPath, OutputPath);

			int normTextureIndex = primMaterial.normalTexture.index;
			DataToFlushOut.write((char*)&normTextureIndex, sizeof(int));
			WriteOutTextureData(normTextureIndex, Model, DataToFlushOut, TextureType::NORMAL, InputPath, OutputPath);

			int metallicIndex = primMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
			DataToFlushOut.write((char*)&metallicIndex, sizeof(int));
			WriteOutTextureData(metallicIndex, Model, DataToFlushOut, TextureType::METALLIC_ROUGHNESS, InputPath, OutputPath);

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

				if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
				{
					cout << "yikes! dx12 accepts 16 bit indices as smallest" << endl;
					return;
				}
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) stride = 2;
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)   stride = 4;

				DataToFlushOut.write((char*)&stride, sizeof(unsigned int));

				const unsigned char* start = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				const unsigned char* end = start + accessor.count * stride;

				size_t size = end - start;
				DataToFlushOut.write((char*)&size, sizeof(size_t));
				DataToFlushOut.write((char*)start, size);

			}
			else
			{
				// if there are no indices, generate our own in a sequence, makes it easier for rendering engine to handle
				std::cout << " No index Data, generating own" << std::endl;
				auto itr = primitive.attributes.find("POSITION");
				const tinygltf::Accessor& accessor = Model.accessors[itr->second];
				unsigned int stride = sizeof(unsigned int);
				size_t indexBufferSize = accessor.count * stride;
				DataToFlushOut.write((char*)&stride, sizeof(unsigned int));
				DataToFlushOut.write((char*)&indexBufferSize, sizeof(size_t));
				for (unsigned int i = 0; i < accessor.count; i++)
				{
					DataToFlushOut.write((char*)&i, sizeof(unsigned int));
				}
			}
		}
	}

	//DataToFlushOut.write((char*)&GlobalBoundingBox, sizeof(AABB));
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

	string path = "..//Assets//InputAssets//";
	string asset(argv[1]);
	string suffix = "//scene.gltf";

	bool success = loader.LoadASCIIFromFile(&model, &error, &warning, path + asset + suffix);

	if (!success)
	{
		std::cout << "Error: " << error.c_str();
		return 1;
	}

	string outputPath = "..//Assets//CookedAssets//";
	string outputSuffix = "//CookedData.Ducky";

	ofstream outputFile(outputPath + asset + outputSuffix, std::ios::binary);

	if (!outputFile) return 1;
	stringstream dataToFlushOut;

	const tinygltf::Scene& scene = model.scenes[model.defaultScene];
	const tinygltf::Node& rootNode = model.nodes[scene.nodes[0]];

	size_t numMeshNodes = 0;
	CountMeshNodes(rootNode, model, numMeshNodes);
	dataToFlushOut.write((char*)&numMeshNodes, sizeof(size_t));

	XMMATRIX Transform = XMMatrixIdentity();
	WriteOutNodeData(rootNode, model, dataToFlushOut, Transform);

	string textureOutputPath = outputPath + asset;

	const std::filesystem::path outputDirectory = std::filesystem::path("..") / "Assets"  / "CookedAssets" / asset;
	std::filesystem::create_directories(outputDirectory);
	const std::filesystem::path inputPath = std::filesystem::path("..") / "Assets" / "InputAssets" / asset;

	size_t numMeshes = model.meshes.size();
	dataToFlushOut.write((char*)&numMeshes, sizeof(size_t));
	std::filesystem::create_directories(textureOutputPath);
	AABB globalBoundingBox;
	WriteOutMeshData(model.nodes[scene.nodes[0]], model, dataToFlushOut, globalBoundingBox, inputPath, outputDirectory);

	outputFile << dataToFlushOut.rdbuf();
	size_t s = outputFile.tellp();
	outputFile.close();

	return 0;
}