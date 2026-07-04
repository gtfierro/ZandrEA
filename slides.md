---
theme: seriph
title: ZandrEA Architecture
background: /bg.png
nav: false
---

# ZandrEA Application Architecture

## AFDD System for HVAC Equipment Surveillance

<!-- This is a note: ZandrEA is an Arc Fault Detection and Diagnosis system for HVAC equipment surveillance -->

---

# Architecture Overview

<div class="text-sm">

```mermaid
graph TB
    subgraph Application["CApplication"]
        subgraph Core["Core Components"]
            Domain["CDomain<br/>Root container"]
            Clock["CClockPerPort<br/>Simulation clock"]
            Agent["CAgent<br/>Simulation agent"]
            Seq["CSequence<br/>Event sequence"]
        end

        subgraph MVC["MVC Layer"]
            Ctrlr["CController<br/>GUI ↔ model bridge"]
            View["CView<br/>GUI representation"]
        end

        subgraph I/O["Interfaces"]
            OmniPort["CPortOmni<br/>HTTP API port"]
        end

        Tools["vector<unique_ptr<ATool>><br/>Tool instances"]
    end

    subgraph Equipment["8 Physical Units"]
        AHU1["AHU-1"]
        AHU2["AHU-2"]
        VAV1["VAV-1"]
        VAV2["VAV-2"]
        VAV3["VAV-3"]
        VAV4["VAV-4"]
        Chiller["Chiller"]
        TES["TES"]
    end

    Tools --> AHU1
    Tools --> AHU2
    Tools --> VAV1
    Tools --> VAV2
    Tools --> VAV3
    Tools --> VAV4
    Tools --> Chiller
    Tools --> TES

    Domain --> Equipment
    Ctrlr --> Tools
    OmniPort --> Ctrlr
```

</div>

<!-- Notes:
- Top-level view of the entire application
- CApplication bootstraps everything in tool.cpp:3786
- 8 tool instances manage 8 physical units
- MVC pattern separates controller, view, and model
-->

---
layout: two
---

# Core Principle: One Tool Per Unit

<div class="pt-4">

Each tool instance owns **one subject** and manages all surveillance (AFDD) logic for that unit.

</div>

<div class="pt-4">

```mermaid
graph LR
    subgraph ToolInstance["CTool_vav_ibal Instance"]
        direction TB
        Subject["CSubj_vav_ibal<br/>Subject"]
        Points["11 Data Points<br/>10 Analog + 1 Binary"]
        Rules["CRuleKit[]<br/>AFDD Rules"]
        Charts["CChartShewhart[]<br/>SPC Charts"]
    end

    ToolInstance -->|"owns"| Subject
    Subject --> Points
    Subject --> Rules
    Subject --> Charts
```

</div>

<!-- Notes:
- Each CTool maps to exactly one ASubject
- The subject owns its points, rules, and charts
- No sharing between units — complete isolation
-->

---

# Subject Hierarchy

<div class="text-sm">

```mermaid
graph TD
    Domain["CDomain<br/>Root Container"] -->|"contains"| Subject1["ASubject<br/>Physical Unit 1"]
    Domain -->|"contains"| Subject2["ASubject<br/>Physical Unit N"]

    Subject1 --> PointsA["CPointAnalog[]<br/>Input data channels"]
    Subject1 --> PointsB["CPointBinary[]<br/>Input binary channels"]
    Subject1 --> Rules["CRuleKit[]<br/>AFDD rules"]
    Subject1 --> Charts["CChartShewhart[]<br/>Statistical process control"]

    Subject2 --> PointsA2["CPointAnalog[]"]
    Subject2 --> PointsB2["CPointBinary[]"]
    Subject2 --> Rules2["CRuleKit[]"]
    Subject2 --> Charts2["CChartShewhart[]"]

    style Domain fill:#3b82f6,color:#fff
    style Subject1 fill:#10b981,color:#fff
    style Subject2 fill:#10b981,color:#fff
```

</div>

