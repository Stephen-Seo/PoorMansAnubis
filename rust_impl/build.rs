use std::env;
use std::path::PathBuf;

fn main() {
    cc::Build::new()
        .file("../c_impl/src/work.c")
        .file("../c_impl/third_party/SimpleArchiver/src/data_structures/linked_list.c")
        .file("../c_impl/third_party/SimpleArchiver/src/data_structures/chunked_array.c")
        .file("../c_impl/third_party/SimpleArchiver/src/data_structures/priority_heap.c")
        .include("../c_impl/third_party/SimpleArchiver/src")
        .compile("c_work");

    let bindings = bindgen::Builder::default()
        .clang_arg("-I../c_impl/third_party/SimpleArchiver/src")
        .header("../c_impl/src/work.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings for \"work.h\"");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("work_bindings.rs"))
        .expect("Couldn't write bindings for \"work\"!");
}
