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
 * Class for the 3DE classic lens distortion model.
 */

#include "lens_model_3de_classic.h"

// STL
#include <functional>

// MM Solver
#include "mmSolver/core/mmhash.h"

void LensModel3deClassic::applyModelUndistort(const double xd, const double yd,
                                              double &xu, double &yu) {
    if (m_state != LensModelState::kClean) {
        // LDPK models must be initialized to work.
        m_lensPlugin->setParameterValue("tde4_focal_length_cm",
                                        LensModel::m_focalLength_cm);
        m_lensPlugin->setParameterValue("tde4_filmback_width_cm",
                                        LensModel::m_filmBackWidth_cm);
        m_lensPlugin->setParameterValue("tde4_filmback_height_cm",
                                        LensModel::m_filmBackHeight_cm);
        m_lensPlugin->setParameterValue("tde4_pixel_aspect",
                                        LensModel::m_pixelAspect);
        m_lensPlugin->setParameterValue("tde4_lens_center_offset_x_cm",
                                        LensModel::m_lensCenterOffsetX_cm);
        m_lensPlugin->setParameterValue("tde4_lens_center_offset_y_cm",
                                        LensModel::m_lensCenterOffsetY_cm);

        m_lensPlugin->setParameterValue("Distortion", m_distortion);
        m_lensPlugin->setParameterValue("Anamorphic Squeeze",
                                        m_anamorphicSqueeze);
        m_lensPlugin->setParameterValue("Curvature X", m_curvatureX);
        m_lensPlugin->setParameterValue("Curvature Y", m_curvatureY);
        m_lensPlugin->setParameterValue("Quartic Distortion",
                                        m_quarticDistortion);

        m_lensPlugin->initializeParameters();
        m_state = LensModelState::kClean;
    }

    // Apply the 'previous' lens model in the chain.
    std::shared_ptr<LensModel> inputLensModel = LensModel::getInputLensModel();
    double xdd = xd;
    double ydd = yd;
    if (inputLensModel != nullptr) {
        inputLensModel->applyModelUndistort(xdd, ydd, xdd, ydd);
    }

    // 'undistort' expects values 0.0 to 1.0, but our inputs are -0.5
    // to 0.5, therefore we must convert.
    m_lensPlugin->undistort(xdd + 0.5, ydd + 0.5, xu, yu);
    xu -= 0.5;
    yu -= 0.5;
    return;
}

void LensModel3deClassic::applyModelDistort(const double xd, const double yd,
                                            double &xu, double &yu) {
    if (m_state != LensModelState::kClean) {
        // LDPK models must be initialized to work.
        m_lensPlugin->setParameterValue("tde4_focal_length_cm",
                                        LensModel::m_focalLength_cm);
        m_lensPlugin->setParameterValue("tde4_filmback_width_cm",
                                        LensModel::m_filmBackWidth_cm);
        m_lensPlugin->setParameterValue("tde4_filmback_height_cm",
                                        LensModel::m_filmBackHeight_cm);
        m_lensPlugin->setParameterValue("tde4_pixel_aspect",
                                        LensModel::m_pixelAspect);
        m_lensPlugin->setParameterValue("tde4_lens_center_offset_x_cm",
                                        LensModel::m_lensCenterOffsetX_cm);
        m_lensPlugin->setParameterValue("tde4_lens_center_offset_y_cm",
                                        LensModel::m_lensCenterOffsetY_cm);

        m_lensPlugin->setParameterValue("Distortion", m_distortion);
        m_lensPlugin->setParameterValue("Anamorphic Squeeze",
                                        m_anamorphicSqueeze);
        m_lensPlugin->setParameterValue("Curvature X", m_curvatureX);
        m_lensPlugin->setParameterValue("Curvature Y", m_curvatureY);
        m_lensPlugin->setParameterValue("Quartic Distortion",
                                        m_quarticDistortion);

        m_lensPlugin->initializeParameters();
        m_state = LensModelState::kClean;
    }

    // Apply the 'previous' lens model in the chain.
    std::shared_ptr<LensModel> inputLensModel = LensModel::getInputLensModel();
    double xdd = xd;
    double ydd = yd;
    if (inputLensModel != nullptr) {
        inputLensModel->applyModelDistort(xdd, ydd, xdd, ydd);
    }

    // 'undistort' expects values 0.0 to 1.0, but our inputs are -0.5
    // to 0.5, therefore we must convert.
    m_lensPlugin->distort(xdd + 0.5, ydd + 0.5, xu, yu);
    xu -= 0.5;
    yu -= 0.5;
    return;
}

mmhash::HashValue LensModel3deClassic::hashValue() {
    // Apply the 'previous' lens model in the chain.
    std::shared_ptr<LensModel> inputLensModel = LensModel::getInputLensModel();
    mmhash::HashValue hash = 0;
    if (inputLensModel != nullptr) {
        hash = inputLensModel->hashValue();
    }

    mmhash::combine(hash, std::hash<double>()(LensModel::m_focalLength_cm));
    mmhash::combine(hash, std::hash<double>()(LensModel::m_filmBackWidth_cm));
    mmhash::combine(hash, std::hash<double>()(LensModel::m_filmBackHeight_cm));
    mmhash::combine(hash, std::hash<double>()(LensModel::m_pixelAspect));
    mmhash::combine(hash,
                    std::hash<double>()(LensModel::m_lensCenterOffsetX_cm));
    mmhash::combine(hash,
                    std::hash<double>()(LensModel::m_lensCenterOffsetY_cm));

    mmhash::combine(hash, std::hash<double>()(m_distortion));
    mmhash::combine(hash, std::hash<double>()(m_anamorphicSqueeze));
    mmhash::combine(hash, std::hash<double>()(m_curvatureX));
    mmhash::combine(hash, std::hash<double>()(m_curvatureY));
    mmhash::combine(hash, std::hash<double>()(m_quarticDistortion));

    return hash;
}
