//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  Alex Nankervis
//

#include "Model.h"
#include "Utility.h"
#include "TextureManager.h"
#include "GraphicsCore.h"
#include "DescriptorHeap.h"
#include "CommandContext.h"
#include <stdio.h>


static void PrintModelStats(const Model *model)
{
	printf("model stats:\n");

	BoundingBox bbox = model->GetBoundingBox();
	printf("bounding box: <%f, %f, %f> <%f, %f, %f>\n"
		, (float)bbox.min.GetX(), (float)bbox.min.GetY(), (float)bbox.min.GetZ()
		, (float)bbox.max.GetX(), (float)bbox.max.GetY(), (float)bbox.max.GetZ());

	printf("vertex data size: %u\n", model->m_Header.vertexDataByteSize);
	printf("index data size: %u\n", model->m_Header.indexDataByteSize);
	printf("vertex data size depth-only: %u\n", model->m_Header.vertexDataByteSizeDepth);
	printf("\n");

	printf("mesh count: %u\n", model->m_Header.meshCount);
	for (unsigned int meshIndex = 0; meshIndex < model->m_Header.meshCount; meshIndex++)
	{
		const Model::Mesh *mesh = model->m_pMesh.get() + meshIndex;

		auto printAttribFormat = [](unsigned int format) -> void
		{
			switch (format)
			{
			case Model::attrib_format_ubyte:
				printf("ubyte");
				break;

			case Model::attrib_format_byte:
				printf("byte");
				break;

			case Model::attrib_format_ushort:
				printf("ushort");
				break;

			case Model::attrib_format_short:
				printf("short");
				break;

			case Model::attrib_format_float:
				printf("float");
				break;
			}
		};

		printf("mesh %u\n", meshIndex);
		printf("vertices: %u\n", mesh->vertexCount);
		printf("indices: %u\n", mesh->indexCount);
		printf("vertex stride: %u\n", mesh->vertexStride);
		for (int n = 0; n < Model::maxAttribs; n++)
		{
			if (mesh->attrib[n].format == Model::attrib_format_none)
				continue;

			printf("attrib %d: offset %u, normalized %u, components %u, format "
				, n, mesh->attrib[n].offset, mesh->attrib[n].normalized
				, mesh->attrib[n].components);
			printAttribFormat(mesh->attrib[n].format);
			printf("\n");
		}

		printf("vertices depth-only: %u\n", mesh->vertexCountDepth);
		printf("vertex stride depth-only: %u\n", mesh->vertexStrideDepth);
		for (int n = 0; n < Model::maxAttribs; n++)
		{
			if (mesh->attribDepth[n].format == Model::attrib_format_none)
				continue;

			printf("attrib %d: offset %u, normalized %u, components %u, format "
				, n, mesh->attribDepth[n].offset, mesh->attribDepth[n].normalized
				, mesh->attribDepth[n].components);
			printAttribFormat(mesh->attrib[n].format);
			printf("\n");
		}
	}
	printf("\n");

	printf("material count: %u\n", model->m_Header.materialCount);
	for (unsigned int materialIndex = 0; materialIndex < model->m_Header.materialCount; materialIndex++)
	{
		const Model::Material *material = model->m_pMaterial.get() + materialIndex;
		printf("material diffuse tex:%s", material->texDiffusePath);
		printf("material %u\n", materialIndex);
	}
	printf("\n");
}

static std::string defaultDiffuseName = "white";

void Model::PrintInfo() const
{
	PrintModelStats(this);
}

