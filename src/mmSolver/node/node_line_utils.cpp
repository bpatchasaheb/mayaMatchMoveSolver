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

#include "node_line_utils.h"

// STL
#include <cassert>
#include <cmath>
#include <cstring>

// Maya
#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MMatrix.h>

// MM Solver
#include "mmSolver/core/mmdata.h"
#include "mmSolver/core/mmmath.h"
#include "mmSolver/mayahelper/maya_utils.h"
#include "mmSolver/utilities/debug_utils.h"
#include "mmSolver/utilities/number_utils.h"

// MM SceneGraph
#include <mmscenegraph/mmscenegraph.h>

namespace mmsg = mmscenegraph;

namespace mmsolver {

MStatus query_line_point_data(const MMatrix parentInverseMatrix,
                              MArrayDataHandle &transformArrayHandle,
                              rust::Vec<mmscenegraph::Real> &out_point_data_x,
                              rust::Vec<mmscenegraph::Real> &out_point_data_y,
                              const bool verbose) {
    MStatus status = MS::kSuccess;

    out_point_data_x.clear();
    out_point_data_y.clear();

    status = transformArrayHandle.jumpToArrayElement(0);
    if (status == MStatus::kSuccess) {
        do {
            MDataHandle transformArrayHandleElement =
                transformArrayHandle.inputValue(&status);
            CHECK_MSTATUS(status);

            MMatrix transformMatrix = transformArrayHandleElement.asMatrix();

            mmsg::Real x = transformMatrix[3][0];
            mmsg::Real y = transformMatrix[3][1];
            mmsg::Real z = transformMatrix[3][2];

            auto point = MPoint(x, y, z);
            point = point * parentInverseMatrix;

            if (verbose) {
                uint32_t element_index =
                    transformArrayHandle.elementIndex(&status);
                CHECK_MSTATUS(status);
                MMSOLVER_VRB("Point X: " << element_index << " : " << point.x);
                MMSOLVER_VRB("Point Y: " << element_index << " : " << point.y);
            }

            out_point_data_x.push_back(point.x);
            out_point_data_y.push_back(point.y);

        } while (transformArrayHandle.next() == MStatus::kSuccess);
    }

    MMSOLVER_VRB("out_point_data_x.size(): " << out_point_data_x.size());
    MMSOLVER_VRB("out_point_data_y.size(): " << out_point_data_x.size());
    assert((out_point_data_x.size() == out_point_data_y.size()));

    return status;
}

MStatus fit_line_to_points(const mmsg::Real line_length,
                           const rust::Vec<mmscenegraph::Real> &point_data_x,
                           const rust::Vec<mmscenegraph::Real> &point_data_y,
                           mmdata::Point2D &out_line_center,
                           mmsg::Real &out_line_slope,
                           mmsg::Real &out_line_angle,
                           mmdata::Point2D &out_line_point_a,
                           mmdata::Point2D &out_line_point_b,
                           const bool verbose) {
    MStatus status = MS::kSuccess;

    auto line_center_x = 0.0;
    auto line_center_y = 0.0;
    auto line_slope = 0.0;
    auto line_angle = 0.0;

    if (point_data_x.size() > 2) {
        rust::Slice<const mmsg::Real> x_slice{point_data_x.data(),
                                              point_data_x.size()};
        rust::Slice<const mmsg::Real> y_slice{point_data_y.data(),
                                              point_data_y.size()};
        auto ok = mmsg::fit_line_to_points_type2(
            x_slice, y_slice, line_center_x, line_center_y, line_slope);

        if (!ok) {
            MMSOLVER_WRN(
                "Line Node Utils: Failed to fit a line to data points.");
            status = MS::kFailure;
            return status;
        }

        out_line_center.x_ = line_center_x;
        out_line_center.y_ = line_center_y;
        out_line_slope = line_slope;
        out_line_angle = line_angle;
    } else if (point_data_x.size() == 2) {
        out_line_center.x_ = (point_data_x[0] + point_data_x[1]) / 2.0;
        out_line_center.y_ = (point_data_y[0] + point_data_y[1]) / 2.0;
        out_line_slope = (point_data_x[1] - point_data_x[0]) /
                         (point_data_y[1] - point_data_y[0]);
    }

    auto line_angle_radian = std::atan(-out_line_slope);
    out_line_angle = line_angle_radian * RADIANS_TO_DEGREES;

    // Convert line center point and slope to 2 points to make
    // up a line we can draw between.
    out_line_point_a.x_ =
        out_line_center.x_ + (std::sin(-line_angle_radian) * line_length);
    out_line_point_a.y_ =
        out_line_center.y_ + (std::cos(-line_angle_radian) * line_length);
    out_line_point_b.x_ =
        out_line_center.x_ - (std::sin(-line_angle_radian) * line_length);
    out_line_point_b.y_ =
        out_line_center.y_ - (std::cos(-line_angle_radian) * line_length);

    MMSOLVER_VRB("Line Node Utils: Center X: " << out_line_center.x_);
    MMSOLVER_VRB("Line Node Utils: Center Y: " << out_line_center.y_);
    MMSOLVER_VRB("Line Node Utils: Slope   : " << out_line_slope);
    MMSOLVER_VRB("Line Node Utils: Angle   : " << out_line_angle);

    return status;
}

}  // namespace mmsolver
