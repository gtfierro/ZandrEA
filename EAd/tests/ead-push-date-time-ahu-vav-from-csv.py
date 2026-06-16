#!/usr/bin/env python3

import argparse
import csv
import json
import os
import sys
import time
from contextlib import nullcontext
from datetime import datetime

METADATA_COLUMNS = {"date", "time", "gtc"}
CSV_COLUMN_MAP = {
    "A1_Psas": {"subject": "AHU-1", "point": "Pressure_static_air_supply"},
    "A1_Tao": {"subject": "AHU-1", "point": "Temperature_air_outside"},
    "A1_Udm": {"subject": "AHU-1", "point": "Position_damper_mixingBox"},
    "A1_Tam": {"subject": "AHU-1", "point": "Temperature_air_mixed"},
    "A1_Tar": {"subject": "AHU-1", "point": "Temperature_air_return"},
    "A1_Uvc": {"subject": "AHU-1", "point": "Position_valve_chw"},
    "A1_Tas": {"subject": "AHU-1", "point": "Temperature_air_supply"},
    "A1_TasSetpt": {"subject": "AHU-1", "point": "Temperature_air_supply_setpt"},
    "A1_Bso": {"subject": "AHU-1", "point": "Binary_systemOccupied"},
    "A1_Uvh": {"subject": "AHU-1", "point": "Position_valve_hw"},
    "A1_Qas": {"subject": "AHU-1", "point": "FlowRateVolume_air_ahu"},
    "A2_Psas": {"subject": "AHU-2", "point": "Pressure_static_air_supply"},
    "A2_Tao": {"subject": "AHU-2", "point": "Temperature_air_outside"},
    "A2_Udm": {"subject": "AHU-2", "point": "Position_damper_mixingBox"},
    "A2_Tam": {"subject": "AHU-2", "point": "Temperature_air_mixed"},
    "A2_Tar": {"subject": "AHU-2", "point": "Temperature_air_return"},
    "A2_Uvc": {"subject": "AHU-2", "point": "Position_valve_chw"},
    "A2_Tas": {"subject": "AHU-2", "point": "Temperature_air_supply"},
    "A2_TasSetpt": {"subject": "AHU-2", "point": "Temperature_air_supply_setpt"},
    "A2_Bso": {"subject": "AHU-2", "point": "Binary_systemOccupied"},
    "A2_Uvh": {"subject": "AHU-2", "point": "Position_valve_hw"},
    "A2_Qas": {"subject": "AHU-2", "point": "FlowRateVolume_air_ahu"},
    "V1_Psai": {"subject": "VAV-1", "point": "Pressure_static_air_supply"},
    "V1_Tai": {"subject": "VAV-1", "point": "Temperature_air_supply"},
    "V1_Tad": {"subject": "VAV-1", "point": "Temperature_air_discharge"},
    "V1_Taz": {"subject": "VAV-1", "point": "Temperature_air_zone"},
    "V1_TazSetptHtg": {"subject": "VAV-1", "point": "Temperature_air_zone_setpt_htg"},
    "V1_TazSetptClg": {"subject": "VAV-1", "point": "Temperature_air_zone_setpt_clg"},
    "V1_Uvh": {"subject": "VAV-1", "point": "Position_valve_hw"},
    "V1_Udd": {"subject": "VAV-1", "point": "Position_damper_vav"},
    "V1_Qad": {"subject": "VAV-1", "point": "FlowRateVolume_air_vav"},
    "V1_QadSetpt": {"subject": "VAV-1", "point": "FlowRateVolume_air_vav_setpt"},
    "V1_Bzo": {"subject": "VAV-1", "point": "Binary_zoneOccupied"},
    "V2_Psai": {"subject": "VAV-2", "point": "Pressure_static_air_supply"},
    "V2_Tai": {"subject": "VAV-2", "point": "Temperature_air_supply"},
    "V2_Tad": {"subject": "VAV-2", "point": "Temperature_air_discharge"},
    "V2_Taz": {"subject": "VAV-2", "point": "Temperature_air_zone"},
    "V2_TazSetptHtg": {"subject": "VAV-2", "point": "Temperature_air_zone_setpt_htg"},
    "V2_TazSetptClg": {"subject": "VAV-2", "point": "Temperature_air_zone_setpt_clg"},
    "V2_Uvh": {"subject": "VAV-2", "point": "Position_valve_hw"},
    "V2_Udd": {"subject": "VAV-2", "point": "Position_damper_vav"},
    "V2_Qad": {"subject": "VAV-2", "point": "FlowRateVolume_air_vav"},
    "V2_QadSetpt": {"subject": "VAV-2", "point": "FlowRateVolume_air_vav_setpt"},
    "V2_Bzo": {"subject": "VAV-2", "point": "Binary_zoneOccupied"},
    "V3_Psai": {"subject": "VAV-3", "point": "Pressure_static_air_supply"},
    "V3_Tai": {"subject": "VAV-3", "point": "Temperature_air_supply"},
    "V3_Tad": {"subject": "VAV-3", "point": "Temperature_air_discharge"},
    "V3_Taz": {"subject": "VAV-3", "point": "Temperature_air_zone"},
    "V3_TazSetptHtg": {"subject": "VAV-3", "point": "Temperature_air_zone_setpt_htg"},
    "V3_TazSetptClg": {"subject": "VAV-3", "point": "Temperature_air_zone_setpt_clg"},
    "V3_Uvh": {"subject": "VAV-3", "point": "Position_valve_hw"},
    "V3_Udd": {"subject": "VAV-3", "point": "Position_damper_vav"},
    "V3_Qad": {"subject": "VAV-3", "point": "FlowRateVolume_air_vav"},
    "V3_QadSetpt": {"subject": "VAV-3", "point": "FlowRateVolume_air_vav_setpt"},
    "V3_Bzo": {"subject": "VAV-3", "point": "Binary_zoneOccupied"},
    "V4_Psai": {"subject": "VAV-4", "point": "Pressure_static_air_supply"},
    "V4_Tai": {"subject": "VAV-4", "point": "Temperature_air_supply"},
    "V4_Tad": {"subject": "VAV-4", "point": "Temperature_air_discharge"},
    "V4_Taz": {"subject": "VAV-4", "point": "Temperature_air_zone"},
    "V4_TazSetptHtg": {"subject": "VAV-4", "point": "Temperature_air_zone_setpt_htg"},
    "V4_TazSetptClg": {"subject": "VAV-4", "point": "Temperature_air_zone_setpt_clg"},
    "V4_Uvh": {"subject": "VAV-4", "point": "Position_valve_hw"},
    "V4_Udd": {"subject": "VAV-4", "point": "Position_damper_vav"},
    "V4_Qad": {"subject": "VAV-4", "point": "FlowRateVolume_air_vav"},
    "V4_QadSetpt": {"subject": "VAV-4", "point": "FlowRateVolume_air_vav_setpt"},
    "V4_Bzo": {"subject": "VAV-4", "point": "Binary_zoneOccupied"},
}


