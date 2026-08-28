# Final Fantasy Tactics decompilation

**Status: 100% decompiled and byte-exact ✅**

❤️ This project is an ode to the [FFHacktics](https://ffhacktics.com/wiki/)
community. This project would not be possible without it.

A matching decompilation of the North American PlayStation release of *Final
Fantasy Tactics* (`SCUS-94221` [redump](http://redump.org/disc/55/)). Every game
function is C that compiles to the original bytes, and the rebuilt disc is a
byte-for-byte match.

Game files and proprietary Sony tools are not included.

## Getting started

Requirements
- Docker
- `SCUS-94221` BIN at `./scus-94221.bin` (optional)

Then run:

```sh
# Prepare environment
make bootstrap

# Then use one of the following (make help for more):
make validate  # verify all code against checksums (without BIN)
make build     # build every module and a byte-matching disc (needs the BIN)
```

`make build` writes `build/disc/output-scus-94221.bin` and `.cue`.
