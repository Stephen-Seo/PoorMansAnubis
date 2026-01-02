// ISC License
//
// Copyright (c) 2025-2026 Stephen Seo
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

pub const DEFAULT_FACTORS_QUADS: u64 = 2200;
pub const DEFAULT_JSON_MAX_SIZE: usize = 50000;
pub const ALLOWED_IP_TIMEOUT_MINUTES: u64 = 60;
pub const CHALLENGE_FACTORS_TIMEOUT_MINUTES: u64 = 2;

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
                console.error(e);
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

function val_to_b64(val) {
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

    return "A";
}

function revb64_long_div_mod(b64_str, val) {
    let result = "";
    let rem = 0;
    for (let idx = 0; idx < b64_str.length; ++idx) {
        let b64_val = b64_to_val(b64_str[b64_str.length - 1 - idx])
                    + rem * 64;
        let inner_result = val_to_b64(Math.floor(b64_val / val));
        if (result.length !== 0 || inner_result !== "A") {
            result = inner_result + result;
        }
        rem = b64_val % val;
    }
    return [result, rem];
}

function getFactors() {
    let ret = [ "{LARGE_NUMBER}", 0 ];

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

    let result_str = "";
    for (let idx = 0; idx < result.length; ++idx) {
        result_str += result[idx] + " ";
    }
    result_str = result_str.trim();

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
                               "factors": result_str});
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
