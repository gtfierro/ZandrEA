# ASHRAE 223 SHACL Inference and Validation

ZandrEA can use the shifty C++ SDK to run SHACL-AF inference and SHACL
validation when loading ASHRAE 223 RDF graphs at startup.

Runtime configuration still uses the existing environment variables:

```sh
export EA_S223_ONTOLOGY_TTL=/path/to/223p.ttl
export EA_S223_SITE_TTL=/path/to/site-model.ttl
```

When the binary is built with shifty, `EA_S223_ONTOLOGY_TTL` is loaded as the
SHACL shapes/rules graph and `EA_S223_SITE_TTL` is loaded as the data graph.
Validation runs with `run_inference = true`, so shifty applies SHACL-AF rules
before evaluating constraints.

## Build with shifty

Until shifty is published as a package, install it into a local prefix and point
this Makefile at that prefix:

```sh
git clone https://github.com/gtfierro/shifty.git
cmake -S shifty/cpp -B shifty-build/cpp
cmake --build shifty-build/cpp
cmake --install shifty-build/cpp --prefix /path/to/shifty-prefix

make compile SHIFTY_PREFIX=/path/to/shifty-prefix
```

For nonstandard layouts, pass explicit flags instead:

```sh
make compile \
  SHIFTY_CFLAGS="-I/path/to/include" \
  SHIFTY_LIBS="-L/path/to/lib -lshifty_cpp -ldl -pthread -lm"
```

If shifty flags are not provided, the loader falls back to the existing
rdf4cpp parse/count path and logs that SHACL inference and validation were
skipped.

## Docker and Podman

The development and production REST Dockerfiles build shifty from source inside
the image and compile `ead` with `SHIFTY_PREFIX=/ea/shifty`. These targets
therefore build a shifty-enabled REST binary by default:

```sh
make docker-build
make docker-up
make docker-test
make podman-build
make podman-up
make podman-test
make docker-production-build
```

To test a fork or branch of shifty in the image build, pass Docker build args
through Compose:

```sh
docker compose build \
  --build-arg SHIFTY_REPO=https://github.com/gtfierro/shifty.git \
  --build-arg SHIFTY_REF=main \
  rest
```
