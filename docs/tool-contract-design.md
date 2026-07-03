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

A new parallel entrypoint (proposed `CApplicationFromProfiles`, or an env switch)
that never touches `AddDefaultTools` or the `CTool_*` constructors:

1. Load `zea-core.ttl` + `zea-profiles.ttl` as shapes/rules; the site model as
   data.
2. `witnesses(site, {key_path:"zea:roleName", run_inference:true})`.
3. Group witnesses by `focus_node` → one candidate tool per conforming equipment.
4. For point roles: build `CPointAnalog`/`CPointBinary` from `zea:present` +
   attach the bound RDF property/channel IRI as the ingestion identity (this is
   what finally lets data ingestion resolve real device channels from the model).
   For antecedent roles: link to the already-built tool.
5. Assemble `RoleBoundPoints`, look up the module by `zea:analysisModule`, call
   `Build`. Antecedents are built first via a topological sort over the
   `zea:requiredToolProfileId` graph.

### The analysis-module boundary

```cpp
class RoleBoundPoints {
 public:
   CPointAnalog* Analog(RoleId) const;   // concrete type (rules need u_Rain etc.)
   CPointBinary* Binary(RoleId) const;
   bool          IsBound(RoleId) const;  // optional roles
   const RoleBoundPoints& Antecedent(RoleId) const;  // an antecedent's own bound points
};

class IAnalysisModule {
   virtual std::vector<RoleSpec> Roles() const = 0;              // reconciled vs. profile
   virtual void Build(const RoleBoundPoints&, CRuleKit&, BuildCtx) = 0;  // builds CFact/CRule as today
};
```

An economizer fact that today reads `u_Tao->u_Rain->NowY() - u_Tar->u_Rain->NowY()`
becomes, inside `AhuVavReheatModule::Build`, `p.Analog(ahu_vav::outsideAirTemp)`
… `p.Analog(ahu_vav::returnAirTemp)` with identical `CFact` construction. The
module is equipment-type-specific but tool-instance- and RDF-agnostic, reusable
across every AHU in every site. A future declarative rule DSL is just another
`IAnalysisModule` — the seam does not move.

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

The legacy default startup (`AddDefaultTools`, the enum-slot `CTool_*`
constructors, `EPointName`) is unchanged and remains the fallback when no S223
config is present. It is the one sanctioned place with hardcoded tool references.
The profile-driven path is a *new* entrypoint; the AHU/VAV analysis modules
reimplement the FDD against roles once, producing the same `CFact`/`CRule`/
`CSubject`/`CView` graph the engine runs — so nothing about the legacy stack
needs to be preserved bug-for-bug.

## 10. Open work

- Implement the assembler in `libEA/s223Model.*`: witnesses → grouped candidates
  → `RoleBoundPoints`; topological antecedent ordering; the `zea:present` →
  engine-enum vocabulary bridge.
- Implement `RoleId`/`RoleSpec`/`RoleBoundPoints`/`IAnalysisModule` and the
  `AhuVavReheatModule` (AHU + VAV FDD against roles).
- Thread the bound RDF property/channel identity into the point objects for
  data ingestion.
- Decide the new entrypoint trigger (`CApplicationFromProfiles` vs. env switch)
  and expose the witness/validation result over the existing `/s223/*` debug
  endpoints.
- A strategy for real (unannotated) models: authoring `zea:pointRole` at
  commissioning vs. deriving some roles from native `s223:hasRole` / topology.
