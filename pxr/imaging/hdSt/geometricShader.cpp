//
// Copyright 2016 Pixar
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

#include "pxr/imaging/hdSt/geometricShader.h"

#include "pxr/imaging/hdSt/debugCodes.h"
#include "pxr/imaging/hdSt/shaderKey.h"

#include "pxr/imaging/hd/binding.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hio/glslfx.h"

#include <boost/functional/hash.hpp>

#include <iostream>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE


HdSt_GeometricShader::HdSt_GeometricShader(std::string const &glslfxString,
                                       PrimitiveType primType,
                                       HdCullStyle cullStyle,
                                       bool useHardwareFaceCulling,
                                       bool hasMirroredTransform,
                                       bool doubleSided,
                                       HdPolygonMode polygonMode,
                                       bool cullingPass,
                                       FvarPatchType fvarPatchType,
                                       SdfPath const &debugId,
                                       float lineWidth)
    : HdStShaderCode()
    , _primType(primType)
    , _cullStyle(cullStyle)
    , _useHardwareFaceCulling(useHardwareFaceCulling)
    , _hasMirroredTransform(hasMirroredTransform)
    , _doubleSided(doubleSided)
    , _polygonMode(polygonMode)
    , _lineWidth(lineWidth)
    , _frustumCullingPass(cullingPass)
    , _fvarPatchType(fvarPatchType)
    , _hash(0)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // XXX
    // we will likely move this (the constructor or the entire class) into
    // the base class (HdStShaderCode) at the end of refactoring, to be able to
    // use same machinery other than geometric shaders.

    if (TfDebug::IsEnabled(HDST_DUMP_GLSLFX_CONFIG)) {
        std::cout << debugId << "\n"
                  << glslfxString << "\n";
    }

    std::stringstream ss(glslfxString);
    _glslfx.reset(new HioGlslfx(ss));
    boost::hash_combine(_hash, _glslfx->GetHash());
    boost::hash_combine(_hash, cullingPass);
    boost::hash_combine(_hash, primType);
    boost::hash_combine(_hash, cullStyle);
    boost::hash_combine(_hash, fvarPatchType);
    //
    // note: Don't include polygonMode into the hash.
    //       It is independent from the GLSL program.
    //
}

HdSt_GeometricShader::~HdSt_GeometricShader() = default;

/* virtual */
HioGlslfx const *
HdSt_GeometricShader::_GetGlslfx() const
{
    return _glslfx.get();
}

/* virtual */
HdStShaderCode::ID
HdSt_GeometricShader::ComputeHash() const
{
    return _hash;
}

/* virtual */
std::string
HdSt_GeometricShader::GetSource(TfToken const &shaderStageKey) const
{
    return _glslfx->GetSource(shaderStageKey);
}

void
HdSt_GeometricShader::BindResources(const int program,
                                    HdSt_ResourceBinder const &binder)
{
    // no-op
}

void
HdSt_GeometricShader::UnbindResources(const int program,
                                      HdSt_ResourceBinder const &binder)
{
    // no-op
}

/*virtual*/
void
HdSt_GeometricShader::AddBindings(HdBindingRequestVector *customBindings)
{
    // no-op
}

int
HdSt_GeometricShader::GetPrimitiveIndexSize() const
{
    int primIndexSize = 1;

    switch (_primType)
    {
        case PrimitiveType::PRIM_POINTS:
            primIndexSize = 1;
            break;
        case PrimitiveType::PRIM_BASIS_CURVES_LINES:
        case PrimitiveType::PRIM_BASIS_CURVES_LINEAR_PATCHES:
            primIndexSize = 2;
            break;
        case PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
        case PrimitiveType::PRIM_MESH_REFINED_TRIANGLES:
        case PrimitiveType::PRIM_VOLUME:
            primIndexSize = 3;
            break;
        case PrimitiveType::PRIM_BASIS_CURVES_CUBIC_PATCHES:
        case PrimitiveType::PRIM_MESH_COARSE_QUADS:
        case PrimitiveType::PRIM_MESH_REFINED_QUADS:
            primIndexSize = 4;
            break;
        case PrimitiveType::PRIM_MESH_COARSE_TRIQUADS:
        case PrimitiveType::PRIM_MESH_REFINED_TRIQUADS:
            primIndexSize = 6;
            break;
        case PrimitiveType::PRIM_MESH_BSPLINE:
            primIndexSize = 16;
            break;
        case PrimitiveType::PRIM_MESH_BOXSPLINETRIANGLE:
            primIndexSize = 12;
            break;
    }

    return primIndexSize;
}

