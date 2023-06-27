//
// Copyright (C) 2023 David Cattermole.
//
// This file is part of mmSolver.
//
// mmSolver is free software: you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// mmSolver is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with mmSolver.  If not, see <https://www.gnu.org/licenses/>.
// ====================================================================
//

use crate::constants::PARAMETER_COUNT_3DE_ANAMORPHIC_STD_DEG4;
use crate::constants::PARAMETER_COUNT_3DE_ANAMORPHIC_STD_DEG4_RESCALED;
use crate::constants::PARAMETER_COUNT_3DE_CLASSIC;
use crate::constants::PARAMETER_COUNT_3DE_RADIAL_STD_DEG4;
use crate::constants::STATIC_FRAME_NUMBER;
use crate::cxxbridge::ffi::CameraParameters as BindCameraParameters;
use crate::cxxbridge::ffi::LensModelType as BindLensModelType;
use crate::cxxbridge::ffi::OptionParameters3deClassic as BindOptionParameters3deClassic;
use crate::data::FrameNumber;
use crate::data::FrameSize;
use crate::data::HashValue64;
use crate::data::LayerIndex;
use crate::data::LayerSize;
use crate::data::ParameterBlock;
use crate::data::ParameterIndex;
use crate::data::ParameterSize;
use crate::hash_float::HashableF64;
use rustc_hash::FxHasher;
use smallvec::SmallVec;
use std::collections::HashMap;
use std::convert::TryInto;
use std::hash::Hash;
use std::hash::Hasher;

// use crate::constants::MAX_LENS_PARAMETER_COUNT;
// use crate::cxxbridge::ffi::OptionCameraParameters as BindOptionCameraParameters;
// use crate::cxxbridge::ffi::OptionParameters3deAnamorphicStdDeg4 as BindOptionParameters3deAnamorphicStdDeg4;
// use crate::cxxbridge::ffi::OptionParameters3deAnamorphicStdDeg4Rescaled as BindOptionParameters3deAnamorphicStdDeg4Rescaled;
// use crate::cxxbridge::ffi::OptionParameters3deRadialStdDeg4 as BindOptionParameters3deRadialStdDeg4;

impl BindLensModelType {
    fn parameter_size(&self) -> ParameterSize {
        match *self {
            BindLensModelType::TdeClassic => PARAMETER_COUNT_3DE_CLASSIC,
            BindLensModelType::TdeRadialStdDeg4 => {
                PARAMETER_COUNT_3DE_RADIAL_STD_DEG4
            }
            BindLensModelType::TdeAnamorphicStdDeg4 => {
                PARAMETER_COUNT_3DE_ANAMORPHIC_STD_DEG4
            }
            BindLensModelType::TdeAnamorphicStdDeg4Rescaled => {
                PARAMETER_COUNT_3DE_ANAMORPHIC_STD_DEG4_RESCALED
            }
            _ => 0,
        }
    }
}

