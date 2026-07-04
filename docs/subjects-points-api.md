# Subjects and Points API Surface Documentation

This document describes the current API surface for Subjects and Points in the ZandrEA system, and outlines what is needed to export this information via a new API call.

---

## 1. Overview

### 1.1 Key Concepts

| Term | Description |
|------|-------------|
| **Subject** | Represents a physical piece of equipment under AFDD surveillance (e.g., VAV, AHU, Chiller, TES, CHWP) |
| **Point** | A data channel representing a single measurement or control signal (analog or binary) |
| **EPointName** | Enum identifying the semantic meaning of a point (e.g., `Temperature_air_supply`) |
| **NGuiKey** | Unique identifier for GUI objects (subjects, points, knobs, rules, etc.) |

### 1.2 Data Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ZandrEA Runtime Architecture                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ┌──────────────┐     ┌──────────────┐     ┌──────────────────────────┐   │
│   │   Tool       │────▶│   Subject    │────▶│   Points (DataChannels)  │   │
│   │ (CTool_*)    │     │ (CSubj_*_ibal)│     │   (CPointAnalog/Binary)  │   │
│   └──────────────┘     └──────────────┘     └──────────────────────────┘   │
│         │                    │                        │                     │
│         │                    │                        │                     │
│         ▼                    ▼                        ▼                     │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                    CController (MVC Controller)                     │   │
│   │  - pointNamesZeroToN_bySubjKey: Map<NGuiKey, Vector<EPointName>>   │   │
│   │  - pointObjectsZeroToN_bySubjKey: Map<NGuiKey, Vector<ADataChannel*>> │ │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                              │                                              │
│                              ▼                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                    IExportOmni / CPortOmni                          │   │
│   │              (Backend API Interface for Frontend)                   │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                              │                                              │
│                              ▼                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                         Frontend (GUI)                              │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Current API Surface

### 2.1 Existing API Calls Related to Subjects

| API Method | Location | Description |
|------------|----------|-------------|
| `SayInfoFromSubject(NGuiKey)` | `IExportOmni`, `CPortOmni` | Returns `GuiPackSubjectBasic_t` with subject metadata |
| `SayCurrentCasesFromSubject(NGuiKey)` | `IExportOmni`, `CPortOmni` | Returns `GuiPackSubjectCases_t` with case info |
| `SayTextIdentifyingSubject(NGuiKey)` | `IExportOmni`, `CPortOmni` | Returns human-readable subject name |
| `SayInputPointNameOrderExpectedBySubject(NGuiKey)` | `IExportOmni`, `CPortOmni`, `CController` | Returns ordered list of `EPointName` for a subject |
| `SetCoincidentInputsForSubject(Vector<GuiFpn_t>, NGuiKey)` | `IExportOmni`, `CPortOmni`, `CController` | Sets input values for all points of a subject |

### 2.2 Subject Data Structure

**`GuiPackSubjectBasic_t`** (`exportTypes.hpp:599-624`):
```cpp
typedef struct SGuiPackSubjectBasic {
   EGuiReply                                 getterReply;
   EGuiType                                  ownType;
   NGuiKey                                   ownKey;
   NGuiKey                                   hostDomainKey;
   std::string                               ownNameText;
   std::vector<std::string>                  infoText_byCR;  // 1st line = subject label
   std::vector<NGuiKey>                      featureKeys;
   std::vector<NGuiKey>                      paramKnobKeys;
   std::vector<NGuiKey>                      ruleKitKeys;
} GuiPackSubjectBasic_t;
```

**Internal Controller Data** (`mvc_ctrlr.hpp:79-80`):
```cpp
typedef std::unordered_map<NGuiKey, std::vector<EPointName>>       SubjPointNameTable_t;
typedef std::unordered_map<NGuiKey, std::vector<ADataChannel*>>    SubjPointObjectTable_t;

// Member variables:
SubjPointNameTable_t    pointNamesZeroToN_bySubjKey;
SubjPointObjectTable_t  pointObjectsZeroToN_bySubjKey;
```

### 2.3 Point Data Structure

