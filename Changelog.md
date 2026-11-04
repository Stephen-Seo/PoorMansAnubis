# Changelog

## Upcoming Changes

First versioned PoorMansAnubs with features:

 - Protects a destination url with 2 possible challenge-verification types
    - Verify via "factors" that is done automatically in the browser with JS
       - This method generates a large number and the client's answer must be all the factors of the large number
    - Verify via generated image and manual input with matching word (captcha)
       - This verification method defaults to JS/JSON submission, but can be changed to use a REST API by using the `--enable-rest-api` option
 - Implemented in both C++ and Rust
    - Rust version uses reqwest to forward requests, and salvo to handle requests
    - C++ version uses unix sockets for most requests, and libcurl (when enabled with `--enable-libcurl`) to forward requests to https destinations
 - Uses either Sqlite or MySQL to persist data (temporarily) to keep track of allowed clients, timed-out clients, and data related to challenges presented to the client

Check the `-h` or `--help` options for either Rust or C++ versions for more info.
