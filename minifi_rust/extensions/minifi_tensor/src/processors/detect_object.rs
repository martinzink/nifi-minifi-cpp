// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

//! `DetectObject` collapses the ImageToTensor -> InvokeTractModel ->
//! FilterBoundingBoxes chain into a single processor. It decodes the image,
//! builds the input tensor, runs one inference through a `TractModelService`,
//! and post-processes the outputs into bounding boxes. The flow file content is
//! left unchanged (the original image); the boxes are written as a JSON array
//! to a configurable output attribute for a downstream `DrawBoundingBox`.
//!
//! This processor is intentionally self-contained: it reuses the shared enums,
//! the `BoundingBox` struct + NMS, and the controller service's `run_inference`,
//! but re-implements the resize/normalise and box-scoring helpers inline rather
//! than depending on the private internals of the individual processors.

mod processor_definition;

use crate::processors::filter_bounding_boxes::{BoxFormat, ScoreActivation, CONFIDENCE_THRESHOLD, IOU_THRESHOLD, SCORE_OUTPUT_INDEX, SCORE_ACTIVATION, BOX_FORMAT, BOX_OUTPUT_INDEX, BACKGROUND_CLASS_INDEX, OUTPUT_ATTRIBUTE_NAME};
use crate::processors::image_to_tensor::{ColorFormat, ResizeFilter, ResizeMode, TensorShapeFormat, TARGET_WIDTH, TARGET_HEIGHT, RESIZE_FILTER, RESIZE_MODE, COLOR_FORMAT, TENSOR_SHAPE_FORMAT, MEAN, STD_DEV, PIXEL_DIVISOR, LETTERBOX_PAD_VALUE};
use crate::utils::bounding_box::BoundingBox;
use minifi_native::error;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    FlowFileTransform, GetAttribute, GetControllerService, GetId, GetProperty, InputStream, Logger,
    MinifiError, Schedule, TransformedFlowFile, debug, unwrap_or_route,
};
use std::collections::HashMap;
use tract::__ndarray_interop::TensorInterface;
use tract::Tensor;
use crate::processors::detect_object::processor_definition::{FAILURE, SUCCESS};
use crate::processors::invoke_tract_model::TRACT_MODEL_SERVICE;

tract::impl_ndarray_interop!();

// ---------------------------------------------------------------------------
// Image normalisation helpers (mirrors ImageToTensor)
// ---------------------------------------------------------------------------

/// Parses a scalar or comma-separated triple into a per-channel Vec.
fn parse_per_channel_f32(input: &str) -> Result<Vec<f32>, String> {
    let parts: Vec<&str> = input.split(',').map(|s| s.trim()).collect();
    match parts.len() {
        1 | 3 => parts
            .iter()
            .map(|p| {
                p.parse::<f32>()
                    .map_err(|e| format!("invalid float '{}': {}", p, e))
            })
            .collect(),
        n => Err(format!("expected 1 or 3 comma-separated values, got {}", n)),
    }
}

/// Returns `values[c]` when the vector has per-channel entries, or `values[0]`
/// when it has a single entry (broadcast).
fn per_channel(values: &[f32], c: usize) -> f32 {
    if values.len() == 1 { values[0] } else { values[c] }
}

// ---------------------------------------------------------------------------
// Box post-processing helpers (mirrors FilterBoundingBoxes)
// ---------------------------------------------------------------------------

/// Convert the four floats at `box_floats[offset..offset+4]` into a canonical
/// `(x_min, y_min, x_max, y_max)` tuple, regardless of the source layout.
fn decode_box(box_floats: &[f32], offset: usize, format: BoxFormat) -> (f32, f32, f32, f32) {
    let a = box_floats[offset];
    let b = box_floats[offset + 1];
    let c = box_floats[offset + 2];
    let d = box_floats[offset + 3];
    match format {
        BoxFormat::Xyxy => (a, b, c, d),
        BoxFormat::Yxyx => (b, a, d, c),
        BoxFormat::Cxcywh => {
            let (cx, cy, w, h) = (a, b, c, d);
            (cx - w / 2.0, cy - h / 2.0, cx + w / 2.0, cy + h / 2.0)
        }
    }
}

