mod args;
mod ffi;

fn main() {
    let mut parsed_args = args::parse_args();
    if parsed_args.factors == None {
        parsed_args.factors = Some(10);
        println!("\"--factors=<digits>\" not specified, defaulting to \"10\"");
    }
    println!(
        "Generating value and factors of {} digits:",
        parsed_args.factors.as_ref().unwrap()
    );
    let (v, f) = ffi::generate_value_and_factors_strings(*parsed_args.factors.as_ref().unwrap());
    println!("{}", v);
    println!("{}", f);
}
