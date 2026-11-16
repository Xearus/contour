import ctypes
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


USE_LIBRARY = sys.argv[1] == "--library"
if USE_LIBRARY:
    sys.argv.pop(1)
SLICER = Path(sys.argv.pop(1)).resolve()
if USE_LIBRARY:
    LIBRARY = ctypes.CDLL(str(SLICER))
    NATIVE_SLICE = LIBRARY.contour_slice_stl
    NATIVE_SLICE.argtypes = [
        ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_char), ctypes.c_size_t,
    ]
    NATIVE_SLICE.restype = ctypes.c_int


def write_box(path, width=10, height=2):
    vertices = [
        (20, 20, 0), (20 + width, 20, 0),
        (20 + width, 30, 0), (20, 30, 0),
        (20, 20, height), (20 + width, 20, height),
        (20 + width, 30, height), (20, 30, height),
    ]
    triangles = [
        (0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7),
        (0, 1, 5), (0, 5, 4), (1, 2, 6), (1, 6, 5),
        (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7),
    ]
    lines = ["solid synthetic_box"]
    for triangle in triangles:
        lines += ["facet normal 0 0 0", "outer loop"]
        lines += ["vertex {} {} {}".format(*vertices[index]) for index in triangle]
        lines += ["endloop", "endfacet"]
    path.write_text("\n".join(lines + ["endsolid synthetic_box", ""]), encoding="ascii")


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="contour-pipeline-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.model = self.directory / "input.stl"
        self.output = self.directory / "output.gcode"
        write_box(self.model)

    def run_slicer(self, config=None):
        arguments = [str(SLICER), str(self.model), str(self.output)]
        if config is not None:
            profile = self.directory / "profile.ini"
            profile.write_text(config, encoding="ascii")
            arguments.append(str(profile))
        if USE_LIBRARY:
            diagnostic = ctypes.create_string_buffer(b"stale diagnostic", 4096)
            status = NATIVE_SLICE(
                os.fsencode(self.model), os.fsencode(self.output),
                os.fsencode(profile) if config is not None else None,
                diagnostic, len(diagnostic))
            if status == 0:
                self.assertEqual(diagnostic.value, b"")
            return subprocess.CompletedProcess(
                arguments, 0 if status == 0 else 1, "",
                diagnostic.value.decode("utf-8", errors="replace"))
        environment = os.environ.copy()
        environment.pop("DISPLAY", None)
        environment.pop("WAYLAND_DISPLAY", None)
        return subprocess.run(arguments, capture_output=True, text=True,
                              env=environment, timeout=90)

    def inspect_output(self, height, layer_height):
        output = self.output.read_text(encoding="utf-8")
        layers = [float(value) for value in re.findall(
            r"^;\s*Z_HEIGHT:\s*(\S+)", output, flags=re.MULTILINE)]
        expected = round(height / layer_height)
        self.assertEqual(len(layers), expected)
        for index, value in enumerate(layers, 1):
            self.assertAlmostEqual(value, index * layer_height, places=5)

        relative_e = False
        position = dict.fromkeys("XYZE", 0.0)
        deposited = 0.0
        extruding_layers = set()
        for raw in output.splitlines():
            command = raw.partition(";")[0].strip().split()
            if not command:
                continue
            if command[0] in ("M82", "M83"):
                relative_e = command[0] == "M83"
            if command[0] not in ("G0", "G1", "G92"):
                continue
            values = {}
            for token in command[1:]:
                axis, number = token[0], float(token[1:])
                self.assertTrue(math.isfinite(number), raw)
                values[axis] = number
            if command[0] == "G92":
                position.update({key: value for key, value in values.items() if key in position})
                continue
            destination = {**position, **{key: value for key, value in values.items()
                                         if key in position}}
            extrusion = values.get("E", 0) if relative_e else destination["E"] - position["E"]
            if relative_e:
                destination["E"] = position["E"] + extrusion
            distance = math.hypot(destination["X"] - position["X"],
                                  destination["Y"] - position["Y"])
            if extrusion > 0 and distance > 0:
                self.assertGreaterEqual(destination["X"], 0)
                self.assertLessEqual(destination["X"], 200)
                self.assertGreaterEqual(destination["Y"], 0)
                self.assertLessEqual(destination["Y"], 200)
                self.assertGreater(destination["Z"], 0)
                self.assertLessEqual(destination["Z"], height + 1e-5)
                deposited += extrusion
                extruding_layers.add(round(destination["Z"], 5))
            position = destination
        self.assertGreater(deposited, 0)
        self.assertEqual(extruding_layers, {round(z, 5) for z in layers})
        return deposited

    def test_box_generates_finite_extrusion_on_every_layer(self):
        result = self.run_slicer()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.inspect_output(2, 0.2)

    def test_taller_model_changes_layer_count(self):
        write_box(self.model, width=20, height=4)
        result = self.run_slicer()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.inspect_output(4, 0.2)

    def test_profile_changes_layer_height(self):
        result = self.run_slicer("layer_height = 0.1\ninitial_layer_print_height = 0.1\n")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.inspect_output(2, 0.1)

    def test_missing_and_malformed_models_do_not_produce_gcode(self):
        self.model.unlink()
        for content in (None, "not an STL\n"):
            with self.subTest(content=content):
                if content is not None:
                    self.model.write_text(content, encoding="ascii")
                result = self.run_slicer()
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertFalse(self.output.exists())

    def test_invalid_layer_height_does_not_produce_gcode(self):
        result = self.run_slicer("layer_height = 0\n")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertFalse(self.output.exists())

    def test_existing_output_is_not_overwritten(self):
        self.output.write_text("existing output\n", encoding="ascii")
        result = self.run_slicer()
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertEqual(self.output.read_text(encoding="ascii"), "existing output\n")


if __name__ == "__main__":
    unittest.main()