/// Winning class id + confidence for one box's per-class scores.
struct ScoredClass {
    class_id: usize,
    confidence: f32,
}

/// Pick the winning class for one box's per-class scores, applying the chosen
/// activation and honouring the background-class filter.
fn score_box(
    logits: &[f32],
    activation: ScoreActivation,
    background_class_index: Option<usize>,
) -> ScoredClass {
    let should_skip = |class_id: usize, num_classes: usize| -> bool {
        match background_class_index {
            Some(idx) => num_classes > 1 && class_id == idx,
            None => false,
        }
    };

    match activation {
        ScoreActivation::Softmax => {
            let max_logit = logits.iter().cloned().fold(f32::NEG_INFINITY, f32::max);
            let sum_exp: f32 = logits.iter().map(|&l| (l - max_logit).exp()).sum();

            let mut best = ScoredClass {
                class_id: 0,
                confidence: 0.0,
            };
            for (class_id, &logit) in logits.iter().enumerate() {
                if should_skip(class_id, logits.len()) {
                    continue;
                }
                let prob = (logit - max_logit).exp() / sum_exp;
                if prob > best.confidence {
                    best = ScoredClass {
                        class_id,
                        confidence: prob,
                    };
                }
            }
            best
        }
        ScoreActivation::Sigmoid => {
            let mut best = ScoredClass {
                class_id: 0,
                confidence: 0.0,
            };
            for (class_id, &logit) in logits.iter().enumerate() {
                if should_skip(class_id, logits.len()) {
                    continue;
                }
                let prob = 1.0 / (1.0 + (-logit).exp());
                if prob > best.confidence {
                    best = ScoredClass {
                        class_id,
                        confidence: prob,
                    };
                }
            }
            best
        }
        ScoreActivation::None => {
            let mut best = ScoredClass {
                class_id: 0,
                confidence: f32::NEG_INFINITY,
            };
            for (class_id, &score) in logits.iter().enumerate() {
                if should_skip(class_id, logits.len()) {
                    continue;
                }
                if score > best.confidence {
                    best = ScoredClass {
                        class_id,
                        confidence: score,
                    };
                }
            }
            best
        }
    }
}

fn bytes_to_f32s(bytes: &[u8]) -> Vec<f32> {
    bytes
        .chunks_exact(4)
        .map(|c| f32::from_le_bytes(c.try_into().unwrap()))
        .collect()
}

// ---------------------------------------------------------------------------
// Processor
// ---------------------------------------------------------------------------

#[derive(ComponentIdentifier)]
pub(crate) struct DetectObject {
    // Image pre-processing
    target_width: u32,
    target_height: u32,
    resize_filter: ResizeFilter,
    resize_mode: ResizeMode,
    color_format: ColorFormat,
    tensor_shape_format: TensorShapeFormat,
    mean: Vec<f32>,
    std_dev: Vec<f32>,
    pixel_divisor: f32,
    letterbox_pad_value: f32,
    // Post-processing
    confidence_threshold: f32,
    iou_threshold: f32,
    score_output_index: usize,
    box_output_index: usize,
    box_format: BoxFormat,
    score_activation: ScoreActivation,
    /// `None` disables background suppression.
    background_class_index: Option<usize>,
}

impl Schedule for DetectObject {
    fn schedule<Ctx: GetProperty, L: Logger>(
        context: &Ctx,
        _logger: &L,
    ) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let target_width = context.get_property(&TARGET_WIDTH)?;
        let target_height = context.get_property(&TARGET_HEIGHT)?;
        let resize_filter = context.get_property(&RESIZE_FILTER)?;
        let resize_mode = context.get_property(&RESIZE_MODE)?;
        let color_format = context.get_property(&COLOR_FORMAT)?;
        let tensor_shape_format =
            context.get_property(&TENSOR_SHAPE_FORMAT)?;
        let mean = context.get_property(&MEAN)?;
        let std_dev = context.get_property(&STD_DEV)?;
        if std_dev.contains(&0.0) {
            return Err(MinifiError::trigger_err(
                "Standard Deviation components must be non-zero",
            ));
        }
        let pixel_divisor = context.get_property(&PIXEL_DIVISOR)?;
        if pixel_divisor == 0.0 {
            return Err(MinifiError::trigger_err("Pixel divisor must be non-zero"));
        }
        let letterbox_pad_value = context.get_property(&LETTERBOX_PAD_VALUE)?;

