"""Tests for the PUT /ctrl/sampletimestep-named endpoint.

Maps 1:1 to the validation checks in handler.cpp so every
``goto done`` branch is exercised.

Expected request body:
{
    "time": <unix-timestamp>,
    "values_by_subject": [
        {
            "subject": <subject-key>,
            "values": {"PointName1": 1.0, "PointName2": 2.0, ...}
        }
    ]
}
"""

import time

import pytest
import requests


def _sample_timestamp():
    """Return a reasonable Unix timestamp for test data."""
    return int(time.time())


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def profiles_data(base_url):
    """Return the full /profiles response body."""
    resp = requests.get(f"{base_url}/profiles", timeout=10)
    resp.raise_for_status()
    return resp.json()


@pytest.fixture
def first_subject(profiles_data):
    """Return the first subject entry from /profiles, or skip."""
    if not profiles_data["subjects"]:
        pytest.skip("no subjects configured")
    return profiles_data["subjects"][0]


@pytest.fixture
def valid_sample_body(first_subject):
    """Build a well-formed sample body for *first_subject*."""
    values = {pt: 1.0 for pt in first_subject["points"]}
    return {
        "time": _sample_timestamp(),
        "values_by_subject": [
            {"subject": first_subject["key"], "values": values},
        ],
    }


# ---------------------------------------------------------------------------
# Helper
# ---------------------------------------------------------------------------

def _put_named(base_url, body):
    """Shortcut for PUT /ctrl/sampletimestep-named."""
    return requests.put(
        f"{base_url}/ctrl/sampletimestep-named",
        json=body,
        timeout=10,
    )


# ===================================================================
# Validation checks — each handler.cpp ``goto done`` branch
# ===================================================================

class TestValidationTopLevel:
    """
    handler.cpp: get_json_value(..., "values_by_subject", ...) &&
                 get_json_value(..., "time", ...)
    Error: "time and/or values_by_subject parameters not found"
    """

    def test_missing_time_returns_400(self, base_url):
        body = {"values_by_subject": []}
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "time" in resp.json()["error"].lower()

    def test_missing_values_by_subject_returns_400(self, base_url):
        body = {"time": _sample_timestamp()}
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "values_by_subject" in resp.json()["error"].lower()

    def test_missing_both_returns_400(self, base_url):
        body = {}
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        data = resp.json()
        assert "time" in data["error"].lower() or "values_by_subject" in data["error"].lower()

    def test_both_present_but_null_values(self, base_url):
        """time is null (not an int) — get_json_value can't extract a time_t."""
        body = {"time": None, "values_by_subject": None}
        resp = _put_named(base_url, body)
        assert resp.status_code == 400


class TestValidationValuesBySubjectIsArray:
    """
    handler.cpp: valuesbysubject.is_array()
    Error: "values_by_subject parameter must be array of objects..."
    """

    def test_values_by_subject_is_object_returns_400(self, base_url):
        # Looks like a valid entry, but the outer container must be an array
        # so the handler can iterate entries via as_array().
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": {"subject": 1, "values": {}},
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "array" in resp.json()["error"].lower()

    def test_values_by_subject_is_string_returns_400(self, base_url):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": "not-an-array",
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400

    def test_values_by_subject_is_number_returns_400(self, base_url):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": 42,
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400


class TestValidationEntryIsObject:
    """
    handler.cpp: !json_o.is_object()
    Error: "values_by_subject entries must be objects..."
    """

    def test_entry_is_string_returns_400(self, base_url):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": ["not-an-object"],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "object" in resp.json()["error"].lower()

    def test_entry_is_number_returns_400(self, base_url):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [42],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400

    def test_entry_is_null_returns_400(self, base_url):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [None],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400

    def test_entry_is_array_returns_400(self, base_url):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [[1, 2, 3]],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400


class TestValidationSubjectKeyPresent:
    """
    handler.cpp: json_o.at(U("subject")).as_number().to_uint64()
    Error: "subject parameter missing or invalid..."
    """

    def test_missing_subject_key_returns_400(self, base_url):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [{"values": {"Temperature": 1.0}}],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "subject" in resp.json()["error"].lower()

    def test_subject_key_is_string_returns_400(self, base_url):
        # as_number().to_uint64() throws on a JSON string — caught by the
        # handler's catch-all and returns "subject parameter missing or invalid".
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [{"subject": "not-a-number", "values": {}}],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400

    def test_subject_key_is_null_returns_400(self, base_url):
        # JSON null has no as_number() — caught the same way as a missing key.
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [{"subject": None, "values": {}}],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400


