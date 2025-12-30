================================================================================

POOR MAN'S ANUBIS: Website Access Challenge Verification

================================================================================

This software is heavily inspired by Anubis.
(https://github.com/TecharoHQ/anubis)

There is currently one method of challenge verification: factors of a large
number. It is generated per request, and the challenge response should be the
factors of the number in sorted increasing order.

The "challenge_impl" contains the challenge generation code.

The "rust_impl" contains the frontend code and is the "meat" of the software.

The "cxx_impl" contains a C++ implementation that should function the same as
the "rust_impl". It is more lightweight and there are some differences:

- cxx_impl uses `libcurl` while rust_impl uses `reqwest`
- cxx_impl uses Unix-style ipv4/ipv6 socket handling while rust_impl uses
  `salvo`
- cxx_impl uses `sqlite` while rust_impl uses `mariadb` (or `mysql`) by default
    (it is possible to use `sqlite` with the rust_impl)
- cxx_impl uses `blake3` primarly for hashing and the rust_impl also does so


================================================================================

USAGE

================================================================================

Build the "cxx_backend" (in "cxx_impl"). If there are problems using it, you
can fall back to the Rust frontend (in "rust_impl") . You may need to do a `git
submodule update --init --recursive` to pull in the dependency on
SimpleArchiver.

The C++ impl uses sqlite, so a path given in the paramaters is only required.
MySQL/MariaDB support was added later in the C++ impl, and is only enabled if
"--mysql-conf=<filename>" is specified. The config file format is the same as
the rust_impl/mysql.conf config. Note that it may be insecure to connect to
MySQL/MariaDB outside of the local network, so it would be better to use a VPN
if your DB is hosted non-locally.

The Rust impl by default requires a MySQL server with a database and user with
password with access to said database. If using MySQL, then it is recommended to
use MariaDB.

Sqlite usage is also possible with the Rust impl, by compiling with the
following options:

Enable only sqlite:
cargo build --no-default-features --features sqlite

or

Enable both mysql and sqlite:
cargo build --all-features

Check "rust_impl/src/args.rs" for flags you can pass to the Rust impl. Also
check "rust_impl/mysql.conf" for how the config file should be set up for MySQL
access.

Point your webserver to the Rust-frontend's listening ip and port with the full
url as if it were the endpoint, and use a flag on the frontend to determine
where PoorMan'sAnubis will load from on challenge success.

You may use the "x-real-ip" header to ensure the frontend knows the correct
ip address. (The "--enable-x-real-ip-header" flag enables this.)

Args for the Rust-frontend are as follows:

Args:
  --factors=<digits> : Generate factors challenge with <digits> digits
  --dest-url=<url> : Destination URL for verified clients;
    example: "--dest-url=http://127.0.0.1:9999"
  --addr-port=<addr>:<port> : Listening addr/port;
    example: "--addr-port=127.0.0.1:8080"
  NOTICE: Specify --addr-port=... multiple times to listen on multiple ports
  NOTE: There is no longer a hard limit on the number of ports one can listen to
  --port-to-dest-url=<port>:<url> : Ensure requests from listening on <port> is forwarded to <url>
  example: "--port-to-dest-url=9001:https://example.com"
  NOTICE: Specify --port-to-dest-url=... multiple times to add more mappings
  --mysql-conf=<config_file> : Set path to config file for mysql settings
  --sqlite-path=<filename> : Set sqlite db filename path
  --enable-x-real-ip-header : Enable trusting "x-real-ip" header as client ip addr
  --api-url=<url> : Set endpoint for client to POST to this software;
    example: "--api-url=/pma_api"
  --js-factors-url=<url> : Set endpoint for client to request factors.js from this software;
    example: "--js-factors-url=/pma_factors.js"
  --challenge-timeout=<minutes> : Set minutes for how long challenge answers are stored in db
  --allowed-timeout=<minutes> : Set how long a client is allowed to access before requiring challenge again
  --enable-override-dest-url : Enable "override-dest-url" request header to determine where to forward;
    example header: "override-dest-url: http://127.0.0.1:8888"
  WARNING: If --enable-override-dest-url is used, you must ensure that
    PoorMansAnubis always receives this header as set by your server. If you
    don't then anyone accessing your server may be able to set this header and
    direct PoorMansAnubis to load any website!
    If you are going to use this anyway, you must ensure that a proper firewall is configured!
  --important-warning-has-been-read : Use this option to enable potentially dangerous options

thread 'main' panicked at src/main.rs:1082:46:
called `Result::unwrap()` on an `Err` value: Generic("Printed help text")
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace


Args for the C++ implementation are as follows:

    Args:
      --factors=<digits> : Generate factors challenge with <digits> digits
      --dest-url=<url> : Destination URL for verified clients;
        example: "--dest-url=http://127.0.0.1:9999"
      --addr-port=<addr>:<port> : Listening addr/port;
        example: "--addr-port=127.0.0.1:8080"
      NOTICE: Specify --addr-port=... multiple times to listen on multiple ports
      NOTE: There is no longer a hard limit on the number of ports one can listen to
      --port-to-dest-url=<port>:<url> : Ensure requests from listening on <port> is forwarded to <url>
      example: "--port-to-dest-url=9001:https://example.com"
      NOTICE: Specify --port-to-dest-url=... multiple times to add more mappings
      --mysql-conf=<config_file> : Set path to config file for mysql settings
      --sqlite-path=<path> : Set filename for sqlite db
      --enable-x-real-ip-header : Enable trusting "x-real-ip" header as client ip addr
      --api-url=<url> : Set endpoint for client to POST to this software;
        example: "--api-url=/pma_api"
      --js-factors-url=<url> : Set endpoint for client to request factors.js from this software;
        example: "--js-factors-url=/pma_factors.js"
      --challenge-timeout=<minutes> : Set minutes for how long challenge answers are stored in db
      --allowed-timeout=<minutes> : Set how long a client is allowed to access before requiring challenge again
      --enable-override-dest-url : Enable "override-dest-url" request header to determine where to forward;
        example header: "override-dest-url: http://127.0.0.1:8888"
      WARNING: If --enable-override-dest-url is used, you must ensure that
        PoorMansAnubis always receives this header as set by your server. If you
        don't then anyone accessing your server may be able to set this header and
        direct PoorMansAnubis to load any website!
        If you are going to use this anyway, you must ensure that a proper firewall is configured!
      --important-warning-has-been-read : Use this option to enable potentially dangerous options

================================================================================

EXAMPLE Nginx CONFIG

================================================================================
An example nginx config for this is as follows:

# My Site I want to Protect with PoorMan'sAnubis
server {
    listen 9999;
    server_name localhost 127.0.0.1;

    set_real_ip_from 127.0.0.1;
    real_ip_header x-real-ip;

    location / {
        root /srv/http/mysite;
        autoindex on;
        index index.html;
    }
}

# Access to Frontend to PoorMan'sAnubis
server {
    listen 443 ssl;
    ssl_certificate ...;
    ssl_certificate_key ...;
    # other ssl cert stuff e.g. Let'sEncrypt

    server_name example.com;

    location / {
        proxy_set_header 'x-real-ip' $remote_addr;
        proxy_pass http://127.0.0.1:8888;
    }

    # Needed if would point to elsewhere otherwise.
    # Check the args to customize this endpoint.
    location /pma_factors.js {
        proxy_set_header 'x-real-ip' $remote_addr;
        proxy_pass http://127.0.0.1:8888;
    }

    # Needed if would point to elsewhere otherwise.
    # Check the args to customize this endpoint.
    location /pma_api {
        proxy_set_header 'x-real-ip' $remote_addr;
        proxy_pass http://127.0.0.1:8888;
    }
}

 -------------      -----------------------------      --------------
|nginx ...:443| -> |PoorMansAnubis 127.0.0.1:8888| -> |nginx ...:9999|
 -------------      -----------------------------      --------------

This assumes PoorMan'sAnubis is listening on port 8888 and will forward requests
to 127.0.0.1:9999 with the url passed verbatim. Check the args for details.


PoorMan'sAnubis can listen on multiple ports by specifying
"--addr-port=..." multiple times.

For example, using:
    --addr-port=127.0.0.1:8888
    --addr-port=127.0.0.1:9001
    --addr-port=127.0.0.1:9002
    --port-to-dest-url=8888:http://127.0.0.1:9999
    --port-to-dest-url=9001:https://google.com
    --port-to-dest-url=9002:https://microsoft.com

Will work as expected. If there is no matching port mapping, the default dest
url is used. The default can be set with "--dest-url=<url>".


PoorMan'sAnubis can protect multiple endpoints by accepting a header by using
"--enable-override-dest-url" (YOU MUST READ THE WARNING ABOUT THIS FLAG!):

server {
    listen 443 ssl;
    ssl_certificate ...;
    ssl_certificate_key ...;
    # other ssl cert stuff e.g. Let'sEncrypt

    server_name example.com;

    # Prevent hijacking of 'override-dest-url' in case it isn't specified.
    proxy_set_header 'override-dest-url' "";

    location / {
        proxy_set_header 'x-real-ip' $remote_addr;
        proxy_set_header 'override-dest-url' 'http://127.0.0.1:9999';
        proxy_pass http://127.0.0.1:8888;
    }

    location /other_site {
        # "rewrite" may be needed if destination uses different "sub-urls".
        rewrite /other_site(.*) $1 break;

        proxy_set_header 'x-real-ip' $remote_addr;
        proxy_set_header 'override-dest-url' 'http://127.0.0.1:12121';
        proxy_pass http://127.0.0.1:8888;
    }

    # Needed if would point to elsewhere otherwise.
    # Check the args to customize this endpoint.
    location /pma_factors.js {
        proxy_set_header 'x-real-ip' $remote_addr;
        proxy_pass http://127.0.0.1:8888;
    }

    # Needed if would point to elsewhere otherwise.
    # Check the args to customize this endpoint.
    location /pma_api {
        proxy_set_header 'x-real-ip' $remote_addr;
        proxy_pass http://127.0.0.1:8888;
    }
}

The warning about using 'override-dest-url' is repeated here:

WARNING: If --enable-override-dest-url is used, you must ensure that
PoorMansAnubis always receives this header as set by your server. If you don't
then anyone accessing your server may be able to set this header and direct
PoorMansAnubis to load any website!

A nginx directive you can use to prevent this from happening is:

proxy_set_header 'override-dest-url' "";

https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_set_header

It may be safer to rely on multiple "--addr-port=..." and
"--port-to-dest-url=..." instead of using "--enable-override-dest-url".


================================================================================

Other Info

================================================================================

Contact me at:

sseo6@alumni.jh.edu
seo.disparate@gmail.com
stephen@seodisparate.com
