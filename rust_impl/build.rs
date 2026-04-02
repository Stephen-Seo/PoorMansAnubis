use std::env;
use std::path::PathBuf;

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

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

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