<!-- Notes:
- CDomain is the root container for all subjects
- Each subject represents one physical piece of equipment
- Points, rules, and charts are all owned by the subject
-->

---

# Concrete Subject Types

<div class="text-sm">

```mermaid
graph LR
    Domain["CDomain"] --> Chiller["CSubj_chlr_ibal<br/>Chiller"]
    Domain --> TES["CSubj_tes_ibal<br/>Thermal Energy Storage"]
    Domain --> AHU["CSubj_ahu_ibal<br/>Air Handling Unit"]
    Domain --> VAV["CSubj_vav_ibal<br/>VAV Terminal Unit"]
    Domain --> CHW["CSubj_chwp_ibal<br/>CHW Plant"]

    style Chiller fill:#ef4444,color:#fff
    style TES fill:#3b82f6,color:#fff
    style AHU fill:#f59e0b,color:#fff
    style VAV fill:#10b981,color:#fff
    style CHW fill:#8b5cf6,color:#fff
```

</div>

<div class="text-sm mt-4">

Each subject has:
- A unique **`NGuiKey`** (`unsigned long long`)
- An **`ERealName`** (enum-based identifier)

</div>

---

# Equipment Topology

<div class="text-sm">

```mermaid
graph TB
    subgraph Supply["CHW Supply"]
        CHWP["CHW Plant"]
        AHU1["AHU-1"]
        AHU2["AHU-2"]
    end

    subgraph Distribution["VAV Distribution"]
        VAV3["VAV-3"]
        VAV4["VAV-4"]
        VAV1["VAV-1"]
        VAV2["VAV-2"]
    end

    subgraph Storage["Storage & Cooling"]
        Chiller["Chiller"]
        TES["TES"]
    end

    CHWP --> AHU1
    CHWP --> AHU2
    AHU1 --> VAV3
    AHU1 --> VAV4
    AHU2 --> VAV1
    AHU2 --> VAV2
    Chiller --> CHWP
    TES --> CHWP

    style CHWP fill:#8b5cf6,color:#fff
    style AHU1 fill:#f59e0b,color:#fff
    style AHU2 fill:#f59e0b,color:#fff
    style VAV1 fill:#10b981,color:#fff
    style VAV2 fill:#10b981,color:#fff
    style VAV3 fill:#10b981,color:#fff
    style VAV4 fill:#10b981,color:#fff
    style Chiller fill:#ef4444,color:#fff
    style TES fill:#3b82f6,color:#fff
```

</div>

<!-- Notes:
- Arrows show antecedent relationships
- VAVs depend on their parent AHU
- AHUs depend on the CHW plant
- Chiller and TES feed the CHW plant
-->

---
layout: two
---

# VAV Point Inventory (11 per unit)

<div class="pt-4 text-sm">

```mermaid
graph TD
    VAV["VAV Unit"] -->|"1"| Pressure["Pressure_static_air_supply"]
    VAV -->|"2"| TempSupply["Temperature_air_supply"]
    VAV -->|"3"| TempDisch["Temperature_air_discharge"]
    VAV -->|"4"| TempZone["Temperature_air_zone"]
    VAV -->|"5"| SetptHtg["Temperature_air_zone_setpt_htg"]
    VAV -->|"6"| SetptClg["Temperature_air_zone_setpt_clg"]
    VAV -->|"7"| ValveHW["Position_valve_hw"]
    VAV -->|"8"| Damper["Position_damper_vav"]
    VAV -->|"9"| FlowVAV["FlowRateVolume_air_vav"]
    VAV -->|"10"| FlowSetpt["FlowRateVolume_air_vav_setpt"]
    VAV -->|"11"| Occupied["Binary_zoneOccupied"]

    style VAV fill:#10b981,color:#fff
    style Occupied fill:#ef4444,color:#fff
```

</div>

<!-- Notes:
- 10 analog points + 1 binary point
- Order matters for data reading and GUI display
- Each point is a CPointAnalog or CPointBinary -->

---

# Point Registration Flow

