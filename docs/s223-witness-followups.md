# S223 profile/witness startup follow-ups

Status: resolved in the working tree.

## Usage

Enable the witness path by setting `EA_S223_PROFILE_SHAPES_TTL` as a
colon-separated list beside the ontology and site model variables:

```sh
EA_S223_ONTOLOGY_TTL=223p.ttl \
EA_S223_SITE_TTL=EAd/tests/testdata/simple-ahu-vav-223.ttl \
EA_S223_PROFILE_SHAPES_TTL=EAd/tests/testdata/zea-core.ttl:EAd/tests/testdata/zea-profiles.ttl \
make podman-up
```

The loader also reads `qudt-all.ttl` from the same directory as
`EA_S223_ONTOLOGY_TTL`. In the container stack, `223p.ttl` is mounted at
`/ea/223p.ttl` and `qudt-all.ttl` is mounted at `/ea/qudt-all.ttl`, so the S223
and QUDT SHACL graphs are prepared together by default.

Then inspect the startup model:

```sh
curl -s http://127.0.0.1:34568/s223/status
curl -s http://127.0.0.1:34568/s223/tools
```

For the simple AHU/VAV test model, `/s223/status` should report three
candidates, three creatable tools, and three instantiated tools.

## Diagnostics

Motivation: `/s223/status` and `/s223/tools` should describe the same
profile/witness model that startup used to instantiate tools.

Implementation: `LoadS223ApplicationStartupModel` now uses a witness-specific
diagnostic report when `config.profileShapeTurtlePaths` is set. The report is
projected from the witness rows, `ResolvedToolModel`, and assembler diagnostics,
so the REST view reflects the actual per-candidate creatable/blocked state.

The generated-CONSTRUCT diagnostic report remains the fallback when profile
shape files are not configured.

## Startup Construction

Motivation: S223 discovery runs SHACL-AF inference and witness extraction, so it
must not run during loader-time initialization or more than once per daemon
process.

Implementation: `LibMain` is now an explicit, idempotent initializer.
`EAd/main.cpp` calls it once before retrieving the exported port pointer, and
`libEA/libmain.cpp` no longer uses a dynamic-loader constructor.

Expected runtime signal: process startup should print one
`S223 profile/witness discovery:` line for one daemon process.