        let confidence_threshold = context.get_property(&CONFIDENCE_THRESHOLD)?;
        let iou_threshold = context.get_property(&IOU_THRESHOLD)?;
        let score_output_index = context.get_property(&SCORE_OUTPUT_INDEX)?;
        let box_output_index = context.get_property(&BOX_OUTPUT_INDEX)?;
        let box_format = context.get_property(&BOX_FORMAT)?;
        let score_activation = context.get_property(&SCORE_ACTIVATION)?;
        // `-1` (or any negative) turns off background suppression.
        let background_class_index = context.get_property(&BACKGROUND_CLASS_INDEX)?;

        Ok(Self {
            target_width,
            target_height,
            resize_filter,
            resize_mode,
            color_format,
            tensor_shape_format,
            mean,
            std_dev,
            pixel_divisor,
            letterbox_pad_value,
            confidence_threshold,
            iou_threshold,
            score_output_index,
            box_output_index,
            box_format,
            score_activation,
            background_class_index,
        })
    }
}

impl DetectObject {
    /// Resize `img` into a (target_width, target_height) RGB image according to
    /// the configured resize mode. Returns the resized image plus a mask of
    /// "valid" pixels (`true` = source, `false` = letterbox padding).
    fn resize_rgb(&self, img: &image::DynamicImage) -> (image::RgbImage, Vec<bool>) {
        let filter: image::imageops::FilterType = self.resize_filter.into();
        match self.resize_mode {
            ResizeMode::Stretch => {
                let resized = img
                    .resize_exact(self.target_width, self.target_height, filter)
                    .to_rgb8();
                let mask = vec![true; (self.target_width * self.target_height) as usize];
                (resized, mask)
            }
            ResizeMode::Letterbox => {
                let (src_w, src_h) = (img.width() as f32, img.height() as f32);
                let scale =
                    (self.target_width as f32 / src_w).min(self.target_height as f32 / src_h);
                let new_w = (src_w * scale).round().max(1.0) as u32;
                let new_h = (src_h * scale).round().max(1.0) as u32;
                let scaled = img.resize_exact(new_w, new_h, filter).to_rgb8();

                let pad_x = (self.target_width - new_w) / 2;
                let pad_y = (self.target_height - new_h) / 2;

                let mut canvas = image::RgbImage::from_pixel(
                    self.target_width,
                    self.target_height,
                    image::Rgb([0, 0, 0]),
                );
                image::imageops::overlay(&mut canvas, &scaled, pad_x as i64, pad_y as i64);

                let mut mask = vec![false; (self.target_width * self.target_height) as usize];
                for y in pad_y..(new_h + pad_y) {
                    for x in pad_x..(new_w + pad_x) {
                        mask[(y * self.target_width + x) as usize] = true;
                    }
                }
                (canvas, mask)
            }
        }
    }