**`ADataChannel` Base Class** (`dataChannel.hpp:34-60`):
```cpp
class ADataChannel : public ISeqElement {
   public:
      ADataChannel(  CSequence&, ASubject&, EApiType, EDataLabel,
                     EDataUnit, EDataRange, EDataSuffix, EPlotGroup,
                     int, EPointName );

      EPointName SayPointName(void) const;

   protected:
      double               xGivenDbl;     // Current value
      double               xPrevDbl;      // Previous value
      const EPointName     pointName;     // Semantic identifier
};
```

**`CPointAnalog`** (concrete class for analog/proportional data):
```cpp
class CPointAnalog : public ADataChannel {
   public:
      CPointAnalog(  CSequence&, ASubject&, EDataLabel, EDataUnit,
                     EDataRange, EPlotGroup, EPointName, CController& );
   // Additional fields: xPosted, xValidMin, xValidMax, xLastValid, etc.
};
```

**`CPointBinary`** (concrete class for binary on/off data):
```cpp
class CPointBinary : public ADataChannel {
   public:
      CPointBinary(  CSequence&, ASubject&, EDataLabel, EDataLabel,
                     EPointName, CController& );
   // Additional fields: binaryPosted
};
```

### 2.4 Enumerations

**`EPointName`** (`exportTypes.hpp:187-220`):
```cpp
enum class EPointName : unsigned int {
   Undefined,
   Binary_systemOccupied,
   Binary_zoneOccupied,
   Command_damper_mixingBox,
   Command_damper_outsideAir,
   Command_damper_vav,
   Command_fanSpeed,
   Command_valve_chw,
   Command_valve_hw,
   FlowRateVolume_air_ahu,
   FlowRateVolume_air_ahu_setpt,
   FlowRateVolume_air_vav,
   FlowRateVolume_air_vav_setpt,
   Position_damper_mixingBox,
   Position_damper_outsideAir,
   Position_damper_vav,
   Position_valve_chw,
   Position_valve_hw,
   Pressure_static_air_inlet,
   Pressure_static_air_supply,
   Pressure_static_air_supply_setpt,
   Temperature_air_discharge,
   Temperature_air_inlet,
   Temperature_air_mixed,
   Temperature_air_outside,
   Temperature_air_return,
   Temperature_air_supply,
   Temperature_air_supply_setpt,
   Temperature_air_zone,
   Temperature_air_zone_setpt_clg,
   Temperature_air_zone_setpt_htg,
   Temperature_glycol_leaving,
   Temperature_glycol_leaving_setpt,
   Temperature_water_leaving
};
```

**Related Enums** (for full point metadata):
- `EDataLabel` (`customTypes.hpp:637`) - Descriptive label (e.g., `Point_temperature_air_supply`)
- `EDataUnit` (`customTypes.hpp`) - Physical units (e.g., `Temperature_degC`)
- `EDataRange` (`customTypes.hpp`) - Valid value ranges
- `EDataSuffix` (`customTypes.hpp`) - Display suffixes
- `EPlotGroup` (`customTypes.hpp`) - GUI plotting group

---

## 3. How Point Names Are Registered

### 3.1 Registration Flow

1. **Point Creation** (`tool.cpp`):
   ```cpp
   u_Tas = std::make_unique<CPointAnalog>(
       seq0Ref,
       *u_Subject,
       EDataLabel::Point_temperature_air_supply,
       EDataUnit::Temperature_degC,
       EDataRange::Analog_n18To49,
       EPlotGroup::GroupA,
       EPointName::Temperature_air_supply,  // ← Semantic name
       ctrlrRef
   );
   ```

2. **Constructor Registration** (`dataChannel.cpp:118`):
   ```cpp
   CPointAnalog::CPointAnalog(...) {
       // ... initialization ...
       bArg0.Register(this);  // Register with Controller
       arg0.RegisterBasPointToSubjectKey(this, bArg1.SayGuiKey());  // Register point to subject
   }
   ```

