"use strict";

let values = [
String(process.argv[2])
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

function long_div_mod(value_str, factor_str) {
    let factor = str_to_value(factor_str);
    let div_str = "";
    let modulus_str = "";
    let mod = 0;
    for (let idx = 0; idx < value_str.length; ++idx) {
        let v_value = str_to_value(value_str[idx]);

        let div = parseInt((mod * 10 + v_value) / factor);
        mod = (mod * 10 + v_value) % factor;
        if (div_str.length != 0 || div != 0) {
            div_str += div;
        }
    }

    return [mod, div_str];
}

function get_factors(value_str) {
    let factor = 2;
    let modulus_str = "";
    let first_print = 1;
    let factors = [];
    while (1) {
        let m_d = long_div_mod(value_str, String(factor));
        if (m_d[0] === 0) {
            factors.push(factor);

            if (m_d[1] === "1") {
                break;
            } else {
                value_str = m_d[1];
            }
        } else {
            factor += 1;
        }
    }
    return factors;
}

for (let idx = 0; idx < values.length; ++idx) {
    console.log(values[idx]);
    let factors = get_factors(values[idx]);
    let f_string = "";
    let first = 1;
    for (let fidx = 0; fidx < factors.length; ++fidx) {
        if (first === 1) {
            f_string += factors[fidx];
            first = 0;
        } else {
            f_string += " " + factors[fidx];
        }
    }
    console.log(f_string);
}
