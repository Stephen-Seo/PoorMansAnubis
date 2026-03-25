use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    cc::Build::new()
        .cpp(true)
        .cpp_link_stdlib_static(true)
        .file("../challenge_impl/src/work2.cc")
        .include("../challenge_impl/third_party/SimpleArchiver/src")
        .compile("cpp_work");

    cc::Build::new()
        .file("../challenge_impl/src/work.c")
        .file("../challenge_impl/src/base64.c")
        .file("../challenge_impl/third_party/SimpleArchiver/src/data_structures/linked_list.c")
        .file("../challenge_impl/third_party/SimpleArchiver/src/data_structures/chunked_array.c")
        .file("../challenge_impl/third_party/SimpleArchiver/src/data_structures/priority_heap.c")
        .include("../challenge_impl/third_party/SimpleArchiver/src")
        .compile("c_work");

    Command::new("make")
        .args(["-C", "../cxx_impl", "libdb_msql_capi.a"])
        .spawn()
        .expect("Should be able to \"make\" on cxx_impl!")
        .wait()
        .expect("\"make\" on cxx_impl should have completed successfully!");

    println!("cargo::rustc-link-search=../cxx_impl");
    println!("cargo::rustc-link-lib=db_msql_capi");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

    let msql_bindings = bindgen::Builder::default()
        .header("../cxx_impl/src/db_msql_capi.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings for \"db_msql_capi.h\"");

    msql_bindings
        .write_to_file(out_path.join("msql_bindings.rs"))
        .expect("Couldn't write bindings for \"msql\"!");

    let bindings = bindgen::Builder::default()
        .clang_arg("-I../challenge_impl/third_party/SimpleArchiver/src")
        .header("../challenge_impl/src/work.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings for \"work.h\"");

    bindings
        .write_to_file(out_path.join("work_bindings.rs"))
        .expect("Couldn't write bindings for \"work\"!");
}
