#pragma once

#include "Typedef.h"

/**
 * \brief Read a binary file and returns its bytes
 * \param fileName 
 * \return bytes from the binary file
 */
std::vector<Byte>
ReadBinaryFile(
	const std::string& fileName
	);


/**
 * \brief Load SPIR_V binary
 * \param vertShaderFilePath 
 * \param fragShaderFilePath 
 */
void 
LoadSPIR_V(
	const char* filepath, 
	std::vector<Byte>& outShader
);