class TestValidationSubjectKeyValid:
    """
    handler.cpp: SayInputPointNameOrderExpectedBySubject(subject)
    Error: "invalid subject key <key>"
    """

    def test_nonexistent_subject_key_returns_400(self, base_url):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [{"subject": 999999999, "values": {}}],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "invalid subject key" in resp.json()["error"]

    def test_zero_subject_key_returns_400(self, base_url):
        # NGuiKey(0) is not a valid subject — SayInputPointNameOrderExpectedBySubject
        # throws, hitting the "invalid subject key" branch.
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [{"subject": 0, "values": {}}],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400


class TestValidationValuesPresent:
    """
    handler.cpp: json_o.at(U("values"))
    Error: "values parameter missing for subject <key>"
    """

    def test_missing_values_returns_400(self, base_url, first_subject):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [{"subject": first_subject["key"]}],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "values" in resp.json()["error"].lower()
        assert "missing" in resp.json()["error"].lower()


class TestValidationValuesIsObject:
    """
    handler.cpp: !values.is_object()
    Error: "values parameter for subject <key> must be an object keyed by point name..."
    """

    def test_values_is_array_returns_400(self, base_url, first_subject):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [{"subject": first_subject["key"], "values": [1.0, 2.0]}],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "object" in resp.json()["error"].lower()

    def test_values_is_string_returns_400(self, base_url, first_subject):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [{"subject": first_subject["key"], "values": "bad"}],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400

    def test_values_is_null_returns_400(self, base_url, first_subject):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [{"subject": first_subject["key"], "values": None}],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400

    def test_values_is_number_returns_400(self, base_url, first_subject):
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [{"subject": first_subject["key"], "values": 42}],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400


class TestValidationPointNamePresent:
    """
    handler.cpp: value_iter == value_object.end()
    Error: "missing point name <name> for subject <key>"
    """

    def test_missing_one_point_returns_400(self, base_url, first_subject):
        points = first_subject["points"]
        if not points:
            pytest.skip("subject has no registered points")
        # Send all but the last point
        values = {pt: 1.0 for pt in points[:-1]}
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": values},
            ],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "missing point name" in resp.json()["error"]

    def test_empty_values_object_returns_400(self, base_url, first_subject):
        if not first_subject["points"]:
            pytest.skip("subject has no registered points")
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": {}},
            ],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "missing point name" in resp.json()["error"]

    def test_missing_all_points_returns_400(self, base_url, first_subject):
        """Same as empty_values_object — all expected names absent."""
        if not first_subject["points"]:
            pytest.skip("subject has no registered points")
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": {"nonexistent": 1.0}},
            ],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "missing point name" in resp.json()["error"]


class TestValidationPointValueNumeric:
    """
    handler.cpp: value_iter->second.as_double()  (catch ...)
    Error: "point <name> for subject <key> must be numeric"
    """

    def test_string_value_returns_400(self, base_url, first_subject):
        values = {pt: 1.0 for pt in first_subject["points"]}
        if values:
            first_key = next(iter(values))
            values[first_key] = "not-a-number"
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": values},
            ],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "must be numeric" in resp.json()["error"]

    def test_null_value_returns_400(self, base_url, first_subject):
        values = {pt: 1.0 for pt in first_subject["points"]}
        if values:
            first_key = next(iter(values))
            values[first_key] = None
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": values},
            ],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "must be numeric" in resp.json()["error"]

    def test_boolean_value_returns_400(self, base_url, first_subject):
        """JSON booleans are not doubles — as_double() throws on a bool type."""
        values = {pt: 1.0 for pt in first_subject["points"]}
        if values:
            first_key = next(iter(values))
            values[first_key] = True
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": values},
            ],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "must be numeric" in resp.json()["error"]

    def test_nested_object_value_returns_400(self, base_url, first_subject):
        # as_double() on a JSON object throws — caught as "must be numeric".
        values = {pt: 1.0 for pt in first_subject["points"]}
        if values:
            first_key = next(iter(values))
            values[first_key] = {"nested": "object"}
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": values},
            ],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "must be numeric" in resp.json()["error"]

    def test_nested_array_value_returns_400(self, base_url, first_subject):
        # as_double() on a JSON array throws — caught as "must be numeric".
        values = {pt: 1.0 for pt in first_subject["points"]}
        if values:
            first_key = next(iter(values))
            values[first_key] = [1, 2, 3]
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": values},
            ],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400
        assert "must be numeric" in resp.json()["error"]

    def test_integer_value_accepted(self, base_url, first_subject):
        """Integers are valid — as_double() accepts them."""
        values = {pt: 1 for pt in first_subject["points"]}
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": values},
            ],
        }
        resp = _put_named(base_url, body)
        resp.raise_for_status()

    def test_negative_float_accepted(self, base_url, first_subject):
        values = {pt: -42.5 for pt in first_subject["points"]}
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": values},
            ],
        }
        resp = _put_named(base_url, body)
        resp.raise_for_status()

    def test_zero_value_accepted(self, base_url, first_subject):
        values = {pt: 0.0 for pt in first_subject["points"]}
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": values},
            ],
        }
        resp = _put_named(base_url, body)
        resp.raise_for_status()


