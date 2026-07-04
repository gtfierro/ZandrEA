# Model-Driven Tool Contract: Design, Configuration, Usage

Status: **proven end to end** against shifty SHACL-AF inference + validation on a
purpose-built model (see [Proof](#proof-what-has-been-validated)). This is the
target architecture for a *new* ZandrEA configuration entrypoint driven entirely
by tool definitions + the S223 ontology + inference/validation. It runs
alongside — and does not modify — the legacy `CTool_*` startup path.

It supersedes the generated-SPARQL discovery bridge described in
`docs/sif-223.md` and `docs/tool-rdf-requirements.md`.

---

## 1. Thesis

Two things are fixed: the **analysis engine** (points, facts, rules, subjects,
views evaluate as they do today and produce the same MVC export) and **S223 as
the SIF** (the site model is the authoritative semantic input). Everything about
how a *tool is defined* is redesigned around one idea:

> The binding seam between the RDF model and the analysis code is the
> **semantic role** — a tool-local, abstract slot (`outsideAirTemp`,
> `airSource`) that has exactly one definition on the RDF side (how to *find*
> it) and one meaning on the engine side (what the analysis *calls* it).

Roles replace the two bad seams in the current code: the global `EPointName`
enum (both the CSV-ingestion key and the rule-reference key) and the named point
members (`u_Tao`) the hardcoded rules bind to.

## 2. Layers

```
┌─ S223 site model (the SIF) ─────────── deployment data, authoritative
│     ex:AHU_1 a s223:AirHandlingUnit ; s223:contains ... ;
│     + ZandrEA commissioning annotations: zea:name, zea:pointRole
│
├─ zea-core.ttl ──────────────────────── vocabulary + SHACL-AF inference rules
│     materializes clean virtual edges: zea:hasBoundPoint,
│     zea:connectedFromEquipment  (closes over containment/channel/cnx plumbing)
│
├─ zea-profiles.ttl ──────────────────── application profiles = tool contracts
│     one sh:NodeShape per tool type; each sh:property is a ROLE
│     (finder + disambiguator + construction attributes)
│
├─ assembler (generic, new) ──────────── no per-tool code
│     witnesses(site) -> group by focus node -> RoleBoundPoints -> module
│
├─ analysis modules (compiled, role-typed) ── the irreducible FDD code
│     Build(RoleBoundPoints, RuleKit) — never sees an IRI or SHACL
│
└─ analysis engine (fixed) ───────────── CPoint / CFact / CRuleKit / CSubject / CView
```

The tool contract is the single authoritative definition. The assembler is
generic. The engine is untouched. The only irreducible per-tool *code* is the
analysis module, and it never sees RDF.

## 3. Configuration

Three RDF artifacts. Live examples under `EAd/tests/testdata/`.

### 3.1 `zea-core.ttl` — vocabulary + inference rules

Loaded as part of the shapes/rules graph; its SHACL-AF rules run during the
shifty inference pass and materialize two virtual edges so profile finders never
touch raw s223 plumbing:

| Edge | Meaning | Closes over |
|---|---|---|
| `zea:hasBoundPoint` | equipment → an observable/actuatable property it owns | `contains*` / `hasConnectionPoint` reach to a `Sensor`(`observes`) or `Actuator`(`actuatedByProperty`), plus `hasProperty` (setpoints) |
| `zea:connectedFromEquipment` | terminal unit → upstream equipment feeding it | the air-medium `s223:cnx` connection network |

Each rule is a `sh:SPARQLRule` with `sh:construct` and a shared `sh:prefixes`
block. Rules target the specific equipment classes (`s223:AirHandlingUnit`,
`s223:TerminalUnit`) so they fire without needing the full 223 ontology loaded
for subclass entailment.

### 3.2 `zea-profiles.ttl` — the tool contracts

One `sh:NodeShape` per tool type (`zea:AhuProfile`, `zea:VavProfile`), tagged
`zea:toolProfileId`, `zea:analysisModule`, and `sh:targetClass` (the equipment
type definition). Every `sh:property` is a **role**:

```turtle
sh:property [
   zea:roleName "outsideAirTemp" ;                       # stable role id == module RoleId; the witness key
   sh:path zea:hasBoundPoint ;                            # finder: a zea-core virtual edge, never raw plumbing
   sh:qualifiedValueShape [                               # disambiguator
      sh:property [ sh:path zea:pointRole ; sh:hasValue zea:outsideAirTemp ] ] ;
   sh:qualifiedMinCount 1 ; sh:qualifiedMaxCount 1 ;      # required + exactly-one
   zea:present [ zea:valueKind "analog" ; zea:unit "degC" ;
                 zea:range "n18To49" ; zea:plotGroup "Free" ] ] ;  # engine construction attrs
```

- **`sh:path`** is the finder. For points it is `zea:hasBoundPoint`; for
  antecedents it is `[ sh:oneOrMorePath zea:connectedFromEquipment ]`.
- **`sh:qualifiedValueShape`** is the disambiguator. `zea:pointRole` is the
  reliable discriminator where quantity kind collides (four air temperatures).
  Add a `qudt:hasQuantityKind` guard when you want belt-and-suspenders.
- **`sh:qualifiedMinCount 1`** makes a missing role a *validation violation* —
  that is the `creatable` gate and the blocking-reason source, for free.
- **`zea:present`** carries the engine attributes the assembler needs to build
  the point object (unit, range, plot group, value kind). Values are a small
  closed `zea:` vocabulary the assembler maps to `EDataUnit`/`EDataRange`/
  `EPlotGroup` enums.
- An **antecedent role** (`airSource`) carries `zea:requiredToolProfileId` and a
  `sh:qualifiedValueShape [ sh:class s223:AirHandlingUnit ]`; its value node is
  another equipment that must itself be a creatable tool.

### 3.3 Site-model annotations (commissioning knowledge)

The model must carry two ZandrEA annotations beyond raw S223:

- **`zea:name`** on each createable equipment — the display/startup name.
- **`zea:pointRole`** on each bound observable/actuatable property — the semantic
  role it plays.

`zea:pointRole` is deliberate. Digging into the real NIST-IBAL model showed the
four AHU air temperatures (outside/mixed/return/supply) carry **no** role,
aspect, or usable discriminator in the graph — bare `QuantifiableObservableProperty`
+ quantity kind, on roleless air `Segment`s, with labels that even contradict
the topology (`ahu1_in_rtd` observed at AHU_2's coil). That disambiguation is
genuinely site-commissioning knowledge, not derivable semantics. `zea:pointRole`
asserts it as RDF data (same tier as `zea:name`), keeping everything declarative.
Where the model *does* carry native roles (coil `s223:hasRole Role-Cooling`/
`Role-Heating`), a finder can use those directly instead. See the annotation
conventions block at the top of `EAd/tests/testdata/simple-ahu-vav-223.ttl`.

## 4. Discovery and extraction: the witness API

shifty's `PreparedValidator::witnesses()` (added in shifty `5df3e9d`, see
`docs/shifty-s223.md`) is the extraction seam. It is the inverse of `validate()`:
for every focus node that **conforms** to a profile node shape, it returns the
value node(s) each `sh:property`'s path resolved to, narrowed by any
`sh:qualifiedValueShape`.

```cpp
shifty::ValidationOptions opts;
opts.key_path = "zea:roleName";   // witness.key := the role id
opts.run_inference = true;        // materialize zea-core edges first
auto witnesses = validator.witnesses(dataset, opts);
// each PropertyWitness = { focus_node, shape_id, key(=roleName), value_nodes }
```

This replaces the entire generated-SPARQL layer:

- **Extraction** = one `witnesses()` call → `(focus_node, roleName) → value_nodes`.
  This is exactly the assembler's `RoleBoundPoints` input. The
  `Generate…DiscoverySparql` / `…StartupConstructSparql` CONSTRUCT bridge is no
  longer needed.
- **Discovery + diagnostics** = `validate()`. Conforming focus node = creatable
  tool; a `QualifiedMinCount` violation = a missing required role = a blocking
  reason, pointing at the exact focus node and path. The separate diagnostic
  CONSTRUCT is no longer needed. Non-conforming nodes simply do not appear in the
  witness output — the creatable gate falls out for free.

## 5. The role, compile-time

The analysis module owns the role vocabulary at compile time; the RDF profile
supplies the finder + presentation for each. `RoleId` is a strong type wrapping a
`string_view`:

```cpp
namespace ahu_vav {
   inline constexpr RoleId outsideAirTemp {"outsideAirTemp"};
   inline constexpr RoleId airSource      {"airSource"};   // antecedent role
   // ...
}
```

Module code uses `p.Analog(ahu_vav::outsideAirTemp)` — typo-proof, greppable,
refactorable. The profile references the same string in `zea:roleName`. At load
time the assembler **reconciles**: every `RoleId` the module declares must have
exactly one matching finder in the profile, kinds agree, no orphan finders. Drift
between "what the analysis needs" and "what the profile finds" becomes a startup
error, not a silent null.

## 6. The assembler / new entrypoint

**We reuse the existing `CTool_*` constructors; we do not rebuild the analysis
graph.** The `CTool_ahu_ibal`/`CTool_vav_ibal` constructors already build the
whole 117-object graph (points, formulas, charts, facts, evidence, hypotheses,
rules, features) internally. The model-driven path only decides *which* tools to
instantiate, with *what* name and *what* antecedent wiring, and calls those
existing constructors — exactly what `AddToolFromS223Spec` + the `CApplication`
loop already do. The pipeline:

1. Load `zea-core.ttl` + `zea-profiles.ttl` as shapes/rules; the site model as
   data.
2. `witnesses(site, {key_path:"zea:roleName", run_inference:true})`.
3. `AssembleTools(...)` → group by `focus_node`, dispatch to the module by
   `shape_id`, reconcile against the manifest, resolve antecedents, topologically
   order (`libEA/toolAssembler.cpp`).
4. `StartupSpecsFromModel(...)` projects the ordered model onto
   `std::vector<S223ToolStartupSpec>` — the surface `AddToolFromS223Spec` already
   consumes.
5. The existing `CApplication` loop calls the existing `CTool_*` constructors in
   order. **No engine/FDD code is touched.**

### The analysis module's job

Because instantiation reuses the existing constructors, the module does **not**
rebuild facts against roles. Its `ModuleManifest` (`libEA/toolModules.cpp`) only:

- names the tool's point roles as `role → EPointName` — used to validate the
  model supplies them, and to carry the resolved RDF property per role for future
  channel-native ingestion; and
- declares its antecedent roles + the profile each must resolve to.

The engine-facing point attributes (unit, range, plot group, label) stay owned by
the constructor — the manifest deliberately does **not** duplicate them. (A future
option, if desired, is to have a module `Build` the graph against roles instead of
the legacy constructor — a large, verification-heavy port — but it is explicitly
out of scope; the existing constructors are the instantiation path.)

## 7. Usage

### Build shifty with the witness API

```sh
git -C /path/to/shifty pull                     # need >= 5df3e9d
cmake -S /path/to/shifty/cpp -B /path/to/shifty/build/cpp \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/path/to/shifty/prefix
CARGO_BUILD_JOBS=$(nproc) cmake --build /path/to/shifty/build/cpp \
      --target shifty_rust_build --parallel $(nproc)
cmake --build /path/to/shifty/build/cpp --parallel $(nproc)
cmake --install /path/to/shifty/build/cpp
```

The container builds already clone `SHIFTY_REF=main`, so `make docker-build`
picks up the witness API with no change.

### Run the witness pass against a model

The reference harness is `EAd/tests/witness_probe.cpp`, used to validate this
design (see Proof below). Given the three artifacts, it constructs a
`PreparedValidator` from `zea-core.ttl` + `zea-profiles.ttl`, loads the site
model as data, runs `validate()` (conformance) and `witnesses()`, and prints
`(focus_node, role) → value_nodes`. Compile against the shifty prefix:

```sh
g++ -std=c++17 -I$PREFIX/include EAd/tests/witness_probe.cpp -o /tmp/witness_probe \
    -L$PREFIX/lib -lshifty_cpp -ldl -pthread -lm
/tmp/witness_probe EAd/tests/testdata
```

### Add a new tool type

1. Add a `sh:NodeShape` to `zea-profiles.ttl`: `sh:targetClass`, one `sh:property`
   per role (`zea:roleName` + finder + qualifier + `zea:present`), antecedents as
   roles with `zea:requiredToolProfileId`.
2. Write the role constants (`RoleId`) and the `IAnalysisModule` for that tool;
   register it under its `zea:analysisModule` id.
3. If the tool needs a new structural pattern, add a `zea-core.ttl` inference
   rule for it once; do not put plumbing in the profile.

No `s223Model.cpp` discovery edit, no `LegacyToolConstructorId` switch case.

### Prepare a site model

Author the S223 model, then add the ZandrEA annotations: `zea:name` on each
createable equipment, `zea:pointRole` on each bound property. Run the witness
pass; anything that does not conform is reported by `validate()` with the exact
missing role/antecedent.

### Enable the profile-driven path in `ead`

Startup discovery switches to the witness/assembler path when the profile shapes
are configured, alongside the existing S223 env vars:

```sh
export EA_S223_ONTOLOGY_TTL=/path/to/223p.ttl
export EA_S223_SITE_TTL=/path/to/site.ttl
export EA_S223_PROFILE_SHAPES_TTL=/path/to/zea-core.ttl:/path/to/zea-profiles.ttl   # ':'-separated
```

When `EA_S223_PROFILE_SHAPES_TTL` is set, `LoadS223ApplicationStartupModel`
discovers tools via `tool_specs_via_profile_witnesses` (witnesses → assembler →
`StartupSpecsFromModel`) instead of the legacy generated-CONSTRUCT discovery; the
`CApplication` loop and `CTool_*` constructors are unchanged. Unset, the legacy
discovery path runs as before.

## 8. Proof: what has been validated

Run against `EAd/tests/testdata/simple-ahu-vav-223.ttl` (1 AHU + 2 VAVs, fully
annotated) with `zea-core.ttl` + `zea-profiles.ttl` as shapes, real SHACL-AF
inference on:

- **Positive:** `Conforms: YES`. All 3 focus nodes bind every role to exactly one
  node — AHU_1: 12 roles (name + 11 points); VAV_1/VAV_2: 13 roles each
  (name + `airSource` + 11 points). 38 witness rows, zero ambiguous, zero
  unbound. The four air temperatures each resolve to their own distinct property;
  `qualifiedMaxCount 1` + conformance proves the `zea:pointRole` qualifier
  discriminates.
- **Negative:** dropping one `zea:pointRole` and cutting one VAV's supply duct →
  `Conforms: NO` with two precise `QualifiedMinCount` violations (AHU_1 missing
  `mixedAirTemp`; VAV_2 missing `airSource`). Both non-conforming nodes drop out
  of the witness output; the other VAV still binds. That is the creatable gate and
  blocking-reason report, straight from `validate()`.

Every load-bearing claim of the design — inference closing over plumbing, role +
`zea:pointRole` disambiguation, witnesses as extraction, `validate()` as
discovery/diagnostics — is exercised by this run.

## 9. Relationship to the legacy path

The legacy default startup (`AddDefaultTools`, the enum-slot layout) is unchanged
and remains the fallback when no S223 config is present. The profile-driven path
reuses the *same* `CTool_ahu_ibal`/`CTool_vav_ibal` constructors as the legacy
and existing-S223 paths — it only changes how the tool list is discovered
(witnesses + assembler) and keys instances by RDF resource + `zea:name` instead
of fixed enum slots. No FDD/engine code is reimplemented or duplicated.

## 10. Implementation status

The **full discovery → instantiation path is implemented** (`libEA`, built into
`ead` via the `libEA/*.cpp` wildcard):

- `libEA/toolRole.hpp` — `RoleId`, `PointRoleSpec` (`role → EPointName`),
  `AntecedentRoleSpec`, `ModuleManifest`, and `RoleWitness` →
  `ResolvedTool`/`ResolvedToolModel`. Depends only on the leaf `EPointName` enum,
  no shifty.
- `libEA/analysisModule.{hpp,cpp}` — `IAnalysisModule` + `CModuleRegistry` keyed
  by profile node-shape IRI (== witness `shape_id`).
- `libEA/toolModules.cpp` — `CAhuModule`/`CVavModule` manifests: `role → EPointName`
  + antecedent roles. The point unit/range/label/plot-group stay owned by the
  existing constructor; the manifest does not duplicate them.
- `libEA/toolAssembler.{hpp,cpp}` — `AssembleTools(witnesses, registry)` (group,
  dispatch, reconcile, resolve antecedents, topological order, diagnostics) and
  `StartupSpecsFromModel()` (project onto `std::vector<S223ToolStartupSpec>`).
- `libEA/s223Model.cpp` — `tool_specs_via_profile_witnesses()` runs the witness
  pass and assembler; `LoadS223ApplicationStartupModel` uses it when
  `EA_S223_PROFILE_SHAPES_TTL` is set (`ReadS223ModelLoadConfigFromEnvironment`).
  The existing `CApplication` loop + `AddToolFromS223Spec` + `CTool_*`
  constructors instantiate the tools unchanged.

Verified: `assembler_probe` assembles 3 tools in antecedent-safe order with all
22 roles reconciled and zero diagnostics; the real entrypoint
`LoadS223ApplicationStartupModel` (exercised against `223p.ttl` +
`simple-ahu-vav-223.ttl` + the zea shapes) returns the 3 `S223ToolStartupSpec`s
(`ahu_ibal` AHU-1; `vav_ibal` VAV-1/VAV-2 with `airSource → AHU-1`). The one
piece not exercised here is the final `CTool_*` construction, which needs the
full `ead` link (heavy deps) but is unchanged existing code.

### Remaining work

- Verify a full `ead` build/run instantiates the tools (link-level check of the
  unchanged constructor path).
- Thread each `ResolvedPointBinding::rdfProperty` into its point object for
  channel-native data ingestion (a small additive field on the point; today's
  `EPointName` + column-map ingestion is untouched).
- Expose the resolved model + `validate()` diagnostics over the `/s223/*`
  debug endpoints (the profile-driven `.tools` currently flows through the
  existing `S223ApplicationStartupModel`; the diagnostic report still comes from
  the legacy path — reconcile so `/s223/tools` reflects the witness discovery).
- A strategy for real (unannotated) models: authoring `zea:pointRole` at
  commissioning vs. deriving some roles from native `s223:hasRole` / topology.
- Optional/only-if-wanted: a role-typed `Build` that constructs the FDD graph
  instead of the legacy constructor — a large, verification-heavy port, not
  required by this design.
