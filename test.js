"use strict";

let values = [
"5932149216103954637651359556264729951830877885135277353425604884373557713570295240254423060322787281623730782972602721045751574766516069545276051092736165553542840078286492132680338541506920719222067845254620505803016319587586405007492868296365085934361705022839868044164645853733915291948463810191418712985488442496925520809804142484513124672273923989012271770753196338751159509110281434369352133955149420308979485999864742467500468950893459659051059835560926132735469143668225722830460320318489038563269633119926350547145249736268304840539730808388415917622797928315168322230773567253366255856431484456318913428089851313643085224848278758445075479337355263871622089175594640170407613100202582001279671883253178397884686653161655476075323974215777369901830413840976964935005384592909314275536386177029050902079494215845864556957919699714935359292545966298239584182586884895694119822712187500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
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
        if (m === 0) {
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
