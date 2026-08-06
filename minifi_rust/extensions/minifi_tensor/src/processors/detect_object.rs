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
//! but re-implements the resize/normalize and box-scoring helpers inline rather
//! than depending on the private internals of the individual processors.

mod processor_definition;

use crate::processors::detect_object::processor_definition::FAILURE;
use crate::processors::filter_bounding_boxes::FilterBoundingBoxes;
use crate::processors::image_to_tensor::ImageToTensor;
use crate::processors::invoke_tract_model::TRACT_MODEL_SERVICE;
use crate::utils::dimensions::Dimensions;
use minifi_native::error;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    unwrap_or_route, FlowFileTransform, GetAttribute, GetControllerService, GetId, GetProperty, InputStream,
    Logger, MinifiError, Schedule, TransformedFlowFile,
};
use tract::Tensor;

tract::impl_ndarray_interop!();

// ---------------------------------------------------------------------------
// Box post-processing helpers (mirrors FilterBoundingBoxes)
// ---------------------------------------------------------------------------

#[derive(ComponentIdentifier)]
pub(crate) struct DetectObject {
    image_to_tensor: ImageToTensor,
    filter_bounding_boxes: FilterBoundingBoxes,
}

impl Schedule for DetectObject {
    fn schedule<Ctx: GetProperty, L: Logger>(context: &Ctx, logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let image_to_tensor = ImageToTensor::schedule(context, logger)?;
        let filter_bounding_boxes = FilterBoundingBoxes::schedule(context, logger)?;
        Ok(Self {
            image_to_tensor,
            filter_bounding_boxes,
        })
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
        let tract_model_service = context.get_controller_service(&TRACT_MODEL_SERVICE)?;

        let mut raw_bytes = Vec::new();
        input_stream.read_to_end(&mut raw_bytes)?;

        let img = unwrap_or_route!(
            image::load_from_memory(&raw_bytes),
            &FAILURE,
            logger,
            "decode image"
        );
        let input_tensor: Tensor = unwrap_or_route!(
            self.image_to_tensor.get_tensor(&img),
            &FAILURE,
            logger,
            "build input tensor"
        );
        let output_tensors = unwrap_or_route!(
            tract_model_service.run_inference(vec![input_tensor]),
            &FAILURE,
            logger,
            "run tract inference"
        );

        let score_floats = self
            .filter_bounding_boxes
            .get_score_floats(&output_tensors)?;
        let box_floats = self.filter_bounding_boxes.get_box_floats(&output_tensors)?;

        let orig_dim = Dimensions {
            width: img.width() as f32,
            height: img.height() as f32,
        };
        let target_dim = self.image_to_tensor.get_target_dim();

        self.filter_bounding_boxes.filter(
            context,
            logger,
            &score_floats,
            &box_floats,
            orig_dim,
            target_dim,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use minifi_native::{MockLogger, MockProcessContext};
    use std::io::Cursor;
    use crate::processors::image_to_tensor::{PIXEL_DIVISOR, TARGET_HEIGHT, TARGET_WIDTH};

    #[test]
    fn test_missing_controller_service_errors() {
        let mut context = MockProcessContext::new();
        context.properties.insert(TARGET_HEIGHT.name(), "100");
        context.properties.insert(TARGET_WIDTH.name(), "100");
        context.properties.insert(PIXEL_DIVISOR.name(), "1.0");
        let processor = DetectObject::schedule(&context, &MockLogger::new())
            .expect("Expected to schedule");
        let mut input_stream = Cursor::new(vec![]);

        let result = processor.transform(&context, &mut input_stream, &MockLogger::new());
        assert!(
            result.is_err(),
            "Should error when TractModelService is missing"
        );
    }
}