    /// Decode/resize/normalise `img` into a flat f32 buffer and its tensor shape
    /// (`[1, C, H, W]` / `[1, H, W, C]` / `[1, 1, H, W]` for grayscale).
    fn build_input_data(&self, img: &image::DynamicImage) -> (Vec<f32>, Vec<usize>) {
        let num_channels: usize = match self.color_format {
            ColorFormat::Grayscale => 1,
            _ => 3,
        };
        let total_pixels = (self.target_width * self.target_height) as usize;
        let mut data = Vec::with_capacity(total_pixels * num_channels);

        let (rgb_img, mask) = self.resize_rgb(img);

        if self.color_format == ColorFormat::Grayscale {
            let mean = self.mean[0];
            let std_dev = self.std_dev[0];
            for (idx, pixel) in rgb_img.pixels().enumerate() {
                let val = if mask[idx] {
                    // Rec. 601 luma coefficients — matches image::to_luma8().
                    let luma =
                        0.299 * pixel[0] as f32 + 0.587 * pixel[1] as f32 + 0.114 * pixel[2] as f32;
                    (luma / self.pixel_divisor - mean) / std_dev
                } else {
                    self.letterbox_pad_value
                };
                data.push(val);
            }
        } else {
            let channel_order: [usize; 3] = match self.color_format {
                ColorFormat::Rgb => [0, 1, 2],
                ColorFormat::Bgr => [2, 1, 0],
                _ => unreachable!(),
            };

            let normalized = |px_idx: usize, out_c: usize, src_c: usize| -> f32 {
                if !mask[px_idx] {
                    return self.letterbox_pad_value;
                }
                let raw = rgb_img.get_pixel(
                    (px_idx as u32) % self.target_width,
                    (px_idx as u32) / self.target_width,
                )[src_c] as f32;
                (raw / self.pixel_divisor - per_channel(&self.mean, out_c))
                    / per_channel(&self.std_dev, out_c)
            };

            match self.tensor_shape_format {
                TensorShapeFormat::Chw => {
                    for (out_c, &src_c) in channel_order.iter().enumerate() {
                        for px_idx in 0..total_pixels {
                            data.push(normalized(px_idx, out_c, src_c));
                        }
                    }
                }
                TensorShapeFormat::Hwc => {
                    for px_idx in 0..total_pixels {
                        for (out_c, &src_c) in channel_order.iter().enumerate() {
                            data.push(normalized(px_idx, out_c, src_c));
                        }
                    }
                }
            }
        }

        let shape: Vec<usize> = match (self.color_format, self.tensor_shape_format) {
            (ColorFormat::Grayscale, _) => {
                vec![1, 1, self.target_height as usize, self.target_width as usize]
            }
            (_, TensorShapeFormat::Chw) => vec![
                1,
                num_channels,
                self.target_height as usize,
                self.target_width as usize,
            ],
            (_, TensorShapeFormat::Hwc) => vec![
                1,
                self.target_height as usize,
                self.target_width as usize,
                num_channels,
            ],
        };

        (data, shape)
    }

    /// Extract the flat f32 values of the output tensor at `index`.
    fn output_f32s(output_tensors: &[Tensor], index: usize) -> Result<Vec<f32>, MinifiError> {
        let tensor = output_tensors.get(index).ok_or_else(|| {
            MinifiError::trigger_err(format!(
                "Model produced {} output tensor(s); output index {} is out of range",
                output_tensors.len(),
                index
            ))
        })?;
        let (_datum_type, _shape, raw_bytes) = tensor
            .as_bytes()
            .map_err(|e| MinifiError::trigger_err(format!("Failed to read tensor bytes: {}", e)))?;
        Ok(bytes_to_f32s(raw_bytes))
    }
}

