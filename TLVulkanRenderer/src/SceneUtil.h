#pragma once
#include "Typedef.h"

// ---------
// VERTEX
// ----------

typedef enum
{
	INDEX,
	POSITION,
	NORMAL,
	TEXCOORD
} EVertexAttributeType;

typedef struct VertexAttributeInfoTyp
{
	size_t byteStride;
	size_t count;
	int componentLength;
	int componentTypeByteSize;

} VertexAttributeInfo;

// ---------
// GEOMETRY
// ----------

typedef struct GeometryDataTyp
{
	std::map<EVertexAttributeType, std::vector<Byte>> vertexData;
	std::map<EVertexAttributeType, VertexAttributeInfo> vertexAttributes;
} GeometryData;

struct Triangle
{
	glm::vec3 vert0;
	uint32_t id;
	glm::vec3 vert1;
	uint32_t materialid;
	glm::vec3 vert2; 
	uint32_t _pad;
};

// ---------
// MATERIAL
// ----------

typedef struct MaterialTyp
{
	glm::vec4 diffuse;
	glm::vec4 ambient;
	glm::vec4 emission;
	glm::vec4 specular;
	float shininess;
	float transparency;
} Material;



