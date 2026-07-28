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

use crate::processors::detect_object::DetectObject;
use crate::processors::filter_bounding_boxes::{BoxFormat, ScoreActivation};
use crate::processors::image_to_tensor::{
    ColorFormat, ResizeFilter, ResizeMode, TensorShapeFormat,
};
use crate::services::tract_model_service::TractModelService;
use minifi_native::PropertyConstraints::{AllowedType, AllowedValues, NoConstraints, Validator};
use minifi_native::{
    ComponentIdentifier, OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property,
    Relationship, StandardPropertyValidator,
};
use strum::VariantNames;

// --- Image pre-processing (mirrors ImageToTensor) ---------------------------

pub(super) const TARGET_WIDTH: Property = Property {
    name: "Target width",
    description: "Width in pixels the decoded image is resized to before normalisation and \
                  inference.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    constraints: Validator(StandardPropertyValidator::U64Validator),
};

pub(super) const TARGET_HEIGHT: Property = Property {
    name: "Target height",
    description: "Height in pixels the decoded image is resized to before normalisation and \
                  inference.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    constraints: Validator(StandardPropertyValidator::U64Validator),
};

pub(super) const RESIZE_FILTER: Property = Property {
    name: "Resize filter",
    description: "Interpolation filter applied when resizing the decoded image. Nearest is fastest \
                  but blocky; Bilinear is a good default; Bicubic and Lanczos3 are higher-quality \
                  but slower.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(ResizeFilter::Bilinear.into_str()),
    constraints: AllowedValues(ResizeFilter::VARIANTS),
};

pub(super) const RESIZE_MODE: Property = Property {
    name: "Resize mode",
    description: "How the source image is fitted into the target dimensions. 'Stretch' scales each \
                  axis independently, distorting aspect ratio. 'Letterbox' preserves aspect ratio \
                  and pads the remaining border with 'Letterbox pad value' (applied in normalised \
                  output space).",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(ResizeMode::Stretch.into_str()),
    constraints: AllowedValues(ResizeMode::VARIANTS),
};

pub(super) const LETTERBOX_PAD_VALUE: Property = Property {
    name: "Letterbox pad value",
    description: "Value written for padding pixels when 'Resize mode' is 'Letterbox'. This is a \
                  normalised value (post mean/std), so 0.0 corresponds to a neutral input for most \
                  networks. Ignored when 'Resize mode' is 'Stretch'.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("0.0"),
    constraints: NoConstraints,
};

pub(super) const COLOR_FORMAT: Property = Property {
    name: "Color format",
    description: "Colour space of the tensor fed to the model. RGB and BGR produce three-channel \
                  tensors (channel order determined by the format); Grayscale produces a \
                  single-channel luma tensor.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(ColorFormat::Rgb.into_str()),
    constraints: AllowedValues(ColorFormat::VARIANTS),
};

pub(super) const TENSOR_SHAPE_FORMAT: Property = Property {
    name: "Tensor shape format",
    description: "Memory layout of the tensor fed to the model. CHW (channels-first) is typical \
                  for PyTorch/ONNX detectors. HWC (channels-last) matches TensorFlow/TFLite. \
                  Ignored for Grayscale (always effectively 1xHxW).",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(TensorShapeFormat::Chw.into_str()),
    constraints: AllowedValues(TensorShapeFormat::VARIANTS),
};

pub(super) const MEAN: Property = Property {
    name: "Mean",
    description: "Mean subtracted from each pixel before dividing by 'Standard Deviation'. Accepts \
                  either a single value (broadcast to all channels) or three comma-separated \
                  values applied per channel in the order dictated by 'Color format'. Example: \
                  '0.485, 0.456, 0.406' for ImageNet-style RGB normalisation.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("0.0"),
    constraints: NoConstraints,
};

pub(super) const STD_DEV: Property = Property {
    name: "Standard Deviation",
    description: "Divisor applied after subtracting 'Mean'. Accepts a single value (broadcast) or \
                  three comma-separated values (per channel). Must be non-zero. Example: '255.0' \
                  to scale u8 pixels into [0.0, 1.0]; '0.229, 0.224, 0.225' for ImageNet.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("255.0"),
    constraints: NoConstraints,
};

pub(super) const PIXEL_DIVISOR: Property = Property {
    name: "Pixel divisor",
    description: "Divisor applied to raw u8 pixel values before subtracting 'Mean' and dividing \
                  by 'Standard Deviation'. Defaults to 1.0 (mean/std interpreted in [0, 255] pixel \
                  space, e.g. UltraFace's mean=127, std=128). Set to 255 to bring pixels into \
                  [0.0, 1.0] first so ImageNet-style mean/std values can be used directly. Must be \
                  non-zero.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("1.0"),
    constraints: NoConstraints,
};

// --- Inference (mirrors InvokeTractModel) -----------------------------------

pub(super) const TRACT_MODEL_SERVICE: Property = Property {
    name: "Tract model service",
    description: "Reference to a TractModelService controller service. The referenced service \
                  owns the compiled model (ONNX or NNEF) that will be evaluated for each \
                  incoming flow file.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    constraints: AllowedType(TractModelService::CLASS_NAME),
};

// --- Post-processing (mirrors FilterBoundingBoxes) --------------------------

pub(super) const CONFIDENCE_THRESHOLD: Property = Property {
    name: "Confidence Threshold",
    description: "Minimum per-box class probability (0.0 to 1.0) required to keep a bounding box \
                  after applying the chosen 'Score activation'. Boxes below the threshold are \
                  discarded before NMS.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: true,
    default_value: Some("0.7"),
    constraints: NoConstraints,
};

