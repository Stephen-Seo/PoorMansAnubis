mod ffi;

fn main() {
    println!("Generating value and factors of 10 digits:");
    let (v, f) = ffi::generate_value_and_factors_strings(10);
    println!("{}", v);
    println!("{}", f);
}
