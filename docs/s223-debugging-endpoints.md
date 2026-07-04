# ASHRAE 223 Startup Debugging Endpoints

When ZandrEA starts with the S223 RDF path enabled (`EA_S223_ONTOLOGY_TTL` /
`EA_S223_SITE_TTL` set), `CApplication`'s constructor stashes the full startup
result on `CDomain` (`CDomain::SetS223StartupModel()` /
`CDomain::SayS223StartupModel()`, `libEA/subject.hpp`). Four read-only `EAd`
GET endpoints expose that result for debugging discovery/validation issues
without rebuilding or adding print statements. Nothing is recomputed per
request; all four read the same cached `S223ApplicationStartupModel`.

If S223 startup was not used for the current run (no env vars, or built
without shifty), each endpoint still responds, with `active: false` and
otherwise empty/zeroed fields.

## `GET /s223/status`

Headline counts, most useful first stop when something looks wrong:

```json
{
  "active": true,
  "conforms": false,
  "ontologyPath": "/ea/223p.ttl",
  "ontologyQuadCount": 8779,
  "sitePath": "/ea/EAd/tests/testdata/NIST-IBAL.ttl",
  "siteQuadCount": 6022,
  "candidateEquipmentCount": 7,
  "creatableToolCount": 2,
  "instantiatedToolCount": 2
}
```

`candidateEquipmentCount` → `creatableToolCount` → `instantiatedToolCount` is
the funnel: how many RDF resources matched a tool profile's focus class, how
many of those had enough bound information to be constructed, and how many
were actually instantiated (should equal `creatableToolCount` barring a
construction-time exception). A gap between the first two numbers means
`/s223/tools` (below) has near-misses worth reading.

## `GET /s223/validation`

SHACL validation results from the shifty `PreparedValidator` run against the
inferred dataset:

```json
{
  "active": true,
  "conforms": false,
  "resultCount": 2,
  "results": [
    {
      "resultNode": "_:a1",
      "severity": "Violation",
      "sourceConstraintComponent": "MinCountConstraintComponent",
      "sourceShape": "_:shape",
      "focusNode": "http://example.org/site#ahu1_occ",
      "hasResultPath": true,
      "resultPath": "http://data.ashrae.org/standard223#hasEnumerationKind",
      "hasValue": false,
      "value": "",
      "messages": [
        "s223: An `EnumerableProperty` shall be associated with exactly one `EnumerationKind` using the relation `hasEnumerationKind`."
      ]
    }
  ],
  "diagnosticsJson": "[]",
  "reportTurtle": "..."
}
```

`results` is the default client-facing view of the SHACL report: one object per
`sh:ValidationResult`, with stable fields for filtering by severity,
constraint component, focus node, path, value, and message. This avoids parsing
the human text report in clients while preserving the raw `reportTurtle` for
RDF-level debugging.

`results`/`reportTurtle` can be large; the response is gzip'd
(`Content-Encoding: gzip`), so fetch with a client that decompresses
automatically (e.g. `curl --compressed`).

## `GET /s223/vaildation_algebra`

Algebra-path SHACL validation results from shifty
`PreparedValidator::validate_algebra()`. This endpoint is useful when clients
want the engine's grouped violation/reason tree directly instead of the W3C
`sh:ValidationReport` graph exposed by `/s223/validation`.

`/s223/validation_algebra` is accepted as a correctly spelled alias.

```json
{
  "active": true,
  "engineAvailable": true,
  "validationRun": true,
  "conforms": false,
  "violationCount": 1,
  "resultsText": "Validation Report\nConforms: False\n...",
  "violations": [
    {
      "focusNode": "<http://example.org/site#ahu1_occ>",
      "shapeName": "http://example.org/shapes#OccupancyShape",
      "severity": "Violation",
      "reasons": [
        {
          "value": "<http://example.org/site#ahu1_occ>",
          "path": "s223:hasEnumerationKind",
          "message": "MinCount 1 failed",
          "authorMessage": "s223: An `EnumerableProperty` shall be associated with exactly one `EnumerationKind` using the relation `hasEnumerationKind`.",
          "severity": "Violation"
        }
      ]
    }
  ]
}
```

