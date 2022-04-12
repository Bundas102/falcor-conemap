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
#include "Parallax.h"

struct Vertex
{
    float3 pos;
    float2 tex;
    float3 norm;
};
namespace
{
    const Vertex kVertices[] =
    {
        {float3(-1, 0, -1), float2(0, 0), float3(0, 1, 0)},
        {float3(-1, 0, +1), float2(0, 1), float3(0, 1, 0)},
        {float3(+1, 0, -1), float2(1, 0), float3(0, 1, 0)},
        {float3(+1, 0, +1), float2(1, 1), float3(0, 1, 0)},
    };
    const char kParallaxFunDefine[] = "PARALLAX_FUN";
    const Gui::DropdownList kParallaxFunList = {
        {0, "0: Bump mapping"},
        {1, "1: Parallax mapping"},
        {2, "2: Linear search"},
        {3, "3: Cone step mapping"},
    };
    const char kRefinementFunDefine[] = "REFINE_FUN";
    const Gui::DropdownList kRefinementFunList = {
        {0, "0: No refinement"},
        {1, "1: Linear approx"},
        {2, "2: Binary search"},
    };
    const char kHeightFunDefine[] = "HEIGHT_FUN";
    const Gui::DropdownList kHeightFunList = {
        {0, "Sinc"},
        {1, "Spheres"},
    };
    const char kConeTypeDefine[] = "CONE_TYPE";
    const char kQuickGenAlgDefine[] = "QUICK_GEN_ALG";
    const char kDebugModeDefine[] = "DEBUG_MODE";
    const char kMaxAtTexelCenterDefine[] = "MAX_AT_TEXEL_CENTER";
    const Gui::DropdownList kDebugTexList = {
        {0, "Heightmap"},
        {1, "Conemap"},
        {2, "Min max"},
        {3, "Loaded albedo map"},
    };
    const Gui::DropdownList kDebugChannelList = {
        {0, "X RED"},
        {1, "Y GREEN"},
        {2, "Z BLUE"},
        {3, "W ALPHA"},
        {4, "0 ZERO"},
        {5, "H HALF"},
        {6, "1 ONE"},
    };
   
}

std::string filenameFromPath(const std::string& path)
{
    size_t pos = path.find_last_of("\\/");
    return (std::string::npos != pos) ? path.substr(pos + 1) : path;
}

std::string TextureDescription(const Texture* const tex) {
    if (!tex) return "  [ no texture ]";

    const auto& format = tex->getFormat();
    return " * Resolution: " + std::to_string(tex->getWidth()) + " x " + std::to_string(tex->getHeight()) +
        "\n * Format: " + to_string(format) +
        "\n * Source: " + tex->getName();
}

void Parallax::onGuiRender(Gui* pGui)
{
    Gui::Window w(pGui, "Parallax playground", { 420, 900 });
    gpFramework->renderGlobalUI(pGui);
    w.separator();

    auto cameraGroup = Gui::Group(w, "Camera Controls");
    if (cameraGroup.open())
    {
        mpCamera->renderUI(cameraGroup);
        cameraGroup.release();
    }
    w.separator();

    auto mainGroup = Gui::Group( w, "Parallax Controls", true );
    if ( mainGroup.open() )
    {
        guiTexureInfo(mainGroup);
        w.separator();
        guiRenderSettings(mainGroup);
        w.separator();
        guiProceduralGeneration(mainGroup);
        w.separator();
        guiConemapGeneration(mainGroup);
        w.separator();
        guiQuickconemapGeneration(mainGroup);
        w.separator();
        guiLoadImage(mainGroup);
        w.separator();
        guiDebugRender(mainGroup);
        w.separator();
        guiSaveImage(mainGroup);

        mainGroup.release();
    }
    w.separator();

    bool isVsyncOn = gpDevice->isVsyncEnabled();
    if(w.checkbox("VSync", isVsyncOn)){
        gpDevice->toggleVSync(isVsyncOn);
    }
    w.separator();
}

