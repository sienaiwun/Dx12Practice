#pragma once

#include <cstdint>
#include "PipelineState.h"
#include "RootSignature.h"
#include "CommandContext.h"
#include "Camera.h"
#include "BufferManager.h"

class StructuredBuffer;
class ByteAddressBuffer;
class ColorBuffer;
class ShadowBuffer;
class GraphicsContext;
class IntVar;
class RootParameter;


namespace Math
{
	class Vector3;
	class Matrix4;
	class Camera;
}


namespace SceneView
{
	extern IntVar LightGridDim;

	enum { MaxLights = 128 };

	enum { shadowDim = 512 };

	struct LightData
	{
		float pos[3];
		float radiusSq;
		float color[3];

		uint32_t type;
		float coneDir[3];
		float coneAngles[2];

		float shadowTextureMatrix[16];
	};

	class Lighting
	{
	private:
		RootSignature m_FillLightRootSig;
		ComputePSO m_FillLightGridCS_8;
		ComputePSO m_FillLightGridCS_16;
		ComputePSO m_FillLightGridCS_24;
		ComputePSO m_FillLightGridCS_32;

		LightData m_LightData[MaxLights];
		StructuredBuffer m_LightBuffer;
		ByteAddressBuffer m_LightGrid;

		ByteAddressBuffer m_LightGridBitMask;
		uint32_t m_FirstConeLight;
		uint32_t m_FirstConeShadowedLight;

		ColorBuffer m_LightShadowArray;
		ShadowBuffer m_LightShadowTempBuffer;
		Math::Matrix4 m_LightShadowMatrix[MaxLights];


	public:
		[[nodiscard]]
		inline StructuredBuffer& GetLightBuffer()
		{
			return m_LightBuffer;
		}

		[[nodiscard]]
		inline ByteAddressBuffer& GetLightGrid()
		{
			return m_LightGrid;
		}

		[[nodiscard]]
		inline ByteAddressBuffer& GetLightGridBitMask()
		{
			return m_LightGridBitMask;
		}

		[[nodiscard]]
		inline const uint32_t& GetFirstConeLight() const
		{
			return m_FirstConeLight;
		}
		
		[[nodiscard]]
		inline const uint32_t& GetFirstConeShadowedLight() const
		{
			return m_FirstConeShadowedLight;
		}

		[[nodiscard]]
		inline ColorBuffer& GetLightShadowArray()
		{
			return m_LightShadowArray;
		}

		[[nodiscard]]
		inline ShadowBuffer& GetLightShadowTempBuffer()
		{
			return m_LightShadowTempBuffer;
		}

		[[nodiscard]]
		inline Math::Matrix4& LightShadowMatrix(const int i)
		{
			return m_LightShadowMatrix[i];
		}

		void InitializeResources(void);
		void CreateRandomLights(const Math::Vector3 minBound, const Math::Vector3 maxBound);
		void FillLightGrid(GraphicsContext& gfxContext, const Math::Camera& camera);
		void Shutdown(void);
	};
}