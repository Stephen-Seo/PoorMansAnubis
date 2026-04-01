use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let cargo_manifest_dir: String = std::env::var("CARGO_MANIFEST_DIR").unwrap();

    cc::Build::new()
        .cpp(true)
        .cpp_link_stdlib_static(true)
        .file(format!(
            "{cargo_manifest_dir}/../challenge_impl/src/work2.cc"
        ))
        .include(format!(
            "{cargo_manifest_dir}/../challenge_impl/third_party/SimpleArchiver/src"
        ))
        .compile("cpp_work");

    cc::Build::new()
        .file(format!("{cargo_manifest_dir}/../challenge_impl/src/work.c"))
        .file(format!("{cargo_manifest_dir}/../challenge_impl/src/base64.c"))
        .file(format!("{cargo_manifest_dir}/../challenge_impl/third_party/SimpleArchiver/src/data_structures/linked_list.c"))
        .file(format!("{cargo_manifest_dir}/../challenge_impl/third_party/SimpleArchiver/src/data_structures/chunked_array.c"))
        .file(format!("{cargo_manifest_dir}/../challenge_impl/third_party/SimpleArchiver/src/data_structures/priority_heap.c"))
        .include(format!("{cargo_manifest_dir}/../challenge_impl/third_party/SimpleArchiver/src"))
        .compile("c_work");

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
        .args([
            "-C",
            &format!("{cargo_manifest_dir}/../cxx_impl/bundled"),
            "-j",
            &format!("{jobs}"),
        ])
        .output()
        .expect("\"make\" on cxx_impl should have completed successfully!");

    if !command_output.status.success() {
        let err = String::from_utf8(command_output.stderr)
            .expect("Should be able to get string from command output!");
        panic!("{err}");
    }

    println!("cargo::rustc-link-search={cargo_manifest_dir}/../cxx_impl");
    println!("cargo::rustc-link-search={cargo_manifest_dir}/../cxx_impl/bundled/out/ssl/usr/lib");
    println!("cargo::rustc-link-lib=db_msql_capi");
    println!("cargo::rustc-link-lib=crypto");
    println!("cargo::rustc-link-lib=stdc++");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

    let msql_bindings = bindgen::Builder::default()
        .header(format!(
            "{cargo_manifest_dir}/../cxx_impl/src/db_msql_capi.h"
        ))
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings for \"db_msql_capi.h\"");

    msql_bindings
        .write_to_file(out_path.join("msql_bindings.rs"))
        .expect("Couldn't write bindings for \"msql\"!");

    let bindings = bindgen::Builder::default()
        .clang_arg(format!(
            "-I{cargo_manifest_dir}/../challenge_impl/third_party/SimpleArchiver/src"
        ))
        .header(format!("{cargo_manifest_dir}/../challenge_impl/src/work.h"))
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings for \"work.h\"");

    bindings
        .write_to_file(out_path.join("work_bindings.rs"))
        .expect("Couldn't write bindings for \"work\"!");
}