void Parallax::guiTexureInfo(Gui::Widgets& parent)
{
    auto w = Gui::Group(parent, "Loaded Textures", true);
    if (!w.open())
        return;
    // HEIGHTMAP
    if (w.button("Use##heightmap")) {
        mpParallaxVars["gTexture"] = mpHeightmapTex;
    }
    w.text("HEIGHTMAP", true);
    w.tooltip("Don't forget to set `Render Settings > PARALLAX_FUN`");
    {
        static const Texture* last = nullptr;
        static std::string sizeText = TextureDescription(last);
        if (last != mpHeightmapTex.get()) {
            last = mpHeightmapTex.get();
            sizeText = TextureDescription(last);
        }
        const auto& tx = mpParallaxVars["gTexture"].getTexture();
        if (tx && tx.get() == last) w.text("IN USE", true);
        w.text(sizeText);
    }
    // CONEMAP
    if (w.button("Use##conemap")) {
        mpParallaxVars["gTexture"] = mpConeTex;
    }
    w.text("CONEMAP", true);
    w.tooltip("Don't forget to set `Render Settings > PARALLAX_FUN`");
    {
        static const Texture* last = nullptr;
        static std::string sizeText = TextureDescription(last);
        if (last != mpConeTex.get()) {
            last = mpConeTex.get();
            sizeText = TextureDescription(last);
        }
        const auto& tx = mpParallaxVars["gTexture"].getTexture();
        if (tx && tx.get() == last) w.text("IN USE", true);
        w.text(sizeText);
    }
    // ALBEDO
    if (w.button("Use##use__albedo")) {
        mpParallaxProgram->addDefine("USE_ALBEDO_TEXTURE", "1");
    }
    if (w.button("Unuse##unuse__albedo", true)) {
        mpParallaxProgram->addDefine("USE_ALBEDO_TEXTURE", "0");
    }
    w.text("ALBEDO", true);
    w.tooltip("Use/unuse the loaded albedo texture");
    {
        static const Texture* last = nullptr;
        static std::string sizeText = TextureDescription(last);
        if (last != mpAlbedoTex.get()) {
            last = mpAlbedoTex.get();
            sizeText = TextureDescription(last);
        }
        w.text(sizeText);
    }
    w.release();
}
void Parallax::guiRenderSettings(Gui::Widgets& parent)
{
    auto w = Gui::Group(parent, "Render Settings");
    if (!w.open())
        return;
    w.direction("Light directon", mRenderSettings.lightDir);
    w.tooltip("Directional light, world space\nPress L to set it to the camera view direction", true);
    w.slider("Light intensity", mRenderSettings.lightIntensity, 0.5f, 8.0f);
    w.slider("Heightmap height", mRenderSettings.heightMapHeight, .0f, .5f);
    w.tooltip("World space", true);
    w.checkbox("Discard fragments", mRenderSettings.discardFragments);
    w.tooltip("Discard fragments when the ray misses the Heightmap, otherwise the pixel is colored red.", true);
    w.checkbox("Debug: display non-converged", mRenderSettings.displayNonConverged);
    w.tooltip("Color fragments to magenta if the primary search is not converged.", true);
    w.slider("Max step number", mRenderSettings.stepNum, 2U, 200U);
    w.tooltip("Step number for iterative primary searches", true);
    w.slider("Max refine step number", mRenderSettings.refineStepNum, 0U, 20U);
    w.tooltip("Step number for iterative refinement searches", true);
    w.slider("Relax multiplier", mRenderSettings.relax, 1.0f, 8.0f);
    w.tooltip("Primary search step relaxation", true);
    w.separator();

    if (w.dropdown(kParallaxFunDefine, kParallaxFunList, mRenderSettings.selectedParallaxFun)) {
        mRenderSettings.setParallaxFun();
    }
    if (w.dropdown(kRefinementFunDefine, kRefinementFunList, mRenderSettings.selectedRefinementFun)) {
        mRenderSettings.setRefinementFun();
    }
    w.separator();

    w.text("Transformation");
    w.slider("Scale", mRenderSettings.scale, 0.1f, 2.f);
    w.slider("Rot. angle", mRenderSettings.angle, 0.0f, 2 * 3.14159f);
    w.direction("Rot. axis", mRenderSettings.axis);
    w.slider("Translate", mRenderSettings.translate, -1.f, 1.f);
    w.release();
}
void Parallax::guiProceduralGeneration(Gui::Widgets& parent)
{
    auto w = Gui::Group(parent, "Procedural Heightmap Generation");
    if (!w.open())
        return;
    if (w.var("New Heightmap Size", mProcHMCompSettings.newHMapSize, 1, 1024)) {
        mProcHMCompSettings.newHMapSize = glm::max(mProcHMCompSettings.newHMapSize, uint2(1));
    }
    w.checkbox("16 bit texture##procedural", mProcHMCompSettings.newHmap16bit);
    w.tooltip("Checked: R16Unorm, unchecked: R8Unorm");
    w.dropdown(kHeightFunDefine, kHeightFunList, mProcHMCompSettings.heightFun);
    w.var("Int parameters", mProcHMCompSettings.HMapIntParams);
    w.tooltip("x: num of hemispheres in a row\ny: unused\nz: unused\nw: unused");
    w.var("Float parameters", mProcHMCompSettings.HMapFloatParams, std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max(), 0.01f, false, "%.2f");
    w.tooltip("x: relative radius (1: the spheres touch)\ny: unused\nz: unused\nw: unused");
    if (w.button("Generate procedural Heightmap") && mpHeightmapTex && mpHeightmapCompute)
    {
        mRunHeightmapCompute = true;
        mpParallaxProgram->addDefine("USE_ALBEDO_TEXTURE", "0");
        mpConeTex.reset();
        mpMinmaxTex.reset();
    }
    w.tooltip("Generates a Heightmap; deletes the Conemap");
    w.release();
}
void Parallax::guiConemapGeneration(Gui::Widgets& parent)
{
    auto w = Gui::Group(parent, "Conemap Generation from Heightmap");
    if (!w.open())
        return;
    w.checkbox("16 bit texture##conemap", mCMCompSettings.newHmap16bit);
    w.tooltip("Checked: R16Unorm, unchecked: R8Unorm");
    if (w.button("Generate Conemap from Heightmap") && mpHeightmapTex && mpConemapCompute)
    {
        mRunConemapCompute = true;
        mCMCompSettings.algorithm = "1";
        mCMCompSettings.name = "Standard Conemap";
    }
    w.tooltip("Single dispatch; might crash for larger textures.");
    w.var("Relaxed Cone Search Steps", mCMCompSettings.relaxedConeSearchSteps, 2u);
    if (w.button("Generate Relaxed Conemap from Heightmap") && mpHeightmapTex && mpConemapCompute)
    {
        mRunConemapCompute = true;
        mCMCompSettings.algorithm = "2";
        mCMCompSettings.name = "Relaxed Conemap";
    }
    w.tooltip("Single dispatch; might crash for larger textures.");
    w.release();
}
void Parallax::guiQuickconemapGeneration(Gui::Widgets& parent)
{
    auto w = Gui::Group(parent, "Quick Conemap Generation from Heightmap");
    if (!w.open())
        return;
    w.checkbox("16 bit texture##quickconemap", mQCMCompSettings.newHmap16bit);
    w.tooltip("Checked: R16Unorm, unchecked: R8Unorm");
    w.checkbox("Max at texel centers", mQCMCompSettings.maxAtTexelCenter);
    w.tooltip("Texel center heuristic", true);
    if (w.button("Generate Quick Conemap - naive") && mpQuickConemapCompute)
    {
        mRunMinmaxCompute = true;
        mRunQuickConemapCompute = true;
        mQCMCompSettings.algorithm = "1";
        mQCMCompSettings.name = "Naive Quick Conemap"_s + (mQCMCompSettings.maxAtTexelCenter ? " + Center Heuristic" : "");
    }
    if (w.button("Generate Quick Conemap - region growing 3x3") && mpQuickConemapCompute)
    {
        mRunMinmaxCompute = true;
        mRunQuickConemapCompute = true;
        mQCMCompSettings.algorithm = "2";
        mQCMCompSettings.name = "Quick Conemap"_s + (mQCMCompSettings.maxAtTexelCenter ? " + Center Heuristic" : "");
    }
    w.release();
}
void Parallax::guiLoadImage(Gui::Widgets& parent)
{
    auto w = Gui::Group(parent, "Load Image");
    if (!w.open())
        return;
    w.checkbox("Generate Mipmaps on Image Load", mGenerateMips);
    bool reloadHeightmap = false;
    if (w.button("Choose Height File"))
    {
        reloadHeightmap |= openFileDialog({}, mHeightmapName);
    }
    w.tooltip("Loads texture to Heightmap; deletes the Conemap");

    bool reloadAlbedo = false;
    if (w.button("Choose Albedo File"))
    {
        reloadAlbedo |= openFileDialog({}, mAlbedoName);
    }
    w.dummy("", float2(10, 10), true);
    if (w.button("Use##use_albedo", true)) {
        mpParallaxProgram->addDefine("USE_ALBEDO_TEXTURE", "1");
    }
    if (w.button("Unuse##unuse_albedo", true)) {
        mpParallaxProgram->addDefine("USE_ALBEDO_TEXTURE", "0");
    }
    w.tooltip("Use/unuse the loaded albedo texture");


    if (reloadHeightmap && !mHeightmapName.empty())
    {
        mHeightmapName = stripDataDirectories(mHeightmapName);
        LoadHeightmapTexture();
    }

    if (reloadAlbedo && !mAlbedoName.empty())
    {
        mAlbedoName = stripDataDirectories(mAlbedoName);
        LoadAlbedoTexture();
    }
    w.release();
}
void Parallax::guiDebugRender(Gui::Widgets& parent)
{
    auto w = Gui::Group(parent, "Debug Texture View");
    if (!w.open())
        return;
    bool changed = w.checkbox("Debug: draw texture", mDebugSettings.drawDebug);
    if (mDebugSettings.drawDebug)
    {
        changed |= w.button("Re-assign debug texture", true);
        static uint32_t selectedTexId = 0;
        changed |= w.dropdown("debug texture", kDebugTexList, selectedTexId);
        if (changed) {
            switch (selectedTexId)
            {
            case 0: mpDebugTex = mpHeightmapTex; break;
            case 1: mpDebugTex = mpConeTex; break;
            case 2: mpDebugTex = mpMinmaxTex; break;
            case 3: mpDebugTex = mpAlbedoTex; break;
            }
        }
        w.slider("Mip level", mDebugSettings.mipLevel, (uint32_t)0, mpDebugTex ? mpDebugTex->getMipCount() - 1 : 0);

        w.dropdown("RED", kDebugChannelList, mDebugSettings.debugChannels.x);
        w.dropdown("GREEN", kDebugChannelList, mDebugSettings.debugChannels.y);
        w.dropdown("BLUE", kDebugChannelList, mDebugSettings.debugChannels.z);

        if (w.button("XXX"))       mDebugSettings.debugChannels = { 0, 0, 0 };
        if (w.button("YYY", true)) mDebugSettings.debugChannels = { 1, 1, 1 };
        if (w.button("ZZZ", true)) mDebugSettings.debugChannels = { 2, 2, 2 };
        if (w.button("WWW", true)) mDebugSettings.debugChannels = { 3, 3, 3 };
        if (w.button("X00"))       mDebugSettings.debugChannels = { 0, 4, 4 };
        if (w.button("0Y0", true)) mDebugSettings.debugChannels = { 4, 1, 4 };
        if (w.button("00Z", true)) mDebugSettings.debugChannels = { 4, 4, 2 };
        if (w.button("XYZ"))       mDebugSettings.debugChannels = { 0, 1, 2 };
        if (w.button("YZ0", true)) mDebugSettings.debugChannels = { 1, 2, 4 };
        if (w.button("ZW0", true)) mDebugSettings.debugChannels = { 2, 3, 4 };
    }
    w.release();
}
void Parallax::guiSaveImage(Gui::Widgets& parent)
{
    auto w = Gui::Group(parent, "Save to File");
    if (!w.open())
        return;
    if (w.button("Save Heightmap to texture") && mpHeightmapTex) {
        if (saveFileDialog({ {"exr","EXR"} }, saveFilePath)) {
            doSaveTexture = 1;
        }
    }
    if (w.button("Save Conemap to texture") && mpConeTex) {
        if (saveFileDialog({ {"exr","EXR"} }, saveFilePath)) {
            doSaveTexture = 2;
        }
    }
    w.release();
}

