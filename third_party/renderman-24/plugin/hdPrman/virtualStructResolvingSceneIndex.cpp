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
#include "hdPrman/virtualStructResolvingSceneIndex.h"
#include "hdPrman/dataSourceMaterialNetworkInterface.h"
#include "hdPrman/matfiltResolveVstructs.h"

PXR_NAMESPACE_OPEN_SCOPE

static void
_ResolveVirtualStructs(
    HdMaterialNetworkInterface *interface, bool enableConditions)
{
    static const NdrTokenVec shaderTypePriority = {
        TfToken("OSL"),
        TfToken("RmanCpp"),
    };

    MatfiltResolveVstructs(interface, shaderTypePriority, enableConditions);
}

static void
_ResolveVirtualStructsWithConditionals(
    HdMaterialNetworkInterface *networkInterface)
{
    _ResolveVirtualStructs(networkInterface, true);
}

static void
_ResolveVirtualStructsWithoutConditionals(
    HdMaterialNetworkInterface *networkInterface)
{
    _ResolveVirtualStructs(networkInterface, false);
}

HdPrmanVirtualStructResolvingSceneIndex::HdPrmanVirtualStructResolvingSceneIndex(
    const HdSceneIndexBaseRefPtr &inputSceneIndex, bool applyConditionals)
: HdPrmanMaterialFilteringSceneIndexBase(inputSceneIndex)
, _applyConditionals(applyConditionals)
{
}

HdPrmanMaterialFilteringSceneIndexBase::FilteringFnc
HdPrmanVirtualStructResolvingSceneIndex::_GetFilteringFunction() const
{
    // could capture _applyCondition but instead use wrapper function
    if (_applyConditionals) {
        return _ResolveVirtualStructsWithConditionals;
    } else {
        return _ResolveVirtualStructsWithoutConditionals;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
