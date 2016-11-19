#pragma once

typedef unsigned char Byte;

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
	int materialid;
	glm::vec3 vert0; 
	glm::vec3 vert1; 
	glm::vec3 vert2; 
	glm::vec3 norm0; 
	glm::vec3 norm1; 
	glm::vec3 norm2; 
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



