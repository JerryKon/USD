//
// Copyright 2021 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//

#include "pxr/imaging/garch/glApi.h"

#include "pxr/usdImaging/usdImagingGL/unitTestGLDrawing.h"

#include "pxr/base/arch/systemInfo.h"
#include "pxr/base/gf/frustum.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/range3d.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/tf/getenv.h"

#include "pxr/imaging/hd/mesh.h"
#include "pxr/imaging/hd/renderIndex.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/base/work/threadLimits.h"

#include "pxr/usdImaging/usdImaging/delegate.h"
#include "pxr/usdImaging/usdImaging/tokens.h"
#include "pxr/usdImaging/usdImaging/unitTestHelper.h"

#include "pxr/usdImaging/usdImagingGL/engine.h"

#include <iostream>
#include <sstream>

PXR_NAMESPACE_USING_DIRECTIVE

class My_TestGLDrawing : public UsdImagingGL_UnitTestGLDrawing {
public:
    My_TestGLDrawing() {
        _mousePos[0] = _mousePos[1] = 0;
        _mouseButton[0] = _mouseButton[1] = _mouseButton[2] = false;
        _rotate[0] = _rotate[1] = 0;
        _translate[0] = _translate[1] = _translate[2] = 0;
        _time = 0.0;
        _delegate2 = NULL;
    }

    // UsdImagingGL_UnitTestGLDrawing overrides
    virtual void InitTest();
    virtual void DrawTest(bool offscreen);

    virtual void MousePress(int button, int x, int y, int modKeys);
    virtual void MouseRelease(int button, int x, int y, int modKeys);
    virtual void MouseMove(int x, int y, int modKeys);

private:
    std::unique_ptr<UsdImagingGLEngine> _engine;
    HdRenderIndex *_batchIndex;

    UsdStageRefPtr _stage1;
    UsdImagingDelegate* _delegate1;

    UsdStageRefPtr _stage2;
    UsdImagingDelegate* _delegate2;

    UsdStageRefPtr _stage3;
    UsdImagingDelegate* _delegate3;

    UsdStageRefPtr _stage4;
    UsdImagingDelegate* _delegate4;

    float _rotate[2];
    float _translate[3];
    int _mousePos[2];
    bool _mouseButton[3];
    double _time;
};

GLuint vao;

static
UsdStageRefPtr
_CreateStage(std::string const& primName) {
    static int offset = -2;
    UsdStageRefPtr stage = UsdStage::CreateInMemory();
    UsdGeomCube cube = UsdGeomCube::Define(stage, SdfPath("/" + primName));
    cube.GetSizeAttr().Set((double)(primName[0]*2.0+primName[1]*3.0));
    VtVec3fArray color(1);
    color[0] = GfVec3f(primName[0]/100., primName[1]/100., primName[2]/100.);
    cube.GetDisplayColorAttr().Set(color);
    GfMatrix4d xf(1);
    xf[3][0] = offset*500.0;
    cube.MakeMatrixXform().Set(xf);
    offset += 2;
    return stage;
}

