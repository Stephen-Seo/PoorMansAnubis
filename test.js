"use strict";

let values = [
"32311985263658100",
"13103345182677750",
"13602088424925000",
"76156194224110000",
"75742369728480000",
"8717507803294290",
"13199340385481250",
"34876428538551600",
"7715287814434200",
"5856514053390000"
];

function str_to_value(s) {
    let value = 0;
    let digit = 0;

    for (let idx = s.length; idx-- > 0;) {
        let sub_value = parseInt(s[idx]);
        for (let didx = 0; didx < digit; ++didx) {
            sub_value *= 10;
        }
        value += sub_value;
        ++digit;
    }

    return value;
}

function long_modulus(value_str, factor_str) {
    let factor = str_to_value(factor_str);
    let modulus = 0;
    for (let idx = 0; idx < value_str.length; ++idx) {
        let v_value = str_to_value(value_str[idx]);
        modulus = (modulus * 10 + v_value) % factor;
    }
    return modulus;
}

function long_div(value_str, factor_str) {
    let factor = str_to_value(factor_str);
    let div_str = "";
    let mod = 0;
    for (let idx = 0; idx < value_str.length; ++idx) {
        let v_value = str_to_value(value_str[idx]);
        let div = parseInt((mod * 10 + v_value) / factor);
        mod = (mod * 10 + v_value) % factor;
        if (div_str.length != 0 || div != 0) {
            div_str += div;
        }
    }

    return div_str;
}

function get_factors(value_str) {
    let factor = 2;
    let modulus_str = "";
    let first_print = 1;
    let factors = [];
    while (1) {
        let m = long_modulus(value_str, String(factor));
        if (m == 0) {
            value_str = long_div(value_str, String(factor));
            factors.push(factor);

            if (value_str === "1") {
                break;
            }
        } else {
            factor += 1;
        }
    }
    return factors;
}

for (let idx = 0; idx < values.length; ++idx) {
    console.log(values[idx]);
    console.log(get_factors(values[idx]));
}
