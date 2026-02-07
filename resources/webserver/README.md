# REST API

DOSBox has a REST API running on port 8080 by default. It can be used to read
and manipulate the memory of the emulated machine.

For example, open this URL in your browser to download a snapshot of the entire
lower memory: http://localhost:8080/api/memory/0/0x100000

NOTE: This feature currently requires `core = normal`.


# Endpoints

All endpoints access the internal state atomically between steps of the
emulated CPU. They will never be executed during natively implemented DOS
functions or interrupts, this means the internal data structures are
guaranteed to be consistent in any given snapshot.


## GET /api/cpu

Read CPU registers.


## GET /api/memory/:segment/:offset/:len

Read memory from the given address.

Segment is optional, if provided it can either be a number or the name of a
segment register. All URL parameters accept hex strings if prefixed with `0x`.

By default this outputs the raw binary data, set
`Accept: application/json` to request a JSON response with the data encoded
in Base64.

## PUT /api/memory/:segment/:offset

Write memory to the given address.

Path parameters work the same as for GET.

This accept either raw binary data with `Content-Type: application/octet-stream`
or a JSON object with a Base64 encoded field `data` with
`Content-Type: application/json`.


## POST /api/memory/allocate

Allocate memory with the DOS memory allocator.

Request:
```
{
	"size": size_bytes,
	"area": "UMA"|"conv"
}
```

Response:
```
{
	"addr": physical_address
}
```


## POST /api/memory/free

Frees memory allocated with the DOS memory allocator.

Request:
```
{
	"addr": physical_address,
}
```

Returns error 400 on invalid addresses.


## GET /api/pointers

Retrieve pointers to internal data structures.
