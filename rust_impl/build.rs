use std::env;
use std::path::PathBuf;

fn main() {
    let cargo_manifest_dir: String = std::env::var("CARGO_MANIFEST_DIR").unwrap();

    let dst = cmake::build(format!("{cargo_manifest_dir}/../challenge_impl"));
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    println!("cargo:rustc-link-lib=static=PMA_Challenge");
    println!("cargo:rustc-link-lib=static=raylib");
    println!("cargo:rustc-link-lib=static=QuinqueFive_ttf");

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
