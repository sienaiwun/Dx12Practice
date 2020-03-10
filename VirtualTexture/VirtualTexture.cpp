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
// Author(s):  Alex Nankervis
//             James Stanard
//

#include "GameCore.h"
#include "GraphicsCore.h"
#include "hlsl.hpp"
#include "CameraController.h"
#include "BufferManager.h"
#include "Camera.h"
#include "World.hpp"
#include "GpuBuffer.h"
#include "CommandContext.h"
#include "SamplerManager.h"
#include "TemporalEffects.h"
#include "MotionBlur.h"
#include "DepthOfField.h"
#include "PostEffects.h"
#include "SSAO.h"
#include "FXAA.h"
#include "SkyPass.hpp"
#include "SystemTime.h"
#include "TextRenderer.h"
#include "ShadowCamera.h"
#include "ParticleEffectManager.h"
#include "GameInput.h"
#include "TiledTexture.h"

// To enable wave intrinsics, uncomment this macro and #define DXIL in Core/GraphcisCore.cpp.
// Run CompileSM6Test.bat to compile the relevant shaders with DXC.
//#define _WAVE_OP

#include "CompiledShaders/DepthViewerVS.h"
#include "CompiledShaders/DepthViewerPS.h"
#include "CompiledShaders/ModelViewerVS.h"
#include "CompiledShaders/ModelViewerPS.h"
#ifdef _WAVE_OP
#include "CompiledShaders/DepthViewerVS_SM6.h"
#include "CompiledShaders/ModelViewerVS_SM6.h"
#include "CompiledShaders/ModelViewerPS_SM6.h"
#endif

#define TEXTURETILESIZE 128
#define TILENUMBER1D 32

using namespace GameCore;
using namespace Math;
using namespace Graphics;
#pragma warning(disable:4996)


enum RootParams
{
	CameraParam,
	MaterialsSRVs,
    VisibilitiUAVs,
	PerModelConstant,
	WorldParam,
	NumPassRootParams,
};

struct MipInfo
{
    UINT heapIndex;
    bool packedMip;
    bool mapped;
    D3D12_TILED_RESOURCE_COORDINATE startCoordinate;
    D3D12_TILE_REGION_SIZE regionSize;
};


class VirtureTexture : public GameCore::IGameApp
{
public:

    VirtureTexture(void);
    virtual void Startup( void ) override;
    virtual void Cleanup( void ) override;

    virtual void Update( float deltaT ) override;
    virtual void RenderScene( void ) override;

 

private:

    void UpdateGpuWorld(GraphicsContext& gfxContext);

    enum eObjectFilter { kOpaque = 0x1, kCutout = 0x2, kTransparent = 0x4, kAll = 0xF, kNone = 0x0 };
    void RenderObjects( GraphicsContext& Context, const Matrix4& ViewProjMat, eObjectFilter Filter = kAll);
  

    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

    RootSignature m_RootSig;
    GraphicsPSO m_DepthPSO;
    GraphicsPSO m_ForwardPlusPSO;
#ifdef _WAVE_OP
    GraphicsPSO m_DepthWaveOpsPSO;
    GraphicsPSO m_ModelWaveOpsPSO;
#endif
    D3D12_CPU_DESCRIPTOR_HANDLE m_DefaultSampler;
    D3D12_CPU_DESCRIPTOR_HANDLE m_ShadowSampler;
    D3D12_CPU_DESCRIPTOR_HANDLE m_BiasedDefaultSampler;

    SceneView::World m_world;

    Vector3 m_SunDirection;
    ShadowCamera m_SunShadow;

    TiledTexture m_tiledTexture;
};

CREATE_APPLICATION(VirtureTexture)
enum LightingType { kForward_plus ,kNumLightingModels};
const char* LightingModelLabels[kNumLightingModels] = { "Forward+"};
EnumVar g_LightingModel("LightingModel", kForward_plus, kNumLightingModels, LightingModelLabels);


ExpVar m_SunLightIntensity("Application/Lighting/Sun Light Intensity", 4.0f, 0.0f, 16.0f, 0.1f);
ExpVar m_AmbientIntensity("Application/Lighting/Ambient Intensity", 0.1f, -16.0f, 16.0f, 0.1f);
NumVar m_SunOrientation("Application/Lighting/Sun Orientation", -0.5f, -100.0f, 100.0f, 0.1f );
NumVar m_SunInclination("Application/Lighting/Sun Inclination", 0.75f, 0.0f, 1.0f, 0.01f );
NumVar ShadowDimX("Application/Lighting/Shadow Dim X", 5000, 1000, 10000, 100 );
NumVar ShadowDimY("Application/Lighting/Shadow Dim Y", 3000, 1000, 10000, 100 );
NumVar ShadowDimZ("Application/Lighting/Shadow Dim Z", 3000, 1000, 10000, 100 );