<div class="text-sm">

```mermaid
sequenceDiagram
    participant CTool as CTool_vav_ibal
    participant CP as CPointAnalog
    participant CS as CSubj_vav_ibal
    participant CC as CController

    CTool->>CP: make_unique<CPointAnalog>(..., *CS, ctrlrRef)
    activate CP
    CP->>CS: RegisterBasPointToSubjectKey(this, guiKey)
    activate CS
    CS-->>CP: subject key
    deactivate CS
    CP->>CC: RegisterBasPointToSubjectKey()
    activate CC
    CC->>CC: pointObjects_bySubjKey[key].push_back(ptr)
    CC->>CC: pointNames_bySubjKey[key].push_back(name)
    CC-->>CP: registered
    deactivate CC
    CP-->>CTool: point created
    deactivate CP
```

</div>

<!-- Notes:
- Points self-register during construction
- Controller maintains parallel maps by subject key
- Two vectors: object pointers and name enums
- Order matches declaration order in tool constructor
-->

---

# Controller Data Maps

<div class="text-sm">

```mermaid
graph TB
    CC["CController"] -->|"key: NGuiKey"| Map1["pointObjectsZeroToN_bySubjKey"]
    CC -->|"key: NGuiKey"| Map2["pointNamesZeroToN_bySubjKey"]

    Map1 -->|"Subject_vav1"| VAV1Pts["CPointAnalog* ptrs<br/>[11 entries in declaration order]"]
    Map1 -->|"Subject_vav2"| VAV2Pts["CPointAnalog* ptrs<br/>[11 entries]"]
    Map1 -->|"Subject_ahu1"| AHU1Pts["CPointAnalog* ptrs<br/>[N entries]"]

    Map2 -->|"Subject_vav1"| VAV1Names["EPointName enums<br/>[11 entries]"]
    Map2 -->|"Subject_vav2"| VAV2Names["EPointName enums<br/>[11 entries]"]
    Map2 -->|"Subject_ahu1"| AHU1Names["EPointName enums<br/>[N entries]"]

    style CC fill:#3b82f6,color:#fff
    style Map1 fill:#10b981,color:#fff
    style Map2 fill:#f59e0b,color:#fff
```

</div>

---
layout: two
---

# Data Flow: Input Pipeline

<div class="pt-4 text-sm">

```mermaid
sequenceDiagram
    participant GUI as GUI / BACnet
    participant HTTP as HTTP API
    participant Omni as CPortOmni
    participant Ctrlr as CController
    participant P1 as point[0]
    participant P2 as point[1]
    participant PN as point[N]

    GUI->>HTTP: POST samples [ordered]
    HTTP->>Omni: ReadInDataForSubject(samples, key)
    Omni->>Ctrlr: ReadInDataForSubject(samples, key)

    Ctrlr->>P1: ReadFromPortAsNextValue(samples[0])
    Ctrlr->>P2: ReadFromPortAsNextValue(samples[1])
    Ctrlr->>PN: ReadFromPortAsNextValue(samples[N])

    P1-->>Ctrlr: value updated
    P2-->>Ctrlr: value updated
    PN-->>Ctrlr: value updated
```

</div>

<!-- Notes:
- Samples must be in exact declaration order
- Controller iterates pointObjects_bySubjKey map
- Each point reads its next value from the samples array
-->

---

# Data Flow: AFDD Processing

<div class="text-sm">

```mermaid
flowchart LR
    Points["CPointAnalog[]<br/>Input Values"] -->|"on new sample"| Rules["CRuleKit[]<br/>AFDD Rules"]
    Points -->|"on new sample"| Charts["CChartShewhart[]<br/>SPC Charts"]
    Rules -->|"alert / anomaly"| View["CView<br/>GUI Notification"]
    Charts -->|"trend / outlier"| View
    Rules -->|"state change"| Subject["ASubject<br/>Internal State"]
    Charts -->|"statistical update"| Subject

    style Points fill:#10b981,color:#fff
    style Rules fill:#ef4444,color:#fff
    style Charts fill:#f59e0b,color:#fff
    style View fill:#3b82f6,color:#fff
    style Subject fill:#8b5cf6,color:#fff
```

