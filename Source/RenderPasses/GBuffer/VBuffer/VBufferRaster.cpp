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
#include "VBufferRaster.h"
#include "Scene/HitInfo.h"
#include "RenderGraph/RenderPassHelpers.h"
#include "RenderGraph/RenderPassStandardFlags.h"

const char* VBufferRaster::kDesc = "Rasterized V-buffer generation pass";

namespace
{
    const std::string kProgramFile = "RenderPasses/GBuffer/VBuffer/VBufferRaster.3d.slang";
    const std::string kShaderModel = "6_2";
    const RasterizerState::CullMode kDefaultCullMode = RasterizerState::CullMode::Back;

    const std::string kVBufferName = "vbuffer";
    const std::string kVBufferDesc = "V-buffer in packed format (indices + barycentrics)";

    const ChannelList kVBufferExtraChannels =
    {
        { "mvec",             "gMotionVectors",      "Motion vectors",                   true /* optional */, ResourceFormat::RG32Float   },
    };

    const std::string kDepthName = "depth";
}

RenderPassReflection VBufferRaster::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    reflector.addOutput(kDepthName, "Depth buffer").format(ResourceFormat::D32Float).bindFlags(Resource::BindFlags::DepthStencil);
    reflector.addOutput(kVBufferName, kVBufferDesc).bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess).format(mVBufferFormat);
    addRenderPassOutputs(reflector, kVBufferExtraChannels, Resource::BindFlags::UnorderedAccess);

    return reflector;
}

VBufferRaster::SharedPtr VBufferRaster::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    return SharedPtr(new VBufferRaster(dict));
}

VBufferRaster::VBufferRaster(const Dictionary& dict)
    : GBufferBase()
{
    // Check for required features.
    if (!gpDevice->isFeatureSupported(Device::SupportedFeatures::Barycentrics))
    {
        throw std::exception("Pixel shader barycentrics are not supported by the current device");
    }
    if (!gpDevice->isFeatureSupported(Device::SupportedFeatures::RasterizerOrderedViews))
    {
        throw std::exception("Rasterizer ordered views (ROVs) are not supported by the current device");
    }

    parseDictionary(dict);

    // Create raster program
    Program::Desc desc;
    desc.addShaderLibrary(kProgramFile).vsEntry("vsMain").psEntry("psMain");
    desc.setShaderModel(kShaderModel);
    mRaster.pProgram = GraphicsProgram::create(desc);

    // Initialize graphics state
    mRaster.pState = GraphicsState::create();
    mRaster.pState->setProgram(mRaster.pProgram);

    // Set depth function
    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthFunc(DepthStencilState::Func::LessEqual).setDepthWriteMask(true);
    mRaster.pState->setDepthStencilState(DepthStencilState::create(dsDesc));

    mpFbo = Fbo::create();
}

void VBufferRaster::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    GBufferBase::setScene(pRenderContext, pScene);

    mRaster.pVars = nullptr;

    if (pScene)
    {
        if (pScene->getVao()->getPrimitiveTopology() != Vao::Topology::TriangleList)
        {
            throw std::exception("VBufferRaster only works with triangle list geometry due to usage of SV_Barycentrics.");
        }

        mRaster.pProgram->addDefines(pScene->getSceneDefines());
    }
}

void VBufferRaster::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    GBufferBase::execute(pRenderContext, renderData);

    // Clear depth and output buffer.
    auto pDepth = renderData[kDepthName]->asTexture();
    auto pOutput = renderData[kVBufferName]->asTexture();
    pRenderContext->clearUAV(pOutput->getUAV().get(), uint4(0)); // Clear as UAV for integer clear value
    pRenderContext->clearDsv(pDepth->getDSV().get(), 1.f, 0);

    // Clear extra output buffers.
    auto clear = [&](const ChannelDesc& channel)
    {
        auto pTex = renderData[channel.name]->asTexture();
        if (pTex) pRenderContext->clearUAV(pTex->getUAV().get(), float4(0.f));
    };
    for (const auto& channel : kVBufferExtraChannels) clear(channel);

    // If there is no scene, we're done.
    if (mpScene == nullptr)
    {
        return;
    }

    // Set program defines.
    mRaster.pProgram->addDefine("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");

    // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
    // TODO: This should be moved to a more general mechanism using Slang.
    mRaster.pProgram->addDefines(getValidResourceDefines(kVBufferExtraChannels, renderData));

    // Create program vars.
    if (!mRaster.pVars)
    {
        mRaster.pVars = GraphicsVars::create(mRaster.pProgram.get());
    }

    mpFbo->attachColorTarget(pOutput, 0);
    mpFbo->attachDepthStencilTarget(pDepth);
    mRaster.pState->setFbo(mpFbo); // Sets the viewport
    mRaster.pVars["PerFrameCB"]["gFrameDim"] = mFrameDim;

    // Bind extra channels as UAV buffers.
    for (const auto& channel : kVBufferExtraChannels)
    {
        Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
        mRaster.pVars[channel.texname] = pTex;
    }

    // Rasterize the scene.
    RasterizerState::CullMode cullMode = mForceCullMode ? mCullMode : kDefaultCullMode;
    mpScene->rasterize(pRenderContext, mRaster.pState.get(), mRaster.pVars.get(), cullMode);
}
