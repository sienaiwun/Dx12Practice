#pragma once


#include "pch.h"


#include "GameCore.h"
#include "GraphicsCore.h"
#include "hlsl.hpp"
#include "CameraController.h"
#include "BufferManager.h"
#include "Camera.h"
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


using namespace GameCore;
using namespace Math;
using namespace Graphics;



class Compute : public GameCore::IGameApp
{
public:
    Compute(void) {}

    virtual void Startup(void) override;
    virtual void Cleanup(void) override;

    virtual void Update(float deltaT) override;
    virtual void RenderScene(void) override;
};


