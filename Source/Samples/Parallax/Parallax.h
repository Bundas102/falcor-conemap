/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#pragma once
#include "Falcor.h"
#include "ComputeProgramWrapper.h"

using namespace Falcor;

class Parallax : public IRenderer
{
public:
    Parallax() : mRenderSettings(*this) {}

    void onLoad(RenderContext* pRenderContext) override;
    void onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo) override;
    void onShutdown() override;
    void onResizeSwapChain(uint32_t width, uint32_t height) override;
    bool onKeyEvent(const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(const MouseEvent& mouseEvent) override;
    void onHotReload(HotReloadFlags reloaded) override;
    void onGuiRender(Gui* pGui) override;

private:

    void guiTexureInfo(Gui::Widgets& w);
    void guiRenderSettings(Gui::Widgets& w);
    void guiProceduralGeneration(Gui::Widgets& w);
    void guiConemapGeneration(Gui::Widgets& w);
    void guiQuickconemapGeneration(Gui::Widgets& w);
    void guiLoadImage(Gui::Widgets& w);
    void guiDebugRender(Gui::Widgets& w);
    void guiSaveImage(Gui::Widgets& w);

    // program
    GraphicsProgram::SharedPtr mpDebugProgram = nullptr;
    GraphicsVars::SharedPtr    mpDebugVars = nullptr;
    GraphicsState::SharedPtr   mpDebugRenderState = nullptr;

    GraphicsProgram::SharedPtr mpParallaxProgram = nullptr;
    GraphicsVars::SharedPtr    mpParallaxVars = nullptr;
    GraphicsState::SharedPtr   mpParallaxRenderState = nullptr;
    // compute
    ComputeProgramWrapper::SharedPtr mpHeightmapCompute = nullptr;
    bool mRunHeightmapCompute = false;
    struct ProceduralHeightmapComputeSettings {
        uint2 newHMapSize = { 512, 512 };
        bool newHmap16bit = true;
        uint32_t heightFun = 0;
        int4 HMapIntParams = { 5, 0, 0, 0 };
        float4 HMapFloatParams = { 1.0f, 0.0f, 0.0f, 0.0f };
    } mProcHMCompSettings;
    
    ComputeProgramWrapper::SharedPtr mpConemapCompute = nullptr;
    bool mRunConemapCompute = false;
    struct ConemapComputeSettings {
        bool newHmap16bit = true;
        uint relaxedConeSearchSteps = 64;
        std::string algorithm = "1";
        std::string name = "";
    } mCMCompSettings;

    ComputeProgramWrapper::SharedPtr mpTextureCopyCompute = nullptr;

    ComputeProgramWrapper::SharedPtr mpMinmaxCopyCompute = nullptr;
    ComputeProgramWrapper::SharedPtr mpMinmaxMipmapCompute = nullptr;
    bool mRunMinmaxCompute = false;

    ComputeProgramWrapper::SharedPtr mpQuickConemapCompute = nullptr;
    bool mRunQuickConemapCompute = false;
    struct QuickConemapComputeSettings {
        bool newHmap16bit = true;
        bool maxAtTexelCenter = false;
        std::string algorithm = "2";
        std::string name = "";
    } mQCMCompSettings;
    // geometry
    Buffer::SharedPtr mpVertexBuffer = nullptr;
    Vao::SharedPtr mpVao = nullptr;
    // camera
    Camera::SharedPtr mpCamera = nullptr;
    FirstPersonCameraController::SharedPtr mpCameraController = nullptr;
    // texture
    Texture::SharedPtr mpHeightmapTex = nullptr;
    std::string mHeightmapName = "Dirt_Cracked/Dirt_Cracked_height 512.png";
    void LoadHeightmapTexture(); // load texture from file(mHeightmapName) and set variables
    Texture::SharedPtr mpAlbedoTex = nullptr;
    std::string mAlbedoName = "Dirt_Cracked/Dirt_Cracked_diffuse 4k.png";
    void LoadAlbedoTexture(); // load texture from file(mAlbedoName) and set variables

    bool mGenerateMips = false;
    Sampler::SharedPtr mpSampler = nullptr;
    Sampler::SharedPtr mpSamplerNearest = nullptr;
    Texture::SharedPtr mpConeTex = nullptr;
    Texture::SharedPtr mpMinmaxTex = nullptr;

    std::string saveFilePath = "";
    int doSaveTexture = 0;
    Texture::SharedPtr mpDebugTex = nullptr; // the texture that is drawn in debug mode
    // texture debug
    struct DebugSettings {
        bool drawDebug = false;
        uint32_t mipLevel = 0;
        uint3 debugChannels = { 0, 1, 2 };
    } mDebugSettings;

    // render settings
    friend struct RenderSettings;
    struct RenderSettings {
        RenderSettings(Parallax& app) : app(app) {}
        Parallax& app;
        float3 lightDir{ glm::normalize(glm::vec3(.198f, -.462f, -.865f)) };
        float heightMapHeight = 0.2f;
        bool discardFragments = true;
        bool displayNonConverged = false;
        float lightIntensity = 1.0f;
        uint stepNum = 32;
        uint refineStepNum = 5;
        float3 scale{ 1, 1, 1 };
        float angle = 0.0f;
        float3 axis{ 0, 0, 1 };
        float3 translate{ 0, 0, 0 };
        float relax = 1.0f;
        uint32_t selectedParallaxFun = 3; // see kParallaxFunList
        void setParallaxFun();
        uint32_t selectedRefinementFun = 1; // see kRefineFunList
        void setRefinementFun();
    } mRenderSettings;


    // compute calls
    Texture::SharedPtr generateProceduralHeightmap(const ProceduralHeightmapComputeSettings& settings, RenderContext* pRenderContext) const;
    Texture::SharedPtr generateConemap(const ConemapComputeSettings& settings, const Texture::SharedPtr& pHeightmap, RenderContext* pRenderContext) const;
    Texture::SharedPtr generateMinmaxMipmap(const Texture::SharedPtr& pHeightmap, RenderContext* pRenderContext) const;
    Texture::SharedPtr generateQuickConemap(const QuickConemapComputeSettings& settings, const Texture::SharedPtr& pMinmaxMipmap, RenderContext* pRenderContext) const;
};
