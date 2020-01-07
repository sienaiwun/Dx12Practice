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
class IndirectDraw : public GameCore::IGameApp
{
public:
    IndirectDraw(void) {}

    virtual void Startup(void) override ;
    virtual void Cleanup(void) override ;

    virtual void Update(float deltaT) override ;
    virtual void RenderScene(void) override ;
};



void RandomColor(float v[4])
{
    do
    {
        v[0] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
        v[1] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
        v[2] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
    } while (v[0] < 0 && v[1] < 0 && v[2] < 0);    // prevent black colors
    v[3] = 1.0f;
}