# ===================================================================
# Error response shape
# ===================================================================

class TestErrorResponseShape:
    """Every 400 response must include error and returncode fields."""

    def test_error_field_is_string(self, base_url):
        resp = _put_named(base_url, {"time": _sample_timestamp()})
        data = resp.json()
        assert "error" in data
        assert isinstance(data["error"], str)
        assert len(data["error"]) > 0

    def test_returncode_field_present(self, base_url):
        resp = _put_named(base_url, {"time": _sample_timestamp()})
        data = resp.json()
        assert "returncode" in data

    def test_apiver_always_present_on_error(self, base_url):
        resp = _put_named(base_url, {"time": _sample_timestamp()})
        data = resp.json()
        assert "apiver" in data
        assert isinstance(data["apiver"], int)


# ===================================================================
# Success path
# ===================================================================

class TestSuccessPath:
    """Happy-path tests — require a configured subject with known points."""

    def test_valid_named_sample_returns_200(self, base_url, valid_sample_body):
        resp = _put_named(base_url, valid_sample_body)
        assert resp.status_code == 200

    def test_valid_sample_includes_apiver(self, base_url, valid_sample_body):
        resp = _put_named(base_url, valid_sample_body)
        assert "apiver" in resp.json()

    def test_valid_sample_includes_returncode(self, base_url, valid_sample_body):
        resp = _put_named(base_url, valid_sample_body)
        assert "returncode" in resp.json()

    def test_valid_sample_includes_status(self, base_url, valid_sample_body):
        resp = _put_named(base_url, valid_sample_body)
        data = resp.json()
        assert "status" in data
        assert isinstance(data["status"], str)
        assert "named sample" in data["status"]

    def test_extra_point_names_ignored(self, base_url, first_subject):
        """Extra keys in values that aren't in the profile are harmless."""
        values = {pt: 1.0 for pt in first_subject["points"]}
        values["___nonexistent_point___"] = 999.0
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": values},
            ],
        }
        resp = _put_named(base_url, body)
        resp.raise_for_status()

    def test_seq_increments_after_successful_sample(self, base_url, valid_sample_body):
        resp_before = requests.get(f"{base_url}/noop", timeout=10)
        seq_before = resp_before.json().get("seq", 0)

        resp = _put_named(base_url, valid_sample_body)
        seq_after = resp.json().get("seq", 0)
        assert seq_after > seq_before


# ===================================================================
# Multi-subject
# ===================================================================

class TestMultipleSubjects:
    """Send data for multiple subjects in a single request."""

    def test_two_subjects_in_one_request(self, base_url, profiles_data):
        subjects = profiles_data["subjects"]
        if len(subjects) < 2:
            pytest.skip("need at least 2 subjects")
        values_by_subject = []
        for subj in subjects[:2]:
            values = {pt: 1.0 for pt in subj["points"]}
            values_by_subject.append({"subject": subj["key"], "values": values})
        body = {"time": _sample_timestamp(), "values_by_subject": values_by_subject}
        resp = _put_named(base_url, body)
        resp.raise_for_status()

    def test_one_bad_subject_fails_whole_request(self, base_url, first_subject):
        """Handler uses goto done — first error aborts the entire request."""
        values = {pt: 1.0 for pt in first_subject["points"]}
        body = {
            "time": _sample_timestamp(),
            "values_by_subject": [
                {"subject": first_subject["key"], "values": values},  # valid
                {"subject": 999999999, "values": {}},                   # invalid
            ],
        }
        resp = _put_named(base_url, body)
        assert resp.status_code == 400

    def test_all_subjects_in_one_request(self, base_url, profiles_data):
        subjects = profiles_data["subjects"]
        if not subjects:
            pytest.skip("no subjects configured")
        values_by_subject = []
        for subj in subjects:
            values = {pt: 1.0 for pt in subj["points"]}
            values_by_subject.append({"subject": subj["key"], "values": values})
        body = {"time": _sample_timestamp(), "values_by_subject": values_by_subject}
        resp = _put_named(base_url, body)
        resp.raise_for_status()
