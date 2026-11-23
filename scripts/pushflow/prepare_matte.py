#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
"""The matting model of PushFPS Character Hold (src/pipeline/PushCharacterPass.cpp), prepared for the release.

Character Hold keeps the people in a generated frame whole: it needs to know, for every real frame, which pixels
are a person. That comes from Robust Video Matting (Lin, Yang, Saleemi, Sengupta; GPL-3.0;
https://github.com/PeterL1n/RobustVideoMatting), its MobileNetV3 model as published in the project's own
release v1.0.0. Nothing of it is committed: this script downloads the pinned file, checks its SHA-256 and writes

  <out>/runtime/pushflow/models/pushmatte.onnx       the model with fixed sizes (see below)
  <out>/runtime/pushflow/models/pushmatte.onnx.txt   what PushFPS has to know about it
  <out>/runtime/pushflow/licenses/RobustVideoMatting_LICENSE.txt

Run it after prepare_runtime.py (which empties the runtime folder first).

What is changed in the model, and why. The published model takes a picture of any size, four recurrent states
of any size and the downsample ratio as an input. Every one of its free sizes is named "height" or "width", the
picture's and the states' alike, so they cannot be fixed by name when a session is made; and the DirectML
provider works best with sizes known when the session is created. PushFPS runs one size only, so here:
  - the picture is fixed at 1 x 3 x 1080 x 1920 and the ratio becomes the constant 0.25 (the size the matte was
    studied at),
  - the four states get the sizes the model itself gives them at that picture size (read from a run of the
    published model, never written down by hand),
  - the foreground output ("fgr") is dropped, with the nodes only it needed: Character Hold takes colours from
    the real frame.
The weights are untouched. The result is checked against the published model on the processor, over several
frames with the states carried along; a difference fails the build.
"""
from __future__ import annotations

import argparse
import hashlib
import shutil
import sys
import urllib.request
from pathlib import Path