3. **Controller Storage** (`mvc_ctrlr.cpp:139-157`):
   ```cpp
   void CController::RegisterBasPointToSubjectKey(ADataChannel* ptr, NGuiKey subjectKey) {
       // Ensure vector exists for this subject
       pointNamesZeroToN_bySubjKey.insert({subjectKey, std::vector<EPointName>(0)});
       
       // Append point name in order of registration
       pointNamesZeroToN_bySubjKey.at(subjectKey).push_back(ptr->SayPointName());
   }
   ```

4. **Retrieval** (`mvc_ctrlr.cpp:69-71`):
   ```cpp
   std::vector<EPointName> CController::SayInputPointNameOrderExpectedBySubject(NGuiKey subjectKey) const {
       return pointNamesZeroToN_bySubjKey.at(subjectKey);
   }
   ```

### 3.2 Subject Types and Their Typical Points

| Subject Type | Class | Typical Points |
|--------------|-------|----------------|
| VAV | `CSubj_vav_ibal` | Temperature, pressure, damper position, flow rate |
| AHU | `CSubj_ahu_ibal` | Temperature (outside, mixed, return, supply), damper/valve positions, flow rate |
| Chiller | `CSubj_chlr_ibal` | Temperature (water leaving, glycol), pressure, power, flow rate |
| TES | `CSubj_tes_ibal` | Temperature, flow rate, ice content, valve position |
| CHWP | `CSubj_chwp_ibal` | Pressure, temperature, flow rate |

---

## 4. Proposed New API: Export Subject/Point Metadata

### 4.1 Goal

Provide a single API call that returns complete metadata about all subjects and their associated points, including:
- Subject identification (name, key, type)
- Point list for each subject (name, label, unit, range, current value)

### 4.2 Proposed API Signature

**In `IExportOmni`** (`exportCalls.hpp`):
```cpp
// Returns complete subject/point metadata for all subjects
virtual std::vector<GuiPackSubjectPoints_t> SayAllSubjectsAndPoints(void) const = 0;

// Or optionally, per-subject:
virtual GuiPackSubjectPoints_t SaySubjectAndPoints(NGuiKey subjectKey) const = 0;
```

**In `CPortOmni`** (`portOmni.hpp`):
```cpp
virtual std::vector<GuiPackSubjectPoints_t> SayAllSubjectsAndPoints(void) const override;
virtual GuiPackSubjectPoints_t SaySubjectAndPoints(NGuiKey subjectKey) const override;
```

**In `CController`** (`mvc_ctrlr.hpp`):
```cpp
std::vector<GuiPackSubjectPoints_t> SayAllSubjectsAndPoints(void) const;
GuiPackSubjectPoints_t SaySubjectAndPoints(NGuiKey subjectKey) const;
```

### 4.3 Proposed Data Structures

**`GuiPackSubjectPoints_t`** (new struct in `exportTypes.hpp`):
```cpp
typedef struct SGuiPackPointMetadata {
   EGuiReply         getterReply;
   NGuiKey           pointKey;           // Unique point identifier
   EPointName        pointName;          // Semantic name (e.g., Temperature_air_supply)
   EDataLabel        pointLabel;         // Descriptive label
   EDataUnit         pointUnit;          // Physical units
   EDataRange        pointRange;         // Valid value range
   EPlotGroup        plotGroup;          // GUI plotting group
   GuiFpn_t          currentValue;       // Latest value (NaN if unavailable)
   bool              isValid;            // Whether current value is valid
} GuiPackPointMetadata_t;

typedef struct SGuiPackSubjectPoints {
   EGuiReply                           getterReply;
   NGuiKey                             subjectKey;
   std::string                         subjectNameText;
   EDataLabel                          subjectLabel;
   EApiType                            subjectType;
   std::vector<GuiPackPointMetadata_t> points;  // Ordered list of points for this subject
} GuiPackSubjectPoints_t;
```

### 4.4 Implementation Requirements

#### 4.4.1 Changes to `ADataChannel` (`dataChannel.hpp`)