fn set_parameter_block_values(
    layer_num: LayerIndex,
    frame_number: FrameNumber,
    lens_model_type: BindLensModelType,
    lens_parameters: &HashMap<(LayerIndex, FrameNumber), ParameterBlock>,
    output_values: &mut [f64],
) {
    assert!(output_values.len() > 0);
    match lens_parameters.get(&(layer_num, frame_number)) {
        Some(input_values) => match lens_model_type {
            BindLensModelType::TdeClassic => {
                output_values[0] = input_values[0];
                output_values[1] = input_values[1];
                output_values[2] = input_values[2];
                output_values[3] = input_values[3];
                output_values[4] = input_values[4];
            }
            BindLensModelType::TdeRadialStdDeg4 => {
                output_values[0] = input_values[0];
                output_values[1] = input_values[1];
                output_values[2] = input_values[2];
                output_values[3] = input_values[3];
                output_values[4] = input_values[4];
                output_values[5] = input_values[5];
                output_values[6] = input_values[6];
                output_values[7] = input_values[7];
            }
            BindLensModelType::TdeAnamorphicStdDeg4 => {
                output_values[0] = input_values[0];
                output_values[1] = input_values[1];
                output_values[2] = input_values[2];
                output_values[3] = input_values[3];
                output_values[4] = input_values[4];
                output_values[5] = input_values[5];
                output_values[6] = input_values[6];
                output_values[7] = input_values[7];
                output_values[8] = input_values[8];
                output_values[9] = input_values[9];
                output_values[10] = input_values[10];
                output_values[11] = input_values[11];
                output_values[12] = input_values[12];
            }
            BindLensModelType::TdeAnamorphicStdDeg4Rescaled => {
                output_values[0] = input_values[0];
                output_values[1] = input_values[1];
                output_values[2] = input_values[2];
                output_values[3] = input_values[3];
                output_values[4] = input_values[4];
                output_values[5] = input_values[5];
                output_values[6] = input_values[6];
                output_values[7] = input_values[7];
                output_values[8] = input_values[8];
                output_values[9] = input_values[9];
                output_values[10] = input_values[10];
                output_values[11] = input_values[11];
                output_values[12] = input_values[12];
                output_values[13] = input_values[13];
            }
            _ => (),
        },
        None => {
            panic!("Could not find frame number in lens_parameters HashMap: layer_num={} frame_number={}.",
                   layer_num, frame_number)
        }
    };
}

fn lens_frame_count(
    layer_num: LayerIndex,
    layer_frame_range: &SmallVec<[(FrameNumber, FrameNumber); 4]>,
) -> FrameSize {
    let (start_frame, end_frame) = layer_frame_range[layer_num as usize];
    let is_static =
        start_frame == STATIC_FRAME_NUMBER || end_frame == STATIC_FRAME_NUMBER;

    let frame_count = match is_static {
        true => 1,
        false => (end_frame - start_frame) + 1,
    };

    frame_count
}

fn count_parameters(
    layer_count: LayerSize,
    layer_frame_range: &SmallVec<[(FrameNumber, FrameNumber); 4]>,
    layer_lens_model_types: &SmallVec<[BindLensModelType; 4]>,
) -> (usize, usize) {
    // Count up all the parameters that will be used.
    let mut total_parameter_count: usize = 0;
    let mut total_parameter_value_count: usize = 0;
    for layer_num in 0..layer_count {
        let frame_count = lens_frame_count(layer_num, &layer_frame_range);

        let lens_model_type = layer_lens_model_types[layer_num as usize];
        let parameter_size = lens_model_type.parameter_size();

        total_parameter_count += frame_count as usize;
        total_parameter_value_count +=
            parameter_size as usize * frame_count as usize;
    }
    (total_parameter_count, total_parameter_value_count)
}

/// Represents "Layers" of (lens) distortion data.
///
/// The layers are conceptually stacked top-to-bottom, so the zeroth
/// Layer is computed, and then index 1, index 2, etc. These layers
/// are ordered for removing distortion ("undistortion") from the
/// input points. If you need to apply distortion to an undistorted
/// point, you must reverse the order (start at N-1, then N-2, etc.)
///
/// This data structure is intended for optimized lookup of
/// precomputed read-only camera and lens distortion parameter
/// data.
#[derive(Debug, Clone, PartialEq, PartialOrd)]
pub struct ShimDistortionLayers {
    // The lens_count is assumed to be most often 1, sometimes 2, and
    // in rare cases 3. Values of 4 or above do not seem practical.
    layer_count: LayerSize,

    // If there are no animated parameters, the start or end
    // FrameNumbers will be STATIC_FRAME_NUMBER.
    layer_lens_model_types: SmallVec<[BindLensModelType; 4]>,
    layer_frame_range: SmallVec<[(FrameNumber, FrameNumber); 4]>,

