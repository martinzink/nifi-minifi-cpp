# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

@SUPPORTS_WINDOWS
Feature: End-to-end face detection with the single DetectObject processor

  # DetectObject collapses ImageToTensor -> InvokeTractModel ->
  # FilterBoundingBoxes into one node: it decodes the image, builds the input
  # tensor, runs inference through the TractModelService, and post-processes the
  # outputs into bounding boxes. The flow file content is left unchanged (the
  # original JPEG), and the boxes are written as a JSON array to a configurable
  # attribute. A downstream DrawBoundingBox reads that attribute and annotates
  # the untouched image, so no ForkEnrichment/JoinEnrichmentAttributes plumbing
  # is needed to carry the original image past the inference stage.
  Scenario: Grace Hopper image yields at least one face detection in a single processor
    Given a host resource file "grace_hopper.jpg" is copied to the "/tmp/input/grace_hopper.jpg" path in the MiNiFi container
    And a host resource file "version-RFB-320.onnx" is copied to the "/tmp/models/ultraface.onnx" path in the MiNiFi container

    And a TractModelService controller service named "UltraFace" is set up and the "Model File Path" property set to "/tmp/models/ultraface.onnx"
    And the "Model format" property of the UltraFace controller service is set to "Onnx"

    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "false"

    # All of ImageToTensor's + InvokeTractModel's + FilterBoundingBoxes' knobs
    # live on this one processor. UltraFace RFB-320 expects 320x240 (WxH) RGB
    # with mean=127, std=128; aspect-preserving letterbox keeps Grace Hopper's
    # face geometry intact instead of stretching it.
    And a DetectObject processor with the "Target width" property set to "320"
    And the "Target height" property of the DetectObject processor is set to "240"
    And the "Resize filter" property of the DetectObject processor is set to "Bilinear"
    And the "Resize mode" property of the DetectObject processor is set to "Letterbox"
    And the "Color format" property of the DetectObject processor is set to "RGB"
    And the "Tensor shape format" property of the DetectObject processor is set to "CHW"
    And the "Mean" property of the DetectObject processor is set to "127.0"
    And the "Standard Deviation" property of the DetectObject processor is set to "128.0"
    And the "Tract model service" property of the DetectObject processor is set to "UltraFace"
    # UltraFace: output 0 = scores [1, N, 2] (softmax over background/face),
    # output 1 = boxes [1, N, 4] in Xyxy normalised to 0..1. Class 0 is
    # background so leave the defaults.
    And the "Confidence Threshold" property of the DetectObject processor is set to "0.5"
    And the "IoU Threshold" property of the DetectObject processor is set to "0.45"
    And the "Score output index" property of the DetectObject processor is set to "0"
    And the "Box output index" property of the DetectObject processor is set to "1"
    And the "Box format" property of the DetectObject processor is set to "Xyxy"
    And the "Score activation" property of the DetectObject processor is set to "Softmax"
    And the "Background class index" property of the DetectObject processor is set to "0"
    And the "Output attribute name" property of the DetectObject processor is set to "detected_objects"

    # The content is still the original JPEG, so DrawBoundingBox reads the boxes
    # from the attribute DetectObject wrote and paints them onto the image.
    And a DrawBoundingBox processor with the "Bounding boxes" property set to "${detected_objects}"
    And the "Line color" property of the DrawBoundingBox processor is set to "0, 255, 0"
    And the "Line tickness" property of the DrawBoundingBox processor is set to "5"

    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN

    And a PutFile processor with the "Directory" property set to "/tmp/output"

    And the "success" relationship of the GetFile processor is connected to the DetectObject
    And the "success" relationship of the DetectObject processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the DrawBoundingBox
    And the "success" relationship of the DrawBoundingBox processor is connected to the PutFile
    And DetectObject's failure relationship is auto-terminated
    And DrawBoundingBox's failure relationship is auto-terminated
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated

    When the MiNiFi instance starts up

    # At least one face detected, written to the configured attribute as JSON.
    Then the Minifi logs match the following regex: "key:object.count value:[1-9][0-9]*" in less than 60 seconds
    And the Minifi logs match the following regex: "key:detected_objects value:.*\"class_id\":1" in less than 1 seconds
    And the Minifi logs match the following regex: "key:detected_objects value:.*\"confidence\":0\.[5-9][0-9]*" in less than 1 seconds
    # DrawBoundingBox emits the annotated image as PNG (magic bytes contain "PNG").
    And the Minifi logs do not contain errors
