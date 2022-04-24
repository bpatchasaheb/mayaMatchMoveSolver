/*
 * Copyright (C) 2022 David Cattermole.
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
 */

#ifndef MM_IMAGE_PLANE_GEOMETRY_OVERRIDE_H
#define MM_IMAGE_PLANE_GEOMETRY_OVERRIDE_H

// Maya
#include <maya/MString.h>
#include <maya/MColor.h>
#include <maya/MGlobal.h>
#include <maya/MPointArray.h>
#include <maya/MStreamUtils.h>
#include <maya/MEventMessage.h>

// Maya Viewport 2.0
#include <maya/MPxGeometryOverride.h>
#include <maya/MDrawRegistry.h>
#include <maya/MUserData.h>
#include <maya/MDrawContext.h>
#include <maya/MHWGeometryUtilities.h>

// MM Solver
#include "ImagePlaneShapeNode.h"
#include "mmSolver/utilities/debug_utils.h"

namespace mmsolver {

class ImagePlaneGeometryOverride : public MPxGeometryOverride {
public:
    static MPxGeometryOverride *Creator(const MObject &obj) {
        return new ImagePlaneGeometryOverride(obj);
    }

    ~ImagePlaneGeometryOverride() override;

    MHWRender::DrawAPI
    supportedDrawAPIs() const override;

    bool hasUIDrawables() const override;

    void updateDG() override;

    void updateRenderItems(
        const MDagPath &path,
        MRenderItemList &list) override;

    void addUIDrawables(
        const MDagPath &path,
        MUIDrawManager &drawManager,
        const MFrameContext &frameContext) override;

    void populateGeometry(
        const MGeometryRequirements &requirements,
        const MRenderItemList &renderItems,
        MGeometry &data) override;

    void cleanUp() override;

#if MAYA_API_VERSION >= 20190000
    bool requiresGeometryUpdate() const override;
    bool requiresUpdateRenderItems(const MDagPath &path) const;
#endif

    bool traceCallSequence() const override {
        // Return true if internal tracing is desired.
        return false;
    }

    void handleTraceMessage(const MString &message) const override {
        MGlobal::displayInfo("ImagePlaneGeometryOverride: " + message);
        MMSOLVER_INFO("ImagePlaneGeometryOverride: " << message.asChar());
    }

protected:
    ImagePlaneGeometryOverride(const MObject &obj);

    MObject m_this_node;
    MDagPath m_geometry_node_path;
    MFn::Type m_geometry_node_type;
    MFn::Type m_shader_node_type;
    MObject m_geometry_node;
    MObject m_shader_node;

    bool m_enable;
    int32_t m_image_width;
    int32_t m_image_height;
    double m_image_pixel_aspect;
};

} // namespace mmsolver

#endif // MM_IMAGE_PLANE_GEOMETRY_OVERRIDE_H