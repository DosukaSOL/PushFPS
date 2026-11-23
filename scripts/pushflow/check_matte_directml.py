#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
"""Does the fixed matting model (prepare_matte.py) run under the DirectML provider, and give what the processor gives?

A quick answer without a Windows build of PushFPS: the matte-check workflow runs this on a Windows runner (the
software renderer there) with the onnxruntime-directml wheel of the pinned ONNX Runtime version. It prints which
provider each node ran on, the difference to the processor's result over four frames with the states carried
along, and the time of a run (the software renderer's time says nothing about a graphics card).
"""
import json
import sys
import time
from pathlib import Path

import numpy as np
import onnxruntime as ort

sys.path.insert(0, str(Path(__file__).resolve().parent))
from prepare_matte import STATES, picture  # noqa: E402


def main() -> None:
    model = Path(sys.argv[1])
    facts = dict(line.split("=", 1) for line in model.with_suffix(".onnx.txt").read_text(encoding="utf-8").split())
    shapes = {s: [int(d) for d in facts[s].split("x")] for s in STATES}
    print("providers offered:", ort.get_available_providers())
    cpu = ort.InferenceSession(str(model), providers=["CPUExecutionProvider"])
    options = ort.SessionOptions()
    options.enable_mem_pattern = False
    options.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
    options.enable_profiling = True
    dml = ort.InferenceSession(str(model), options, providers=["DmlExecutionProvider", "CPUExecutionProvider"])
    print("session providers:", dml.get_providers())
    rec_c = [np.zeros(shapes[s], np.float32) for s in STATES]
    rec_d = [x.copy() for x in rec_c]
    names = ["pha", "r1o", "r2o", "r3o", "r4o"]
    worst = 0.0
    times = []
    for step in range(4):
        src = picture(step)
        c = cpu.run(names, {"src": src, "r1i": rec_c[0], "r2i": rec_c[1], "r3i": rec_c[2], "r4i": rec_c[3]})
        t0 = time.perf_counter()
        d = dml.run(names, {"src": src, "r1i": rec_d[0], "r2i": rec_d[1], "r3i": rec_d[2], "r4i": rec_d[3]})
        times.append(round((time.perf_counter() - t0) * 1000, 1))
        rec_c, rec_d = c[1:], d[1:]
        diff = float(np.abs(c[0] - d[0]).max())
        worst = max(worst, diff)
        print(f"frame {step}: matte differs by at most {diff:.2e}; above a half on {float((d[0] > 0.5).mean()) * 100:.2f} %")
    profile = Path(dml.end_profiling())
    per = {}
    for event in json.loads(profile.read_text(encoding="utf-8")):
        provider = event.get("args", {}).get("provider")
        if provider and event.get("name", "").endswith("_kernel_time"):
            kind = event["args"].get("op_name", "?")
            per.setdefault(provider, {}).setdefault(kind, 0)
            per[provider][kind] += 1
    for provider, kinds in per.items():
        print(provider, "ran", sum(kinds.values()) // 4, "nodes a frame:", dict(sorted((k, v // 4) for k, v in kinds.items())))
    print("milliseconds a run here:", times)
    if "DmlExecutionProvider" not in dml.get_providers():
        sys.exit("::error::the DirectML provider was not used")
    if not worst <= 2e-2:
        sys.exit(f"::error::DirectML differs from the processor by {worst}")
    print(f"DirectML against the processor: largest difference {worst:.2e}")


if __name__ == "__main__":
    main()