void initSquare(Buffer::SharedPtr& pVB, Vao::SharedPtr& pVao)
{
    sizeof(Vertex);
    // Create VB
    const uint32_t vbSize = (uint32_t)(sizeof(Vertex) * arraysize(kVertices));
    pVB = Buffer::create(vbSize, Buffer::BindFlags::Vertex, Buffer::CpuAccess::Write, (void*)kVertices);
    assert(pVB);

    // Create VAO
    VertexLayout::SharedPtr pLayout = VertexLayout::create();
    VertexBufferLayout::SharedPtr pBufLayout = VertexBufferLayout::create();
    pBufLayout->addElement("POSITION", 0*4, ResourceFormat::RGB32Float, 1, 0);
    pBufLayout->addElement("TEXCOORD", 3*4, ResourceFormat::RG32Float, 1, 1);
    pBufLayout->addElement("NORMAL",   5*4, ResourceFormat::RGB32Float, 1, 2);
    pLayout->addBufferLayout(0, pBufLayout);

    Vao::BufferVec buffers{ pVB };
    pVao = Vao::create(Vao::Topology::TriangleStrip, pLayout, buffers);
    assert(pVao);
}

void Parallax::RenderSettings::setParallaxFun() {
    app.mpParallaxProgram->addDefine(kParallaxFunDefine, std::to_string(selectedParallaxFun));
}

