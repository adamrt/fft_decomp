# Final Fantasy Tactics decompilation

> Status: byte-exact rebuild; source recovery and refinement ongoing

A matching decompilation of the North American PlayStation release of *Final
Fantasy Tactics* (`SCUS-94221` [redump](http://redump.org/disc/55/)). Every
configured function and owned data span is checked against original-byte
hashes. The rebuilt disc is a byte-for-byte match; unconfigured spans,
including some native routines, are preserved from the original image.

Game files and proprietary Sony tools are not included.

## Next Steps

Remaining work includes classifying the uncovered native routines and recovering
those that belong to the game, alongside improvements to names, types, enums
and source organization. Sony SDK reconstruction is a separate concern.

Each time a change is made `make check` will ensure the changes still match
byte-for-byte.

## Getting started

Requirements
- Docker
- `SCUS-94221` BIN at `./scus-94221.bin` (optional)

Then run:

```sh
# Prepare environment
make bootstrap

# Then use one of the following (make help for more):
make check  # verify all code against checksums (without BIN)
make build  # build every module and a byte-matching disc (needs the BIN)
```

`make build` writes `build/disc/output-scus-94221.bin` and `.cue`.

## Thanks

This project is an ode to the [FFHacktics](https://ffhacktics.com/wiki/)
community. ❤️
