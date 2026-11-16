use std::{env, path::PathBuf};

fn main() {
    println!("cargo:rerun-if-env-changed=CONTOUR_ENGINE_LIB_DIR");
    let directory = PathBuf::from(
        env::var_os("CONTOUR_ENGINE_LIB_DIR")
            .expect("Set CONTOUR_ENGINE_LIB_DIR to the CMake build's slicer directory"),
    )
    .canonicalize()
    .expect("CONTOUR_ENGINE_LIB_DIR must be an existing directory");
    println!("cargo:rerun-if-changed={}", directory.display());
    println!("cargo:rustc-link-search=native={}", directory.display());
    println!("cargo:rustc-link-lib=dylib=contour_engine");
}
