"""Deterministic CPU fixtures; requires onnx==1.16.2 and numpy (offline generation only)."""
import argparse
from pathlib import Path

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper

parser = argparse.ArgumentParser(description="生成通用 ONNX CPU 性能基线模型")
parser.add_argument("output", type=Path, help="模型输出目录")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
shape = [1, 16, 128, 128]

def save(name, nodes, initializers=(), padding=0):
    graph = helper.make_graph(nodes, "xvision-cpu-baseline",
        [helper.make_tensor_value_info("input", TensorProto.FLOAT, shape)],
        [helper.make_tensor_value_info("output", TensorProto.FLOAT, shape)], initializers)
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)], ir_version=8)
    if padding:
        model.doc_string = "x" * padding
    onnx.checker.check_model(model)
    target = args.output / name
    onnx.save(model, target)
    print(f"{name}: {target.stat().st_size} bytes")

save("identity.onnx", [helper.make_node("Identity", ["input"], ["output"])])
save("identity-32mb.onnx", [helper.make_node("Identity", ["input"], ["output"])], padding=32*1024*1024)
rng = np.random.default_rng(19)
nodes, weights = [], []
previous = "input"
for index in range(6):
    weight = f"weight{index}"
    weights.append(numpy_helper.from_array(rng.normal(0, 0.04, (16,16,3,3)).astype(np.float32), weight))
    conv, result = f"conv{index}", "output" if index == 5 else f"relu{index}"
    nodes.extend([helper.make_node("Conv", [previous, weight], [conv], pads=[1,1,1,1]),
                  helper.make_node("Relu", [conv], [result])])
    previous = result
save("conv.onnx", nodes, weights)