void Parallax::RenderSettings::setRefinementFun() {
    app.mpParallaxProgram->addDefine(kRefinementFunDefine, std::to_string(selectedRefinementFun));
}

void Parallax::onLoad(RenderContext* pRenderContext)
{
    // parallax program
    {
        Program::Desc d;
        d.addShaderLibrary( "Samples/Parallax/Parallax.vs.slang" ).vsEntry( "main" );
        d.addShaderLibrary( "Samples/Parallax/Parallax.ps.slang" ).psEntry( "main" );

        mpParallaxProgram = GraphicsProgram::create( d, {
            {kParallaxFunDefine, std::to_string(mRenderSettings.selectedParallaxFun )},
            {kRefinementFunDefine, std::to_string(mRenderSettings.selectedRefinementFun )},
            } );
    }

    // debug texture program
    {
        Program::Desc d;
        d.addShaderLibrary( "Samples/Parallax/Parallax.vs.slang" ).vsEntry( "main" );
        d.addShaderLibrary( "Samples/Parallax/ParallaxDebug.ps.slang" ).psEntry( "mainDebug" );

        mpDebugProgram = GraphicsProgram::create( d );
    }

    mpParallaxRenderState = GraphicsState::create();
    mpParallaxRenderState->setProgram(mpParallaxProgram);
    mpParallaxVars = GraphicsVars::create(mpParallaxProgram.get());
    assert(mpParallaxRenderState);

    mpDebugRenderState = GraphicsState::create();
    mpDebugRenderState->setProgram( mpDebugProgram );
    mpDebugVars = GraphicsVars::create( mpDebugProgram.get() );
    assert( mpDebugRenderState );

    // compute
    mpHeightmapCompute = ComputeProgramWrapper::create();
    mpHeightmapCompute->createProgram("Samples/Parallax/ProceduralHeightmap.cs.slang");

    mpConemapCompute = ComputeProgramWrapper::create();
    mpConemapCompute->createProgram("Samples/Parallax/Conemap.cs.slang", "main", { {kConeTypeDefine, mCMCompSettings.algorithm} });

    mpTextureCopyCompute = ComputeProgramWrapper::create();
    mpTextureCopyCompute->createProgram( "Samples/Parallax/TextureCopy.cs.slang");

    mpMinmaxCopyCompute = ComputeProgramWrapper::create();
    mpMinmaxCopyCompute->createProgram( "Samples/Parallax/Minmax.cs.slang", "copyFromScalarToVector");

    mpMinmaxMipmapCompute = ComputeProgramWrapper::create();
    mpMinmaxMipmapCompute->createProgram( "Samples/Parallax/Minmax.cs.slang", "mipmapMinmax" );

    mpQuickConemapCompute = ComputeProgramWrapper::create();
    mpQuickConemapCompute->createProgram("Samples/Parallax/QuickConemap.cs.slang", "main", {
        {kQuickGenAlgDefine, mQCMCompSettings.algorithm},
        {kMaxAtTexelCenterDefine, mQCMCompSettings.maxAtTexelCenter ? "1" : "0"} }
    );

    // geometry
    initSquare(mpVertexBuffer, mpVao);

    mpParallaxRenderState->setVao(mpVao);
    mpDebugRenderState->setVao( mpVao );

    // camera
    mpCamera = Camera::create("Square Viewer Camera");
    mpCameraController = FirstPersonCameraController::create(mpCamera);
    mpCamera->setPosition({ 0.9f,0.1f,0.9f });
    mpCamera->setTarget({ 0.0f,-0.6f,0.0f });
    mpCamera->setNearPlane(0.001f);
    mpCamera->beginFrame();

    // texture
    mGenerateMips = true;
    LoadAlbedoTexture();

    mGenerateMips = false;
    LoadHeightmapTexture();


    {
        Sampler::Desc desc;
        desc.setFilterMode( Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear );
        mpSampler = Sampler::create( desc );
    }
    {
        Sampler::Desc desc;
        desc.setFilterMode( Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point );
        mpSamplerNearest = Sampler::create( desc );
    }

    mpParallaxVars[ "gSampler" ] = mpSampler;
    mpDebugVars["gSampler"] = mpSamplerNearest;

    // other
    gpDevice->toggleVSync(true);

    // generate a conemap in the first frame
    mRunMinmaxCompute = true;
    mRunQuickConemapCompute = true;
    mQCMCompSettings.algorithm = "2";
    mQCMCompSettings.maxAtTexelCenter = false;
    mQCMCompSettings.name = "Quick Conemap";
}

