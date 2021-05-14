#pragma once
#include"allincludes.h"
#include"Renderer.h"
class GltfLoader
{
public:
	GltfLoader(Renderer* renderer);
	~GltfLoader();

	void modelLoader();

	// A primitive contains the data for a single draw call
	struct Primitive {
		uint32_t firstIndex;
		uint32_t indexCount;
		int32_t materialIndex;
	};

	// Contains the node's (optional) geometry and can be made up of an arbitrary number of primitives
	struct Mesh {
		std::vector<Primitive> primitives;
	};

	// A node represents an object in the glTF scene graph
	struct Node {
		Node* parent;
		std::vector<Node> children;
		Mesh mesh;
		glm::mat4 matrix;
	};

	// A glTF material stores information in e.g. the texture that is attached to it and colors
	struct Material {
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		uint32_t baseColorTextureIndex;
	};
	std::vector<GltfLoader::Material> materials;
	std::vector<GltfLoader::Node> nodess;
private:

	Renderer* _renderer = nullptr;
};

