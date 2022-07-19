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
 * Calculate camera relative poses.
 */

#include "camera_relative_pose.h"

// STL
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

// OpenMVG
#ifdef MMSOLVER_USE_OPENMVG

#include <openMVG/numeric/numeric.h>

#include <openMVG/cameras/Camera_Intrinsics.hpp>
#include <openMVG/cameras/Camera_Pinhole.hpp>
#include <openMVG/features/feature.hpp>
#include <openMVG/features/feature_container.hpp>
#include <openMVG/geometry/pose3.hpp>
#include <openMVG/matching/indMatch.hpp>
#include <openMVG/matching/indMatchDecoratorXY.hpp>
#include <openMVG/matching/regions_matcher.hpp>
#include <openMVG/multiview/conditioning.hpp>
#include <openMVG/multiview/motion_from_essential.hpp>
#include <openMVG/multiview/solver_essential_eight_point.hpp>
#include <openMVG/multiview/solver_essential_kernel.hpp>
#include <openMVG/multiview/solver_fundamental_kernel.hpp>
#include <openMVG/multiview/triangulation.hpp>
#include <openMVG/numeric/eigen_alias_definition.hpp>
#include <openMVG/robust_estimation/robust_estimator_ACRansac.hpp>
#include <openMVG/robust_estimation/robust_estimator_ACRansacKernelAdaptator.hpp>
#include <openMVG/sfm/pipelines/sfm_robust_model_estimation.hpp>
#include <openMVG/sfm/sfm_data.hpp>
#include <openMVG/sfm/sfm_data_BA.hpp>
#include <openMVG/sfm/sfm_data_BA_ceres.hpp>
#include <openMVG/sfm/sfm_data_io.hpp>
#include <openMVG/types.hpp>

#endif  // MMSOLVER_USE_OPENMVG

// Maya
#include <maya/MArgDatabase.h>
#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItSelectionList.h>
#include <maya/MMatrix.h>
#include <maya/MMatrixArray.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MSyntax.h>
#include <maya/MTime.h>
#include <maya/MTimeArray.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MTypes.h>

// Internal
#include "mmSolver/adjust/adjust_defines.h"
#include "mmSolver/mayahelper/maya_attr.h"
#include "mmSolver/mayahelper/maya_bundle.h"
#include "mmSolver/mayahelper/maya_camera.h"
#include "mmSolver/mayahelper/maya_marker.h"
#include "mmSolver/mayahelper/maya_utils.h"
#include "mmSolver/utilities/debug_utils.h"
#include "mmSolver/utilities/number_utils.h"

