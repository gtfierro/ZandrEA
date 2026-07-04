# Current Application Architecture

## Overview

The application is built around a **one-tool-per-physical-unit** model. Each tool instance owns one subject and manages all surveillance (AFDD) logic for that unit.

## Top-Level Structure (`CApplication`)

Located in `libEA/tool.cpp:3786`. The application constructor creates:

| Component | Type | Role |
|-----------|------|------|
| `u_Domain` | `CDomain` | Root container for all subjects in the building |
| `u_Clock` | `CClockPerPort` | Simulation clock, drives tick/bell cycle |
| `u_Agent` | `CAgent` | Core simulation agent |
| `u_Ctrlr` | `CController` | MVC controller, manages GUI ↔ model bridge |
| `u_Seq0` | `CSequence` | Event sequence for observations |
| `u_View` | `CView` | MVC view, GUI representation |
| `u_OmniPort` | `CPortOmni` | External API port (HTTP interface) |
| `u_EachToolInApp` | `vector<unique_ptr<ATool>>` | Collection of all tool instances |

## Subject Hierarchy

```
CDomain
  └── ASubject (one per physical unit)
        ├── CPointAnalog[] (input data channels)
        ├── CPointBinary[] (input binary channels)
        ├── CRuleKit[] (AFDD rules)
        └── CChartShewhart[] (statistical process control charts)
```

A **Subject** represents a single physical piece of equipment placed under AFDD surveillance. Concrete subject types:

| Subject Class | Physical Equipment |
|---------------|-------------------|
| `CSubj_chlr_ibal` | Chiller |
| `CSubj_tes_ibal` | Thermal energy storage (ice tank) |
| `CSubj_ahu_ibal` | Air handling unit (single-duct, VAV reheat) |
| `CSubj_vav_ibal` | VAV terminal unit (pressure-independent, HW reheat) |
| `CSubj_chwp_ibal` | CHW plant |

Each subject has a unique `NGuiKey` (auto-generated `unsigned long long`) and an `ERealName` (enum-based identifier).

## Point Registration Flow

Points self-register with the controller during construction:

```
CTool constructor
  └── std::make_unique<CPointAnalog>(..., *u_Subject, ..., ctrlrRef)
        └── CPointAnalog ctor calls:
              arg0.RegisterBasPointToSubjectKey(this, bArg1.SayGuiKey())
                    └── CController::RegisterBasPointToSubjectKey()
                          ├── pointObjectsZeroToN_bySubjKey[subjectKey].push_back(ptr)
                          └── pointNamesZeroToN_bySubjKey[subjectKey].push_back(ptr->SayPointName())
```

- **Key**: `NGuiKey` from `ASubject::SayGuiKey()` (unique per subject)
- **Values**: Vectors of `ADataChannel*` pointers and `EPointName` enum values
- **Order**: Matches declaration order in the tool constructor
- this determines the point order for data reading and GUI display

## Current Application (4 VAVs, 2 AHUs, 1 Chiller, 1 TES)

From `tool.cpp:3806–3895`:

```
CTool_ahu_ibal  — Subject_ahu1  (antecedent: CHW plant, HW plant sim)
CTool_ahu_ibal  — Subject_ahu2  (antecedent: CHW plant, HW plant sim)
CTool_vav_ibal  — Subject_vav1  (antecedent: AHU2)
CTool_vav_ibal  — Subject_vav2  (antecedent: AHU2)
CTool_vav_ibal  — Subject_vav3  (antecedent: AHU1)
CTool_vav_ibal  — Subject_vav4  (antecedent: AHU1)
CTool_chlr_ibal — Chiller       (antecedent: CT, CHW flow, condenser flow)
CTool_tes_ibal  — TES           (antecedent: CHW plant, kWh rated, tube flow)
```

Each tool is added via an explicit `push_back` call — no loops.

## VAV Point Inventory (per VAV unit)

Each VAV has 11 input points, in declaration order:

