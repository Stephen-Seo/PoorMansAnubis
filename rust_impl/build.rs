use std::env;
use std::fs::File;
use std::io::Write;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let cargo_manifest_dir: String = std::env::var("CARGO_MANIFEST_DIR").unwrap();

    let dst = cmake::build(format!("{cargo_manifest_dir}/../challenge_impl"));
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    println!("cargo:rustc-link-lib=static=PMA_Challenge");
    println!("cargo:rustc-link-lib=static=raylib");
    println!("cargo:rustc-link-lib=static=JupiteroidBold");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

    let bindings = bindgen::Builder::default()
        .clang_arg(format!(
            "-I{cargo_manifest_dir}/../challenge_impl/third_party/SimpleArchiver/src"
        ))
        .header(format!("{cargo_manifest_dir}/../challenge_impl/src/work.h"))
        .header(format!(
            "{cargo_manifest_dir}/../challenge_impl/src/i_work.h"
        ))
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings for \"work.h\"");

    bindings
        .write_to_file(out_path.join("work_bindings.rs"))
        .expect("Couldn't write bindings for \"work\"!");

    let version_path = out_path.join("version.txt");
    let mut version_file =
        File::create(version_path).expect("Should be able to create version.txt at OUT_DIR");
    version_file
        .write(format!("{}", env!("CARGO_PKG_VERSION")).as_bytes())
        .expect("Should be able to write to OUT_DIR/version.txt");

    let git_version_path = out_path.join("git_version.txt");
    let git_cmd_out_res = Command::new("git")
        .args(["describe", "--tags", "--long"])
        .output();

    if let Ok(git_cmd_out) = git_cmd_out_res {
        let git_version_file_res = File::create(git_version_path);
        if let Ok(mut git_version_file) = git_version_file_res {
            git_version_file
                .write_all(git_cmd_out.stdout.trim_ascii())
                .ok();
            println!("cargo::rerun-if-changed={cargo_manifest_dir}/../.git");
        }
    }
}
