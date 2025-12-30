"use strict";

//let values = [
//String(process.argv[2])
//];

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

function long_div_mod(value_str, factor) {
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
        let m_d = long_div_mod(value_str, factor);
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

//for (let idx = 0; idx < values.length; ++idx) {
//    console.log(values[idx]);
//    let factors = get_factors(values[idx]);
//    let f_string = "";
//    let first = 1;
//    for (let fidx = 0; fidx < factors.length; ++fidx) {
//        if (first === 1) {
//            f_string += factors[fidx];
//            first = 0;
//        } else {
//            f_string += " " + factors[fidx];
//        }
//    }
//    console.log(f_string);
//}

function b64_b64_to_value(b64) {
    let c = b64.charCodeAt(0);
    if (c >= 0x41 && c <= 0x5A) {
        return c - 0x41;
    } else if (c >= 0x61 && c <= 0x7A) {
        return c - 0x61 + 26;
    } else if (c >= 0x30 && c <= 0x39) {
        return c - 0x30 + 52;
    } else if (c === 0x2B || c === 0x2D) {
        return 62;
    } else if (c === 0x2F || c === 0x5F) {
        return 63;
    }

    throw new Error("b64_b64_to_value not b64! b64 is: " + b64);
}

function b64_value_to_b64(val) {
    if (val >= 0 && val <= 25) {
        return String.fromCharCode(val + 0x41);
    } else if (val >= 26 && val <= 51) {
        return String.fromCharCode(val + 0x61 - 26);
    } else if (val >= 52 && val <= 61) {
        return String.fromCharCode(val + 0x30 - 52);
    } else if (val === 62) {
        return "+";
    } else if (val === 63) {
        return "/";
    }

    throw new Error("b64_value_to_b64 invalid value! val is: " + val);
}

// Probably unnecessary
//function b64_invert(b64) {
//    return b64_value_to_b64(63 - b64_b64_to_value(b64));
//}

// Probably unnecessary
//function b64_reverse(b64_str) {
//    let new_str = "";
//    for (let idx = 0; idx < b64_str.length; ++idx) {
//        new_str += b64_str[b64_str.length - 1 - idx];
//    }
//    return new_str;
//}

function revb64_long_div_mod(b64_str, val) {
    let result = "";
    let rem = 0;
    for (let idx = 0; idx < b64_str.length; ++idx) {
        let b64_val = b64_b64_to_value(b64_str[b64_str.length - 1 - idx])
                    + rem * 64;
        let inner_result = b64_value_to_b64(Math.floor(b64_val / val));
        if (result.length !== 0 || inner_result !== "A") {
            result = inner_result + result;
        }
        rem = b64_val % val;
    }
    return [result, rem];
}

//console.log("input: " + process.argv[2]);
//let reversed = b64_reverse(process.argv[2]);
//console.log("reversed input: " + reversed);
//let ret = revb64_long_div_mod(process.argv[2], 2);
//console.log("div by 2: " + ret[0]);
//console.log("remainder: " + ret[1]);

let ret = [ process.argv[2], 0 ];
console.log("input: " + ret[0]);
let current = 2;
let current_count = 0;
let result = [];
let ticks = 1000000;
while (ret[0].length > 1 || ret[0][0] != "B") {
    let inner_ret = revb64_long_div_mod(ret[0], current);
    if (inner_ret[1] === 0) {
        current_count += 1;
        ret = inner_ret;
    } else {
        if (current_count !== 0) {
            result.push(String(current) + "x" + String(current_count));
        }
        if (current === 2) {
            current += 1;
        } else {
            current += 2;
        }
        current_count = 0;
    }
    //console.log(current + ": " + ret[0] + ", " + ret[1]);
    if (--ticks === 0) {
        break;
    }
}

if (current_count !== 0) {
    result.push(String(current) + "x" + String(current_count));
}

for (let idx = 0; idx < result.length; ++idx) {
    console.log(result[idx]);
}
