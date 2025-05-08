pub const HTML_BODY_FACTORS: &str = "
    <!DOCTYPE html>
    <html lang=\"en\">
    <head>
        <meta charset=\"utf-8\">
        <title>Checking Your Browser...</title>
        <style>
            body {
                color: #FFF;
                background: #555;
                font-family: sans-serif;
            }
            .center {
                text-align: center;
                display: block;
                margin-left: auto;
                margin-right: auto;
            }
        </style>
    </head>
    <body>
        <h2 class=\"center\">Checking Your Browser...</h2>
        <script>
            \"use strict\";

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

            addEventListener(\"load\", (event) => {
                let factors = [];
                let factor = 2;
                let value = \"{}\";
                while (1) {
                    let div_str = \"\";
                    let modulus_str = \"\";
                    let mod = 0;
                    for (let idx = 0; idx < value.length; ++idx) {
                        let v_value = str_to_value(value[idx]);

                        let div = parseInt((mod * 10 + v_value) / factor);
                        mod = (mod * 10 + v_value) % factor;
                        if (div_str.length !== 0 || div !== 0) {
                            div_str += div;
                        }
                    }

                    if (mod === 0) {
                        factors.push(factor);

                        if (div_str === \"1\") {
                            break;
                        } else {
                            value = div_str;
                        }
                    } else {
                        factor += 1;
                    }
                }

                let f_string = \"\";
                let first = 1;
                for (let idx = 0; idx < factors.length; ++idx) {
                    if (first === 1) {
                        f_string += factors[idx];
                        first = 0;
                    } else {
                        f_string += \" \" + factors[idx];
                    }
                }

                let xhr = new XMLHttpRequest();
                let url = \"{}\";
                xhr.open(\"POST\", url, true);
                xhr.setRequestHeader(\"Content-Type\", \"application/json\");
                xhr.onreadystatechange = function () {
                    if (xhr.readyState === 4 && xhr.status === 200) {
                        window.location.reload(true);
                    }
                };
                let data = JSON.stringify({\"type\": \"factors\", \"id\": \"{}\", \"factors\": f_string});
                xhr.send(data);
            });
        </script>
    </body>
    </html>
";