    // Camera parameters are assumed to be static because the film
    // back is the only real values that contribute to the lens
    // distortion result.
    camera_parameters: BindCameraParameters,

    // Indices can be stored as pairs of indices into the
    // parameter block, and the count of parameters. Since we know
    // the maximum number of parameters is less than 255, we can
    // use a u8 to save memory.
    parameter_indices: Vec<(ParameterIndex, ParameterSize)>,

    // A big block of parameter values that is indexed into.
    parameter_block: Vec<f64>,
}

pub fn shim_create_distortion_layers_box() -> Box<ShimDistortionLayers> {
    Box::new(ShimDistortionLayers::new())
}

impl ShimDistortionLayers {
    pub fn new() -> ShimDistortionLayers {
        return ShimDistortionLayers {
            layer_count: 0,
            layer_lens_model_types: SmallVec::new(),
            layer_frame_range: SmallVec::new(),
            camera_parameters: BindCameraParameters::default(),
            parameter_indices: Vec::new(),
            parameter_block: Vec::new(),
        };
    }

    // Construct the DistortionLayers from the various parts that can
    // be read from a file.
    pub fn from_parts(
        layer_count: LayerSize,
        layer_lens_model_types: &SmallVec<[BindLensModelType; 4]>,
        layer_frame_range: &SmallVec<[(FrameNumber, FrameNumber); 4]>,
        camera_parameters: BindCameraParameters,
        lens_parameters: &HashMap<(LayerIndex, FrameNumber), ParameterBlock>,
    ) -> ShimDistortionLayers {
        if layer_count == 0 {
            return ShimDistortionLayers::new();
        }

        // Check the validity of the input data.
        for layer_num in 0..layer_count {
            if usize::from(layer_num) >= layer_frame_range.len() {
                panic!("Distortion Layer {}: layer_frame_range does not have valid value.", layer_num);
            }
            if usize::from(layer_num) >= layer_lens_model_types.len() {
                panic!("Distortion Layer {}: layer_lens_model_types does not have valid value.", layer_num);
            }
            let lens_model_type = layer_lens_model_types[layer_num as usize];
            if lens_model_type == BindLensModelType::Uninitialized {
                panic!(
                    "Distortion Layer {}: lens model type is not initialized.",
                    layer_num
                );
            }
        }

        // Count up all the parameters that will be used.
        let (total_parameter_count, total_parameter_value_count) =
            count_parameters(
                layer_count,
                &layer_frame_range,
                &layer_lens_model_types,
            );

        let mut parameter_block = vec![0.0 as f64; total_parameter_value_count];
        let mut parameter_indices =
            vec![
                (0 as ParameterIndex, 0 as ParameterSize);
                total_parameter_count
            ];

        let mut parameter_num: usize = 0;
        let mut layer_parameter_index: usize = 0;
        for layer_num in 0..layer_count {
            let frame_count = lens_frame_count(layer_num, &layer_frame_range);
            let is_static = frame_count == 1;

            let lens_model_type = layer_lens_model_types[layer_num as usize];
            let parameter_size = lens_model_type.parameter_size();

            let mut index_end: usize = 0;
            if is_static {
                let index_start: usize = layer_parameter_index;
                index_end = index_start + parameter_size as usize;

                let frame_number = STATIC_FRAME_NUMBER;
                let (_, output_values) =
                    parameter_block.split_at_mut(index_start);
                set_parameter_block_values(
                    layer_num,
                    frame_number,
                    lens_model_type,
                    &lens_parameters,
                    output_values,
                );

                parameter_indices[parameter_num] = (
                    index_start.try_into().unwrap(),
                    parameter_size.try_into().unwrap(),
                );
                parameter_num += 1;
            } else {
                let mut frame_index: usize = 0;
                let (start_frame, end_frame) =
                    layer_frame_range[layer_num as usize];
                for frame_number in start_frame..=end_frame {
                    let index_start: usize = layer_parameter_index
                        + (parameter_size as usize * frame_index as usize);
                    index_end = index_start + parameter_size as usize;

                    let (_, output_values) =
                        parameter_block.split_at_mut(index_start);
                    set_parameter_block_values(
                        layer_num,
                        frame_number,
                        lens_model_type,
                        &lens_parameters,
                        output_values,
                    );

                    parameter_indices[parameter_num] = (
                        index_start.try_into().unwrap(),
                        parameter_size.try_into().unwrap(),
                    );
                    parameter_num += 1;
                    frame_index += 1;
                }
            }

            layer_parameter_index = index_end;
        }

        ShimDistortionLayers {
            layer_count,
            layer_lens_model_types: layer_lens_model_types.clone(),
            layer_frame_range: layer_frame_range.clone(),
            camera_parameters,
            parameter_indices: parameter_indices,
            parameter_block: parameter_block,
        }
    }

