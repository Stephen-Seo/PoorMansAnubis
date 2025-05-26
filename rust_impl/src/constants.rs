// ISC License
//
// Copyright (c) 2025 Stephen Seo
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

pub const DEFAULT_FACTORS_DIGITS: u64 = 17000;
pub const DEFAULT_JSON_MAX_SIZE: usize = 50000;
pub const ALLOWED_IP_TIMEOUT_MINUTES: u64 = 60;
pub const CHALLENGE_FACTORS_TIMEOUT_MINUTES: u64 = 7;

pub const HTML_BODY_FACTORS: &str = r#"<!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="utf-8">
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
            pre {
                font-size: 32px;
            }
        </style>
    </head>
    <body>
        <h2 class="center">Checking Your Browser...</h2>
        <pre id="progress" class="center">Waiting to start verification...</pre>
        <script>
            "use strict";

            const progress_values = ["-", "\\", "|", "/"];
            let progress_idx = 0;
            let progress_text = document.getElementById("progress");
            function update_anim() {
                progress_idx = (progress_idx + 1) % progress_values.length;
                progress_text.innerText = progress_values[
                    progress_idx
                ];
            }
            var interval_id = -1;

            if (!window.Worker) {
                console.warn("Workers are not available!?");
            }

            const worker = new Worker("{JS_FACTORS_URL}");

            worker.addEventListener("message", (message) => {
                if (message.data.status === "done") {
                    if (interval_id >= 0) {
                        clearInterval(interval_id);
                        interval_id = -1;
                    }
                    progress_text.innerText = "Verified.";
                    window.location.reload(true);
                } else if (message.data.status === "error_from_api") {
                    if (interval_id >= 0) {
                        clearInterval(interval_id);
                        interval_id = -1;
                    }
                    setTimeout(() => {
                        progress_text.innerText = "Error, verification failed!";
                    }, 500);
                } else if (message.data.status === "error_decoding") {
                    if (interval_id >= 0) {
                        clearInterval(interval_id);
                        interval_id = -1;
                    }
                    setTimeout(() => {
                        progress_text.innerText = "Error, failed to decode challenge!";
                    }, 500);
                } else {
                    if (message.data.status === "Starting...") {
                        if (interval_id >= 0) {
                            clearInterval(interval_id);
                        }
                        interval_id = setInterval(update_anim, 500);
                    }
                    console.log(message.data.status);
                }
            });

            worker.addEventListener("error", (e) => {
                console.error(e.message);
                console.error(e.lineno);
            });

            addEventListener("load", (event) => {
                setTimeout(() => {
                    worker.postMessage("start");
                }, 50);
            });
        </script>
    </body>
    </html>
"#;

pub const JAVASCRIPT_FACTORS_WORKER: &str = r#""use strict";

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

function b64_to_val(c) {
    c = c.charCodeAt(0);
    if (c >= 'A'.charCodeAt(0) && c <= 'Z'.charCodeAt(0)) {
        return c - 'A'.charCodeAt(0);
    } else if (c >= 'a'.charCodeAt(0) && c <= 'z'.charCodeAt(0)) {
        return c - 'a'.charCodeAt(0) + 26;
    } else if (c >= '0'.charCodeAt(0) && c <= '9'.charCodeAt(0)) {
        return c - '0'.charCodeAt(0) + 52;
    } else if (c === '+'.charCodeAt(0)) {
        return 62;
    } else if (c === '/'.charCodeAt(0)) {
        return 63;
    } else {
        return 0xFF;
    }
}

function b64_to_str(b64) {
    let out = "";

    let current = 0;
    let current_len = 0;
    let temp = 0;

    for (let idx = 0; idx < b64.length; ++idx) {
        temp = b64_to_val(b64[idx]);
        if (temp === 0xFF) {
            return undefined;
        }
        current = (current << 6) + temp;
        current_len += 6;
        while (current_len >= 4) {
            temp = current_len - 4;
            temp = current >> temp;
            if (temp < 10) {
                out += new String(temp);
            }
            current_len -= 4;
            temp = 0;
            for (let temp2 = 0; temp2 < current_len; ++temp2) {
                temp = (temp << 1) | 1;
            }
            current = current & temp;
        }
    }
    if (current_len == 2 && current != 3) {
        return undefined;
    }

    return out;
}

function getFactors() {
    let value = "{LARGE_NUMBER}";
    value = b64_to_str(value);
    if (value === undefined) {
        postMessage({status: "error_decoding"});
        return;
    }
    let factor = 2;
    let factor_count = 0;
    let f_str = "";
    let iter = 0;

    while (1) {
        let div_str = "";
        let modulus_str = "";
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
            ++factor_count;

            if (div_str === "1") {
                if (f_str.length !== 0) {
                    f_str += " ";
                }
                f_str += new String(factor) + "x" + new String(factor_count);
                break;
            } else {
                value = div_str;
            }
        } else {
            if (factor_count !== 0) {
                if (f_str.length !== 0) {
                    f_str += " ";
                }
                f_str += new String(factor) + "x" + new String(factor_count);
            }
            factor += 1;
            factor_count = 0;
        }
    }

    let xhr = new XMLHttpRequest();
    let url = "{API_URL}";
    xhr.open("POST", url, true);
    xhr.setRequestHeader("Content-Type", "application/json");
    xhr.onreadystatechange = function () {
        if (xhr.readyState === 4) {
            if (xhr.status === 200) {
                postMessage({status: "done"});
            } else {
                postMessage({status: "error_from_api"});
            }
        }
    };
    let data = JSON.stringify({"type": "factors",
                               "id": "{UUID}",
                               "factors": f_str});
    xhr.send(data);
}

addEventListener("message", (message) => {
    if (message.data === "start") {
        postMessage({status: "Starting..."});
        getFactors();
    } else {
        postMessage({status: "Invalid start message."});
    }
});
"#;