impl FlowFileTransform for DetectObject {
    fn transform<
        'a,
        Context: GetProperty + GetControllerService + GetAttribute + GetId,
        LoggerImpl: Logger,
    >(
        &self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, MinifiError> {
        // The controller service is resolved before we consume the input so a
        // misconfiguration surfaces as an error rather than a decode failure.
        let controller_service = context
            .get_controller_service(&TRACT_MODEL_SERVICE)?;

        // Read the original image bytes; content is left unchanged downstream.
        let mut raw_bytes = Vec::new();
        input_stream.read_to_end(&mut raw_bytes)?;

        let img = unwrap_or_route!(
            image::load_from_memory(&raw_bytes),
            &FAILURE,
            logger,
            "decode image"
        );
        // Original dimensions come straight from the decoded image, so no
        // attribute round-trip is needed to reverse the letterbox transform.
        let (orig_w, orig_h) = (img.width() as f32, img.height() as f32);

        // Pre-process -> input tensor.
        let (data, shape) = self.build_input_data(&img);
        let input_tensor: Tensor = unwrap_or_route!(
            ndarray::Array::from_shape_vec(shape, data)
                .map_err(|e| MinifiError::trigger_err(format!("Failed to create array: {}", e)))
                .and_then(|array| array
                    .tract()
                    .map_err(|e| MinifiError::trigger_err(format!("Failed to build tensor: {}", e)))),
            &FAILURE,
            logger,
            "build input tensor"
        );

        // Inference.
        let output_tensors = unwrap_or_route!(
            controller_service.run_inference(vec![input_tensor]),
            &FAILURE,
            logger,
            "run tract inference"
        );

        let score_floats = unwrap_or_route!(
            Self::output_f32s(&output_tensors, self.score_output_index),
            &FAILURE,
            logger,
            "read score output"
        );
        let box_floats = unwrap_or_route!(
            Self::output_f32s(&output_tensors, self.box_output_index),
            &FAILURE,
            logger,
            "read box output"
        );

        // Letterbox geometry for reversing the pre-processing transform.
        let target_w = self.target_width as f32;
        let target_h = self.target_height as f32;
        let scale = (target_w / orig_w).min(target_h / orig_h);
        let pad_x = (target_w - (orig_w * scale)) / 2.0;
        let pad_y = (target_h - (orig_h * scale)) / 2.0;

        if box_floats.len() % 4 != 0 {
            return Err(MinifiError::trigger_err(
                "Box tensor byte length is not a multiple of 16 (4 f32 per box)",
            ));
        }
        let num_boxes = box_floats.len() / 4;

        let filtered_boxes = if num_boxes == 0 {
            debug!(logger, "Model produced no boxes");
            Vec::new()
        } else {
            if score_floats.len() % num_boxes != 0 {
                return Err(MinifiError::trigger_err(format!(
                    "Scores length ({}) not divisible by number of boxes ({})",
                    score_floats.len(),
                    num_boxes
                )));
            }
            let num_classes = score_floats.len() / num_boxes;

            debug!(
                logger,
                "Filtering {} boxes across {} classes (activation={:?}, box_format={:?})",
                num_boxes,
                num_classes,
                self.score_activation,
                self.box_format
            );

            let mut valid_boxes = Vec::new();
            for i in 0..num_boxes {
                let logits = &score_floats[i * num_classes..(i + 1) * num_classes];
                let scored = score_box(logits, self.score_activation, self.background_class_index);
                if scored.confidence >= self.confidence_threshold {
                    let (raw_x_min, raw_y_min, raw_x_max, raw_y_max) =
                        decode_box(&box_floats, i * 4, self.box_format);

                    let true_x_min = (((raw_x_min * target_w) - pad_x) / scale) / orig_w;
                    let true_y_min = (((raw_y_min * target_h) - pad_y) / scale) / orig_h;
                    let true_x_max = (((raw_x_max * target_w) - pad_x) / scale) / orig_w;
                    let true_y_max = (((raw_y_max * target_h) - pad_y) / scale) / orig_h;

                    valid_boxes.push(BoundingBox {
                        class_id: scored.class_id,
                        confidence: scored.confidence,
                        x_min: true_x_min.clamp(0.0, 1.0),
                        y_min: true_y_min.clamp(0.0, 1.0),
                        x_max: true_x_max.clamp(0.0, 1.0),
                        y_max: true_y_max.clamp(0.0, 1.0),
                    });
                }
            }
            BoundingBox::apply_non_maximum_suppression(valid_boxes, self.iou_threshold)
        };

        debug!(
            logger,
            "Detected {} object(s) after NMS",
            filtered_boxes.len()
        );

        let boxes_json = unwrap_or_route!(
            serde_json::to_string(&filtered_boxes),
            &FAILURE,
            logger,
            "serialize json"
        );

        let mut attributes = HashMap::new();
        attributes.insert("object.count".to_string(), filtered_boxes.len().to_string());

        match context.get_property(&OUTPUT_ATTRIBUTE_NAME)? {
            None => {
                Ok(TransformedFlowFile::new(&SUCCESS, Some(boxes_json.as_bytes().to_owned()), attributes))
            }
            Some(output_attr) => {
                attributes.insert(output_attr, boxes_json);
                Ok(TransformedFlowFile::new(&SUCCESS, None, attributes))
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use minifi_native::{MockLogger, MockProcessContext};
    use std::io::Cursor;

    fn default_processor() -> DetectObject {
        DetectObject {
            target_width: 2,
            target_height: 2,
            resize_filter: ResizeFilter::Nearest,
            resize_mode: ResizeMode::Stretch,
            color_format: ColorFormat::Rgb,
            tensor_shape_format: TensorShapeFormat::Chw,
            mean: vec![0.0],
            std_dev: vec![255.0],
            pixel_divisor: 1.0,
            letterbox_pad_value: 0.0,
            confidence_threshold: 0.7,
            iou_threshold: 0.45,
            score_output_index: 0,
            box_output_index: 1,
            box_format: BoxFormat::Xyxy,
            score_activation: ScoreActivation::Softmax,
            background_class_index: Some(0),
        }
    }

    // Note: the happy path (decode -> inference -> boxes) requires a compiled
    // model owned by a live TractModelService, which the mock context cannot
    // hold (it needs a real tract Runnable). That path is exercised end-to-end
    // via `cargo behave` / the demo flow. Here we cover the inlined helpers and
    // the missing-service guard.

    #[test]
    fn test_missing_controller_service_errors() {
        let processor = default_processor();
        let context = MockProcessContext::new();
        let mut input_stream = Cursor::new(vec![]);

        let result = processor.transform(&context, &mut input_stream, &MockLogger::new());
        assert!(
            result.is_err(),
            "Should error when TractModelService is missing"
        );
    }

    #[test]
    fn test_build_input_data_rgb_chw_shape_and_values() {
        // 2x2 image, white at (0,0), black at (1,1). mean=0 std=255 -> [0,1].
        let processor = default_processor();
        let mut img = image::RgbImage::new(2, 2);
        img.put_pixel(0, 0, image::Rgb([255, 255, 255]));
        img.put_pixel(1, 1, image::Rgb([0, 0, 0]));
        let dyn_img = image::DynamicImage::ImageRgb8(img);

        let (data, shape) = processor.build_input_data(&dyn_img);
        assert_eq!(shape, vec![1, 3, 2, 2]);
        assert_eq!(data.len(), 12);
        // CHW: first value is R of pixel (0,0) = white -> 1.0.
        assert!((data[0] - 1.0).abs() < 1e-6);
    }

    #[test]
    fn test_decode_box_cxcywh_converts_to_corners() {
        let raw = [100.0, 200.0, 20.0, 40.0];
        let (x0, y0, x1, y1) = decode_box(&raw, 0, BoxFormat::Cxcywh);
        assert!((x0 - 90.0).abs() < 1e-6);
        assert!((y0 - 180.0).abs() < 1e-6);
        assert!((x1 - 110.0).abs() < 1e-6);
        assert!((y1 - 220.0).abs() < 1e-6);
    }

    #[test]
    fn test_score_box_softmax_excludes_background() {
        let logits = [5.0, 0.5, 2.0];
        let scored = score_box(&logits, ScoreActivation::Softmax, Some(0));
        assert_eq!(scored.class_id, 2);
        assert!(scored.confidence > 0.0 && scored.confidence < 1.0);
    }

    #[test]
    fn test_parse_per_channel_f32_accepts_scalar_and_triple() {
        assert_eq!(parse_per_channel_f32("0.5").unwrap(), vec![0.5]);
        assert_eq!(
            parse_per_channel_f32("0.485, 0.456, 0.406").unwrap(),
            vec![0.485, 0.456, 0.406]
        );
        assert!(parse_per_channel_f32("1, 2").is_err());
    }
}
