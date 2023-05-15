/*
 * Copyright (C) 2021 David Cattermole.
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
 * mmSolver viewport 2.0 renderer override.
 *
 */

#ifndef MM_SOLVER_RENDER_RENDER_OVERRIDE_H
#define MM_SOLVER_RENDER_RENDER_OVERRIDE_H

// STL
#include <vector>

// Maya
#include <maya/MBoundingBox.h>
#include <maya/MDagMessage.h>
#include <maya/MObjectHandle.h>
#include <maya/MRenderTargetManager.h>
#include <maya/MString.h>
#include <maya/MUiMessage.h>
#include <maya/MViewport2Renderer.h>

// MM Solver
#include "RenderGlobalsNode.h"
#include "mmSolver/render/data/RenderMode.h"
#include "mmSolver/render/data/constants.h"
#include "mmSolver/render/ops/SceneRender.h"

namespace mmsolver {
namespace render {

class RenderOverride : public MHWRender::MRenderOverride {
public:
    RenderOverride(const MString& name);
    ~RenderOverride() override;

    MHWRender::DrawAPI supportedDrawAPIs() const override;

    MStatus setup(const MString& destination) override;
    MStatus cleanup() override;

    // Called by Maya to determine the name in the "Renderers" menu.
    MString uiName() const override { return m_ui_name; }

protected:
    MStatus updateParameters();

    // UI name
    MString m_ui_name;

    // Allow the command to access this class.
    friend class MMRendererCmd;

private:
    SceneRender* m_backgroundOp;

    MSelectionList m_image_plane_nodes;
};

}  // namespace render
}  // namespace mmsolver

#endif  // MAYA_MM_SOLVER_RENDER_RENDER_OVERRIDE_H