void Parallax::onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
    // save texture to file
    if (doSaveTexture == 1 || doSaveTexture == 2) {
        Texture::SharedPtr& pTexToCopy = doSaveTexture == 1 ? mpHeightmapTex : mpConeTex;
        doSaveTexture = 0;
        if (pTexToCopy) {
            Texture::SharedPtr pCopiedTexture = Texture::create2D(pTexToCopy->getWidth(), pTexToCopy->getHeight(), ResourceFormat::RGBA32Float, 1, 1, nullptr, ResourceBindFlags::RenderTarget);
            pRenderContext->blit(pTexToCopy->getSRV(0, 1), pCopiedTexture->getRTV());
            pCopiedTexture->captureToFile(0, 0, saveFilePath, Bitmap::FileFormat::ExrFile);
        }
    }
    // procedural heightmap generation
    if (mRunHeightmapCompute) {
        mRunHeightmapCompute = false;
        mpHeightmapTex = generateProceduralHeightmap(mProcHMCompSettings, pRenderContext);
        mpParallaxVars["gTexture"] = mpHeightmapTex;
        float2 res = float2(mpHeightmapTex->getWidth(), mpHeightmapTex->getHeight());
        mpParallaxVars["FScb"]["HMres"] = res;
        mpParallaxVars["FScb"]["HMres_r"] = 1.f / res;
    }
    // conemap or relaxed conemap generation
    if (mRunConemapCompute) {
        mRunConemapCompute = false;
        mpConeTex = generateConemap(mCMCompSettings, mpHeightmapTex, pRenderContext);
        mpParallaxVars["gTexture"] = mpConeTex;
    }
    // minmax mipmap for quick conemap generation
    if ( mRunMinmaxCompute )
    {
        mRunMinmaxCompute = false;
        mpMinmaxTex = generateMinmaxMipmap(mpHeightmapTex, pRenderContext);
    }
    // quick conemap generation
    if (mRunQuickConemapCompute) {
        mRunQuickConemapCompute = false;
        mpConeTex = generateQuickConemap(mQCMCompSettings, mpMinmaxTex, pRenderContext);
        mpParallaxVars["gTexture"] = mpConeTex;
    }

    // camera
    mpCameraController->update();
    mpCamera->beginFrame();

    // clear bg
    const float4 clearColor( 1.0f, 1.0f, 1.0f, 1 );
    pRenderContext->clearFbo( pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All );

    // main draw
    if ( !mDebugSettings.drawDebug )
    {
        mpParallaxRenderState->setFbo( pTargetFbo );
        mpParallaxVars[ "FScb" ][ "center" ] = float2( 0 );
        mpParallaxVars[ "FScb" ][ "scale" ] = 1.f;
        mpParallaxVars[ "FScb" ][ "lightDir" ] = mRenderSettings.lightDir;
        mpParallaxVars[ "FScb" ][ "camPos" ] = mpCamera->getPosition();
        mpParallaxVars[ "FScb" ][ "heightMapHeight" ] = mRenderSettings.heightMapHeight;
        mpParallaxVars[ "FScb" ][ "discardFragments" ] = mRenderSettings.discardFragments;
        mpParallaxVars[ "FScb" ][ "displayNonConverged" ] = mRenderSettings.displayNonConverged;
        mpParallaxVars[ "FScb" ][ "lightIntensity" ] = mRenderSettings.lightIntensity;
        mpParallaxVars[ "FScb" ][ "steps" ] = mRenderSettings.stepNum;
        mpParallaxVars[ "FScb" ][ "refine_steps" ] = mRenderSettings.refineStepNum;
        mpParallaxVars[ "FScb" ][ "relax" ] = mRenderSettings.relax;
        mpParallaxVars[ "FScb" ][ "oneOverSteps" ] = 1.0f / mRenderSettings.stepNum;
        mpParallaxVars[ "FScb" ][ "const_isolate" ] = 1;

        mpParallaxVars[ "VScb" ][ "viewProj" ] = mpCamera->getViewProjMatrix();
        float4x4 m = glm::translate( glm::mat4(), mRenderSettings.translate ); ;
        m = glm::rotate( m, mRenderSettings.angle, glm::normalize(mRenderSettings.axis ) );
        m = glm::scale( m, mRenderSettings.scale );
        mpParallaxVars[ "VScb" ][ "model" ] = m;
        mpParallaxVars[ "VScb" ][ "modelIT" ] = glm::inverse( glm::transpose( m ) );

        pRenderContext->draw( mpParallaxRenderState.get(), mpParallaxVars.get(), arraysize( kVertices ), 0 );
    }
    else
    {
        if (!mpDebugTex) return;

        mpDebugRenderState->setFbo( pTargetFbo );

        // debug stuff!
        mpDebugVars["VScb"]["viewProj"] = mpCamera->getViewProjMatrix();
        float4x4 m = glm::translate( glm::mat4(), mRenderSettings.translate ); ;
        m = glm::rotate( m, mRenderSettings.angle, glm::normalize(mRenderSettings.axis ) );
        m = glm::scale( m, mRenderSettings.scale );
        mpDebugVars[ "VScb" ][ "model" ] = m;
        mpDebugVars[ "VScb" ][ "modelIT" ] = glm::inverse( glm::transpose( m ) );
        mpDebugVars[ "FScb" ][ "currentLevel" ] = mDebugSettings.mipLevel;
        mpDebugVars[ "FScb" ][ "channels" ] = mDebugSettings.debugChannels;

        mpDebugVars[ "gTexture" ].setSrv( mpDebugTex->getSRV() );

        pRenderContext->draw( mpDebugRenderState.get(), mpDebugVars.get(), arraysize( kVertices ), 0 );
    }
}

