================================================================================

POOR MAN'S ANUBIS: Website Access Challenge Verification

================================================================================

This software is heavily inspired by Anubis.
(https://github.com/TecharoHQ/anubis)

There is currently one method of challenge verification: factors of a large
number. It is generated per request, and the challenge response should be the
factors of the number in sorted increasing order.

The "c_impl" contains the challenge generation code.

The "rust_impl" contains the frontend code and is the "meat" of the software.


================================================================================

USAGE

================================================================================

Build the Rust frontend.

Set up a MySQL server with a database and a user with password with access to
said database. (It is recommended to use MariaDB in place of MySQL.)

Check "rust_impl/src/args.rs" for flags you can pass to the program. Also check
"rust_impl/mysql.conf" for how the config file should be set up for MySQL
access.

Point your webserver to the Rust-frontend's listening ip and port with the full
url as if it were the endpoint, and use a flag on the frontend to determine
where PoorMan'sAnubis will load from on challenge success.

You may use the "x-real-ip" header to ensure the frontend knows the correct
ip address.


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

    location /pma_api {
        proxy_set_header 'x-real-ip' $remote_addr;
        proxy_pass http://127.0.0.1:8888;
    }
}

This assumes PoorMan'sAnubis is listening on port 8888 and will forward requests
to 127.0.0.1:9999 with the url passed verbatim. Check the args for details.


================================================================================

Other Info

================================================================================

Contact me at:

sseo6@alumni.jh.edu
seo.disparate@gmail.com
stephen@seodisparate.com