Each violation groups the failing reasons for one focus node and one shape.
`shapeName` is empty for anonymous shapes. Each reason carries the failed value,
the algebra path when available, the engine message, the author-supplied SHACL
message when available, and the reason severity.

## `GET /s223/graph?inferred=true|false`

Raw download of the site data graph, serialized as N-Triples
(`Content-Type: application/n-triples`) — not prettified Turtle, since
shifty's C++ API (`shifty::Dataset::ntriples()`) only exposes N-Triples
serialization. N-Triples is valid RDF and a syntactic subset of Turtle.

- `inferred=false` (default): the site graph exactly as loaded from
  `EA_S223_SITE_TTL`, before SHACL-AF inference.
- `inferred=true`: the same graph after `PreparedValidator::validate(...,
  {run_inference = true})`. Despite `validate()` taking `Dataset` by `const&`,
  SHACL-AF inference materializes new triples into the underlying shifty
  dataset in place — the C++ constness doesn't reach through shifty's FFI
  handle. Diffing the two is the fastest way to check whether inference
  actually added anything (see "Known gap" in `docs/tool-rdf-requirements.md`
  — for the current IBAL test data, the two are identical: inference adds
  zero triples).

## `GET /s223/tools`

Structured per-candidate diagnostic report — the "what had enough info, what
was close" view. One entry per RDF resource that matched any tool profile's
focus class, whether or not it ended up creatable:

```json
{
  "creatableCount": 2,
  "candidates": [
    {
      "rdfResource": "http://example.org/IBAL#VAV_1",
      "profileId": "vav_ibal",
      "profileDisplayName": "VAV IBAL",
      "hasName": true,
      "name": "VAV-1",
      "creatable": false,
      "blockingReasons": ["missing required antecedent 'air_source'"],
      "antecedents": [
        {
          "role": "air_source",
          "requiredToolProfileId": "ahu_ibal",
          "required": true,
          "bound": false,
          "rdfResource": "",
          "boundResourceIsCreatable": false
        }
      ],
      "points": [
        { "pointName": "Pressure_static_air_supply", "required": true, "bound": false }
      ]
    }
  ]
}
```

Notes on reading this:

- `antecedents[].bound == false` only affects `creatable` when
  `required == true` — an unbound optional antecedent (e.g. AHU's
  `chilled_water_source`) is normal and doesn't block anything.
- `antecedents[].boundResourceIsCreatable` matters when `bound == true`: the
  bound target must itself be a creatable instance of
  `requiredToolProfileId` (matching the same gate
  `tool_specs_from_startup_graph()` uses), not just any resource of the right
  RDF class.
- `points[].required == true` does **not** currently affect `creatable`. Point
  requirements are informational only right now — construction doesn't check
  them at all, because the legacy `CTool_ahu_ibal`/`CTool_vav_ibal`
  constructors still build their own fixed internal point objects rather than
  reading points from RDF. See "Known gaps" in
  `docs/tool-rdf-requirements.md` before relying on `points[].bound` for
  anything beyond visibility.

## Implementation path

`S223ToolDiagnosticReport` and its nested types
(`S223ToolDiagnosticCandidate`/`Antecedent`/`Point`) live in
`libEA/s223Model.hpp`, built by `build_diagnostic_report()` in
`libEA/s223Model.cpp` from a dedicated diagnostic CONSTRUCT query
(`GenerateBuiltInToolProfileDiagnosticConstructSparql()` in
`libEA/toolProfile.cpp`) that mirrors the real startup CONSTRUCT but wraps
every antecedent/point pattern in `OPTIONAL` regardless of
`AntecedentRequirement::required`/`PointRequirement::required`, so nothing is
silently dropped the way the real (gating) startup query drops incomplete
candidates.

The boundary out to `EAd` follows the existing `IExportOmni` → `CPortOmni` →
`CView` → `CDomain` pattern used everywhere else in the codebase
(`libEA/exportCalls.hpp`, `libEA/portOmni.*`, `libEA/mvc_view.*`); JSON is
built directly in `EAd/handler.cpp`'s `handle_get()`, in new
`else if (path == U("/s223/..."))` branches. `/s223/graph` is the one
exception that bypasses the shared JSON-reply tail, since it returns a raw
text body with its own content type.
