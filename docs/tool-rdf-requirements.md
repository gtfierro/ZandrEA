# Tool RDF Requirements

> **Direction note.** The C++ `ToolProfile` builder + generated SHACL/SPARQL
> described here was the first declarative pass. It has been superseded by the
> role/witness architecture in `docs/tool-contract-design.md`, where tool
> contracts are authored directly as SHACL node shapes, extraction is done with
> shifty's witness API (not generated CONSTRUCT/SELECT), and disambiguation uses
> `zea:pointRole`. This doc is retained for the property-path AST design and the
> static-initialization lesson, which still apply.

This is a parallel design path for defining tool requirements declaratively.
The existing `CTool_*` constructors remain in place. The S223 startup path uses
new constructor overloads that accept model-native startup specs. AHU/VAV
instances are dynamically keyed by RDF resource and displayed with `zea:name`.

The new profile layer lives in `libEA/toolProfile.*` and describes:

- the tool type identifier, such as `ahu_ibal` or `vav_ibal`
- the S223 focus classes that can satisfy the tool
- required antecedent equipment by role
- required input points by `EPointName`
- SHACL/SPARQL property paths used to validate or bind the RDF model

## Property Paths

`S223PropertyPath` models property paths as an AST instead of raw text. It can
render the same requirement to SPARQL property path syntax or SHACL path Turtle.

Supported operators:

| S223/SPARQL syntax | Builder |
|--------------------|---------|
| `p1 / p2` | `S223PropertyPath::Sequence(...)` |
| `p1 \| p2` | `S223PropertyPath::Alternative(...)` |
| `^p` | `S223PropertyPath::Inverse(...)` |
| `p*` | `S223PropertyPath::ZeroOrMore(...)` |
| `p+` | `S223PropertyPath::OneOrMore(...)` |
| `p?` | `S223PropertyPath::ZeroOrOne(...)` |

Example:

```cpp
auto path = S223PropertyPath::Alternative({
   S223PropertyPath::Predicate("s223:connectedFrom"),
   S223PropertyPath::Sequence({
      S223PropertyPath::Predicate("s223:hasConnectionPoint"),
      S223PropertyPath::Inverse(S223PropertyPath::Predicate("s223:connectsThrough")),
      S223PropertyPath::Inverse(S223PropertyPath::Predicate("s223:hasConnectionPoint"))
   })
});
```

This renders to SPARQL as an alternative between the inferred
`s223:connectedFrom` edge and a lower-level connection-point path. It renders to
SHACL as `sh:alternativePath` with nested `sh:inversePath` nodes.

## Built-In Draft Profiles

The first two draft profiles are:

- `ahu_ibal`, backed by `LegacyToolConstructorId::AhuIbal`
- `vav_ibal`, backed by `LegacyToolConstructorId::VavIbal`

They intentionally mirror the current constructor point lists. Point
requirements use `s223:hasProperty` plus broad observable-property and
quantity-kind constraints for now. These are hooks for refinement; the exact
S223 point binding model can become more specific without changing the legacy
constructors. That flat `hasProperty` path does not match how the real IBAL
test model actually expresses points (`s223:Channel`-mediated, via
sub-components) — see "Known Gaps" in `docs/sif-223.md` before treating
`points[].bound` in `/s223/tools` (`docs/s223-debugging-endpoints.md`) as
meaningful beyond visibility.

Each profile's point list (`AhuPoints()`/`VavPoints()` in `toolProfile.cpp`) is
a function-local `static const` returned by a function, not a namespace-scope
global. This matters: `BuiltInToolProfiles()` can run as early as `LibMain()`'s
`__attribute__((constructor))`, which does not guarantee this translation
unit's namespace-scope globals have finished dynamic initialization yet (C++
does not order dynamic initialization across translation units). A
namespace-scope `const std::vector<PointRequirement> AHU_POINTS = {...}` was
observed to still be an empty, zero-initialized vector at that point — every
`ToolProfile.points` silently came back empty, with no error, until something
finally read it (the diagnostic report added in
`docs/s223-debugging-endpoints.md`). A function-local static is guaranteed
fully constructed on first call regardless of cross-TU init order, which is
why the fix was to wrap each point list in its own accessor function rather
than declare it as a plain global. Follow the same pattern for any new
profile's point (or antecedent, if ever pulled out to a shared global) list.

Antecedent requirements are first-class:

- `vav_ibal` requires an `air_source` antecedent of tool type `ahu_ibal`
- `vav_ibal` has a draft `hot_water_source` antecedent hook
- `ahu_ibal` has draft `chilled_water_source` and `hot_water_source` antecedent hooks

The VAV `air_source` antecedent uses `s223:connectedFrom+` in the draft profile.
That is intentional for the IBAL model: AHUs and VAVs are separated by the air
distribution network, so the requirement should accept an upstream AHU rather
than requiring direct adjacency.

