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
- cxx_impl uses `blake3` primarly for hashing and the rust_impl also does so

================================================================================

COMPILING

================================================================================

cxx_impl/ contains a Makefile that builds a Debug build and expects dependencies
already installed on the system. Use "RELEASE=1 make ..." to build a Release
build.

cxx_impl/bundled/ contains a Makefile that builds all dependencies and links
them statically to the executable which is also placed in cxx_impl/ .


rust_impl/ contains a Rust project which will call `make` on cxx_impl/bundled/
as it uses the cxx_impl's msql code and at least 1 library built in the bundled
build. Note that because of this, compilling the Rust project will be slow as it
builds all of cxx_impl/bundled/ as well as its own dependencies.


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

The Rust impl provides support for both sqlite and MySQL. Using MySQL requires
a MySQL server with a database and user with password with access to said
database. If using MySQL, then it is recommended to use MariaDB. Note that just
like the C++ implementation, it is insecure to access a MySQL server over the
internet, so a VPN or locally-hosted instance is necessary to keep things
secure.

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
  --factors=<quads> : Generate factors challenge with <quads> 24-bit-segments
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

Args for the C++ implementation are as follows:

    Args:
      --factors=<quads> : Generate factors challenge with <quads> 24-bit segments
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
      --threads=<integer> : Defaults to 4. Setting it to 2x of your maximum thread count is a sane value
      --enable-libcurl : Enables fetching dest urls by using libcurl
      --req-timeout-millis=<milliseconds> : Sets the number of milliseconds until timeout during forwarding requests (default 5000)
      --enable-override-dest-url : Enable "override-dest-url" request header to determine where to forward;
        example header: "override-dest-url: http://127.0.0.1:8888"
      WARNING: If --enable-override-dest-url is used, you must ensure that
        PoorMansAnubis always receives this header as set by your server. If you
        don't then anyone accessing your server may be able to set this header and
        direct PoorMansAnubis to load any website!
        If you are going to use this anyway, you must ensure that a proper firewall is configured!
      --important-warning-has-been-read : Use this option to enable potentially dangerous options

================================================================================

USING PoorMansAnubis C++ IMPLEMENTATION WITHOUT LIBCURL

================================================================================

If using the C++ version of PoorMansAnubis, and `--enable-libcurl` is not
specified, all destination urls (for `--dest-url=...` or
`--port-to-dest-url=...`) must be in the format:
"http://<ipv4_address>:<port>". If it is required to use more advanced urls,
then one must enable libcurl with `--enable-libcurl`.


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

Maintenance

================================================================================

To upkeep the "bundled" version of the C++ version of this software, the
bundled dependencies need to be checked periodically. In other words, they need
to be kept up to date. This is required because the "bundled" version statically
links all of its dependencies; all depenencies are included in the executable.
If it had been linked with shared libraries, the shared libraries would need to
be installed on systems running the software or bundled with the software
separately.

PoorMansAnbuis/cxx_impl/bundled/links.txt contains a list of links that should
lead to pages that list the version number of the latest version of said
depenencies. They should be checked against the list of SHA256SUM files.

To update a dependency "foo" that was version 1.0 and now is 2.0, the download
link to its source must be updated in the Makefile. Note that the Makefile
depends on a corresponding SHA256SUM file. In this case,
SHA256SUM_foo-1.0.tar.gz.txt shall be replaced with
SHA256SUM_foo-2.0.tar.gz.txt with an updated sha256 hash (for example; the
filenames may be somewhat different). It is apparent, then, to download the
source tarball to create the sha256 hash file. Note that these SHA256SUM files
expect the tarball to exist in "download_cache/foo-2.0.tar.gz" (using "foo" as
an example; please rename accordingly). Such a SHA256SUM file can be created
with: `sha256sum download_cache/foo-2.0.tar.gz > SHA256SUM_foo-2.0.tar.gz.txt`.
(Note that this will only work if the current-working-directory is the
directory holding the "download_cache" directory; The "download_cache"
directory is ignored with ".gitignore" and is created upon downloading
dependencies via the Makefile.)

IT IS IMPERATIVE that the Makefile is fully updated when updating a dependency,
otherwise there will be build issues that can be hard to diagnose. Note that
dependencies are expected to extract to a directory named e.g. "foo-2.0/", and
that the Makefile expects such a directory to exist in multiple lines. If a
dependency extracts to a directory name that is not like "foo-2.0/", then the
Makefile will need to be updated to handle such.

One may argue that it would be better to store the version number in a
variable, but this will cause developers to assume that only the version number
should be changed to update. Though it may be pedantic, the Makefile is thus
designed in this manner.

Note that if a new version of a dependency breaks the build, it will require
some changing of commands in the Makefile e.g. changing a parameter to a
command invoking "cmake". In such cases, it may be helpful to compare against
another existing build process (should it be allowed), or to ask for help from
those knowledgable with dependency management with C/C++.

================================================================================

Other Info

================================================================================

Contact me at:

sseo6@alumni.jh.edu
seo.disparate@gmail.com
stephen@seodisparate.com