def parse_args():
    cli = argparse.ArgumentParser(
        description="Push IBAL AHU/VAV CSV samples to the EA REST server.",
        epilog="""
CSV columns are mapped by this driver to subject names and point names, then
validated against the backend /profiles document before upload.
""",
    )
    cli.add_argument(
        "filenames", help="Name of the CSV file(s) to load (- for stdin)", nargs="+"
    )
    cli.add_argument(
        "-a",
        "--ahu-count",
        help="Deprecated; kept for compatibility. Header names now drive AHU discovery.",
        type=int,
        default=None,
    )
    cli.add_argument("-l", "--loop", help="Repeat forever", action="store_true")
    cli.add_argument(
        "-i", "--interval", help="Time to pause between samples", type=int, default=0
    )
    cli.add_argument(
        "-t",
        "--time",
        help="Starting timestamp override; use 'now' for current time",
    )
    cli.add_argument(
        "-s",
        "--timestep",
        help="Seconds to increment timestamp between samples when --time is used",
        type=int,
        default=60,
    )
    cli.add_argument("-u", "--baseurl", help="Base URL to use for REST API")
    cli.add_argument(
        "-p",
        "--port",
        help="Port number to use for REST API URL (EA_PORT)",
        type=int,
        default=int(os.getenv("EA_PORT", "9876")),
    )
    cli.add_argument(
        "-H",
        "--host",
        help="Hostname to use for REST API URL (EA_HOST)",
        default=os.getenv("EA_HOST", "127.0.0.1"),
    )
    cli.add_argument(
        "-P",
        "--protocol",
        help="Protocol to use for REST API URL (EA_PROTO)",
        default=os.getenv("EA_PROTO", "http"),
    )
    cli.add_argument(
        "-A",
        "--api-path",
        help="Path prefix to use for REST API URL (EA_APIPATH)",
        default=os.getenv("EA_APIPATH", ""),
    )
    cli.add_argument(
        "--dry-run",
        action="store_true",
        help="Validate through /profiles and print payloads without uploading samples",
    )
    cli.add_argument(
        "--max-rows",
        type=int,
        help="Stop after this many CSV data rows per file",
    )
    return cli.parse_args()


