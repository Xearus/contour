# Contour

Contour is an engine-first fork of Bambu Studio. The goal is a portable,
headless slicing backend with a Rust API, independently tested before adding
desktop, mobile, or server frontends.

**Current scope: component characterization, not a standalone slicer.** The
build below exercises the upstream Clipper2 polygon library and Bambu's
standalone G-code checker without wxWidgets, OpenGL, printer networking, or a
display. It does not yet load models, run the complete slicing pipeline, or
produce printer-ready packages. There is no Rust API yet.

The upstream baseline is recorded in [`engine/upstream.json`](engine/upstream.json).
Slicing algorithms and printer profiles remain unchanged. Initial upstream
source patches supply a missing standard header in the G-code checker and
normalize CRLF input so validation is consistent across operating systems.

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
to the components in this build, not the entire Bambu slicing engine.

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
A configured job is not evidence that it passed.
Full model-to-G-code regressions, X2D output verification, and physical print
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