URL = "https://github.com/PeterL1n/RobustVideoMatting/releases/download/v1.0.0/rvm_mobilenetv3_fp32.onnx"
SHA256 = "88d4531297118f595bf2fd60f6f566aec2e559393802d1f436c380f0cbbd2828"
WIDTH, HEIGHT, RATIO = 1920, 1080, 0.25
STATES = ("r1", "r2", "r3", "r4")
NOTICE = """Robust Video Matting
Copyright (c) Shanchuan Lin, Linjie Yang, Imran Saleemi, Soumyadip Sengupta
https://github.com/PeterL1n/RobustVideoMatting

Licensed under the GNU General Public License v3.0 (the licence PushFPS itself is under; its text is in
LICENSE next to PushFPS.exe).

pushmatte.onnx in this folder's models directory is the project's published model
rvm_mobilenetv3_fp32.onnx (release v1.0.0, SHA-256 {sha}) with its input sizes fixed for PushFPS:
the picture at 1 x 3 x {h} x {w}, the downsample ratio as the constant {ratio}, the recurrent states at the sizes
the model gives them for that picture, and the foreground output removed. The weights are unchanged. The script
that makes it is scripts/pushflow/prepare_matte.py in PushFPS's source.
"""


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def picture(step: int):
    """A drawn picture that changes from step to step (the check only needs the two models to see the same)."""
    import numpy as np
    y, x = np.mgrid[0:HEIGHT, 0:WIDTH].astype(np.float32)
    img = np.stack([0.35 + 0.25 * np.sin(x / 97.0 + step), 0.40 + 0.20 * np.cos(y / 61.0), 0.45 + 0.15 * np.sin((x + y) / 131.0)], 0)
    cx, cy = 900.0 + 14.0 * step, 560.0 + 3.0 * step
    body = ((x - cx) / 150.0) ** 2 + ((y - cy - 140.0) / 330.0) ** 2 < 1.0
    head = ((x - cx) / 80.0) ** 2 + ((y - cy + 250.0) / 95.0) ** 2 < 1.0
    img[0][body] = 0.62; img[1][body] = 0.12; img[2][body] = 0.20
    img[0][head] = 0.55; img[1][head] = 0.40; img[2][head] = 0.30
    return np.clip(img, 0.0, 1.0)[None].astype(np.float32)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--folded", action="store_true", help="also write pushmatte-folded.onnx (ONNX Runtime's basic optimisations done beforehand; for tools/character_probe)")
    ap.add_argument("--variants", action="store_true", help="also write pushmatte-noexpand.onnx (for tools/character_probe)")
    ap.add_argument("--describe", action="store_true", help="print the model's operators and the attributes that matter to a provider")
    args = ap.parse_args()
    out = Path(args.out).resolve()
    work = out / "download"
    work.mkdir(parents=True, exist_ok=True)
    models = out / "runtime" / "pushflow" / "models"
    licences = out / "runtime" / "pushflow" / "licenses"
    if not models.is_dir():
        sys.exit("::error::run prepare_runtime.py first: " + str(models) + " is not there")
    licences.mkdir(parents=True, exist_ok=True)

    source = work / "rvm_mobilenetv3_fp32.onnx"
    if not (source.is_file() and sha256(source) == SHA256):
        print(f"download {URL}")
        urllib.request.urlretrieve(URL, source)
    digest = sha256(source)
    print(f"sha256 {digest}  {source.name}")
    if digest != SHA256:
        sys.exit(f"::error::{source.name}: SHA-256 {digest} does not match the pinned {SHA256}")

    import numpy as np
    import onnx
    import onnxruntime as ort
    from onnx import numpy_helper

    options = ort.SessionOptions()
    options.intra_op_num_threads = 2
    published = ort.InferenceSession(str(source), options, providers=["CPUExecutionProvider"])
    ratio = np.array([RATIO], np.float32)
    zero = np.zeros((1, 1, 1, 1), np.float32)
    first = published.run(None, {"src": picture(0), "r1i": zero, "r2i": zero, "r3i": zero, "r4i": zero, "downsample_ratio": ratio})
    names = [o.name for o in published.get_outputs()]
    if names != ["fgr", "pha", "r1o", "r2o", "r3o", "r4o"]:
        sys.exit(f"::error::the published model's outputs are {names}, not the ones this script knows")
    state_shapes = {s: [int(d) for d in first[2 + i].shape] for i, s in enumerate(STATES)}
    print("states:", state_shapes)

    model = onnx.load(str(source))
    graph = model.graph
    if not any(i.name == "downsample_ratio" for i in graph.input):
        sys.exit("::error::the published model has no downsample_ratio input")
    graph.initializer.append(numpy_helper.from_array(ratio, name="downsample_ratio"))
    kept = [i for i in graph.input if i.name != "downsample_ratio"]
    del graph.input[:]
    graph.input.extend(kept)

    def fix(info, shape):
        dims = info.type.tensor_type.shape.dim
        del dims[:]
        for value in shape:
            dims.add().dim_value = int(value)

    shapes = {"src": [1, 3, HEIGHT, WIDTH], "pha": [1, 1, HEIGHT, WIDTH]}
    for s in STATES:
        shapes[s + "i"] = state_shapes[s]
        shapes[s + "o"] = state_shapes[s]
    for info in graph.input:
        if info.name not in shapes:
            sys.exit(f"::error::unexpected input {info.name}")
        fix(info, shapes[info.name])
    outputs = [o for o in graph.output if o.name != "fgr"]
    del graph.output[:]
    graph.output.extend(outputs)
    for info in graph.output:
        fix(info, shapes[info.name])
    del graph.value_info[:]          # the published sizes of the values in between no longer hold

    def prune(g):
        """Keeps only what the outputs need (the foreground's own nodes go; ONNX Runtime does not drop them itself)."""
        needed = {o.name for o in g.output}
        kept = []
        for node in reversed(list(g.node)):
            if any(o in needed for o in node.output):
                kept.append(node)
                needed.update(i for i in node.input if i)
        kept.reverse()
        dropped = len(g.node) - len(kept)
        del g.node[:]
        g.node.extend(kept)
        constants = [i for i in g.initializer if i.name in needed]
        del g.initializer[:]
        g.initializer.extend(constants)
        return dropped

    print(f"nodes that only the foreground output needed, removed: {prune(graph)}")
    onnx.checker.check_model(model)
    target = models / "pushmatte.onnx"
    onnx.save(model, str(target))

    def verify(path):
        fixed = ort.InferenceSession(str(path), options, providers=["CPUExecutionProvider"])
        got = [(i.name, i.shape) for i in fixed.get_inputs()], [(o.name, o.shape) for o in fixed.get_outputs()]
        for name, shape in got[0] + got[1]:
            if shape != shapes[name]:
                sys.exit(f"::error::{name} is {shape} in {path.name}, not {shapes[name]}")
        if len(got[0]) != 5 or len(got[1]) != 5:
            sys.exit(f"::error::{path.name} has {len(got[0])} inputs and {len(got[1])} outputs")
        rec_a = [zero] * 4
        rec_b = [np.zeros(state_shapes[s], np.float32) for s in STATES]
        worst = 0.0
        covered = []
        for step in range(4):
            src = picture(step)
            a = published.run(None, {"src": src, "r1i": rec_a[0], "r2i": rec_a[1], "r3i": rec_a[2], "r4i": rec_a[3], "downsample_ratio": ratio})
            b = fixed.run(["pha", "r1o", "r2o", "r3o", "r4o"], {"src": src, "r1i": rec_b[0], "r2i": rec_b[1], "r3i": rec_b[2], "r4i": rec_b[3]})
            rec_a, rec_b = a[2:], b[1:]
            worst = max(worst, float(np.abs(a[1] - b[0]).max()), *[float(np.abs(x - y).max()) for x, y in zip(a[2:], b[1:])])
            covered.append(round(float((b[0] > 0.5).mean()) * 100, 2))
        print(f"{path.name} against the published model over 4 frames: largest difference {worst:.2e}; matte above a half on {covered} % of the drawn pictures")
        if not worst <= 1e-4:
            sys.exit(f"::error::{path.name} differs from the published model by {worst}")

    verify(target)
    if args.variants:
        # For tools/character_probe: the same model with one more thing taken out each, to see what a provider minds.
        states_in = {s + "i" for s in STATES}
        renamed = {}
        kept = []
        for node in graph.node:
            if node.op_type == "Expand" and node.input[0] in states_in:
                renamed[node.output[0]] = node.input[0]      # a state already has the size it is expanded to
                continue
            kept.append(node)
        for node in kept:
            for k, name in enumerate(node.input):
                if name in renamed:
                    node.input[k] = renamed[name]
        del graph.node[:]
        graph.node.extend(kept)
        print(f"variant noexpand: Expand nodes of the states removed: {len(renamed)}; nodes then unused: {prune(graph)}")
        onnx.checker.check_model(model)
        onnx.save(model, str(models / "pushmatte-noexpand.onnx"))
        verify(models / "pushmatte-noexpand.onnx")

    if args.folded:
        folded = models / "pushmatte-folded.onnx"
        fold = ort.SessionOptions()
        fold.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_BASIC
        fold.optimized_model_filepath = str(folded)
        ort.InferenceSession(str(target), fold, providers=["CPUExecutionProvider"])
        print(f"wrote {folded} ({folded.stat().st_size} bytes)")
    if args.describe:
        import collections
        for label, path in (("fixed", target),):
            m = onnx.load(str(path))
            kinds = collections.Counter(n.op_type for n in m.graph.node)
            print(f"{label}: opsets {[(o.domain or 'ai.onnx', o.version) for o in m.opset_import]}; {len(m.graph.node)} nodes: {dict(sorted(kinds.items()))}")
            inits = {i.name for i in m.graph.initializer}
            outs = {o.name for o in m.graph.output}
            ins = {i.name for i in m.graph.input}
            produced = {o for n in m.graph.node for o in n.output}
            used = {i for n in m.graph.node for i in n.input} | outs
            print(f"{label}: values made and never used: {sorted(produced - used)[:8]}")
            direct = [o for o in outs if o in ins or o in inits]
            print(f"{label}: outputs that are an input or a constant: {direct}")

    facts = [f"width={WIDTH}", f"height={HEIGHT}", f"ratio={RATIO}", f"source_sha256={SHA256}", f"sha256={sha256(target)}"]
    for s in STATES:
        facts.append(f"{s}=" + "x".join(str(d) for d in state_shapes[s]))
    (models / "pushmatte.onnx.txt").write_text("\n".join(facts) + "\n", encoding="utf-8")
    (licences / "RobustVideoMatting_LICENSE.txt").write_text(NOTICE.format(sha=SHA256, h=HEIGHT, w=WIDTH, ratio=RATIO), encoding="utf-8")
    print("\n".join(facts))
    print(f"wrote {target} ({target.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
