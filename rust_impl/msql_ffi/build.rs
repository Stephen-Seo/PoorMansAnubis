use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    let cargo_manifest_dir: String = std::env::var("CARGO_MANIFEST_DIR").unwrap();

    let jobs: usize;

    {
        let command_output = Command::new("nproc")
            .output()
            .expect("Should be able to call \"nproc\"");
        let output_str = String::from_utf8(command_output.stdout)
            .expect("Should be able to get string from \"nproc\" output!");
        jobs = output_str
            .trim()
            .parse()
            .expect("Should be able to convert \"nproc\" output to integer!");
    }

    let mut command = Command::new("make");

    let command_output = command
        .env("RELEASE", "1")
        .args([
            "-C",
            &format!("{cargo_manifest_dir}/../../cxx_impl"),
            "-j",
            &format!("{jobs}"),
            "-l",
            &format!("{jobs}"),
            "libdb_msql_capi.a",
        ])
        .output()
        .expect("\"make\" on cxx_impl should have completed successfully!");

    if !command_output.status.success() {
        let err = String::from_utf8(command_output.stderr)
            .expect("Should be able to get string from command output!");
        panic!("{err}");
    }

    println!("cargo::rustc-link-search={cargo_manifest_dir}/../../cxx_impl");
    println!("cargo::rustc-link-lib=db_msql_capi");
    println!("cargo::rustc-link-lib=stdc++");

    let msql_bindings = bindgen::Builder::default()
        .header(format!(
            "{cargo_manifest_dir}/../../cxx_impl/src/db_msql_capi.h"
        ))
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings for \"db_msql_capi.h\"");

    msql_bindings
        .write_to_file(out_path.join("msql_bindings.rs"))
        .expect("Couldn't write bindings for \"msql\"!");
}
