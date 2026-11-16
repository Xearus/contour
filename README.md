# Contour

Contour is an engine-first fork of Bambu Studio. The goal is a portable,
headless slicing backend with a Rust API, independently tested before adding
desktop, mobile, or server frontends.

**Current scope: component characterization and experimental FFF isolation.**
The default lightweight build exercises Clipper2 and Bambu's standalone G-code
checker. An opt-in `contour-slice` target connects STL loading to the upstream
FFF pipeline and raw G-code export, without building the desktop application or
printer transport. A narrow C library and safe Rust wrapper expose that same
operation without upstream C++ types. It is not a printer-ready slicer.

The upstream baseline is recorded in [`engine/upstream.json`](engine/upstream.json).
Slicing algorithms and printer profiles remain unchanged. Initial upstream
source patches supply a missing standard header in the G-code checker and
normalize CRLF input so validation is consistent across operating systems.
The experimental build also separates network-backed desktop logging from the
slicing library; it supports local, unencrypted file logging only.

## Building the characterization baseline

Requires CMake 3.21+, Ninja, and a C++17 compiler:

```sh
cmake -S engine -B build/contour -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/contour --parallel 2
ctest --test-dir build/contour --output-on-failure
```

For an isolated build with Docker or rootless Podman:

```sh
podman build -f engine/Containerfile -t contour-engine-dev .
podman run --rm -v "$PWD:/src:ro" contour-engine-dev sh -c \
  'cmake -S /src/engine -B /tmp/build -G Ninja -DCMAKE_BUILD_TYPE=Debug &&
   cmake --build /tmp/build --parallel 2 &&
   ctest --test-dir /tmp/build --output-on-failure'
```

The image also includes an ARM64 cross-compiler and QEMU. To execute the same
tests as ARM64 Linux binaries, add
`-DCMAKE_TOOLCHAIN_FILE=/src/engine/cmake/linux-aarch64.cmake` to the configure
command. This uses QEMU directly and needs no privileged container or host
`binfmt_misc` changes. It is not Android validation or a performance benchmark.

On GCC/Clang, `-DCONTOUR_ENABLE_SANITIZERS=ON` enables address and undefined
behaviour sanitizers. GCC builds can instead use
`-DCONTOUR_ENABLE_COVERAGE=ON` for gcov instrumentation. These options apply only
to enabled Contour targets. Checker coverage is not whole-engine coverage.

## Experimental model-to-G-code build

The initial dependency recipe targets Linux. It retains upstream geometry,
CAD-import and texture-processing dependencies rather than replacing algorithms
or pretending the complete library has already been slimmed down. Resin-only
processing and the network-backed desktop log sink are excluded. No desktop
application, rendering loop or proprietary printer-network plugin is built.
Mesh booleans use upstream's MCUT fallback, not its default OpenMeshCraft
backend; equivalence between those backends has not been established.

From the repository root:

```sh
podman build -f engine/slicer/Containerfile -t contour-slicer-dev .
mkdir -p build/contour-slicer
podman run --rm --network none \
  -v "$PWD:/src:ro" -v "$PWD/build/contour-slicer:/build" \
  contour-slicer-dev sh -c \
  'cmake -S /src/engine -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_CXX_FLAGS_RELEASE="-O1 -DNDEBUG" \
     -DCONTOUR_BUILD_SLICER=ON &&
   cmake --build /build --parallel 2 &&
   ctest --test-dir /build --output-on-failure'
```

The executable accepts
`contour-slice INPUT.stl OUTPUT.gcode [CONFIG.ini]`. Existing output paths are
rejected. The optional INI file overrides upstream configuration keys, for
example:

```ini
layer_height = 0.1
initial_layer_print_height = 0.1
```

The default configuration is a generic, single-filament Marlin experiment, not
an X2D profile. Models retain their XY coordinates and are placed on the bed in
Z. Output is raw G-code, not a Bambu printer-ready package: **do not send these
experiments to a printer**. Printer profiles, machine start/end sequences,
multi-material compatibility and Android remain separate work.

The pipeline contract tests generate synthetic STL boxes, exercise the actual
CLI and repeated direct C calls without a display, and independently inspect
layer heights, finite motion and positive extrusion. The Rust example runs
through the same contracts. They also exercise configuration changes and failures
that must not produce or overwrite an output file. This is initial end-to-end
evidence, not comprehensive slicing correctness or printer-safety certification.

## Experimental Rust API

The slicing build also produces `libcontour_engine` and the C-compatible header
[`contour/engine.h`](engine/slicer/include/contour/engine.h). The CLI calls this
same boundary. Exceptions become status codes and bounded, caller-owned error
messages; no C++ objects or allocator ownership cross the boundary. Calls are
blocking and serialized because upstream process-global state is not yet
verified for concurrent slices. The API is experimental, not a frozen ABI.

The dependency-free crate in [`rust/contour-engine`](rust/contour-engine) links
the prebuilt shared library rather than rebuilding the C++ engine through Cargo:

```rust
contour_engine::slice_stl("model.stl", "output.gcode", None)?;
```

In the dependency container, after the CMake build above:

```sh
export CONTOUR_ENGINE_LIB_DIR=/build/slicer
export LD_LIBRARY_PATH=/build/slicer
export CARGO_TARGET_DIR=/build/rust-target
cargo test --manifest-path /src/rust/contour-engine/Cargo.toml --offline --locked
cargo build --manifest-path /src/rust/contour-engine/Cargo.toml \
  --offline --locked --example slice
python3 /src/engine/slicer/test_pipeline.py /build/rust-target/debug/examples/slice
```

The container includes Rust 1.75+ build, formatting and lint tools. Outside it,
use the corresponding absolute build paths and ensure the native library and
its runtime dependencies can be found by the platform loader. Native builds
remain Linux-first; adding a Rust API does not establish Windows, macOS or
Android slicing support. Unix paths retain native bytes; other platforms
currently require UTF-8 paths, and Windows narrow-path handling remains unverified.

Existing-output rejection is not atomic, and late export failures may leave
partial files. This is not a multi-tenant server API. C++ exceptions are caught,
but native crashes are not isolated; process isolation is still required for
untrusted models. Configuration remains upstream INI rather than a portable
backend-neutral settings schema. No preview, cancellation, progress reporting,
printer transport or X2D output support is added by these bindings.

## What the tests establish

- Polygon intersections with offset nozzle envelopes, holes and fill rules,
  inward offsets, overlapping islands, and translation invariance.
- Characterization of the upstream G-code checker's extrusion-width checks,
  positioning modes, arcs, malformed moves, tool selection, and line endings.
- Explicit known-limit tests: the checker accepts out-of-bed travel and can
  accept a dual-nozzle temperature mismatch. Passing it is **not** permission
  to print. Test G-code is synthetic and is not a printer job.

CI is configured for Linux x86-64/ARM64, Windows, macOS, sanitizers, and ARM64
Linux execution under QEMU, with a separate gcov artifact for the checker.
These jobs cover the lightweight baseline. Model-to-G-code CI runs separately
on native Linux x86-64 and ARM64, including the same output contracts through
the Rust example; Windows, macOS and Android slicing are not yet
verified.
A configured job is not evidence that it passed.
Broad model-to-G-code regressions, X2D output verification, and physical print
validation are still required before describing Contour as a usable slicer.

The inherited project and applicable Contour changes are licensed under
AGPLv3; bundled dependencies retain their own licenses. The original Bambu
Studio documentation follows.

---

![image](https://user-images.githubusercontent.com/106916061/179006347-497d24c0-9bd6-45b7-8c49-d5cc8ecfe5d7.png)
# BambuStudio
Bambu Studio is a cutting-edge, feature-rich slicing software.  
It contains project-based workflows, systematically optimized slicing algorithms, and an easy-to-use graphic interface, bringing users an incredibly smooth printing experience.

Prebuilt Windows, macOS 64-bit and Linux releases are available through the [github releases page](https://github.com/bambulab/BambuStudio/releases/).

Bambu Studio is based on [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research, which is from [Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro Ranellucci and the RepRap community.

See the [wiki](https://github.com/bambulab/BambuStudio/wiki) and the [documentation directory](https://github.com/bambulab/BambuStudio/tree/master/doc) for more information.

# What are Bambu Studio's main features?
Key features are:
- Basic slicing features & GCode viewer
- Multiple plates management
- Remote control & monitoring
- Auto-arrange objects
- Auto-orient objects
- Hybrid/Tree/Normal support types, Customized support
- multi-material printing and rich painting tools
- multi-platform (Win/Mac/Linux) support
- Global/Object/Part level slicing parameters

Other major features are:
- Advanced cooling logic controlling fan speed and dynamic print speed
- Auto brim according to mechanical analysis
- Support arc path(G2/G3)
- Support STEP format
- Assembly & explosion view
- Flushing transition-filament into infill/object during filament change

# How to compile
Following platforms are currently supported to compile:
- Windows 64-bit, [Compile Guide](https://github.com/bambulab/BambuStudio/wiki/Windows-Compile-Guide)
- Mac 64-bit, [Compile Guide](https://github.com/bambulab/BambuStudio/wiki/Mac-Compile-Guide)
- Linux, [Compile Guide](https://github.com/bambulab/BambuStudio/wiki/Linux-Compile-Guide)
  - currently we only provide linux appimages on [github releases](https://github.com/bambulab/BambuStudio/releases) for Ubuntu/Fedora, and a [flathub version](https://flathub.org/apps/com.bambulab.BambuStudio) can be used for all the linux platforms

# Report issue
You can add an issue to the [github tracker](https://github.com/bambulab/BambuStudio/issues) if **it isn't already present.**

# License
Bambu Studio is licensed under the GNU Affero General Public License, version 3. Bambu Studio is based on PrusaSlicer by PrusaResearch.

PrusaSlicer is licensed under the GNU Affero General Public License, version 3. PrusaSlicer is owned by Prusa Research. PrusaSlicer is originally based on Slic3r by Alessandro Ranellucci.

Slic3r is licensed under the GNU Affero General Public License, version 3. Slic3r was created by Alessandro Ranellucci with the help of many other contributors.

The GNU Affero General Public License, version 3 ensures that if you use any part of this software in any way (even behind a web server), your software must be released under the same license.

The bambu networking plugin is based on non-free libraries. It is optional to the Bambu Studio and provides extended networking functionalities for users.
By default, after installing Bambu Studio without the networking plugin, you can initiate printing through the SD card after slicing is completed.

