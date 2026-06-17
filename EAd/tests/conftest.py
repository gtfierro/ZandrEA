"""Pytest configuration and shared fixtures for EAd REST API tests."""

import os

import pytest
import requests


def _base_url():
    """Return the REST API base URL, matching existing script conventions."""
    if "EA_BASEURL" in os.environ:
        return os.environ["EA_BASEURL"]
    proto = os.getenv("EA_PROTO", "http")
    host = os.getenv("EA_HOST", "127.0.0.1")
    port = os.getenv("EA_PORT", "9876")
    return f"{proto}://{host}:{port}"


@pytest.fixture(scope="session")
def base_url():
    """Session-scoped base URL for the REST API."""
    return _base_url()


@pytest.fixture(scope="session")
def ead_ready(base_url):
    """Ensure the ead daemon is reachable before running tests.

    Raises RuntimeError if the server does not respond within a few seconds.
    """
    try:
        resp = requests.get(f"{base_url}/noop", timeout=5)
        resp.raise_for_status()
    except requests.RequestException as exc:
        raise RuntimeError(
            f"ead daemon not reachable at {base_url}/noop — "
            f"start it first or set EA_BASEURL: {exc}"
        ) from exc
    return True


@pytest.fixture
def subject_keys(base_url, ead_ready):
    """Return the list of configured subject keys from the domain."""
    resp = requests.get(f"{base_url}/subjectkeys", timeout=10)
    resp.raise_for_status()
    data = resp.json()
    return data.get("subjectkeys", [])
