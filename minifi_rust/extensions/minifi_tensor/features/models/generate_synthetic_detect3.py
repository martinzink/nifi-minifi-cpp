# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

"""Regenerate synthetic_detect3.onnx — a tiny object-detection fixture used by
detection.feature to exercise FilterBoundingBoxes/DetectObject's "separate
class-id tensor" path (the TF Object Detection API / EfficientNMS layout).

The model takes an f32 image tensor [1,3,64,64] and emits three parallel
outputs, in model order:
    0 = scores  [3]    f32  = [0.92, 0.20, 0.80]  (one confidence per box)
    1 = boxes   [3,4]  f32  = normalised Xyxy corners
    2 = classes [3]    i64  = [5, 3, 17]           (one class id per box)

The image is only consumed through a zeroed ReduceMean so tract keeps it as a
plan input; the detections themselves are constants. That is enough to verify
the extension's wiring: three tensors read by index, the i64 class tensor cast
to f32, confidence filtering, box decode, and JSON output.

Usage:  pip install onnx numpy && python generate_synthetic_detect3.py
"""

import os

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper

boxes = np.array(
    [[0.10, 0.10, 0.20, 0.20], [0.50, 0.50, 0.90, 0.90], [0.30, 0.30, 0.35, 0.35]],
    dtype=np.float32,
)
scores = np.array([0.92, 0.20, 0.80], dtype=np.float32)
classes = np.array([5, 3, 17], dtype=np.int64)

initializers = [
    numpy_helper.from_array(boxes, "boxes_const"),
    numpy_helper.from_array(scores, "scores_const"),
    numpy_helper.from_array(classes, "classes_const"),
    numpy_helper.from_array(np.array(0.0, dtype=np.float32), "zero"),
]

image_input = helper.make_tensor_value_info("images", TensorProto.FLOAT, [1, 3, 64, 64])

nodes = [
    # Consume the input (reduce to a scalar) then multiply by zero and add to the
    # scores so the values are unchanged but tract keeps `images` as a plan input.
    helper.make_node("ReduceMean", ["images"], ["img_mean"], axes=[0, 1, 2, 3], keepdims=0),
    helper.make_node("Mul", ["img_mean", "zero"], ["img_zero"]),
    helper.make_node("Add", ["scores_const", "img_zero"], ["scores"]),
    helper.make_node("Identity", ["boxes_const"], ["boxes"]),
    helper.make_node("Identity", ["classes_const"], ["classes"]),
]

outputs = [
    helper.make_tensor_value_info("scores", TensorProto.FLOAT, [3]),
    helper.make_tensor_value_info("boxes", TensorProto.FLOAT, [3, 4]),
    helper.make_tensor_value_info("classes", TensorProto.INT64, [3]),
]

graph = helper.make_graph(nodes, "synthetic_detect3", [image_input], outputs, initializer=initializers)
model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)])
model.ir_version = 9
onnx.checker.check_model(model)

dest = os.path.join(os.path.dirname(os.path.realpath(__file__)), "synthetic_detect3.onnx")
onnx.save(model, dest)
print(f"wrote {dest} ({os.path.getsize(dest)} bytes)")
