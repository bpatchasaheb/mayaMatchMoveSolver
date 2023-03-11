/*
 * Copyright (C) 2023 David Cattermole.
 *
 * This file is part of mmSolver.
 *
 * mmSolver is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * mmSolver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with mmSolver.  If not, see <https://www.gnu.org/licenses/>.
 * ====================================================================
 *
 *
 */

#include "BeginPasses.h"

// Maya
#include <maya/MBoundingBox.h>
#include <maya/MObject.h>
#include <maya/MShaderManager.h>
#include <maya/MStreamUtils.h>
#include <maya/MString.h>
#include <maya/MViewport2Renderer.h>

// MM Solver
#include "mmSolver/mayahelper/maya_utils.h"
#include "mmSolver/render/data/constants.h"
#include "mmSolver/render/ops/HudRender.h"
#include "mmSolver/render/ops/SceneRender.h"
#include "mmSolver/render/ops/scene_utils.h"
#include "mmSolver/utilities/debug_utils.h"

namespace mmsolver {
namespace render {

// Set up operations
BeginPasses::BeginPasses() : m_current_op(-1) {
    // Initialise the operations for this override.
    for (auto i = 0; i < BeginPass::kBeginPassCount; ++i) {
        m_ops[i] = nullptr;
    }
}

BeginPasses::~BeginPasses() {
    // Delete all the operations. This will release any references to
    // other resources user per operation.
    for (auto i = 0; i < BeginPass::kBeginPassCount; ++i) {
        delete m_ops[i];
        m_ops[i] = nullptr;
    }
}

bool BeginPasses::startOperationIterator() {
    m_current_op = 0;
    return true;
}

MHWRender::MRenderOperation *BeginPasses::getOperationFromList(
    size_t &current_op, MRenderOperation **ops, const size_t count) {
    const bool verbose = false;
    MMSOLVER_VRB("BeginPasses::getOperationFromList: current_op: "
                 << current_op << " count: " << count);
    if (current_op >= 0 && current_op < count) {
        while (!ops[current_op] || !ops[current_op]->enabled()) {
            current_op++;
            if (current_op >= count) {
                return nullptr;
            }
        }
        if (ops[current_op]) {
            return ops[current_op];
        }
    }
    return nullptr;
}

MHWRender::MRenderOperation *BeginPasses::renderOperation() {
    const auto count = BeginPass::kBeginPassCount;
    auto op = BeginPasses::getOperationFromList(m_current_op, m_ops, count);
    if (op != nullptr) {
        return op;
    } else {
        m_current_op = -1;
    }
    return nullptr;
}

bool BeginPasses::nextRenderOperation() {
    m_current_op++;

    const auto count = BeginPass::kBeginPassCount;
    if (m_current_op >= count) {
        m_current_op = -1;
    }

    return m_current_op >= 0;
}

MStatus BeginPasses::updateRenderOperations() {
    const bool verbose = false;
    MMSOLVER_VRB("BeginPasses::updateRenderOperations: ");

    if (m_ops[BeginPass::kSceneBackgroundPass] != nullptr) {
        // render operations are already up-to-date.
        return MS::kSuccess;
    }

    // Clear Masks
    const auto clear_mask_none =
        static_cast<uint32_t>(MHWRender::MClearOperation::kClearNone);
    const auto clear_mask_all =
        static_cast<uint32_t>(MHWRender::MClearOperation::kClearAll);
    const auto clear_mask_depth =
        static_cast<uint32_t>(MHWRender::MClearOperation::kClearDepth);

    // Display modes
    const auto display_mode_shaded =
        static_cast<MHWRender::MSceneRender::MDisplayMode>(
            MHWRender::MSceneRender::kShaded);
    const auto display_mode_wireframe =
        static_cast<MHWRender::MSceneRender::MDisplayMode>(
            MHWRender::MSceneRender::kWireFrame);

    // Draw these objects for transparency.
    const auto wire_draw_object_types =
        ~(MHWRender::MFrameContext::kExcludeMeshes |
          MHWRender::MFrameContext::kExcludeNurbsCurves |
          MHWRender::MFrameContext::kExcludeNurbsSurfaces |
          MHWRender::MFrameContext::kExcludeSubdivSurfaces);

    // Draw all non-geometry normally.
    const auto non_wire_draw_object_types =
        ((~wire_draw_object_types) |
         MHWRender::MFrameContext::kExcludeImagePlane |
         MHWRender::MFrameContext::kExcludePluginShapes);

    // What objects types to draw for depth buffer?
    const auto depth_draw_object_types =
        wire_draw_object_types | MHWRender::MFrameContext::kExcludeImagePlane;

    // Draw image planes in the background always.
    const auto bg_draw_object_types =
        ~(MHWRender::MFrameContext::kExcludeImagePlane |
          MHWRender::MFrameContext::kExcludePluginShapes);

    // Background pass.
    auto sceneOp = new SceneRender(kSceneBackgroundPassName);
    sceneOp->setBackgroundStyle(BackgroundStyle::kMayaDefault);
    sceneOp->setClearMask(clear_mask_all);
#if MAYA_API_VERSION != 20220000
    const auto draw_object_types =
        ~(MHWRender::MFrameContext::kExcludeGrid |
          MHWRender::MFrameContext::kExcludeImagePlane |
          MHWRender::MFrameContext::kExcludePluginShapes);
    sceneOp->setExcludeTypes(draw_object_types);
    sceneOp->setSceneFilter(MHWRender::MSceneRender::kRenderAllItems);
#else
    // The behaviour of the MSceneRender::MSceneFilterOption was
    // broken in Maya 2022.0, and was fixed in Maya 2022.1 and
    // 2023. The weird behaviour appears to be absent in Maya 2020, so
    // only Maya 2022.0 is affected.
    //
    // See fixed issues MAYA-111526 and MAYA-110627:
    //
    // "VP2: MSceneRender always rendering selection highlight even
    // though it is not set in the MSceneFilterOption
    // (kRenderPostSceneUIItems is disabled)"
    //
    // https://help.autodesk.com/view/MAYAUL/2023/ENU/?guid=Maya_ReleaseNotes_2023_release_notes_fixed_issues2023_html
    // https://help.autodesk.com/view/MAYAUL/2022/ENU/?guid=Maya_ReleaseNotes_2022_1_release_notes_html
    //
    // This workaround provides roughly the same appearance, compared to above.
    sceneOp->setExcludeTypes(MHWRender::MFrameContext::kExcludeNone);
    sceneOp->setSceneFilter(
        static_cast<MHWRender::MSceneRender::MSceneFilterOption>(
            MHWRender::MSceneRender::kRenderPreSceneUIItems |
            MHWRender::MSceneRender::kRenderShadedItems));
#endif

    MStatus status = MS::kSuccess;
    m_image_plane_nodes.clear();
    status = add_all_image_planes(m_image_plane_nodes);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    sceneOp->setObjectSetOverride(&m_image_plane_nodes);
    m_ops[BeginPass::kSceneBackgroundPass] = sceneOp;

    return status;
}

// Update the render targets that are required for the entire
// override.  References to these targets are set on the individual
// operations as required so that they will send their output to the
// appropriate location.
MStatus BeginPasses::updateRenderTargets(MHWRender::MRenderTarget **targets) {
    MStatus status = MS::kSuccess;

    const bool verbose = false;
    MMSOLVER_VRB("BeginPasses::updateRenderTargets");

    // Update the render targets on the individual operations.
    //
    // This section will determine the outputs of each operation.  The
    // input of each operation is assumed to be the Maya provided
    // color and depth targets, but shaders may internally reference
    // specific render targets.

    // Draw viewport background (with image plane).
    auto backgroundPassOp =
        dynamic_cast<SceneRender *>(m_ops[BeginPass::kSceneBackgroundPass]);
    if (backgroundPassOp) {
        backgroundPassOp->setEnabled(true);
        backgroundPassOp->setRenderTargets(targets, kMainColorTarget, 2);
    }

    return status;
}

MStatus BeginPasses::setPanelNames(const MString &name) {
    const bool verbose = false;
    MMSOLVER_VRB("BeginPasses::setPanelNames: " << name.asChar());

    if (m_ops[BeginPass::kSceneBackgroundPass]) {
        auto op =
            dynamic_cast<SceneRender *>(m_ops[BeginPass::kSceneBackgroundPass]);
        op->setPanelName(name);
    }

    return MS::kSuccess;
}

}  // namespace render
}  // namespace mmsolver