void Parallax::onShutdown()
{
}

bool Parallax::onKeyEvent(const KeyboardEvent& keyEvent)
{
    if(mpCameraController->onKeyEvent(keyEvent)) return true;
    if (keyEvent.key == KeyboardEvent::Key::L) {
        mRenderSettings.lightDir = normalize(mpCamera->getTarget() - mpCamera->getPosition());
        return true;
    }
    const uint32_t keycode = (uint32_t)keyEvent.key;
    const uint32_t key0code = (uint32_t)KeyboardEvent::Key::Key0;
    const uint32_t key9code = (uint32_t)KeyboardEvent::Key::Key9;
    if (keycode >= key0code && keycode <= key9code) {
        uint32_t ind = keycode - key0code;
        if (keyEvent.mods.isShiftDown) {
            if (ind < kRefinementFunList.size()) {
                mRenderSettings.selectedRefinementFun = ind;
                mRenderSettings.setRefinementFun();
                return true;
            }
        }
        else
        {
            if (ind < kParallaxFunList.size()) {
                mRenderSettings.selectedParallaxFun = ind;
                mRenderSettings.setParallaxFun();
                return true;
            }
        }
    }

    return false;
}

bool Parallax::onMouseEvent(const MouseEvent& mouseEvent)
{
    mpCameraController->onMouseEvent(mouseEvent);
    return false;
}