def base_url(args):
    if args.baseurl:
        return args.baseurl.rstrip("/")
    apipath = args.api_path.strip().rstrip("/")
    default = f"{args.protocol}://{args.host}:{args.port}{apipath}"
    return os.getenv("EA_BASEURL", default).rstrip("/")


class RestClient:
    def __init__(self, baseurl):
        self.baseurl = baseurl
        self.headers = {"Content-Type": "application/json"}

    def request(self, method, path, **kwargs):
        import requests

        response = requests.request(
            method, f"{self.baseurl}{path}", headers=self.headers, **kwargs
        )
        if not 200 <= response.status_code <= 299:
            raise RuntimeError(
                f"{method} {path} failed with HTTP {response.status_code}: {response.text}"
            )
        return response

    def get_profiles(self):
        return self.request("GET", "/profiles").json()

    def sample_time_step(self, timestamp, values_by_subject):
        body = {"time": timestamp, "values_by_subject": values_by_subject}
        return self.request("PUT", "/ctrl/sampletimestep-named", json=body).json()

    def new_alerts_then_clear(self):
        return self.request("GET", "/alerts").json().get("alerts", [])

    def case_keys(self, subject):
        return (
            self.request("GET", "/casekeys", json={"subject": subject})
            .json()
            .get("casekeys", [])
        )

    def case(self, case_key, subject):
        return self.request(
            "GET", "/case", json={"key": case_key, "subject": subject}
        ).json()


def discover_subjects_from_profiles(profile_doc):
    subjects = {}
    profile_ids = {profile.get("id") for profile in profile_doc.get("profiles", [])}
    for subject in profile_doc.get("subjects", []):
        profile = subject.get("profile")
        if profile not in profile_ids:
            print(
                f"WARNING: subject {subject.get('key')} ({subject.get('name')}) "
                f"uses unsupported profile {profile}"
            )
            continue
        name = subject.get("name")
        if name:
            subjects[name] = subject
        print(
            f"Subject {subject.get('key')} ({subject.get('name')}) uses profile {profile}"
        )
    if not subjects:
        raise RuntimeError("/profiles did not provide any subjects")
    return subjects


def build_column_map(fieldnames, subjects_by_name):
    if not fieldnames:
        raise ValueError("CSV file has no header")

    column_map = {}
    seen_by_subject = {}
    expected_by_subject = {}
    for column in fieldnames:
        if column in METADATA_COLUMNS:
            continue
        adapter = CSV_COLUMN_MAP.get(column)
        if adapter is None:
            raise ValueError(
                f"CSV column {column!r} is not listed in the CSV adapter map"
            )
        subject_name = adapter["subject"]
        point = adapter["point"]
        subject = subjects_by_name.get(subject_name)
        if subject is None:
            raise ValueError(
                f"CSV column {column!r} maps to subject {subject_name!r}, "
                "but /profiles does not list that subject"
            )
        if point not in subject.get("points", []):
            raise ValueError(
                f"CSV column {column!r} maps to point {point!r}, but subject "
                f"{subject_name!r} does not list that point in /profiles"
            )
        subject_key = subject["key"]
        column_map[column] = {"subject": subject, "point": point}
        seen_by_subject.setdefault(subject_key, set()).add(column)
        expected_by_subject.setdefault(
            subject_key,
            {
                mapped_column
                for mapped_column, mapped in CSV_COLUMN_MAP.items()
                if mapped["subject"] == subject_name
                and mapped["point"] in subject.get("points", [])
            },
        )

    missing = {}
    for subject_key, expected_columns in expected_by_subject.items():
        missing_columns = sorted(
            expected_columns - seen_by_subject.get(subject_key, set())
        )
        if missing_columns:
            missing[subject_key] = missing_columns
    if missing:
        details = "; ".join(
            f"subject {subject}: {', '.join(columns)}"
            for subject, columns in sorted(missing.items())
        )
        raise ValueError(
            f"CSV header missing required columns listed by the CSV adapter map: {details}"
        )

    return column_map