void
My_TestGLDrawing::InitTest()
{
    std::cout << glGetString(GL_VENDOR) << "\n";
    std::cout << glGetString(GL_RENDERER) << "\n";
    std::cout << glGetString(GL_VERSION) << "\n";

    WorkSetMaximumConcurrencyLimit();

    HdPerfLog& perfLog = HdPerfLog::GetInstance();
    perfLog.Enable();

    _stage1 = _CreateStage("Foo");
    _stage2 = _CreateStage("Zor");
    _stage3 = _CreateStage("Garply");
    _stage4 = _CreateStage("Bar");

    SdfPathVector excludedPaths;
    _engine.reset(new UsdImagingGLEngine(
        _stage1->GetPseudoRoot().GetPath(), excludedPaths));

    // Setup the batch index to be drawn.
    _batchIndex = _GetRenderIndex(_engine.get());

    // Root the delegate at the same name as the first root prim, for example,
    // _stage1 has </Foo>, so it will be rooted at </Foo/Foo> in the render
    // index.
    _delegate1 = new UsdImagingDelegate(_batchIndex, 
                    _stage1->GetPseudoRoot().GetChildren().begin()->GetPath());

    // Same for delegate2
    _delegate2 = new UsdImagingDelegate(_batchIndex, 
                    _stage2->GetPseudoRoot().GetChildren().begin()->GetPath());

    // Same for delegate3, but make it invisible
    _delegate3 = new UsdImagingDelegate(_batchIndex, 
                    _stage3->GetPseudoRoot().GetChildren().begin()->GetPath());

    // Same for delegate4
    _delegate4 = new UsdImagingDelegate(_batchIndex, 
                    _stage4->GetPseudoRoot().GetChildren().begin()->GetPath());

    _delegate1->Populate(_stage1->GetPseudoRoot());
    _delegate2->Populate(_stage2->GetPseudoRoot());
    _delegate3->Populate(_stage3->GetPseudoRoot());
    _delegate4->Populate(_stage4->GetPseudoRoot());

    // Make sure everything is in the index as we expect.
    SdfPath delegateRoot = _stage1->GetPseudoRoot().GetChildren()
                                                       .begin()->GetPath();
    for (UsdPrim prim: _stage1->Traverse()) {
        if (prim.GetPath() == SdfPath::AbsoluteRootPath())
            continue;
        // We expect to find prim </Foo/Foo> for _stage1, but in _stage1 that
        // prim is stored as </Foo>, so we have to replace the prefix </> with
        // </Foo>, yielding </Foo/Foo>.
        SdfPath path = prim.GetPath().ReplacePrefix(SdfPath::AbsoluteRootPath(),
                                                    delegateRoot);
        TF_VERIFY(
            _batchIndex->HasRprim(path),
            "Failed to find <%s> in the render index.",
            path.GetText());
    }

    if(IsEnabledTestLighting()) {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
    }

    _translate[0] = 0.0;
    _translate[1] = -1000.0;
    _translate[2] = -2500.0;
}