## IBAL Example

The working example is described by
`EAd/tests/testdata/ibal-s223-example.json`.

It uses:

- local ontology path: `223p.ttl`
- site model URL: `https://models.open223.info/NIST-IBAL.ttl`
- local site model path after download:
  `EAd/tests/testdata/NIST-IBAL.ttl`
- column map: `EAd/tests/testdata/ibal-ahu-vav-column-map.json`
- CSV samples under `EAd/tests/testdata/`

The checked-in local copy adds ZandrEA startup annotations such as `zea:name`
for the data-backed equipment resources.

Fetch the site model with:

```sh
curl -fsS https://models.open223.info/NIST-IBAL.ttl \
  -o EAd/tests/testdata/NIST-IBAL.ttl
```

The example is intentionally discovery-oriented. It does not list `IBAL:AHU_1`
or `IBAL:VAV_1` as selected resources. Instead, candidates are discovered from
the S223 focus classes declared by each tool profile, names are read from
`zea:name`, and the column map filters the result to data-backed subjects.
For example, `IBAL:AHU_3` is a valid `s223:AirHandlingUnit` candidate in the
Open223 model, but it drops out of this example because it has no data-backed
`zea:name` present in `ibal-ahu-vav-column-map.json`.

This is the runtime adapter direction: SHACL/profile matching finds tool
candidates and antecedents from RDF, and the created AHU/VAV subjects keep RDF
resource identity inside ZandrEA instead of being assigned to fixed enum slots.

## Generated Shapes

`GenerateBuiltInToolProfileShapesTurtle()` emits draft SHACL node shapes with
ZandrEA annotations:

- `zea:toolType`
- `zea:requirementKind`
- `zea:antecedentRole`
- `zea:requiredToolType`
- `zea:pointName`
- `zea:pointValueKind`

These annotations are meant to make validation reports and later binding logic
traceable back to the ZandrEA tool contract.

## Generated Discovery

The profile layer also generates SPARQL discovery queries. This is the
replacement for hardcoded startup queries.

`GenerateBuiltInToolProfileDiscoverySparql(CandidateEquipment)` asks which RDF
resources match each profile's focus classes. This is useful for explaining what
the model contains.

`GenerateBuiltInToolProfileDiscoverySparql(CreatableTools)` requires `zea:name`
and adds the required antecedent and point patterns from the profile. This is
the dynamic startup question: given the loaded data model graph and the
available tool profiles, which tool instances can ZandrEA create?

The generated query is derived from the same profile data as the SHACL shapes.
Adding a new tool type should mean adding a new profile; it should not require
editing `s223Model.cpp` with new discovery class names.

`GenerateBuiltInToolProfileStartupConstructSparql(false)` is the current startup
bridge. It constructs profile, name, and antecedent triples for dynamic
startup, but does not require RDF point bindings yet because the existing
constructors still instantiate their point objects internally. Passing `true`
enables point-pattern requirements once point bindings are ready to become part
of startup.

`s223Model.cpp` parses the constructed startup graph into
`S223ToolStartupSpec`: profile id, RDF resource, `zea:name`, and antecedent
bindings. The S223 AHU/VAV constructor overloads consume that native spec. VAV
`air_source` antecedents are resolved by RDF resource key against dynamically
registered AHU subjects.

`GenerateBuiltInToolProfileDiagnosticConstructSparql()` is a third generated
query, added alongside the debugging endpoints
(`docs/s223-debugging-endpoints.md`). It mirrors the startup CONSTRUCT, but
wraps every antecedent and point pattern in `OPTIONAL` regardless of
`required`, so `s223Model.cpp` can report on candidates that almost matched
instead of only ones that fully did. All three query generators
(discovery, startup CONSTRUCT, diagnostic CONSTRUCT) that touch point
patterns need `PREFIX quantitykind: <http://qudt.org/vocab/quantitykind/>` —
`append_point_patterns()` emits `qudt:hasQuantityKind quantitykind:...`
triples, but only the Turtle shapes generator
(`GenerateToolProfileShapesTurtle()`) declared that prefix for a long time.
The gap was invisible until profile point lists were non-empty at the point
these queries actually ran (see the static-initialization note above) — once
that was fixed, every point-pattern query immediately failed to parse with
"Prefix not found," which is why all three now declare `quantitykind:`.

Startup order is not hardcoded by equipment kind. `s223Model.cpp` builds an
antecedent graph from the matched tool profile ids and each profile's
`AntecedentRequirement::requiredToolProfileId`, then topologically sorts the
matched tool instances. If a required antecedent type cannot be matched, or if
the antecedent graph contains a cycle, startup fails before constructing tools.