def row_timestamp(row, override_timestamp):
    if override_timestamp is not None:
        return override_timestamp
    return parse_timestamp(f"{row['date']} {row['time']}")


def parse_timestamp(value):
    value = value.strip()
    for fmt in (
        "%Y/%m/%d %H:%M",
        "%Y/%m/%d %H:%M:%S",
        "%Y-%m-%d %H:%M",
        "%Y-%m-%d %H:%M:%S",
        "%Y-%m-%dT%H:%M",
        "%Y-%m-%dT%H:%M:%S",
    ):
        try:
            return int(time.mktime(datetime.strptime(value, fmt).timetuple()))
        except ValueError:
            pass
    try:
        from dateutil.parser import parse
    except ImportError as err:
        raise ValueError(
            f"could not parse timestamp {value!r}; install python-dateutil for "
            "non-standard timestamp formats"
        ) from err
    return int(time.mktime(parse(value).timetuple()))


def row_values_by_subject(row, column_map):
    values_by_subject_key = {}
    for column, mapping in column_map.items():
        raw = row.get(column)
        if raw is None or raw.strip() == "":
            raise ValueError(f"missing value in column {column}")
        subject = mapping["subject"]
        subject_key = subject["key"]
        values_by_subject_key.setdefault(
            subject_key, {"subject": subject, "values": {}}
        )
        values_by_subject_key[subject_key]["values"][mapping["point"]] = float(raw)

    values_by_subject = []
    for subject_key in sorted(values_by_subject_key):
        subject_values = values_by_subject_key[subject_key]
        values_by_subject.append(
            {"subject": subject_key, "values": subject_values["values"]}
        )
    return values_by_subject


def open_csv(filename):
    if filename == "-":
        return nullcontext(sys.stdin)
    if not os.access(filename, os.R_OK):
        raise FileNotFoundError(f"datafile {filename!r} is not readable")
    return open(filename, newline="")


def read_csv_rows(filename):
    with open_csv(filename) as csvfile:
        reader = csv.DictReader(csvfile)
        return reader.fieldnames, list(reader)


def run_rows(filename, rows, column_map, args, client, override_timestamp):
    rows_processed = 0
    for linenum, row in enumerate(rows, start=2):
        if args.max_rows is not None and rows_processed >= args.max_rows:
            break

        ts = row_timestamp(row, override_timestamp)
        try:
            values_by_subject = row_values_by_subject(row, column_map)
        except ValueError as err:
            raise ValueError(f"{filename}:{linenum}: {err}") from err

        if args.dry_run:
            print(json.dumps({"time": ts, "values_by_subject": values_by_subject}))
        else:
            client.sample_time_step(ts, values_by_subject)

        rows_processed += 1
        print(f"Single step {rows_processed} taken for timestamp {ts}")

        if override_timestamp is not None:
            override_timestamp += args.timestep

        if args.interval > 0:
            time.sleep(args.interval)

        if not args.dry_run:
            for alert in client.new_alerts_then_clear():
                print(alert.get("message", alert))

    return override_timestamp


def report_cases(client, subjectkeys):
    for subject in subjectkeys:
        for case_key in client.case_keys(subject):
            print("")
            case = client.case(case_key, subject)
            if case.get("error") is None:
                print(
                    "WARNING: case {} from subject {}: {}: {}".format(
                        case_key, subject, case.get("label"), case.get("report")
                    )
                )
            else:
                print(f"ERROR: case {case_key} from subject {subject}: SayCase failed")


def main():
    args = parse_args()
    client = RestClient(base_url(args))

    profile_doc = client.get_profiles()
    subjectkeys = [
        subject["key"]
        for subject in profile_doc.get("subjects", [])
        if "key" in subject
    ]
    subjects_by_name = discover_subjects_from_profiles(profile_doc)

    override_timestamp = None
    if args.time == "now":
        override_timestamp = int(time.time())
    elif args.time is not None:
        override_timestamp = parse_timestamp(args.time)

    while True:
        for filename in args.filenames:
            fieldnames, rows = read_csv_rows(filename)
            column_map = build_column_map(fieldnames, subjects_by_name)

            override_timestamp = run_rows(
                filename,
                rows,
                column_map,
                args,
                client,
                override_timestamp,
            )

        if not args.loop:
            break

    if not args.dry_run:
        report_cases(client, subjectkeys)


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"ERROR: {err}", file=sys.stderr)
        sys.exit(1)
