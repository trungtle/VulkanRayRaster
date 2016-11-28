#pragma once

#include <glm/glm.hpp>
#include <map>
#include "tinygltfloader/tiny_gltf_loader.h"
#include "SceneUtil.h"

class Scene
{
public:
	Scene(std::string fileName);
	~Scene();
	
	std::vector<MeshData*> meshesData;
	std::vector<Material> materials;
	std::vector<glm::ivec4> indices;
	std::vector<glm::vec4> verticePositions;
	std::vector<glm::vec4> verticeNormals;
};

