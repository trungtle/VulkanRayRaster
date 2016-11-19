#pragma once

#include <glm/glm.hpp>
#include <map>
#include "thirdparty/tinygltfloader/tiny_gltf_loader.h"
#include "SceneUtil.h"

class Scene
{
public:
	Scene(std::string fileName);
	~Scene();
	
	std::vector<GeometryData*> m_geometriesData;
	std::vector<Material> m_materials;
};