#ifdef _WAVE_OP
BoolVar EnableWaveOps("Application/Forward+/Enable Wave Ops", true);
#endif

VirtureTexture::VirtureTexture(void) 
{
    
}

void VirtureTexture::Startup( void )
{
    SamplerDesc DefaultSamplerDesc;
    DefaultSamplerDesc.MaxAnisotropy = 0;
    DefaultSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;

    m_RootSig.Reset(RootParams::NumPassRootParams, 2);
    m_RootSig.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSig.InitStaticSampler(1, SamplerShadowDesc, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSig[RootParams::CameraParam].InitAsConstantBuffer(SLOT_CBUFFER_CAMERA, D3D12_SHADER_VISIBILITY_VERTEX);
    m_RootSig[RootParams::MaterialsSRVs].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 6, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSig[RootParams::VisibilitiUAVs].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSig[RootParams::PerModelConstant].InitAsConstants(1, 4, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootSig[RootParams::WorldParam].InitAsConstantBuffer(SLOT_CBUFFER_WORLD, D3D12_SHADER_VISIBILITY_VERTEX);
    m_RootSig.Finalize(L"ModelViewer", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    DXGI_FORMAT ColorFormat = g_SceneColorBuffer.GetFormat();
    DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();

    D3D12_INPUT_ELEMENT_DESC vertElem[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    gsl::span<const D3D12_INPUT_ELEMENT_DESC> input_element_decs = gsl::make_span(vertElem);

    // Depth-only (2x rate)
    m_DepthPSO.SetRootSignature(m_RootSig);
    m_DepthPSO.SetRasterizerState(RasterizerDefault);
    m_DepthPSO.SetBlendState(BlendNoColorWrite);
    m_DepthPSO.SetDepthStencilState(DepthStateReadWrite);
    m_DepthPSO.SetInputLayout(input_element_decs.size(), input_element_decs.data());
    m_DepthPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_DepthPSO.SetRenderTargetFormats(0, nullptr, DepthFormat);
    m_DepthPSO.SetVertexShader(g_pDepthViewerVS, sizeof(g_pDepthViewerVS));
    m_DepthPSO.Finalize();

    // Full color pass
    m_ForwardPlusPSO = m_DepthPSO;
    m_ForwardPlusPSO.SetBlendState(BlendDisable);
    m_ForwardPlusPSO.SetDepthStencilState(DepthStateTestEqual);
    m_ForwardPlusPSO.SetRenderTargetFormats(1, &ColorFormat, DepthFormat);
	m_ForwardPlusPSO.SetVertexShader(SHADER_ARGS(g_pModelViewerVS));
	m_ForwardPlusPSO.SetPixelShader(SHADER_ARGS(g_pModelViewerPS));
    m_ForwardPlusPSO.Finalize();




#ifdef _WAVE_OP
    m_DepthWaveOpsPSO = m_DepthPSO;
    m_DepthWaveOpsPSO.SetVertexShader( g_pDepthViewerVS_SM6, sizeof(g_pDepthViewerVS_SM6) );
    m_DepthWaveOpsPSO.Finalize();

    m_ModelWaveOpsPSO = m_ForwardPlusPSO;
    m_ModelWaveOpsPSO.SetVertexShader( g_pModelViewerVS_SM6, sizeof(g_pModelViewerVS_SM6) );
    m_ModelWaveOpsPSO.SetPixelShader( g_pModelViewerPS_SM6, sizeof(g_pModelViewerPS_SM6) );
    m_ModelWaveOpsPSO.Finalize();
#endif

	
  



    TextureManager::Initialize(L"Textures/");
	m_world.Create();

    // The caller of this function can override which materials are considered cutouts
    

    MotionBlur::Enable = false;
    TemporalEffects::EnableTAA = false;
    FXAA::Enable = false;
    PostEffects::EnableHDR = false;
    PostEffects::EnableAdaptation = false;
    SSAO::Enable = false;

    SkyPass::Initialize();
    m_tiledTexture.Create(L"", TEXTURETILESIZE*TILENUMBER1D, TEXTURETILESIZE*TILENUMBER1D, DXGI_FORMAT_R8G8B8A8_UNORM);
    //m_tiledTexture.Create(L"StreamingAssets", TEXTURETILESIZE*TILENUMBER1D, TEXTURETILESIZE*TILENUMBER1D, DXGI_FORMAT_R8G8B8A8_UNORM);
}

void VirtureTexture::Cleanup( void )
{
    m_world.Clear();
    m_tiledTexture.Destroy();
}

namespace Graphics
{
    extern EnumVar DebugZoom;
}

void VirtureTexture::Update( float deltaT )
{
    ScopedTimer _prof(L"Update State");

    if (GameInput::IsFirstPressed(GameInput::kLShoulder))
        DebugZoom.Decrement();
    else if (GameInput::IsFirstPressed(GameInput::kRShoulder))
        DebugZoom.Increment();
    else if (GameInput::IsFirstPressed(GameInput::kKey_up))
    {
        m_tiledTexture.LevelUp();
    }
    else if (GameInput::IsFirstPressed(GameInput::kKey_down))
    {
        m_tiledTexture.LevelDown();
    }
	m_world.Update(deltaT);

    float costheta = cosf(m_SunOrientation);
    float sintheta = sinf(m_SunOrientation);
    float cosphi = cosf(m_SunInclination * 3.14159f * 0.5f);
    float sinphi = sinf(m_SunInclination * 3.14159f * 0.5f);
    m_SunDirection = Normalize(Vector3( costheta * cosphi, sinphi, sintheta * cosphi ));

    // We use viewport offsets to jitter sample positions from frame to frame (for TAA.)
    // D3D has a design quirk with fractional offsets such that the implicit scissor
    // region of a viewport is floor(TopLeftXY) and floor(TopLeftXY + WidthHeight), so
    // having a negative fractional top left, e.g. (-0.25, -0.25) would also shift the
    // BottomRight corner up by a whole integer.  One solution is to pad your viewport
    // dimensions with an extra pixel.  My solution is to only use positive fractional offsets,
    // but that means that the average sample position is +0.5, which I use when I disable
    // temporal AA.
    TemporalEffects::GetJitterOffset(m_MainViewport.TopLeftX, m_MainViewport.TopLeftY);

    m_MainViewport.Width = (float)g_SceneColorBuffer.GetWidth();
    m_MainViewport.Height = (float)g_SceneColorBuffer.GetHeight();
    m_MainViewport.MinDepth = 0.0f;
    m_MainViewport.MaxDepth = 1.0f;

    m_MainScissor.left = 0;
    m_MainScissor.top = 0;
    m_MainScissor.right = (LONG)g_SceneColorBuffer.GetWidth();
    m_MainScissor.bottom = (LONG)g_SceneColorBuffer.GetHeight();
}

__declspec(align(16))struct CameraBufferConstant
{
    Matrix4 modelToProjection;
} cameraConstant;

__declspec(align(16))struct WorldBufferConstants
{
    Matrix4 projection_to_camera;
    Matrix4 camera_to_world;
    Matrix4 projection_to_world;
    Matrix4 model_to_shadow;
    XMFLOAT4 invViewport;
    XMFLOAT3 cameraPos;
    float scene_length;
} worldConstant;


void VirtureTexture::UpdateGpuWorld(GraphicsContext& gfxContext)
{
    const Camera& cam = m_world.GetMainCamera();
    const Matrix4 invProjMat = Invert(cam.GetProjMatrix());
    const Matrix4 invViewMat = Invert(cam.GetViewMatrix());

    worldConstant.projection_to_camera = std::move(invProjMat);
    worldConstant.camera_to_world = std::move(invViewMat);
    worldConstant.projection_to_world = Invert(cam.GetViewProjMatrix());
    worldConstant.model_to_shadow = m_SunShadow.GetShadowMatrix();
    XMStoreFloat3(&worldConstant.cameraPos, cam.GetPosition());
    XMStoreFloat4(&worldConstant.invViewport, { 1.0f / m_MainViewport.Width,1.0f / m_MainViewport.Height,0.0f,0.0f });
    worldConstant.scene_length = m_world.GetBoundingBox().Length();
    gfxContext.SetDynamicConstantBufferView(RootParams::WorldParam, sizeof(worldConstant), &worldConstant);

}

void VirtureTexture::RenderObjects(GraphicsContext& gfxContext, const Matrix4& viewProjMat, eObjectFilter Filter)
{

	cameraConstant.modelToProjection = viewProjMat;
    gfxContext.TransitionResource(m_tiledTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	gfxContext.SetDynamicConstantBufferView(RootParams::CameraParam, sizeof(cameraConstant), &cameraConstant);

	uint32_t materialIdx = 0xFFFFFFFFul;
	m_world.ForEach([&](Model &model)
	{
		uint32_t VertexStride = model.m_VertexStride;
		gfxContext.SetRootSignature(m_RootSig);
		gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		gfxContext.SetIndexBuffer(model.m_IndexBuffer.IndexBufferView());
		gfxContext.SetVertexBuffer(0, model.m_VertexBuffer.VertexBufferView());

		for (uint32_t meshIndex = 0; meshIndex < model.m_Header.meshCount; meshIndex++)
		{
			const Model::Mesh& mesh = model.m_pMesh[meshIndex];

			uint32_t indexCount = mesh.indexCount;
			uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint16_t);
			uint32_t baseVertex = mesh.vertexDataByteOffset / VertexStride;

			if (mesh.materialIndex != materialIdx)
			{
				if (model.MaterialIsCutout(mesh.materialIndex) && !(Filter & kCutout) ||
					!model.MaterialIsCutout(mesh.materialIndex) && !(Filter & kOpaque))
					continue;

				materialIdx = mesh.materialIndex;
				gfxContext.SetDynamicDescriptors(RootParams::MaterialsSRVs, 0, 6, model.GetSRVs(materialIdx));
                gfxContext.SetDynamicDescriptor(RootParams::MaterialsSRVs, 2, m_tiledTexture.GetSRV());
                gfxContext.SetDynamicDescriptor(RootParams::VisibilitiUAVs, 0, m_tiledTexture.GetVisibilityUAV());
			}

			gfxContext.SetConstants(RootParams::PerModelConstant, m_tiledTexture.GetMipsLevel(), m_tiledTexture.GetActiveMip(),m_tiledTexture.GetVirtualWidth(),m_tiledTexture.GetTiledWidth());

			gfxContext.DrawIndexed(indexCount, startIndex, baseVertex);
		}
	});


}


void VirtureTexture::RenderScene( void )
{
   
	const Matrix4& camViewProjMat = m_world.GetMainCamera().GetViewProjMatrix();
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");
    // Set the default state for command lists
    auto pfnSetupGraphicsState = [&](void)
    {
        gfxContext.SetRootSignature(m_RootSig);
        gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    };

    pfnSetupGraphicsState();

    UpdateGpuWorld(gfxContext);


    {
        ScopedTimer _prof(L"Z PrePass", gfxContext);

     	
        {
            ScopedTimer _prof1(L"Opaque", gfxContext);
            gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
            gfxContext.ClearDepth(g_SceneDepthBuffer);

#ifdef _WAVE_OP
            gfxContext.SetPipelineState(EnableWaveOps ? m_DepthWaveOpsPSO : m_DepthPSO );
#else
            gfxContext.SetPipelineState(m_DepthPSO);
#endif
            gfxContext.SetDepthStencilTarget(g_SceneDepthBuffer.GetDSV());
            gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);
            RenderObjects(gfxContext, camViewProjMat, kOpaque );
        }

    }


    if (!SSAO::DebugDraw)
    {
        ScopedTimer _prof(L"Main Render", gfxContext);

    

        pfnSetupGraphicsState();

       

        if (SSAO::AsyncCompute)
        {
            gfxContext.Flush();
            pfnSetupGraphicsState();

            // Make the 3D queue wait for the Compute queue to finish SSAO
            g_CommandManager.GetGraphicsQueue().StallForProducer(g_CommandManager.GetComputeQueue());
        }

        {
            ScopedTimer _prof4(L"Render Color", gfxContext);

            gfxContext.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            
			{
				gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
				gfxContext.ClearColor(g_SceneColorBuffer);
#ifdef _WAVE_OP
				gfxContext.SetPipelineState(EnableWaveOps ? m_ModelWaveOpsPSO : m_ForwardPlusPSO);
#else
			    gfxContext.SetPipelineState(m_ForwardPlusPSO);
				
#endif
				gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
				gfxContext.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV_DepthReadOnly());
				gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

				RenderObjects(gfxContext, camViewProjMat, kOpaque);

				
				
			}

			
        }

    }

    m_tiledTexture.Update(gfxContext);
    // Some systems generate a per-pixel velocity buffer to better track dynamic and skinned meshes.  Everything
    // is static in our scene, so we generate velocity from camera motion and the depth buffer.  A velocity buffer
    // is necessary for all temporal effects (and motion blur).
    MotionBlur::GenerateCameraVelocityBuffer(gfxContext, m_world.GetMainCamera(), true);

    TemporalEffects::ResolveImage(gfxContext);

   
    // Until I work out how to couple these two, it's "either-or".
    if (DepthOfField::Enable)
        DepthOfField::Render(gfxContext, m_world.GetMainCamera().GetNearClip(), m_world.GetMainCamera().GetFarClip());
    else
        MotionBlur::RenderObjectBlur(gfxContext, g_VelocityBuffer);

    gfxContext.Finish();
}