Add accessor methods to expose point metadata:
```cpp
class ADataChannel : public ISeqElement {
   public:
      // Existing methods...
      EPointName           SayPointName(void) const;
      
      // NEW: Additional metadata accessors
      EDataLabel           SayPointLabel(void) const;
      EDataUnit            SayPointUnit(void) const;
      EDataRange           SayPointRange(void) const;
      EPlotGroup           SayPlotGroup(void) const;
      GuiFpn_t             SayCurrentValue(void) const;
      bool                 SayIsValid(void) const;
};
```

#### 4.4.2 Changes to `CPointAnalog` and `CPointBinary` (`dataChannel.hpp/cpp`)

Store the additional constructor parameters as member variables and implement the accessors.

**For `CPointAnalog`**:
```cpp
class CPointAnalog : public ADataChannel {
   protected:
      const EDataLabel     pointLabel;
      const EDataUnit      pointUnit;
      const EDataRange     pointRange;
      const EPlotGroup     plotGroup;
      
   public:
      // Implement SayPointLabel(), SayPointUnit(), etc.
};
```

**For `CPointBinary`**:
```cpp
class CPointBinary : public ADataChannel {
   protected:
      const EDataLabel     pointLabel;
      // Binary points may not need unit/range/plotGroup
   public:
      // Implement accessors
};
```

#### 4.4.3 Changes to `CController` (`mvc_ctrlr.hpp/cpp`)

Add the new method to retrieve subject/point metadata:

```cpp
// In mvc_ctrlr.hpp:
std::vector<GuiPackSubjectPoints_t> SayAllSubjectsAndPoints(void) const;
GuiPackSubjectPoints_t              SaySubjectAndPoints(NGuiKey subjectKey) const;

// In mvc_ctrlr.cpp:
std::vector<GuiPackSubjectPoints_t> CController::SayAllSubjectsAndPoints(void) const {
    std::vector<GuiPackSubjectPoints_t> reply;
    
    // Iterate over all subjects (need access to subject map)
    for (const auto& [subjectKey, pointNameList] : pointNamesZeroToN_bySubjKey) {
        GuiPackSubjectPoints_t subjPoints;
        subjPoints.subjectKey = subjectKey;
        
        // Build point list
        auto pointObjIter = pointObjectsZeroToN_bySubjKey.find(subjectKey);
        if (pointObjIter != pointObjectsZeroToN_bySubjKey.end()) {
            const auto& pointObjects = pointObjIter->second;
            
            for (size_t i = 0; i < pointObjects.size(); ++i) {
                GuiPackPointMetadata_t pointMeta;
                pointMeta.pointKey = pointObjects[i]->SayGuiKey();
                pointMeta.pointName = pointObjects[i]->SayPointName();
                pointMeta.pointLabel = pointObjects[i]->SayPointLabel();
                pointMeta.pointUnit = pointObjects[i]->SayPointUnit();
                pointMeta.pointRange = pointObjects[i]->SayPointRange();
                pointMeta.plotGroup = pointObjects[i]->SayPlotGroup();
                pointMeta.currentValue = pointObjects[i]->SayCurrentValue();
                pointMeta.isValid = pointObjects[i]->SayIsValid();
                
                subjPoints.points.push_back(pointMeta);
            }
        }
        
        reply.push_back(subjPoints);
    }
    
    return reply;
}
```

#### 4.4.4 Changes to `CPortOmni` (`portOmni.hpp/cpp`)

Implement the port layer that bridges controller to frontend:

```cpp
// In portOmni.cpp:
std::vector<GuiPackSubjectPoints_t> CPortOmni::SayAllSubjectsAndPoints(void) const {
    return CtrlrRef.SayAllSubjectsAndPoints();
}

GuiPackSubjectPoints_t CPortOmni::SaySubjectAndPoints(NGuiKey subjectKey) const {
    // Could delegate to controller or build here
    auto allData = CtrlrRef.SayAllSubjectsAndPoints();
    
    for (const auto& subj : allData) {
        if (subj.subjectKey == subjectKey) {
            return subj;
        }
    }
    
    return GuiPackSubjectPoints_t(EGuiReply::FAIL_any_givenKeyNotValidForFunctionCalled);
}
```

#### 4.4.5 Lookup Table Updates (`guiShadow.cpp`)

