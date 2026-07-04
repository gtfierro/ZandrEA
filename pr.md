# PR Summary

This branch adds a self-describing named write path for timestep uploads.

## Changes

- Adds `GET /profiles`, which returns reusable subject write profiles and a subject-to-profile map. `profiles` is keyed by profile id; each profile includes a stable model `label_id`, display label, and the ordered point names expected by libEA.
- Adds `PUT /ctrl/sampletimestep-named`, which accepts values keyed by point name, validates each subject against its libEA point contract, converts the named object into the existing ordered numeric vector, and advances the domain once all coincident inputs are accepted.
- Exposes stable subject profile ids from libEA by carrying the subject `EDataLabel` through `SGuiPackSubjectBasic`.
- Updates the CSV push script to discover subjects through `/profiles`, map CSV headers to point names client-side, and upload rows through `/ctrl/sampletimestep-named`.
- Documents the new profile metadata and named timestep endpoint in `EAd/REST-API-v3.md`.

## Example Messages

Fetch the self-describing write contract:

```http
GET /profiles
```

Example `/profiles` response shape:

```json
{
  "profiles": {
    "Subject_vav_pressIndep_hwReheat": {
      "label_id": "Subject_vav_pressIndep_hwReheat",
      "label": "VAV with HW Reheat",
      "points": [
        "Pressure_static_air_supply",
        "Temperature_air_supply",
        "Temperature_air_discharge",
        "Temperature_air_zone",
        "Temperature_air_zone_setpt_htg",
        "Temperature_air_zone_setpt_clg",
        "Position_valve_hw",
        "Position_damper_vav",
        "FlowRateVolume_air_vav",
        "FlowRateVolume_air_vav_setpt",
        "Binary_zoneOccupied"
      ]
    }
  },
  "subjects": [
    {
      "key": 12345,
      "name": "VAV-1",
      "idtext": "VAV-1",
      "profile": "Subject_vav_pressIndep_hwReheat",
      "label_id": "Subject_vav_pressIndep_hwReheat",
      "label": "VAV with HW Reheat",
      "points": [
        "Pressure_static_air_supply",
        "Temperature_air_supply",
        "Temperature_air_discharge",
        "Temperature_air_zone",
        "Temperature_air_zone_setpt_htg",
        "Temperature_air_zone_setpt_clg",
        "Position_valve_hw",
        "Position_damper_vav",
        "FlowRateVolume_air_vav",
        "FlowRateVolume_air_vav_setpt",
        "Binary_zoneOccupied"
      ]
    }
  ]
}
```

In the example above, the subject's `profile` value is the key into the top-level
`profiles` object.

Submit a named timestep:

```http
PUT /ctrl/sampletimestep-named
Content-Type: application/json
```

```json
{
  "time": 1718640000,
  "values_by_subject": [
    {
      "subject": 12345,
      "values": {
        "Pressure_static_air_supply": 1.2,
        "Temperature_air_supply": 55.0,
        "Temperature_air_discharge": 57.3,
        "Temperature_air_zone": 72.4,
        "Temperature_air_zone_setpt_htg": 68.0,
        "Temperature_air_zone_setpt_clg": 74.0,
        "Position_valve_hw": 0.0,
        "Position_damper_vav": 0.42,
        "FlowRateVolume_air_vav": 650.0,
        "FlowRateVolume_air_vav_setpt": 700.0,
        "Binary_zoneOccupied": 1
      }
    }
  ]
}
```