pub(super) const IOU_THRESHOLD: Property = Property {
    name: "IoU Threshold",
    description: "Intersection-over-union cutoff used during non-maximum suppression. Boxes of \
                  the same class whose IoU with a higher-confidence peer exceeds this value are \
                  suppressed. Typical values: 0.45 (SSD/YOLO default), 0.5, 0.3 for stricter \
                  deduplication.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("0.45"),
    constraints: NoConstraints,
};

pub(super) const SCORE_OUTPUT_INDEX: Property = Property {
    name: "Score output index",
    description: "Zero-based index of the model output tensor that holds classification scores.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("0"),
    constraints: Validator(StandardPropertyValidator::U64Validator),
};

pub(super) const BOX_OUTPUT_INDEX: Property = Property {
    name: "Box output index",
    description: "Zero-based index of the model output tensor that holds box coordinates. Must \
                  differ from 'Score output index'.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("1"),
    constraints: Validator(StandardPropertyValidator::U64Validator),
};

pub(super) const BOX_FORMAT: Property = Property {
    name: "Box format",
    description: "Layout of the four floats per box in the box output tensor. Xyxy = \
                  [x_min, y_min, x_max, y_max] (SSD, MobileNet-SSD, most PyTorch exports). \
                  Yxyx = [y_min, x_min, y_max, x_max] (TensorFlow Object Detection API). \
                  Cxcywh = [cx, cy, w, h] (YOLOv3/5/8 raw output).",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(BoxFormat::Xyxy.into_str()),
    constraints: AllowedValues(BoxFormat::VARIANTS),
};

pub(super) const SCORE_ACTIVATION: Property = Property {
    name: "Score activation",
    description: "Activation applied to raw per-class scores before selecting the winning class. \
                  Softmax = mutually-exclusive classes (SSD/MobileNet-SSD raw logits). \
                  Sigmoid = independent classes (YOLOv5/v8 style). \
                  None = the model already emits probabilities/scores; use raw argmax with the \
                  raw score as confidence.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(ScoreActivation::Softmax.into_str()),
    constraints: AllowedValues(ScoreActivation::VARIANTS),
};

pub(super) const BACKGROUND_CLASS_INDEX: Property = Property {
    name: "Background class index",
    description: "Index of the 'background / no-object' class in the score vector. Boxes whose \
                  winning class equals this index are dropped. Set to a negative value (e.g. -1) \
                  to disable background suppression for models where every class is a real \
                  object (YOLO, person-detector heads, etc.). Only honoured when the score \
                  tensor has more than one class per box.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("0"),
    constraints: Validator(StandardPropertyValidator::I64Validator),
};

pub(super) const OUTPUT_ATTRIBUTE_NAME: Property = Property {
    name: "Output attribute name",
    description: "Name of the flow file attribute the detected bounding boxes are written to, as \
                  a JSON array. The flow file content is left unchanged (the original image), so a \
                  downstream processor such as DrawBoundingBox can reference this attribute (e.g. \
                  '${detected_objects}') to annotate the image.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: true,
    default_value: Some("detected_objects"),
    constraints: NoConstraints,
};

// --- Relationships / output attributes --------------------------------------

pub(super) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "Inference and post-processing completed. The flow file content is the original, \
                  unchanged image; the detected boxes are written to the configured output \
                  attribute as a JSON array (may be empty).",
};

pub(super) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "The image could not be decoded, the input tensor could not be built, the model \
                  failed to run, or the model outputs could not be interpreted as scores + boxes.",
};

const OBJECT_COUNT_ATTR: OutputAttribute = OutputAttribute {
    name: "object.count",
    relationships: &["success"],
    description: "Number of bounding boxes retained after confidence filtering and NMS.",
};

const DETECTED_OBJECTS_ATTR: OutputAttribute = OutputAttribute {
    name: "<Output attribute name>",
    relationships: &["success"],
    description: "JSON array of the surviving bounding boxes (fields class_id, confidence, x_min, \
                  y_min, x_max, y_max; coordinates normalised to [0,1] against the original \
                  image). The attribute name is configurable via the 'Output attribute name' \
                  property.",
};

impl ProcessorDefinition for DetectObject {
    const DESCRIPTION: &'static str = "Runs a full object-detection pass in a single processor: decodes the image from the flow \
         file content, resizes and normalises it into an input tensor, runs one inference against \
         the compiled model owned by the referenced TractModelService, and post-processes the \
         model outputs (score activation, confidence filtering, box decoding, per-class \
         non-maximum suppression) into bounding boxes. Collapses the ImageToTensor -> \
         InvokeTractModel -> FilterBoundingBoxes chain into one node. The flow file content is \
         left unchanged (the original image); the detected boxes are written as a JSON array to \
         the configured output attribute so a downstream DrawBoundingBox can annotate the image.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] =
        &[OBJECT_COUNT_ATTR, DETECTED_OBJECTS_ATTR];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];
    const PROPERTIES: &'static [Property] = &[
        TARGET_WIDTH,
        TARGET_HEIGHT,
        RESIZE_FILTER,
        RESIZE_MODE,
        LETTERBOX_PAD_VALUE,
        COLOR_FORMAT,
        TENSOR_SHAPE_FORMAT,
        MEAN,
        STD_DEV,
        PIXEL_DIVISOR,
        TRACT_MODEL_SERVICE,
        CONFIDENCE_THRESHOLD,
        IOU_THRESHOLD,
        SCORE_OUTPUT_INDEX,
        BOX_OUTPUT_INDEX,
        BOX_FORMAT,
        SCORE_ACTIVATION,
        BACKGROUND_CLASS_INDEX,
        OUTPUT_ATTRIBUTE_NAME,
    ];
}