    pub fn is_static(&self) -> bool {
        for layer_num in 0..self.layer_count {
            let frame_count =
                lens_frame_count(layer_num, &self.layer_frame_range);
            if frame_count != 1 {
                return false;
            }
        }
        true
    }

    pub fn frame_range(
        &self,
        out_start_frame: &mut FrameNumber,
        out_end_frame: &mut FrameNumber,
    ) {
        if self.is_static() {
            *out_start_frame = STATIC_FRAME_NUMBER;
            *out_end_frame = STATIC_FRAME_NUMBER;
        } else {
            let mut min_frame = FrameNumber::MAX;
            let mut max_frame = FrameNumber::MIN;
            for layer_num in 0..(self.layer_count as usize) {
                let (start_frame, end_frame) =
                    self.layer_frame_range[layer_num as usize];
                if start_frame != end_frame {
                    min_frame = std::cmp::min(min_frame, end_frame);
                    max_frame = std::cmp::max(max_frame, end_frame);
                }
            }
            *out_start_frame = max_frame;
            *out_end_frame = min_frame;
        }
    }

    pub fn frame_count(&self) -> FrameSize {
        let mut start_frame = STATIC_FRAME_NUMBER;
        let mut end_frame = STATIC_FRAME_NUMBER;
        self.frame_range(&mut start_frame, &mut end_frame);
        (end_frame - start_frame) + 1
    }

    /// Calculate the hash for the parameters for the current frame.
    pub fn frame_hash(&self, frame: FrameNumber) -> HashValue64 {
        let mut s = FxHasher::default();
        self.get_layer_count().hash(&mut s);

        // Camera parameters
        let camera_parameters = self.camera_parameters();
        let focal_length_cm =
            HashableF64::new(camera_parameters.focal_length_cm);
        let film_back_width_cm =
            HashableF64::new(camera_parameters.film_back_width_cm);
        let film_back_height_cm =
            HashableF64::new(camera_parameters.film_back_height_cm);
        let pixel_aspect = HashableF64::new(camera_parameters.pixel_aspect);
        let lens_center_offset_x_cm =
            HashableF64::new(camera_parameters.lens_center_offset_x_cm);
        let lens_center_offset_y_cm =
            HashableF64::new(camera_parameters.lens_center_offset_y_cm);
        focal_length_cm.hash(&mut s);
        film_back_width_cm.hash(&mut s);
        film_back_height_cm.hash(&mut s);
        pixel_aspect.hash(&mut s);
        lens_center_offset_x_cm.hash(&mut s);
        lens_center_offset_y_cm.hash(&mut s);

        // Lens parameters.
        let layer_count = self.get_layer_count();
        for layer_num in 0..layer_count {
            let lens_model_type = self.layer_lens_model_type(layer_num);
            match lens_model_type {
                BindLensModelType::Uninitialized => {
                    // This lens model is a no-op, nothing is done.
                    ()
                }
                BindLensModelType::TdeClassic => {
                    let option_parameters =
                        self.layer_parameters_3de_classic(layer_num, frame);
                    assert!(option_parameters.exists == true);
                    let parameters = option_parameters.value;

                    let distortion = HashableF64::new(parameters.distortion);
                    let anamorphic_squeeze =
                        HashableF64::new(parameters.anamorphic_squeeze);
                    let curvature_x = HashableF64::new(parameters.curvature_x);
                    let curvature_y = HashableF64::new(parameters.curvature_y);
                    let quartic_distortion =
                        HashableF64::new(parameters.quartic_distortion);

                    lens_model_type.hash(&mut s);
                    distortion.hash(&mut s);
                    anamorphic_squeeze.hash(&mut s);
                    curvature_x.hash(&mut s);
                    curvature_y.hash(&mut s);
                    quartic_distortion.hash(&mut s);
                }
                _ => {
                    panic!("Unsupported Lens Model Type: {:?}", lens_model_type)
                }
            }
        }

        s.finish()
        // camera_parameters::hash()
    }

