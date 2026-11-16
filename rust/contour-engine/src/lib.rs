//! Blocking STL-to-G-code access to Contour's experimental native engine.
//!
//! Build the native engine separately and set `CONTOUR_ENGINE_LIB_DIR` when
//! building this crate. Calls are serialized by the native library. Output is
//! generic experimental G-code, not a printer-ready job. Native faults are not
//! isolated: use a separate process for untrusted models.

use std::{
    ffi::{c_char, c_int, CStr, CString},
    fmt,
    path::{Path, PathBuf},
    ptr,
};

extern "C" {
    fn contour_slice_stl(
        input_path: *const c_char,
        output_path: *const c_char,
        config_path: *const c_char,
        error: *mut c_char,
        error_capacity: usize,
    ) -> c_int;
}

#[derive(Debug)]
pub enum Error {
    InvalidPath(PathBuf),
    InvalidArgument(String),
    SlicingFailed(String),
    Internal(String),
}

impl fmt::Display for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidPath(path) => {
                write!(formatter, "Unrepresentable native path: {}", path.display())
            }
            Self::InvalidArgument(message)
            | Self::SlicingFailed(message)
            | Self::Internal(message) => formatter.write_str(message),
        }
    }
}

impl std::error::Error for Error {}

fn native_path(path: &Path) -> Result<CString, Error> {
    #[cfg(unix)]
    let bytes = {
        use std::os::unix::ffi::OsStrExt;
        path.as_os_str().as_bytes()
    };
    #[cfg(not(unix))]
    let bytes = path
        .to_str()
        .ok_or_else(|| Error::InvalidPath(path.to_owned()))?
        .as_bytes();
    CString::new(bytes).map_err(|_| Error::InvalidPath(path.to_owned()))
}

/// Slice an STL, optionally overriding upstream defaults with an INI file.
///
/// Existing output paths are rejected. File creation is not atomic; failures
/// after export starts may leave a partial output. Diagnostics may be truncated.
/// Unix paths preserve native bytes; other platforms currently require UTF-8.
pub fn slice_stl(
    input: impl AsRef<Path>,
    output: impl AsRef<Path>,
    config: Option<&Path>,
) -> Result<(), Error> {
    let input = native_path(input.as_ref())?;
    let output = native_path(output.as_ref())?;
    let config = config.map(native_path).transpose()?;
    let mut diagnostic = [0 as c_char; 4096];
    // All strings and writable diagnostic storage remain valid for the blocking
    // call; the native boundary catches exceptions and serializes engine access.
    let status = unsafe {
        contour_slice_stl(
            input.as_ptr(),
            output.as_ptr(),
            config.as_ref().map_or(ptr::null(), |path| path.as_ptr()),
            diagnostic.as_mut_ptr(),
            diagnostic.len(),
        )
    };
    if status == 0 {
        return Ok(());
    }
    // Native diagnostics are NUL-terminated even when truncated.
    let message = unsafe { CStr::from_ptr(diagnostic.as_ptr()) }
        .to_string_lossy()
        .into_owned();
    Err(match status {
        1 => Error::InvalidArgument(message),
        2 => Error::SlicingFailed(message),
        3 => Error::Internal(message),
        other => Error::Internal(format!("Unknown engine status {other}: {message}")),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rejects_nul_in_any_path_before_entering_native_code() {
        let invalid = Path::new("invalid\0path");
        for result in [
            slice_stl(invalid, "output.gcode", None),
            slice_stl("input.stl", invalid, None),
            slice_stl("input.stl", "output.gcode", Some(invalid)),
        ] {
            assert!(matches!(result, Err(Error::InvalidPath(_))));
        }
    }

    #[test]
    fn native_argument_errors_remain_distinct_from_slicing_failures() {
        assert!(matches!(
            slice_stl("", "output.gcode", None),
            Err(Error::InvalidArgument(_))
        ));
        let directory = Path::new(env!("OUT_DIR"));
        let result = slice_stl(
            directory.join("missing-model.stl"),
            directory.join("unused-output.gcode"),
            None,
        );
        assert!(matches!(&result, Err(Error::SlicingFailed(message)) if message.contains("STL")));
    }
}
