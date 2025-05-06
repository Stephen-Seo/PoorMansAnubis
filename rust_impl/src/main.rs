mod args;
mod ffi;

const DEFAULT_FACTORS_DIGITS: u64 = 17000;

fn main() {
    let mut parsed_args = args::parse_args();
    if parsed_args.factors == None {
        parsed_args.factors = Some(DEFAULT_FACTORS_DIGITS);
        println!(
            "\"--factors=<digits>\" not specified, defaulting to \"{}\"",
            DEFAULT_FACTORS_DIGITS
        );
    }
    println!(
        "Generating value and factors of {} digits:",
        parsed_args.factors.as_ref().unwrap()
    );
    let (v, f) = ffi::generate_value_and_factors_strings(*parsed_args.factors.as_ref().unwrap());
    println!("{}", v);
    println!("{}", f);
}