</div>

<!-- Notes:
- New samples trigger both rules and charts
- Rules check for AFDD conditions
- Charts track statistical process control
- Both feed into the GUI for visualization
-->

---

# Full Application Bootstrap

<div class="text-xs">

```mermaid
flowchart TD
    Start["CApplication ctor<br/>tool.cpp:3786"] --> Core["Create core components"]
    Core -->|"u_Domain"| CDomain["CDomain"]
    Core -->|"u_Clock"| CClock["CClockPerPort"]
    Core -->|"u_Agent"| CAgent["CAgent"]
    Core -->|"u_Ctrlr"| CCtrlr["CController"]
    Core -->|"u_Seq0"| CSeq["CSequence"]
    Core -->|"u_View"| CView["CView"]
    Core -->|"u_OmniPort"| COmni["CPortOmni"]

    Core -->|"push_back × 8"| Tools["u_EachToolInApp"]

    subgraph ToolInstances["8 Tool Instances"]
        T1["CTool_ahu_ibal → AHU-1"]
        T2["CTool_ahu_ibal → AHU-2"]
        T3["CTool_vav_ibal → VAV-1"]
        T4["CTool_vav_ibal → VAV-2"]
        T5["CTool_vav_ibal → VAV-3"]
        T6["CTool_vav_ibal → VAV-4"]
        T7["CTool_chlr_ibal → Chiller"]
        T8["CTool_tes_ibal → TES"]
    end

    Tools --> T1
    Tools --> T2
    Tools --> T3
    Tools --> T4
    Tools --> T5
    Tools --> T6
    Tools --> T7
    Tools --> T8

    style Start fill:#f59e0b,color:#fff
    style Core fill:#3b82f6,color:#fff
    style Tools fill:#10b981,color:#fff
```

</div>

---

# Adding a New VAV — Step 1: Enum

<div class="text-sm">

```mermaid
flowchart LR
    File["libEA/customTypes.hpp"] -->|"contains"| Enum["enum class ERealName"]
    Enum -->|"existing"| S1["Subject_vav1"]
    Enum -->|"existing"| S2["Subject_vav2"]
    Enum -->|"existing"| S3["Subject_vav3"]
    Enum -->|"existing"| S4["Subject_vav4"]
    Enum -->|"NEW ✏️"| S5["Subject_vav5"]

    style File fill:#6b7280,color:#fff
    style Enum fill:#3b82f6,color:#fff
    style S5 fill:#10b981,color:#fff
```

</div>

<div class="text-sm mt-4">

```cpp
enum class ERealName : unsigned int {
   // ... existing entries ...
   Subject_vav4,
   Subject_vav5,       // ← add new VAV name
};
```

</div>

---

# Adding a New VAV — Step 2: Instantiation

<div class="text-sm">

```mermaid
flowchart TD
    File["tool.cpp:3806–3895"] -->|"CApplication ctor"| Push["push_back<CTool_vav_ibal>"]

    Push -->|"creates"| Subject["CSubj_vav_ibal<br/>new subject"]
    Push -->|"creates"| Points["11 × CPointAnalog /<br/>CPointBinary"]
    Push -->|"registers"| Controller["CController<br/>pointObjects + pointNames maps"]

    Subject -->|"NGuiKey"| Key["unique unsigned long long"]
    Subject -->|"ERealName"| Name["ERealName::Subject_vav5"]

    style File fill:#6b7280,color:#fff
    style Push fill:#f59e0b,color:#fff
    style Controller fill:#ef4444,color:#fff
    style Key fill:#10b981,color:#fff
    style Name fill:#10b981,color:#fff
```

</div>

<div class="text-sm mt-4">