namespace mmsolver {
namespace sfm {

using MMMarker = Marker;
using MMCamera = Camera;

bool get_marker_coords_at_frame(const MTime &time, MarkerPtr &mkr, double &x,
                                double &y, double &weight, bool &enable) {
    auto timeEvalMode = TIME_EVAL_MODE_DG_CONTEXT;

    mkr->getPosXY(x, y, time, timeEvalMode);
    mkr->getEnable(enable, time, timeEvalMode);
    mkr->getWeight(weight, time, timeEvalMode);

    weight *= static_cast<double>(enable);
    return weight > 0;
}

bool get_marker_coords_at_frame(const MTime time, MMMarker &mkr, double &x,
                                double &y, double &weight, bool &enable) {
    auto timeEvalMode = TIME_EVAL_MODE_DG_CONTEXT;

    mkr.getPosXY(x, y, time, timeEvalMode);
    mkr.getEnable(enable, time, timeEvalMode);
    mkr.getWeight(weight, time, timeEvalMode);

    weight *= static_cast<double>(enable);
    return weight > 0;
}

// Note: The 'weight' on the bundle is assumed to be a cosntant value
// over all times - it is assumed not to be animated.
bool get_bundle_coords_at_frame(const MTime &time, BundlePtr &bnd, double &x,
                                double &y, double &z, double &weight) {
    auto timeEvalMode = TIME_EVAL_MODE_DG_CONTEXT;

    bnd->getPos(x, y, z, time, timeEvalMode);
    weight = bnd->getWeight();

    return weight > 0;
}

MStatus get_camera_values(const MTime &time, CameraPtr &cam, int &image_width,
                          int &image_height, double &focal_length_mm,
                          double &sensor_width_mm, double &sensor_height_mm) {
    MStatus status = MStatus::kSuccess;

    auto timeEvalMode = TIME_EVAL_MODE_DG_CONTEXT;

    auto filmBackWidth_inch = cam->getFilmbackWidthValue(time, timeEvalMode);
    auto filmBackHeight_inch = cam->getFilmbackHeightValue(time, timeEvalMode);
    sensor_width_mm = filmBackWidth_inch * INCH_TO_MM;
    sensor_height_mm = filmBackHeight_inch * INCH_TO_MM;

    focal_length_mm = cam->getFocalLengthValue(time, timeEvalMode);

    image_width = static_cast<int>(sensor_width_mm * 1000.0);
    image_height = static_cast<int>(sensor_height_mm * 1000.0);

    return status;
}

bool get_camera_image_res(const uint32_t frame_num, const MTime::Unit &uiUnit,
                          MMCamera &cam, int &image_width, int &image_height) {
    auto timeEvalMode = TIME_EVAL_MODE_DG_CONTEXT;
    auto frame = static_cast<double>(frame_num);
    MTime time = MTime(frame, uiUnit);

    auto filmBackWidth = cam.getFilmbackWidthValue(time, timeEvalMode);
    auto filmBackHeight = cam.getFilmbackHeightValue(time, timeEvalMode);
    image_width = static_cast<int>(filmBackWidth * 10000.0);
    image_height = static_cast<int>(filmBackHeight * 10000.0);
    return true;
}

void convert_camera_lens_mm_to_pixel_units(const int32_t image_width,
                                           const int32_t image_height,
                                           const double focal_length_mm,
                                           const double sensor_width_mm,
                                           double &focal_length_pix,
                                           double &ppx_pix, double &ppy_pix) {
    focal_length_pix = focal_length_mm / sensor_width_mm;
    focal_length_pix *= static_cast<double>(image_width);
    ppx_pix = static_cast<double>(image_width) * 0.5;
    ppy_pix = static_cast<double>(image_height) * 0.5;
    return;
}

// Prepare the corresponding 2D marker data.
openMVG::Mat convert_marker_coords_to_matrix(
    const std::vector<std::pair<double, double>> &marker_coords) {
    auto num = marker_coords.size();
    openMVG::Mat result(2, num);
    for (size_t k = 0; k < num; ++k) {
        auto coord = marker_coords[k];
        result.col(k) = openMVG::Vec2(std::get<0>(coord), std::get<1>(coord));
    }
    return result;
}

// Prepare the corresponding 3D bundle data.
openMVG::Mat convert_bundle_coords_to_matrix(
    const std::vector<std::tuple<double, double, double>> &bundle_coords) {
    auto num = bundle_coords.size();
    openMVG::Mat result(3, num);
    for (size_t k = 0; k < num; ++k) {
        auto coord = bundle_coords[k];
        result.col(k) = openMVG::Vec3(std::get<0>(coord), std::get<1>(coord),
                                      std::get<2>(coord));
    }
    return result;
}

MStatus parseCameraSelectionList(
    const MSelectionList &selection_list, const MTime &time, CameraPtr &camera,
    Attr &camera_tx_attr, Attr &camera_ty_attr, Attr &camera_tz_attr,
    Attr &camera_rx_attr, Attr &camera_ry_attr, Attr &camera_rz_attr,
    int32_t &image_width, int32_t &image_height, double &focal_length_mm,
    double &sensor_width_mm, double &sensor_height_mm) {
    MStatus status = MStatus::kSuccess;

    // Enable to print out 'MMSOLVER_VRB' results.
    const bool verbose = false;

    MDagPath nodeDagPath;
    MObject node_obj;

    if (selection_list.length() > 0) {
        status = selection_list.getDagPath(0, nodeDagPath);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        status = selection_list.getDependNode(0, node_obj);
        CHECK_MSTATUS_AND_RETURN_IT(status);

        MString transform_node_name = nodeDagPath.fullPathName();
        MMSOLVER_VRB("Camera name: " << transform_node_name.asChar());

        auto object_type = computeObjectType(node_obj, nodeDagPath);
        if (object_type == ObjectType::kCamera) {
            status = nodeDagPath.extendToShapeDirectlyBelow(0);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MString shape_node_name = nodeDagPath.fullPathName();

            camera = CameraPtr(new Camera());
            camera->setTransformNodeName(transform_node_name);
            camera->setShapeNodeName(shape_node_name);

            camera_tx_attr.setNodeName(transform_node_name);
            camera_ty_attr.setNodeName(transform_node_name);
            camera_tz_attr.setNodeName(transform_node_name);
            camera_rx_attr.setNodeName(transform_node_name);
            camera_ry_attr.setNodeName(transform_node_name);
            camera_rz_attr.setNodeName(transform_node_name);

            camera_tx_attr.setAttrName(MString("translateX"));
            camera_ty_attr.setAttrName(MString("translateY"));
            camera_tz_attr.setAttrName(MString("translateZ"));
            camera_rx_attr.setAttrName(MString("rotateX"));
            camera_ry_attr.setAttrName(MString("rotateY"));
            camera_rz_attr.setAttrName(MString("rotateZ"));

            status = get_camera_values(time, camera, image_width, image_height,
                                       focal_length_mm, sensor_width_mm,
                                       sensor_height_mm);
            CHECK_MSTATUS_AND_RETURN_IT(status);
        } else {
            MMSOLVER_ERR("Given node is not a valid camera: "
                         << transform_node_name.asChar());
            status = MS::kFailure;
            return status;
        }
    }

    return status;
}

MStatus parse_camera_argument(const MSelectionList &selection_list,
                              CameraPtr &camera, Attr &camera_tx_attr,
                              Attr &camera_ty_attr, Attr &camera_tz_attr,
                              Attr &camera_rx_attr, Attr &camera_ry_attr,
                              Attr &camera_rz_attr) {
    MStatus status = MStatus::kSuccess;

    // Enable to print out 'MMSOLVER_VRB' results.
    const bool verbose = false;

    MDagPath nodeDagPath;
    MObject node_obj;

    if (selection_list.length() > 0) {
        status = selection_list.getDagPath(0, nodeDagPath);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        status = selection_list.getDependNode(0, node_obj);
        CHECK_MSTATUS_AND_RETURN_IT(status);

        MString transform_node_name = nodeDagPath.fullPathName();
        MMSOLVER_VRB("Camera name: " << transform_node_name.asChar());

        auto object_type = computeObjectType(node_obj, nodeDagPath);
        if (object_type == ObjectType::kCamera) {
            status = nodeDagPath.extendToShapeDirectlyBelow(0);
            CHECK_MSTATUS_AND_RETURN_IT(status);
            MString shape_node_name = nodeDagPath.fullPathName();

            camera = CameraPtr(new Camera());
            camera->setTransformNodeName(transform_node_name);
            camera->setShapeNodeName(shape_node_name);

            camera_tx_attr.setNodeName(transform_node_name);
            camera_ty_attr.setNodeName(transform_node_name);
            camera_tz_attr.setNodeName(transform_node_name);
            camera_rx_attr.setNodeName(transform_node_name);
            camera_ry_attr.setNodeName(transform_node_name);
            camera_rz_attr.setNodeName(transform_node_name);

            camera_tx_attr.setAttrName(MString("translateX"));
            camera_ty_attr.setAttrName(MString("translateY"));
            camera_tz_attr.setAttrName(MString("translateZ"));
            camera_rx_attr.setAttrName(MString("rotateX"));
            camera_ry_attr.setAttrName(MString("rotateY"));
            camera_rz_attr.setAttrName(MString("rotateZ"));
        } else {
            MMSOLVER_ERR("Given node is not a valid camera: "
                         << transform_node_name.asChar());
            status = MS::kFailure;
            return status;
        }
    }

    return status;
}

bool add_marker_at_frame(
    const MTime &time, const int32_t image_width, const int32_t image_height,
    MarkerPtr &marker, std::vector<std::pair<double, double>> &marker_coords) {
    double x = 0.0;
    double y = 0.0;
    bool enable = true;
    double weight = 1.0;

    auto success =
        get_marker_coords_at_frame(time, marker, x, y, weight, enable);
    if (!success) {
        return success;
    }

    x = (x + 0.5) * static_cast<double>(image_width);
    y = (y + 0.5) * static_cast<double>(image_height);
    auto xy = std::pair<double, double>{x, y};
    marker_coords.push_back(xy);

    return success;
}

// Both markers in the pair must exist in order to be added as valid
// coordinates.
bool add_marker_pair_at_frame(
    const MTime &time_a, const MTime &time_b, const int32_t image_width_a,
    const int32_t image_width_b, const int32_t image_height_a,
    const int32_t image_height_b, MarkerPtr &marker_a, MarkerPtr &marker_b,
    std::vector<std::pair<double, double>> &marker_coords_a,
    std::vector<std::pair<double, double>> &marker_coords_b) {
    double x_a = 0.0;
    double x_b = 0.0;
    double y_a = 0.0;
    double y_b = 0.0;
    bool enable_a = true;
    bool enable_b = true;
    double weight_a = 1.0;
    double weight_b = 1.0;

    auto success_a = get_marker_coords_at_frame(time_a, marker_a, x_a, y_a,
                                                weight_a, enable_a);
    auto success_b = get_marker_coords_at_frame(time_b, marker_b, x_b, y_b,
                                                weight_b, enable_b);
    if (!success_a || !success_b) {
        return false;
    }

    x_a = (x_a + 0.5) * static_cast<double>(image_width_a);
    x_b = (x_b + 0.5) * static_cast<double>(image_width_b);
    y_a = (y_a + 0.5) * static_cast<double>(image_height_a);
    y_b = (y_b + 0.5) * static_cast<double>(image_height_b);
    auto xy_a = std::pair<double, double>{x_a, y_a};
    auto xy_b = std::pair<double, double>{x_b, y_b};
    marker_coords_a.push_back(xy_a);
    marker_coords_b.push_back(xy_b);

    return true;
}

bool add_bundle_at_frame(
    const MTime &time, BundlePtr &bundle,
    std::vector<std::tuple<double, double, double>> &bundle_coords) {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double weight = 1.0;

    auto success = get_bundle_coords_at_frame(time, bundle, x, y, z, weight);
    if (!success) {
        return success;
    }
    auto xyz = std::tuple<double, double, double>{x, y, z};
    bundle_coords.push_back(xyz);

    return success;
}

MTransformationMatrix convert_pose_to_maya_transform_matrix(
    openMVG::geometry::Pose3 &pose) {
    // Enable to print out 'MMSOLVER_VRB' results.
    const bool verbose = false;

    auto pose_center = pose.center();
    auto pose_translation = pose.translation();
    auto pose_rotation = pose.rotation();
    MMSOLVER_VRB("pose center: " << pose_center);
    MMSOLVER_VRB("pose translation: " << pose_translation);
    MMSOLVER_VRB("pose rotation: " << pose_rotation);

    // OpenMVG and Maya have different conventions for the camera Z axis:
    //
    // - In OpenMVG the camera points down +Z.
    //
    // - In Maya the camera points down -Z.
    //
    // The Camera Z axis is inverse scaled, therefore to correct
    // the OpenMVG data for Maya we must:
    //
    // - Invert Camera TX, RX and RY values.
    //
    // - Invert Bundle TZ value (see bundle section).
    MPoint maya_translate(pose_center[0], pose_center[1], -pose_center[2]);
    MVector maya_translate_vector(pose_center[0], pose_center[1],
                                  -pose_center[2]);
    const double c_rotate_matrix[4][4] = {
        // clang-format off
        {pose_rotation(0, 0),
         pose_rotation(0, 1),
         pose_rotation(0, 2),
         pose_rotation(0, 3)},
        {pose_rotation(1, 0),
         pose_rotation(1, 1),
         pose_rotation(1, 2),
         pose_rotation(1, 3)},
        {pose_rotation(2, 0),
         pose_rotation(2, 1),
         pose_rotation(2, 2),
         pose_rotation(2, 3)},
        {pose_rotation(3, 0),
         pose_rotation(3, 1),
         pose_rotation(3, 2),
         pose_rotation(3, 3)},
        // clang-format on
    };
    MMatrix maya_rotate_matrix(c_rotate_matrix);

    const auto rotate_order = MEulerRotation::kZXY;
    const auto transform_rotate_order = MTransformationMatrix::kZXY;

    const auto euler_rotation =
        MEulerRotation::decompose(maya_rotate_matrix, rotate_order);
    // Fixes the Camera +Z/-Z issue with Maya compared to OpenMVG.
    const auto rx = -euler_rotation.x * RADIANS_TO_DEGREES;
    const auto ry = -euler_rotation.y * RADIANS_TO_DEGREES;
    const auto rz = euler_rotation.z * RADIANS_TO_DEGREES;
    double rotate_radians[3] = {-euler_rotation.x, -euler_rotation.y,
                                euler_rotation.z};

    MMSOLVER_VRB("pose maya translation: " << maya_translate_vector.x << ","
                                           << maya_translate_vector.y << ","
                                           << maya_translate_vector.z);
    MMSOLVER_VRB("pose maya euler rotation (ZXY): " << rx << "," << ry << ","
                                                    << rz);

    // Convert back into matrix.
    MTransformationMatrix transform;
    transform.setRotation(rotate_radians, transform_rotate_order);
    transform.setTranslation(maya_translate_vector, MSpace::kWorld);

    return transform;
}

}  // namespace sfm
}  // namespace mmsolver
