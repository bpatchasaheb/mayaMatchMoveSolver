/*
 * Copyright (C) 2018, 2019 David Cattermole.
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
 * Command for running mmSolver.
 */

#ifndef MM_SOLVER_COMMON_ARG_FLAGS_H
#define MM_SOLVER_COMMON_ARG_FLAGS_H

#include "MMSolverCmd.h"

// STL
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>

// Maya
#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>

// Internal Objects
#include "mmSolver/adjust/adjust_base.h"
#include "mmSolver/adjust/adjust_data.h"
#include "mmSolver/mayahelper/maya_attr.h"
#include "mmSolver/mayahelper/maya_bundle.h"
#include "mmSolver/mayahelper/maya_camera.h"
#include "mmSolver/mayahelper/maya_marker.h"
#include "mmSolver/mayahelper/maya_utils.h"
#include "mmSolver/utilities/debug_utils.h"

// Command arguments
#define CAMERA_FLAG "-c"
#define CAMERA_FLAG_LONG "-camera"

#define MARKER_FLAG "-m"
#define MARKER_FLAG_LONG "-marker"

#define ATTR_FLAG "-a"
#define ATTR_FLAG_LONG "-attr"

#define FRAME_FLAG "-f"
#define FRAME_FLAG_LONG "-frame"

// Attribute (Detail) Stiffness
#define STIFFNESS_FLAG "-asf"
#define STIFFNESS_FLAG_LONG "-attrStiffness"

// Attribute (Detail) Smoothness
#define SMOOTHNESS_FLAG "-asm"
#define SMOOTHNESS_FLAG_LONG "-attrSmoothness"

// The Scene Graph used for evaluation.
#define SCENE_GRAPH_MODE_FLAG "-sgm"
#define SCENE_GRAPH_MODE_FLAG_LONG "-sceneGraphMode"

namespace mmsolver {

/*
 * Add flags for solver objects to the command syntax.
 */
void createSolveObjectSyntax(MSyntax &syntax);

/*
 * Parse arguments into solver objects.
 */
MStatus parseSolveObjectArguments(const MArgDatabase &argData,
                                  CameraPtrList &out_cameraList,
                                  MarkerPtrList &out_markerList,
                                  BundlePtrList &out_bundleList,
                                  AttrPtrList &out_attrList);

/*
 * Add flags for attribute details to the command syntax.
 */
void createAttributeDetailsSyntax(MSyntax &syntax);

/*
 * Parse arguments into attribute details.
 */
MStatus parseAttributeDetailsArguments(const MArgDatabase &argData,
                                       const AttrPtrList &attrList,
                                       StiffAttrsPtrList &out_stiffAttrsList,
                                       SmoothAttrsPtrList &out_smoothAttrsList);

/*
 * Add flags for solver frames to the command syntax.
 */
void createSolveFramesSyntax(MSyntax &syntax);

/*
 * Parse arguments into solver frames.
 */
MStatus parseSolveFramesArguments(const MArgDatabase &argData,
                                  MTimeArray &out_frameList);

/*
 * Add flags for solver scene graph to the command syntax.
 */
void createSolveSceneGraphSyntax(MSyntax &syntax);

/*
 * Parse arguments into solver scene graph.
 */
MStatus parseSolveSceneGraphArguments(const MArgDatabase &argData,
                                      SceneGraphMode &out_sceneGraphMode);

}  // namespace mmsolver

#endif  // MM_SOLVER_COMMON_ARG_FLAGS_H
