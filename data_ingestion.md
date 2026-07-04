# ZandrEA Data Ingestion Flow

This document describes how data travels from the Python test script [ead-push-date-time-ahu-vav-from-csv.py](file:///Users/gabe/src/ZandrEA/EAd/tests/ead-push-date-time-ahu-vav-from-csv.py) into the ZandrEA backend and analysis engine.

## Phase 1: Python Data Ingestion

**File**: [ead-push-date-time-ahu-vav-from-csv.py](file:///Users/gabe/src/ZandrEA/EAd/tests/ead-push-date-time-ahu-vav-from-csv.py)

1.  **CSV Parsing**: The script reads rows from a CSV file. Each row contains a date, time, and multiple columns of sensor data for AHUs and VAV boxes.
2.  **Data Structuring**: It groups the data into a list called `values_by_subject`, which contains objects with `subject` (the key) and `values` (the sample vector).
3.  **API Call**: It calls `SampleTimeStep(ts, values_by_subject)`. 
    - **Method**: `SampleTimeStep`
    - **Endpoint**: `PUT /ctrl/sampletimestep`

## Phase 2: REST API Handling

**File**: [handler.cpp](file:///Users/gabe/src/ZandrEA/EAd/handler.cpp)

1.  **Routing**: The `handler` catches the `/ctrl/sampletimestep` path.
2.  **Atomic Preparation**: It performs three operations in sequence:
    - **Timestamp**: `p_Port->SetTimeStampInDomain(tm)` - Updates the internal simulation clock.
    - **Data Injection**: It loops through the subjects in the request and calls `p_Port->SetCoincidentInputsForSubject(dlist, subject)`.
    - **Execution**: `p_Port->SingleStepDomainOnTimeAndInputs()` - Triggers the simulation engine.

## Phase 3: Internal Data Storage (libEA)

**Files**: [portOmni.cpp](file:///Users/gabe/src/ZandrEA/libEA/portOmni.cpp), [mvc_ctrlr.cpp](file:///Users/gabe/src/ZandrEA/libEA/mvc_ctrlr.cpp), [dataChannel.cpp](file:///Users/gabe/src/ZandrEA/libEA/dataChannel.cpp)

1.  **Routing**: `CPortOmni::SetCoincidentInputsForSubject` wraps the controller.
2.  **Injection**: `CController::ReadInDataForSubject` (in `mvc_ctrlr.cpp`) iterates through the `ADataChannel` objects registered to the subject.
3.  **Tracking**: For each data channel (a sensor input), it calls `ReadFromPortAsNextValue(arg)` (in `dataChannel.cpp`).
    - **Transformation**: `xPrevDbl = xGivenDbl; xGivenDbl = arg;` - This shifts the current value to "previous" and stores the new sample as "current".

## Phase 4: Simulation Execution (The "Single Step")

**Files**: [sequence.cpp](file:///Users/gabe/src/ZandrEA/libEA/sequence.cpp), [mvc_ctrlr.cpp](file:///Users/gabe/src/ZandrEA/libEA/mvc_ctrlr.cpp)

1.  **Triggering**: `SingleStepDomainOnTimeAndInputs` calls `CController::SingleStepModelOnTimeAndInputs` (in `mvc_ctrlr.cpp`).
2.  **Sequencing**: This triggers `CSequence::Trigger` (in `sequence.cpp`), which is the heart of the engine.
3.  **Processing Order**: The data is processed in a strict hierarchy to ensure dependencies are met:
    - **Points** (Ingested data is finalized)
    - **Formulas** (Derived values are calculated)
    - **Charts** (UI data is updated)
    - **Processes** (High-level analysis)
    - **Facts** (Boolean conditions/thresholds are evaluated)
    - **RuleKits** (Final fault detection rules are triggered)

This sequential execution ensures that data transformed in one stage (e.g., a Formula) is available for the next (e.g., a Fact) within the same time step.