Ensure all new enum types have text lookups for human-readable output:
- `LookUpText(EPointName)` - Full semantic name
- `LookUpTag(EDataLabel)` - Short label
- `LookUpText(EDataUnit)` - Unit string

---

## 5. Files to Modify

| File | Changes Required |
|------|------------------|
| `libEA/exportTypes.hpp` | Add `GuiPackPointMetadata_t` and `GuiPackSubjectPoints_t` structs |
| `libEA/exportCalls.hpp` | Add `SayAllSubjectsAndPoints()` and `SaySubjectAndPoints()` to `IExportOmni` |
| `libEA/portOmni.hpp` | Add override declarations to `CPortOmni` |
| `libEA/portOmni.cpp` | Implement `CPortOmni` methods |
| `libEA/mvc_ctrlr.hpp` | Add `CController` method declarations |
| `libEA/mvc_ctrlr.cpp` | Implement `CController` methods |
| `libEA/dataChannel.hpp` | Add accessor method declarations to `ADataChannel`, `CPointAnalog`, `CPointBinary` |
| `libEA/dataChannel.cpp` | Implement accessor methods, store metadata in constructors |
| `libEA/guiShadow.cpp` | Add `LookUpText(EPointName)` if not exists |

---

## 6. Example Usage

### 6.1 Frontend Request

```cpp
// Get all subjects with their points
auto subjects = p_Port->SayAllSubjectsAndPoints();

for (const auto& subj : subjects) {
    std::cout << "Subject: " << subj.subjectNameText << std::endl;
    
    for (const auto& point : subj.points) {
        std::cout << "  - " << LookUpText(point.pointName) 
                  << " = " << point.currentValue 
                  << " " << LookUpText(point.pointUnit) << std::endl;
    }
}
```

### 6.2 Expected Response Format (JSON-like)

```json
[
  {
    "subjectKey": 1001,
    "subjectNameText": "AHU-1",
    "subjectLabel": "Air Handling Unit",
    "subjectType": "Subject",
    "points": [
      {
        "pointKey": 2001,
        "pointName": "Temperature_air_supply",
        "pointLabel": "Supply Air Temperature",
        "pointUnit": "degC",
        "pointRange": "Analog_n18To49",
        "currentValue": 13.5,
        "isValid": true
      },
      {
        "pointKey": 2002,
        "pointName": "Position_valve_chw",
        "pointLabel": "CHW Valve Position",
        "pointUnit": "%",
        "pointRange": "Analog_percent",
        "currentValue": 45.2,
        "isValid": true
      }
    ]
  },
  {
    "subjectKey": 1002,
    "subjectNameText": "VAV-101",
    "subjectLabel": "Variable Air Volume",
    "subjectType": "Subject",
    "points": [
      // ... points for VAV
    ]
  }
]
```

---

## 7. Considerations and Trade-offs

### 7.1 Performance

- **Current**: `SayInputPointNameOrderExpectedBySubject` returns only enum values (compact)
- **Proposed**: Returns full metadata (larger payload)
- **Mitigation**: Consider caching or lazy loading for large deployments

### 7.2 Backward Compatibility

- New API methods are additive (no breaking changes)
- Existing methods continue to work as before

### 7.3 Data Consistency

- Point metadata is static (set at construction) - safe to cache
- Current values are dynamic - must be fetched fresh

### 7.4 Subject Map Access

The `CController` currently does not have direct access to a map of all subjects. Options:
1. Add `SubjPtrTable_t` to `CController` (similar to `p_Knobs_byKey`)
2. Query `CView` for subject list first
3. Build subject info from `pointNamesZeroToN_bySubjKey` keys

---

## 8. Related Documentation

- `libEA/exportCalls.hpp` - Runtime API interface
- `libEA/exportTypes.hpp` - Data structures for API
- `libEA/mvc_ctrlr.hpp/cpp` - Controller implementation
- `libEA/dataChannel.hpp/cpp` - Point class definitions
- `libEA/subject.hpp/cpp` - Subject class definitions
- `libEA/portOmni.hpp/cpp` - API port layer
- `libEA/tool.cpp` - Point instantiation examples
