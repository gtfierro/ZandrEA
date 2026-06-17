"""Tests for the GET /profiles endpoint.

/profiles returns the self-describing write contract from the libEA model:
  - profiles: keyed by stable profile id (label_id or label_id + hash suffix)
  - subjects: list mapping each subject key to its profile and expected points
"""

import pytest
import requests


class TestProfilesTopLevel:
    """Structural checks on the /profiles response shape."""

    def test_returns_200(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        resp.raise_for_status()
        assert resp.status_code == 200

    def test_includes_apiver(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        assert "apiver" in data
        assert isinstance(data["apiver"], int)
        assert data["apiver"] > 0

    def test_has_profiles_and_subjects_keys(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        assert "profiles" in data
        assert "subjects" in data
        assert isinstance(data["profiles"], dict)
        assert isinstance(data["subjects"], list)

    def test_profiles_is_dict_keyed_by_string(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for profile_id, profile in data["profiles"].items():
            assert isinstance(profile_id, str)
            assert isinstance(profile, dict)

    def test_subjects_is_list_of_dicts(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for subject in data["subjects"]:
            assert isinstance(subject, dict)


class TestProfileFields:
    """Each profile entry must contain label_id, label, and points."""

    def test_profile_has_label_id(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for profile_id, profile in data["profiles"].items():
            assert "label_id" in profile, (
                f"profile {profile_id!r} missing label_id"
            )
            assert isinstance(profile["label_id"], str)

    def test_profile_has_label(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for profile_id, profile in data["profiles"].items():
            assert "label" in profile, (
                f"profile {profile_id!r} missing label"
            )
            assert isinstance(profile["label"], str)

    def test_profile_has_points(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for profile_id, profile in data["profiles"].items():
            assert "points" in profile, (
                f"profile {profile_id!r} missing points"
            )
            assert isinstance(profile["points"], list)
            # Each point name should be a non-empty string
            for pt in profile["points"]:
                assert isinstance(pt, str)
                assert len(pt) > 0


class TestSubjectFields:
    """Each subject entry must map to a profile and list its points."""

    def test_subject_has_key(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for subject in data["subjects"]:
            assert "key" in subject
            assert isinstance(subject["key"], int)

    def test_subject_has_profile_ref(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for subject in data["subjects"]:
            assert "profile" in subject
            assert isinstance(subject["profile"], str)

    def test_subject_has_name(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for subject in data["subjects"]:
            assert "name" in subject
            assert isinstance(subject["name"], str)

    def test_subject_has_idtext(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for subject in data["subjects"]:
            assert "idtext" in subject
            assert isinstance(subject["idtext"], str)

    def test_subject_has_label(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for subject in data["subjects"]:
            assert "label" in subject
            assert isinstance(subject["label"], str)

    def test_subject_has_label_id(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for subject in data["subjects"]:
            assert "label_id" in subject
            assert isinstance(subject["label_id"], str)

    def test_subject_has_points(self, base_url):
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for subject in data["subjects"]:
            assert "points" in subject
            assert isinstance(subject["points"], list)

    def test_subject_profile_refs_existing_profile(self, base_url):
        """Every subject's profile field must match a key in profiles."""
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        profile_ids = set(data["profiles"].keys())
        for subject in data["subjects"]:
            assert subject["profile"] in profile_ids, (
                f"subject key {subject['key']} references unknown profile "
                f"{subject['profile']!r}"
            )

    def test_subject_points_match_profile_points(self, base_url):
        """A subject's points list must match its profile's points list."""
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        for subject in data["subjects"]:
            profile = data["profiles"][subject["profile"]]
            assert subject["points"] == profile["points"], (
                f"subject key {subject['key']} points differ from profile "
                f"{subject['profile']!r}"
            )


class TestProfilesConsistency:
    """Cross-entity consistency checks."""

    def test_profile_count_positive(self, base_url):
        """At least one profile should exist when subjects are configured."""
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        if len(data["subjects"]) > 0:
            assert len(data["profiles"]) > 0

    def test_subject_keys_unique(self, base_url):
        """No duplicate subject keys in the subjects list."""
        resp = requests.get(f"{base_url}/profiles", timeout=10)
        data = resp.json()
        keys = [s["key"] for s in data["subjects"]]
        assert len(keys) == len(set(keys))

    def test_profiles_matches_subjectkeys_endpoint(self, base_url):
        """Subject keys from /profiles should match /subjectkeys."""
        profiles_resp = requests.get(f"{base_url}/profiles", timeout=10)
        keys_resp = requests.get(f"{base_url}/subjectkeys", timeout=10)
        profiles_data = profiles_resp.json()
        keys_data = keys_resp.json()
        profile_keys = sorted(s["key"] for s in profiles_data["subjects"])
        subject_keys = sorted(keys_data.get("subjectkeys", []))
        assert profile_keys == subject_keys