void Parallax::onHotReload(HotReloadFlags reloaded)
{
}

void Parallax::onResizeSwapChain(uint32_t width, uint32_t height)
{
    mpCamera->setAspectRatio((float)width / (float)height);
}

void Parallax::LoadHeightmapTexture()
{
    mpHeightmapTex = Texture::createFromFile(mHeightmapName, mGenerateMips, false);
    mpHeightmapTex->setName(filenameFromPath(mHeightmapName));
    mpConeTex.reset();
    mpMinmaxTex.reset();
    mpParallaxVars["gTexture"] = mpHeightmapTex;
    float2 res = float2(mpHeightmapTex->getWidth(), mpHeightmapTex->getHeight());
    mpParallaxVars["FScb"]["HMres"] = res;
    mpParallaxVars["FScb"]["HMres_r"] = 1.f / res;
}

void Parallax::LoadAlbedoTexture()
{
    mpAlbedoTex = Texture::createFromFile( mAlbedoName, mGenerateMips, true );
    mpAlbedoTex->setName(filenameFromPath(mAlbedoName));
    mpParallaxProgram->addDefine( "USE_ALBEDO_TEXTURE", "1" );
    mpParallaxVars[ "gAlbedoTexture" ] = mpAlbedoTex;
}

Texture::SharedPtr Parallax::generateProceduralHeightmap(const ProceduralHeightmapComputeSettings& settings, RenderContext* pRenderContext) const
{
    if (!mpHeightmapCompute)
        return nullptr;
    auto& comp = *mpHeightmapCompute;

    auto w = settings.newHMapSize.x;
    auto h = settings.newHMapSize.y;
    ResourceFormat format = settings.newHmap16bit ? ResourceFormat::R16Unorm : ResourceFormat::R8Unorm;
    comp.getProgram()->addDefine(kHeightFunDefine, std::to_string(settings.heightFun));

    auto pTex = Texture::create2D(w, h, format, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    pTex->setName("Procedural Heightmap");

    comp["tex2D_uav"].setUav(pTex->getUAV(0));
    uint2 maxSize = { w, h };
    comp["CScb"]["maxSize"] = maxSize;
    comp["CScb"]["intParams"] = settings.HMapIntParams;
    comp["CScb"]["floatParams"] = settings.HMapFloatParams;
    comp.runProgram(pRenderContext, w, h, 1);

    return pTex;
}

Texture::SharedPtr Parallax::generateConemap(const ConemapComputeSettings& settings, const Texture::SharedPtr& pHeightmap, RenderContext* pRenderContext) const
{
    if (!mpConemapCompute || !pHeightmap)
        return nullptr;
    auto& comp = *mpConemapCompute;
    auto w = pHeightmap->getWidth();
    auto h = pHeightmap->getHeight();
    ResourceFormat format = settings.newHmap16bit ? ResourceFormat::RG16Unorm : ResourceFormat::RG8Unorm;
    auto pTex = Texture::create2D(w, h, format, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    pTex->setName(settings.name);
    comp.getProgram()->addDefine(kConeTypeDefine, settings.algorithm);

    comp["heightMap"].setSrv(pHeightmap->getSRV());
    comp["CScb"]["srcLevel"] = 0;
    comp["gSampler"] = mpSampler;
    comp["coneMap"].setUav(pTex->getUAV(0));
    uint2 maxSize = { w, h };
    comp["CScb"]["maxSize"] = maxSize;
    comp["CScb"]["oneOverMaxSize"] = 1.0f / float2(maxSize);
    comp["CScb"]["searchSteps"] = settings.relaxedConeSearchSteps;
    comp["CScb"]["oneOverSearchSteps"] = 1.0f / settings.relaxedConeSearchSteps;
    comp.runProgram(pRenderContext, w, h, 1);

    return pTex;
}

Texture::SharedPtr Parallax::generateMinmaxMipmap(const Texture::SharedPtr& pHeightmap, RenderContext* pRenderContext) const
{
    // Initialize the minmax LOD 0
    uint2 maxSize = { pHeightmap->getWidth(), pHeightmap->getHeight() };
    auto pTex = Texture::create2D(maxSize.x, maxSize.y, ResourceFormat::RG16Unorm, 1, uint32_t(-1), nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);

    auto& copyCS = *mpMinmaxCopyCompute;
    copyCS["CScb"]["maxSize"] = maxSize;
    copyCS["heightMap"].setSrv(pHeightmap->getSRV(0));
    copyCS["dstMinmaxMap"].setUav(pTex->getUAV(0));
    copyCS.runProgram(pRenderContext, maxSize.x, maxSize.y, 1);

    // And do the minmax pooled mipmapping for all remaining LODs
    auto& minmaxCS = *mpMinmaxMipmapCompute;
    for (uint currentLevel = 0; currentLevel < pTex->getMipCount() - 1; ++currentLevel)
    {
        maxSize /= 2;
        minmaxCS["CScb"]["maxSize"] = maxSize;
        minmaxCS["CScb"]["currentLevel"] = currentLevel;
        minmaxCS["srcMinmaxMap"].setSrv(pTex->getSRV());
        minmaxCS["dstMinmaxMap"].setUav(pTex->getUAV(currentLevel + 1));
        minmaxCS.runProgram(pRenderContext, maxSize.x, maxSize.y, 1);
    }
    return pTex;
}

Texture::SharedPtr Parallax::generateQuickConemap(const QuickConemapComputeSettings& settings, const Texture::SharedPtr& pMinmaxMipmap, RenderContext* pRenderContext) const
{
    if (!mpQuickConemapCompute || !pMinmaxMipmap)
        return nullptr;
    auto& comp = *mpQuickConemapCompute;
    auto w = pMinmaxMipmap->getWidth();
    auto h = pMinmaxMipmap->getHeight();
    ResourceFormat coneMapFormat = settings.newHmap16bit ? ResourceFormat::RG16Unorm : ResourceFormat::RG8Unorm;
    auto pTex = Texture::create2D(w, h, coneMapFormat, 1, 1, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);
    pTex->setName(settings.name);
    comp.getProgram()->addDefine(kQuickGenAlgDefine, settings.algorithm);
    comp.getProgram()->addDefine(kMaxAtTexelCenterDefine, settings.maxAtTexelCenter ? "1" : "0");
    comp["srcMinmaxMap"].setSrv(pMinmaxMipmap->getSRV(0));
    comp["dstConeMap"].setUav(pTex->getUAV(0));
    uint2 maxSize = { w, h };
    comp["CScb"]["maxLevel"] = pMinmaxMipmap->getMipCount() - 2u;
    comp["CScb"]["maxSize"] = maxSize;
    comp["CScb"]["deltaHalf"] = 0.5f / float2(maxSize);
    comp.runProgram(pRenderContext, w, h, 1);
    return pTex;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    Parallax::UniquePtr pRenderer = std::make_unique<Parallax>();
    SampleConfig config;
    config.windowDesc.title = "Parallax Techniques";
    config.windowDesc.resizableWindow = true;
    Sample::run(config, pRenderer);
    return 0;
}