| # | Member | Label | Point Name | Type |
|---|--------|-------|------------|------|
| 1 | `u_Psai` | pressure_static_air_inlet | Pressure_static_air_supply | Analog |
| 2 | `u_Tai` | temperature_air_inlet | Temperature_air_supply | Analog |
| 3 | `u_Tad` | temperature_air_discharge | Temperature_air_discharge | Analog |
| 4 | `u_Taz` | temperature_air_zone | Temperature_air_zone | Analog |
| 5 | `u_TazSetptHtg` | temperature_air_zone_setpt_htg | Temperature_air_zone_setpt_htg | Analog |
| 6 | `u_TazSetptClg` | temperature_air_zone_setpt_clg | Temperature_air_zone_setpt_clg | Analog |
| 7 | `u_Uvh` | command_valve_hw | Position_valve_hw | Analog |
| 8 | `u_Udd` | command_damper_disch | Position_damper_vav | Analog |
| 9 | `u_Qad` | flowVolume_air_disch | FlowRateVolume_air_vav | Analog |
| 10 | `u_QadSetpt` | flowVolume_air_disch_setpt | FlowRateVolume_air_vav_setpt | Analog |
| 11 | `u_Bzo` | binary_zone_occupied | Binary_zoneOccupied | Binary |

## Data Flow for Input

```
HTTP API → CPortOmni → CController::ReadInDataForSubject(samples, subjectKey)
                                                    │
                                                    ├─→ pointObjectsZeroToN_bySubjKey[subjectKey][0].ReadFromPortAsNextValue(samples[0])
                                                    ├─→ pointObjectsZeroToN_bySubjKey[subjectKey][1].ReadFromPortAsNextValue(samples[1])
                                                    └─→ ...
```

The GUI must send samples in the **exact same order** that points were declared in the tool constructor.

---

## Adding a New VAV Unit

Adding a new VAV (e.g., `vav5`) requires changes in **three layers**:

### 1. Enum — `ERealName`

Add a new enum entry in `libEA/customTypes.hpp`:

```cpp
enum class ERealName : unsigned int {
   // ... existing entries ...
   Subject_vav4,
   Subject_vav5,       // ← add new VAV name
};
```

### 2. Tool Instantiation — `tool.cpp`

In the `CApplication` constructor (`tool.cpp:3806–3895`), add a new `push_back` for the tool instance. The key argument is the unique `ERealName` plus antecedent references:

```cpp
u_EachToolInApp.push_back(
    std::make_unique<CTool_vav_ibal>(
        unitSys, *u_Domain,
        EDataLabel::Subject_vav_pressIndep_hwReheat,
        ERealName::Subject_vav5,   // ← this VAV's unique name
        ERealName::Subject_ahu1,   // ← antecedent AHU
        ERealName::Subject_hwPlant_sim,
        *u_Clock, *u_Seq0, *u_Ctrlr, *u_View, *u_OmniPort
    )
);
```

The tool constructor automatically creates:
- A new `CSubj_vav_ibal` subject with its own `NGuiKey`
- All 11 `CPointAnalog`/`CPointBinary` members
- Self-registration into `CController`'s maps

### 3. GUI Display — `guiShadow.cpp`

Add a display-name mapping in the `guiShadow.cpp` lookup table so the GUI knows how to render the new subject:

```cpp
{ ERealName::Subject_vav4, "VAV-4" },
{ ERealName::Subject_vav5, "VAV-5" },   // ← add display name
```

### What You Don't Need to Touch

- **The tool constructor** (`CTool_vav_ibal::CTool_vav_ibal`) — this is the blueprint. It runs once per instance, so the 11 `make_unique<CPointAnalog>` calls automatically execute for every new VAV.
- **Point registration** — self-registering via `RegisterBasPointToSubjectKey()`.
- **Data reading** — `ReadInDataForSubject()` iterates the maps by `NGuiKey`, so any new subject is picked up automatically.