bool Model::LoadH3D(const char *filename)
{
    FILE *file = nullptr;
    if (0 != fopen_s(&file, filename, "rb"))
        return false;

    bool ok = false;

    if (1 != fread(&m_Header, sizeof(Header), 1, file)) goto h3d_load_fail;

    m_pMesh = std::make_unique<Mesh[]>(m_Header.meshCount);
    m_pMaterial = std::make_unique<Material[]>(m_Header.materialCount);;

    if (m_Header.meshCount > 0)
        if (1 != fread(m_pMesh.get(), sizeof(Mesh) * m_Header.meshCount, 1, file)) goto h3d_load_fail;
    if (m_Header.materialCount > 0)
        if (1 != fread(m_pMaterial.get(), sizeof(Material) * m_Header.materialCount, 1, file)) goto h3d_load_fail;

    m_VertexStride = m_pMesh[0].vertexStride;
    m_VertexStrideDepth = m_pMesh[0].vertexStrideDepth;
#if _DEBUG
    for (uint32_t meshIndex = 1; meshIndex < m_Header.meshCount; ++meshIndex)
    {
        const Mesh& mesh = m_pMesh[meshIndex];
        ASSERT(mesh.vertexStride == m_VertexStride);
        ASSERT(mesh.vertexStrideDepth == m_VertexStrideDepth);
    }
    for (uint32_t meshIndex = 0; meshIndex < m_Header.meshCount; ++meshIndex)
    {
        const Mesh& mesh = m_pMesh[meshIndex];

        ASSERT( mesh.attribsEnabled ==
            (attrib_mask_position | attrib_mask_texcoord0 | attrib_mask_normal | attrib_mask_tangent | attrib_mask_bitangent) );
        ASSERT(mesh.attrib[0].components == 3 && mesh.attrib[0].format == Model::attrib_format_float); // position
        ASSERT(mesh.attrib[1].components == 2 && mesh.attrib[1].format == Model::attrib_format_float); // texcoord0
        ASSERT(mesh.attrib[2].components == 3 && mesh.attrib[2].format == Model::attrib_format_float); // normal
        ASSERT(mesh.attrib[3].components == 3 && mesh.attrib[3].format == Model::attrib_format_float); // tangent
        ASSERT(mesh.attrib[4].components == 3 && mesh.attrib[4].format == Model::attrib_format_float); // bitangent

        ASSERT( mesh.attribsEnabledDepth ==
            (attrib_mask_position) );
        ASSERT(mesh.attrib[0].components == 3 && mesh.attrib[0].format == Model::attrib_format_float); // position
    }
#endif

	m_pVertexData = std::make_unique<unsigned char[]>(m_Header.vertexDataByteSize);
    m_pIndexData = std::make_unique<unsigned char[]>(m_Header.indexDataByteSize); 
    m_pVertexDataDepth = std::make_unique<unsigned char[]>(m_Header.vertexDataByteSizeDepth); 
    m_pIndexDataDepth = std::make_unique<unsigned char[]>(m_Header.indexDataByteSize);

    if (m_Header.vertexDataByteSize > 0)
        if (1 != fread(m_pVertexData.get(), m_Header.vertexDataByteSize, 1, file)) goto h3d_load_fail;
    if (m_Header.indexDataByteSize > 0)
        if (1 != fread(m_pIndexData.get(), m_Header.indexDataByteSize, 1, file)) goto h3d_load_fail;

    if (m_Header.vertexDataByteSizeDepth > 0)
        if (1 != fread(m_pVertexDataDepth.get(), m_Header.vertexDataByteSizeDepth, 1, file)) goto h3d_load_fail;
    if (m_Header.indexDataByteSize > 0)
        if (1 != fread(m_pIndexDataDepth.get(), m_Header.indexDataByteSize, 1, file)) goto h3d_load_fail;

    m_VertexBuffer.Create(L"VertexBuffer", m_Header.vertexDataByteSize / m_VertexStride, m_VertexStride, m_pVertexData.get());
    m_IndexBuffer.Create(L"IndexBuffer", m_Header.indexDataByteSize / sizeof(uint16_t), sizeof(uint16_t), m_pIndexData.get());
 

    m_VertexBufferDepth.Create(L"VertexBufferDepth", m_Header.vertexDataByteSizeDepth / m_VertexStrideDepth, m_VertexStrideDepth, m_pVertexDataDepth.get());
    m_IndexBufferDepth.Create(L"IndexBufferDepth", m_Header.indexDataByteSize / sizeof(uint16_t), sizeof(uint16_t), m_pIndexDataDepth.get());
	m_pVertexData = nullptr;
	m_pIndexData = nullptr;
	m_pVertexDataDepth = nullptr;
	m_pIndexDataDepth = nullptr;
    LoadTextures();

    ok = true;

h3d_load_fail:

    if (EOF == fclose(file))
        ok = false;

    return ok;
}