```cpp
u_EachToolInApp.push_back(
    std::make_unique<CTool_vav_ibal>(
        unitSys, *u_Domain,
        EDataLabel::Subject_vav_pressIndep_hwReheat,
        ERealName::Subject_vav5,   // ← unique name
        ERealName::Subject_ahu1,   // ← antecedent AHU
        ERealName::Subject_hwPlant_sim,
        *u_Clock, *u_Seq0, *u_Ctrlr, *u_View, *u_OmniPort
    )
);
```

</div>

---

# Adding a New VAV — Step 3: GUI Display

<div class="text-sm">

```mermaid
flowchart LR
    File["guiShadow.cpp"] -->|"lookup table"| Map{"guiShadow<br/>display names"}
    Map -->|"existing"| M1["Subject_vav4 → VAV-4"]
    Map -->|"NEW ✏️"| M2["Subject_vav5 → VAV-5"]

    style File fill:#6b7280,color:#fff
    style Map fill:#3b82f6,color:#fff
    style M2 fill:#10b981,color:#fff
```

</div>

<div class="text-sm mt-4">

```cpp
{ ERealName::Subject_vav4, "VAV-4" },
{ ERealName::Subject_vav5, "VAV-5" },   // ← add display name
```

</div>

---

# Adding a New VAV — What You DON'T Touch

<div class="text-sm">

```mermaid
flowchart LR
    Auto["Automatic — NO CHANGES NEEDED"]

    Auto -->|"tool constructor runs once"| T1["CTool_vav_ibal::CTool_vav_ibal<br/>11 × make_unique<CPointAnalog>"]
    Auto -->|"self-registering"| T2["RegisterBasPointToSubjectKey()<br/>auto-populates controller maps"]
    Auto -->|"iterates by NGuiKey"| T3["ReadInDataForSubject()<br/>any new subject picked up automatically"]

    style Auto fill:#10b981,color:#fff
    style T1 fill:#6b7280,color:#fff
    style T2 fill:#6b7280,color:#fff
    style T3 fill:#6b7280,color:#fff
```

</div>

---

# Summary: 3 Touch Points

<div class="text-sm">

```mermaid
flowchart TB
    subgraph Step1["Step 1: Enum"]
        E1["Add ERealName::Subject_vav5<br/>libEA/customTypes.hpp"]
    end

    subgraph Step2["Step 2: Instantiate"]
        E2["Add push_back in CApplication<br/>libEA/tool.cpp"]
    end

    subgraph Step3["Step 3: GUI Name"]
        E3["Add display name mapping<br/>guiShadow.cpp"]
    end

    Step1 -->|"triggers"| Step2
    Step2 -->|"creates"| Subject["New CSubj_vav_ibal<br/>+ 11 points + registration"]
    Step3 -->|"renders"| GUI["GUI displays 'VAV-5'"]

    style Step1 fill:#3b82f6,color:#fff
    style Step2 fill:#f59e0b,color:#fff
    style Step3 fill:#10b981,color:#fff
    style Subject fill:#8b5cf6,color:#fff
    style GUI fill:#ef4444,color:#fff
```

</div>

---

# Architecture Summary

<div class="text-sm">

```mermaid
mindmap
  root((ZandrEA<br/>Architecture))
    Design Principle
      One Tool Per Unit
      Complete Isolation
      Self-Registering Points
    Core Layers
      CApplication
        Core Components
        MVC Pattern
        HTTP Interface
      CController
        Point Maps by NGuiKey
        Data Routing
      CView
        GUI Display
        AFDD Notifications
    Equipment
      2 × AHU
      4 × VAV
      1 × Chiller
      1 × TES
    Extensibility
      3 Touch Points
      Automatic Registration
      Map-Based Routing
```

</div>

<!-- Notes:
- Key takeaway: the architecture is designed for extensibility
- New units require changes in exactly 3 places
- Point registration and data reading are fully automatic
- Map-based routing by NGuiKey means no loops needed
-->

---

layout: center
class: text-center
---

# Questions?

<!-- Notes:
- End of architecture overview
- Reference libEA/tool.cpp for the full implementation
-->