    pub fn camera_parameters(&self) -> BindCameraParameters {
        self.camera_parameters
    }

    pub fn get_layer_count(&self) -> u8 {
        self.layer_count
    }

    pub fn layer_lens_model_type(&self, layer_num: u8) -> BindLensModelType {
        if layer_num >= self.layer_count {
            BindLensModelType::Uninitialized
        } else {
            self.layer_lens_model_types[layer_num as usize]
        }
    }

    /// When a 'frame' outside the frame range is requested, the
    /// returned values come from the first or last frame.
    pub fn layer_parameters_3de_classic(
        &self,
        layer_num: u8,
        frame: FrameNumber,
    ) -> BindOptionParameters3deClassic {
        println!(
            "layer_parameters_3de_classic: layer_num={} frame={}",
            layer_num, frame
        );
        if layer_num >= self.layer_count {
            println!("layer_parameters_3de_classic: 1");
            return BindOptionParameters3deClassic::new_as_none();
        }
        let index = layer_num as usize;
        if self.layer_lens_model_types[index] != BindLensModelType::TdeClassic {
            println!("layer_parameters_3de_classic: 2");
            return BindOptionParameters3deClassic::new_as_none();
        }

        let (start_frame, _end_frame) =
            self.layer_frame_range[layer_num as usize];
        let frame_count = lens_frame_count(layer_num, &self.layer_frame_range);

        let mut frame_index: i32 = 0;
        if frame_count > 1 {
            frame_index = frame as i32 - start_frame as i32;
        }
        frame_index = std::cmp::max(frame_index, 0);
        frame_index = std::cmp::min(frame_index, (frame_count - 1) as i32);

        let (_, parameter_value_count) = count_parameters(
            layer_num,
            &self.layer_frame_range,
            &self.layer_lens_model_types,
        );
        println!(
            "layer_parameters_3de_classic: parameter_value_count: {}",
            parameter_value_count
        );

        let index: usize = parameter_value_count + frame_index as usize;
        println!("layer_parameters_3de_classic: index: {}", index);

        let (parameter_index, parameter_size) = self.parameter_indices[index];
        println!(
            "layer_parameters_3de_classic: parameter_index: {}",
            parameter_index
        );
        println!(
            "layer_parameters_3de_classic: parameter_size: {}",
            parameter_size
        );

        let index_start = parameter_index as usize;
        let index_end = parameter_index as usize + parameter_size as usize;
        let values = &self.parameter_block[index_start..index_end];

        let distortion = values[0];
        let anamorphic_squeeze = values[1];
        let curvature_x = values[2];
        let curvature_y = values[3];
        let quartic_distortion = values[4];
        BindOptionParameters3deClassic::new_as_some(
            distortion,
            anamorphic_squeeze,
            curvature_x,
            curvature_y,
            quartic_distortion,
        )
    }

    pub fn as_string(&self) -> String {
        format!("{:#?}", self).to_string()
    }
}