bool Model::SaveH3D(const char *filename) const
{
    FILE *file = nullptr;
    if (0 != fopen_s(&file, filename, "wb"))
        return false;

    bool ok = false;

    if (1 != fwrite(&m_Header, sizeof(Header), 1, file)) goto h3d_save_fail;

    if (m_Header.meshCount > 0)
        if (1 != fwrite(m_pMesh.get(), sizeof(Mesh) * m_Header.meshCount, 1, file)) goto h3d_save_fail;
    if (m_Header.materialCount > 0)
        if (1 != fwrite(m_pMaterial.get(), sizeof(Material) * m_Header.materialCount, 1, file)) goto h3d_save_fail;

    if (m_Header.vertexDataByteSize > 0)
        if (1 != fwrite(m_pVertexData.get(), m_Header.vertexDataByteSize, 1, file)) goto h3d_save_fail;
    if (m_Header.indexDataByteSize > 0)
        if (1 != fwrite(m_pIndexData.get(), m_Header.indexDataByteSize, 1, file)) goto h3d_save_fail;

    if (m_Header.vertexDataByteSizeDepth > 0)
        if (1 != fwrite(m_pVertexDataDepth.get(), m_Header.vertexDataByteSizeDepth, 1, file)) goto h3d_save_fail;
    if (m_Header.indexDataByteSize > 0)
        if (1 != fwrite(m_pIndexDataDepth.get(), m_Header.indexDataByteSize, 1, file)) goto h3d_save_fail;

    ok = true;

h3d_save_fail:

    if (EOF == fclose(file))
        ok = false;

    return ok;
}

void Model::ReleaseTextures()
{
    /*
    if (m_Textures != nullptr)
    {
        for (uint32_t materialIdx = 0; materialIdx < m_Header.materialCount; ++materialIdx)
        {
            for (int n = 0; n < Material::texCount; n++)
            {
                if (m_Textures[materialIdx * Material::texCount + n])
                    m_Textures[materialIdx * Material::texCount + n]->Release();
                m_Textures[materialIdx * Material::texCount + n] = nullptr;
            }
        }
        delete [] m_Textures;
    }
    */
}

void Model::LoadTextures(void)
{
    ReleaseTextures();

    m_SRVs = new D3D12_CPU_DESCRIPTOR_HANDLE[m_Header.materialCount * 6];

    const ManagedTexture* MatTextures[6] = {};

    for (uint32_t materialIdx = 0; materialIdx < m_Header.materialCount; ++materialIdx)
    {
        const Material& pMaterial = m_pMaterial[materialIdx];

        // Load diffuse
        MatTextures[0] = TextureManager::LoadFromFile(pMaterial.texDiffusePath, true);
        if (!MatTextures[0]->IsValid())
            MatTextures[0] = TextureManager::LoadFromFile(defaultDiffuseName.c_str(), true);

        // Load specular
        MatTextures[1] = TextureManager::LoadFromFile(pMaterial.texSpecularPath, true);
        if (!MatTextures[1]->IsValid())
        {
            MatTextures[1] = TextureManager::LoadFromFile(std::string(pMaterial.texDiffusePath) + "_specular", true);
            if (!MatTextures[1]->IsValid())
                MatTextures[1] = TextureManager::LoadFromFile("default_specular", true);
        }

        // Load emissive
        //MatTextures[2] = TextureManager::LoadFromFile(pMaterial.texEmissivePath, true);

        // Load normal
        MatTextures[3] = TextureManager::LoadFromFile(pMaterial.texNormalPath, false);
        if (!MatTextures[3]->IsValid())
        {
            MatTextures[3] = TextureManager::LoadFromFile(std::string(pMaterial.texDiffusePath) + "_normal", false);
            if (!MatTextures[3]->IsValid())
                MatTextures[3] = TextureManager::LoadFromFile("default_normal", false);
        }

        // Load lightmap
        //MatTextures[4] = TextureManager::LoadFromFile(pMaterial.texLightmapPath, true);

        // Load reflection
        //MatTextures[5] = TextureManager::LoadFromFile(pMaterial.texReflectionPath, true);

        m_SRVs[materialIdx * 6 + 0] = MatTextures[0]->GetSRV();
        m_SRVs[materialIdx * 6 + 1] = MatTextures[1]->GetSRV();
        m_SRVs[materialIdx * 6 + 2] = MatTextures[0]->GetSRV();
        m_SRVs[materialIdx * 6 + 3] = MatTextures[3]->GetSRV();
        m_SRVs[materialIdx * 6 + 4] = MatTextures[0]->GetSRV();
        m_SRVs[materialIdx * 6 + 5] = MatTextures[0]->GetSRV();
    }
	ASSERT(m_Header.meshCount > 0, "Model contains no meshes");
	m_pMaterialIsCutout.resize(m_Header.materialCount);
	for (uint32_t i = 0; i < m_Header.materialCount; ++i)
	{
		const Model::Material& mat = m_pMaterial[i];
		if (std::string(mat.texDiffusePath).find("thorn") != std::string::npos ||
			std::string(mat.texDiffusePath).find("plant") != std::string::npos ||
			std::string(mat.texDiffusePath).find("chain") != std::string::npos)
		{
			m_pMaterialIsCutout[i] = true;
		}
		else
		{
			m_pMaterialIsCutout[i] = false;
		}
	}
}