void
My_TestGLDrawing::DrawTest(bool offscreen)
{
    HdPerfLog& perfLog = HdPerfLog::GetInstance();
    perfLog.Enable();

    _time += 1.0;
    if (_time > 20) {
        //delete _delegate2;
        //_delegate2 = NULL;
        _time = 0.0;
    }

    _delegate1->SetTime(_time);
    
    if (_delegate2)
        _delegate2->SetTime(_time);
    
    // Reset all counters we care about.
    perfLog.ResetCache(HdTokens->extent);
    perfLog.ResetCache(HdTokens->points);
    perfLog.ResetCache(HdTokens->topology);
    perfLog.ResetCache(HdTokens->transform);
    perfLog.SetCounter(UsdImagingTokens->usdVaryingExtent, 0);
    perfLog.SetCounter(UsdImagingTokens->usdVaryingPrimvar, 0);
    perfLog.SetCounter(UsdImagingTokens->usdVaryingTopology, 0);
    perfLog.SetCounter(UsdImagingTokens->usdVaryingVisibility, 0);
    perfLog.SetCounter(UsdImagingTokens->usdVaryingXform, 0);

    int width = GetWidth(), height = GetHeight();

    double aspectRatio = double(width)/height;
    GfFrustum frustum;
    frustum.SetPerspective(60.0, aspectRatio, 1, 100000.0);

    GfMatrix4d viewMatrix;
    viewMatrix.SetIdentity();
    viewMatrix *= GfMatrix4d().SetRotate(GfRotation(GfVec3d(0, 1, 0), _rotate[0]));
    viewMatrix *= GfMatrix4d().SetRotate(GfRotation(GfVec3d(1, 0, 0), _rotate[1]));
    viewMatrix *= GfMatrix4d().SetTranslate(GfVec3d(_translate[0], _translate[1], _translate[2]));

    GfMatrix4d projMatrix = frustum.ComputeProjectionMatrix();

    GfMatrix4d modelViewMatrix =
            // rotate from z-up to y-up
            GfMatrix4d().SetRotate(GfRotation(GfVec3d(1.0,0.0,0.0), -90.0)) *
            viewMatrix; 

    GfVec4d viewport(0, 0, width, height);
    _engine->SetCameraState(modelViewMatrix, projMatrix);
    _engine->SetRenderViewport(viewport);
    UsdImagingGLRenderParams params;
    params.drawMode = UsdImagingGLDrawMode::DRAW_SHADED_SMOOTH;
    params.enableLighting =  IsEnabledTestLighting();
    params.cullStyle = GetCullStyle();

    glViewport(0, 0, width, height);

    GLfloat clearColor[4] = { .25f, .25f, 0.25f, 1.0f };
    glClearBufferfv(GL_COLOR, 0, clearColor);

    GLfloat clearDepth[1] = { 1.0f };
    glClearBufferfv(GL_DEPTH, 0, clearDepth);

    glEnable(GL_DEPTH_TEST);


    if(IsEnabledTestLighting()) {
        _engine->SetLightingStateFromOpenGL();
    }

    // ---------------------------------------------------------------------- //
    // Draw both Delegate1 and Delegate2
    // ---------------------------------------------------------------------- //
    std::cout << "\n";
    std::cout << "Rendering delegate 1,2,3,4\n";
    _Render(_engine.get(), params);

    // HdxTaskController pushes 3 render tasks: renderTask, additive, OIT.
    // So we expect to find 3 dirtyLists.
    const double renderTaskCnt = 3.0;

    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->drawBatches) == 1.0);
    // 4 prim drawItems + 1 drawItem for OIT image shader
    TF_VERIFY(perfLog.GetCounter(HdTokens->itemsDrawn) == 5.0);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->dirtyLists) == renderTaskCnt);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->instMeshTopology) == 1.0);

    TfTokenVector counterNames;
    std::cout << "--------------------------------------------------------\n";
    counterNames = perfLog.GetCounterNames();
    TF_FOR_ALL(nameIt, counterNames) {
        std::cout << *nameIt << " : " << perfLog.GetCounter(*nameIt) << "\n";
    }
    std::cout << "--------------------------------------------------------\n\n";


    // ---------------------------------------------------------------------- //
    // Destroy Delegate2 and redraw
    // ---------------------------------------------------------------------- //

    std::cout << "Destroying delegate2\n";
    // Destroy one of the delegates, we expect all resources to be reclaimed.
    delete _delegate2;
    _delegate2 = NULL;

    std::cout << "--------------------------------------------------------\n";
    counterNames = perfLog.GetCounterNames();
    TF_FOR_ALL(nameIt, counterNames) {
        std::cout << *nameIt << " : " << perfLog.GetCounter(*nameIt) << "\n";
    }
    std::cout << "--------------------------------------------------------\n\n";

    std::cout << "Rendering delegate 1\n";
    _Render(_engine.get(), params);
    
    std::cout << "--------------------------------------------------------\n";
    counterNames = perfLog.GetCounterNames();
    TF_FOR_ALL(nameIt, counterNames) {
        std::cout << *nameIt << " : " << perfLog.GetCounter(*nameIt) << "\n";
    }
    std::cout << "--------------------------------------------------------\n\n";

    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->dirtyLists) == renderTaskCnt);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->drawBatches) == 1.0);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->garbageCollected) == 1.0);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->garbageCollectedVbo) == 0.0);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->garbageCollectedSsbo) == 1.0);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->instMeshTopology) == 1.0);
    // 3 prim drawItems + 1 drawItem for OIT image shader
    TF_VERIFY(perfLog.GetCounter(HdTokens->itemsDrawn) == 4.0);

    // ---------------------------------------------------------------------- //
    // Recreate Delegate2 and redraw 1,2,3, but invis 4
    // ---------------------------------------------------------------------- //
    std::cout << "Recreateing delegate 2, invising 4\n";
    _delegate4->SetRootVisibility(false);
    _delegate2 = new UsdImagingDelegate(_batchIndex, 
                    _stage2->GetPseudoRoot().GetChildren().begin()->GetPath());
    _delegate2->Populate(_stage2->GetPseudoRoot());
    _delegate2->SetTime(_time);
    std::cout << "--------------------------------------------------------\n";
    counterNames = perfLog.GetCounterNames();
    TF_FOR_ALL(nameIt, counterNames) {
        std::cout << *nameIt << " : " << perfLog.GetCounter(*nameIt) << "\n";
    }
    std::cout << "--------------------------------------------------------\n\n";
    std::cout << "Rendering delegate 1 & 2 (recreated)\n";
    _Render(_engine.get(), params);
    
    std::cout << "--------------------------------------------------------\n";
    counterNames = perfLog.GetCounterNames();
    TF_FOR_ALL(nameIt, counterNames) {
        std::cout << *nameIt << " : " << perfLog.GetCounter(*nameIt) << "\n";
    }
    std::cout << "--------------------------------------------------------\n\n";

    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->dirtyLists) == renderTaskCnt);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->drawBatches) == 1.0);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->garbageCollected) == 1.0);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->instMeshTopology) == 1.0);
    // 3 prim drawItems + 1 drawItem for OIT image shader
    TF_VERIFY(perfLog.GetCounter(HdTokens->itemsDrawn) == 4.0);

    // ---------------------------------------------------------------------- //
    // Set delegate 4 root visibility = true
    // ---------------------------------------------------------------------- //
    std::cout << "Recreateing re-vising 4\n";
    _delegate4->SetRootVisibility(true);
    // Used to measure delta in the next test, get the initial value here.
    double bufferSourceDelta1 = 
                        perfLog.GetCounter(HdPerfTokens->bufferSourcesResolved);

    _Render(_engine.get(), params);
    
    // Save the delta.
    bufferSourceDelta1 = perfLog.GetCounter(HdPerfTokens->bufferSourcesResolved)
                                - bufferSourceDelta1;
    std::cout << "--------------------------------------------------------\n";
    counterNames = perfLog.GetCounterNames();
    TF_FOR_ALL(nameIt, counterNames) {
        std::cout << *nameIt << " : " << perfLog.GetCounter(*nameIt) << "\n";
    }
    std::cout << "--------------------------------------------------------\n\n";

    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->dirtyLists) == renderTaskCnt);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->drawBatches) == 1.0);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->garbageCollected) == 1.0);
    TF_VERIFY(perfLog.GetCounter(HdPerfTokens->instMeshTopology) == 1.0);
    // 4 prim drawItems + 1 drawItem for OIT image shader
    TF_VERIFY(perfLog.GetCounter(HdTokens->itemsDrawn) == 5.0);

    std::cout << "Set root transform on delegate 3\n";
    GfMatrix4d xf(1);
    xf[2][0] = -42;
    _delegate3->SetRootTransform(xf);
    double bufferSourceDelta2 = 
                        perfLog.GetCounter(HdPerfTokens->bufferSourcesResolved);
    _Render(_engine.get(), params);

    std::cout << "--------------------------------------------------------\n";
    counterNames = perfLog.GetCounterNames();
    TF_FOR_ALL(nameIt, counterNames) {
        std::cout << *nameIt << " : " << perfLog.GetCounter(*nameIt) << "\n";
    }
    std::cout << "--------------------------------------------------------\n\n";
    // 4 prim drawItems + 1 drawItem for OIT image shader
    TF_VERIFY(perfLog.GetCounter(HdTokens->itemsDrawn) == 5.0);

    // Now, if we did everything correctly, we only updated one extra buffer 
    // for the transform, so verify bufferSourceDelta2 - buferSourceDelta1 == 1.
    bufferSourceDelta2 = perfLog.GetCounter(HdPerfTokens->bufferSourcesResolved)
                                - bufferSourceDelta2;
    TF_VERIFY(bufferSourceDelta2 - bufferSourceDelta1 == 2.0,
            "Expected two buffer source updates, one for the transform and "
            "one for the normal, but got %f (%f - %f)", 
            bufferSourceDelta2 - bufferSourceDelta1,
            bufferSourceDelta2, bufferSourceDelta1); 
}

void
My_TestGLDrawing::MousePress(int button, int x, int y, int modKeys)
{
    _mouseButton[button] = 1;
    _mousePos[0] = x;
    _mousePos[1] = y;
}

void
My_TestGLDrawing::MouseRelease(int button, int x, int y, int modKeys)
{
    _mouseButton[button] = 0;
}

void
My_TestGLDrawing::MouseMove(int x, int y, int modKeys)
{
    int dx = x - _mousePos[0];
    int dy = y - _mousePos[1];

    if (_mouseButton[0]) {
        _rotate[0] += dx;
        _rotate[1] += dy;
    } else if (_mouseButton[1]) {
        _translate[0] += dx;
        _translate[1] -= dy;
    } else if (_mouseButton[2]) {
        _translate[2] += dx;
    }

    _mousePos[0] = x;
    _mousePos[1] = y;
}

void
BasicTest(int argc, char *argv[])
{
    My_TestGLDrawing driver;
    driver.RunTest(argc, argv);
}

int main(int argc, char *argv[])
{
    TfErrorMark mark;

    BasicTest(argc, argv);

    if (TF_VERIFY(mark.IsClean()))
        std::cout << "OK" << std::endl;
    else
        std::cout << "FAILED" << std::endl;
}