int
HdSt_GeometricShader::GetNumPatchEvalVerts() const
{
    int numPatchEvalVerts = 0;

    switch (_primType)
    {
        case PrimitiveType::PRIM_BASIS_CURVES_LINEAR_PATCHES:
            numPatchEvalVerts = 2;
            break;
        case PrimitiveType::PRIM_BASIS_CURVES_CUBIC_PATCHES:
            numPatchEvalVerts = 4;
            break;
        case PrimitiveType::PRIM_MESH_BSPLINE:
            numPatchEvalVerts = 16;
            break;
        case PrimitiveType::PRIM_MESH_BOXSPLINETRIANGLE:
            numPatchEvalVerts = 15;
            break;
        default:
            numPatchEvalVerts = 0;
            break;
    }

    return numPatchEvalVerts;
}

int
HdSt_GeometricShader::GetNumPrimitiveVertsForGeometryShader() const
{
    int numPrimVerts = 1;

    switch (_primType)
    {
        case PrimitiveType::PRIM_POINTS:
            numPrimVerts = 1;
            break;
        case PrimitiveType::PRIM_BASIS_CURVES_LINES:
            numPrimVerts = 2;
            break;
        case PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
        case PrimitiveType::PRIM_MESH_REFINED_TRIANGLES:
        case PrimitiveType::PRIM_MESH_COARSE_TRIQUADS:
        case PrimitiveType::PRIM_MESH_REFINED_TRIQUADS:
        case PrimitiveType::PRIM_BASIS_CURVES_LINEAR_PATCHES:
        case PrimitiveType::PRIM_BASIS_CURVES_CUBIC_PATCHES:
        case PrimitiveType::PRIM_MESH_BSPLINE:
        case PrimitiveType::PRIM_MESH_BOXSPLINETRIANGLE:
        // for patches with tesselation, input to GS is still a series of tris
        case PrimitiveType::PRIM_VOLUME:
            numPrimVerts = 3;
            break;
        case PrimitiveType::PRIM_MESH_COARSE_QUADS:
        case PrimitiveType::PRIM_MESH_REFINED_QUADS:
            numPrimVerts = 4;
            break;
    }

    return numPrimVerts;
}

HgiPrimitiveType
HdSt_GeometricShader::GetHgiPrimitiveType() const
{
    HgiPrimitiveType primitiveType = HgiPrimitiveTypePointList;

    switch (GetPrimitiveType())
    {
        case PrimitiveType::PRIM_POINTS:
            primitiveType = HgiPrimitiveTypePointList;
            break;
        case PrimitiveType::PRIM_BASIS_CURVES_LINES:
            primitiveType = HgiPrimitiveTypeLineList;
            break;
        case PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
        case PrimitiveType::PRIM_MESH_REFINED_TRIANGLES:
        case PrimitiveType::PRIM_MESH_COARSE_TRIQUADS:
        case PrimitiveType::PRIM_MESH_REFINED_TRIQUADS:
        case PrimitiveType::PRIM_VOLUME:
            primitiveType = HgiPrimitiveTypeTriangleList;
            break;
        case PrimitiveType::PRIM_MESH_COARSE_QUADS:
        case PrimitiveType::PRIM_MESH_REFINED_QUADS:
            primitiveType = HgiPrimitiveTypeLineListWithAdjacency;
            break;
        case PrimitiveType::PRIM_BASIS_CURVES_CUBIC_PATCHES:
        case PrimitiveType::PRIM_BASIS_CURVES_LINEAR_PATCHES:
        case PrimitiveType::PRIM_MESH_BSPLINE:
        case PrimitiveType::PRIM_MESH_BOXSPLINETRIANGLE:
            primitiveType = HgiPrimitiveTypePatchList;
            break;
    }

    return primitiveType;
}

/*static*/
 HdSt_GeometricShaderSharedPtr
 HdSt_GeometricShader::Create(
    HdSt_ShaderKey const &shaderKey, 
    HdStResourceRegistrySharedPtr const &resourceRegistry)
{
    // Use the shaderKey hash to deduplicate geometric shaders.
    HdInstance<HdSt_GeometricShaderSharedPtr> geometricShaderInstance =
        resourceRegistry->RegisterGeometricShader(shaderKey.ComputeHash());

    if (geometricShaderInstance.IsFirstInstance()) {
        geometricShaderInstance.SetValue(
            std::make_shared<HdSt_GeometricShader>(
                shaderKey.GetGlslfxString(),
                shaderKey.GetPrimitiveType(),
                shaderKey.GetCullStyle(),
                shaderKey.UseHardwareFaceCulling(),
                shaderKey.HasMirroredTransform(),
                shaderKey.IsDoubleSided(),
                shaderKey.GetPolygonMode(),
                shaderKey.IsFrustumCullingPass(),
                shaderKey.GetFvarPatchType(),
                /*debugId=*/SdfPath(),
                shaderKey.GetLineWidth()));
    }
    return geometricShaderInstance.GetValue();
}

PXR_NAMESPACE_CLOSE_SCOPE

