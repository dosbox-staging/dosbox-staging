# HTTP API

DOSBox Staging can expose an HTTP REST API for development and debugging
purposes. When enabled, you can inspect internal emulator state, memory
contents, and control the emulation through simple HTTP requests.

The API only binds to `localhost` by default — never expose it to untrusted
networks, as it gives full control over the emulator.

!!! warning

    This feature is intended for developers and advanced users. Do not
    enable it unless you know what you're doing.


## Configuration settings

You can set the web server parameters in the `[webserver]` configuration
section.


##### webserver_bind_address

:   Bind to the given IP address (`127.0.0.1` by default). This API gives
    full control over DOSBox; do not ever expose this to untrusted hosts. By
    default only local connections are allowed.


##### webserver_enabled

:   Enable the HTTP REST API that exposes internal state and memory. Open
    `http://localhost:8080` in a browser (or use the configured port) to view
    the API documentation.

    Possible values: `on`, `off` *default*{ .default }


##### webserver_port

:   TCP port to bind to (`8080` by default).
