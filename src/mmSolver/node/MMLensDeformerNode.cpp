/*
 * Copyright (C) 2020 David Cattermole.
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
 * Lens Distortion Deformer.
 *
 * To create this node using MEL, select deformable objects and run:
 *
 *   deformer -type mmLensDeformer
 *
 */

#include "MMLensDeformerNode.h"

// Maya
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>

// MM Solver
#include "MMLensData.h"
#include "mmSolver/nodeTypeIds.h"
#include "mmSolver/utilities/debug_utils.h"
#include "mmSolver/utilities/number_utils.h"

namespace mmsolver {

MTypeId MMLensDeformerNode::m_id(MM_LENS_DEFORMER_TYPE_ID);

MObject MMLensDeformerNode::a_inLens;
MObject MMLensDeformerNode::a_focalLength;
MObject MMLensDeformerNode::a_horizontalFilmAperture;
MObject MMLensDeformerNode::a_verticalFilmAperture;
MObject MMLensDeformerNode::a_pixelAspect;
MObject MMLensDeformerNode::a_horizontalFilmOffset;
MObject MMLensDeformerNode::a_verticalFilmOffset;


MMLensDeformerNode::MMLensDeformerNode() {}

MMLensDeformerNode::~MMLensDeformerNode() {}

void* MMLensDeformerNode::creator() {
    return new MMLensDeformerNode();
}

MStatus MMLensDeformerNode::initialize() {
    MStatus status = MS::kSuccess;
    MFnNumericAttribute numericAttr;
    MFnTypedAttribute typedAttr;

    // In Lens
    MTypeId data_type_id(MM_LENS_DATA_TYPE_ID);
    a_inLens = typedAttr.create(
        "inLens", "ilns",
        data_type_id);
    CHECK_MSTATUS(typedAttr.setStorable(false));
    CHECK_MSTATUS(typedAttr.setKeyable(false));
    CHECK_MSTATUS(typedAttr.setReadable(true));
    CHECK_MSTATUS(typedAttr.setWritable(true));

    a_focalLength = numericAttr.create(
        "focalLength", "fl",
        MFnNumericData::kDouble, 35.0, &status);
    CHECK_MSTATUS(numericAttr.setStorable(true));
    CHECK_MSTATUS(numericAttr.setKeyable(true));

    a_horizontalFilmAperture = numericAttr.create(
        "horizontalFilmAperture", "fbkw",
        MFnNumericData::kDouble, 36.0 * MM_TO_INCH, &status);
    CHECK_MSTATUS(numericAttr.setStorable(true));
    CHECK_MSTATUS(numericAttr.setKeyable(true));

    a_verticalFilmAperture = numericAttr.create(
        "verticalFilmAperture", "fbkh",
        MFnNumericData::kDouble, 24.0 * MM_TO_INCH, &status);
    CHECK_MSTATUS(numericAttr.setStorable(true));
    CHECK_MSTATUS(numericAttr.setKeyable(true));

    a_pixelAspect = numericAttr.create(
        "pixelAspect", "pxasp",
        MFnNumericData::kDouble, 1.0, &status);
    CHECK_MSTATUS(numericAttr.setStorable(true));
    CHECK_MSTATUS(numericAttr.setKeyable(true));

    a_horizontalFilmOffset = numericAttr.create(
        "horizontalFilmOffset", "lcox",
        MFnNumericData::kDouble, 0.0, &status);
    CHECK_MSTATUS(numericAttr.setStorable(true));
    CHECK_MSTATUS(numericAttr.setKeyable(true));

    a_verticalFilmOffset = numericAttr.create(
        "verticalFilmOffset", "lcoy",
        MFnNumericData::kDouble, 0.0, &status);
    CHECK_MSTATUS(numericAttr.setStorable(true));
    CHECK_MSTATUS(numericAttr.setKeyable(true));

    CHECK_MSTATUS(addAttribute(a_inLens));
    CHECK_MSTATUS(addAttribute(a_focalLength));
    CHECK_MSTATUS(addAttribute(a_horizontalFilmAperture));
    CHECK_MSTATUS(addAttribute(a_verticalFilmAperture));
    CHECK_MSTATUS(addAttribute(a_pixelAspect));
    CHECK_MSTATUS(addAttribute(a_horizontalFilmOffset));
    CHECK_MSTATUS(addAttribute(a_verticalFilmOffset));

    CHECK_MSTATUS(attributeAffects(a_inLens, outputGeom));
    CHECK_MSTATUS(attributeAffects(a_focalLength, outputGeom));
    CHECK_MSTATUS(attributeAffects(a_horizontalFilmAperture, outputGeom));
    CHECK_MSTATUS(attributeAffects(a_verticalFilmAperture, outputGeom));
    CHECK_MSTATUS(attributeAffects(a_pixelAspect, outputGeom));
    CHECK_MSTATUS(attributeAffects(a_horizontalFilmOffset, outputGeom));
    CHECK_MSTATUS(attributeAffects(a_verticalFilmOffset, outputGeom));

    return MS::kSuccess;
}

MString MMLensDeformerNode::nodeName() {
    return MString("mmLensDeformer");
}

// Linear interpolation function, aka 'mix' function
inline
double lerp(double a, double b, double x) {
    return ((1-x) * a) + (x * b);
}

MStatus
MMLensDeformerNode::deform(MDataBlock& data,
                           MItGeometry& iter,
                           const MMatrix& /*m*/,
                           unsigned int /*multiIndex*/) {
//
// Description:   Deform the point with a MMLensDeformer algorithm
//
// Arguments:
//   data		: the datablock of the node
//	 iter		: an iterator for the geometry to be deformed
//   m          : matrix to transform the point into world space
//	 multiIndex : the index of the geometry that we are deforming
//
    MStatus status = MS::kSuccess;

    // Query the envelope (the global multiplier factor for the
    // deformer)
    MDataHandle envData = data.inputValue(envelope, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    float env = envData.asFloat();
    if (env <= 0.0) {
        return status;
    }

    // Get Input Lens
    MDataHandle inLensHandle = data.inputValue(a_inLens, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MMLensData* inputLensData = (MMLensData*) inLensHandle.asPluginData();
    if (inputLensData == nullptr) {
        return status;
    }

    // Get the underlying lens model.
    std::shared_ptr<LensModel> lensModel = inputLensData->getValue();
    if (lensModel == nullptr) {
        return status;
    }

    MDataHandle focalLengthHandle = data.inputValue(a_focalLength, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle horizontalFilmApertureHandle = data.inputValue(a_horizontalFilmAperture, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle verticalFilmApertureHandle = data.inputValue(a_verticalFilmAperture, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle pixelAspectHandle = data.inputValue(a_pixelAspect, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle horizontalFilmOffsetHandle = data.inputValue(a_horizontalFilmOffset, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MDataHandle verticalFilmOffsetHandle = data.inputValue(a_verticalFilmOffset, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    double focalLength = focalLengthHandle.asDouble();
    double filmBackWidth = horizontalFilmApertureHandle.asDouble();
    double filmBackHeight = verticalFilmApertureHandle.asDouble();
    double pixelAspect = pixelAspectHandle.asDouble();
    double lensCenterOffsetX = horizontalFilmOffsetHandle.asDouble();
    double lensCenterOffsetY = verticalFilmOffsetHandle.asDouble();
    focalLength *= MM_TO_INCH;
    filmBackWidth *= INCH_TO_CM;
    filmBackHeight *= INCH_TO_CM;
    lensCenterOffsetX *= INCH_TO_CM;
    lensCenterOffsetY *= INCH_TO_CM;

    lensModel->setFocalLength(focalLength);
    lensModel->setFilmBackWidth(filmBackWidth);
    lensModel->setFilmBackHeight(filmBackHeight);
    lensModel->setPixelAspect(pixelAspect);
    lensModel->setLensCenterOffsetX(lensCenterOffsetX);
    lensModel->setLensCenterOffsetY(lensCenterOffsetY);

    lensModel->initModel();

    // Deform each point on the input geometry.
    for ( ; !iter.isDone(); iter.next()) {
        MPoint pt = iter.position();

        // Evaluate the lens distortion at (pt.x, pt.y).
        double out_x = pt.x;
        double out_y = pt.y;
        lensModel->applyModelUndistort(pt.x, pt.y, out_x, out_y);

        pt.x = lerp(pt.x, out_x, env);
        pt.y = lerp(pt.y, out_y, env);
        iter.setPosition(pt);
    }

    return status;
}

} // namespace mmsolver
