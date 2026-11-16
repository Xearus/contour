use std::{env, path::PathBuf, process::ExitCode};

fn main() -> ExitCode {
    let arguments: Vec<PathBuf> = env::args_os().skip(1).map(PathBuf::from).collect();
    if arguments.len() != 2 && arguments.len() != 3 {
        eprintln!("Usage: slice INPUT.stl OUTPUT.gcode [CONFIG.ini]");
        return ExitCode::from(2);
    }
    match contour_engine::slice_stl(
        &arguments[0],
        &arguments[1],
        arguments.get(2).map(PathBuf::as_path),
    ) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("Slicing failed: {error}");
            ExitCode::FAILURE
        }
    }
}
