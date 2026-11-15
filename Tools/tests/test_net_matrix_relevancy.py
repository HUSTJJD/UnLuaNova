"""Regression tests for the owner-only relevance gate.

The gate used to hard-code "actor 2 belongs to Client2". Ownership is decided
by connection order at runtime, so the same correct behaviour failed whenever
the two clients connected the other way round. Both orders must pass; a real
relevance violation must still fail.

Run with: python -B -m unittest discover -s Tools/tests -p "test_*.py"
"""

import importlib.util
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "validate_release_relevancy", TOOLS / "ValidateRelease.py")
vr = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(vr)


def peer(role, owned, visible):
    return {"Role": role, "OwnedActorId": owned, "VisibleActorIds": visible}


class RelevancyGateTests(unittest.TestCase):
    def server(self, restricted=2):
        return {"RelevancyRestrictedActor": restricted, "Role": "Server"}

    def reports(self, c1_owned, c1_visible, c2_owned, c2_visible):
        return {
            "Client1": peer("Client1", c1_owned, c1_visible),
            "Client2": peer("Client2", c2_owned, c2_visible),
        }

    def test_client2_owns_restricted_actor_passes(self):
        """The order the previous passing Release happened to produce."""
        errors = vr.relevancy_errors(self.server(), self.reports(1, [1], 2, [1, 2]))
        self.assertEqual(errors, [])

    def test_client1_owns_restricted_actor_passes(self):
        """The opposite connection order, which used to fail the whole Release."""
        errors = vr.relevancy_errors(self.server(), self.reports(2, [1, 2], 1, [1]))
        self.assertEqual(errors, [])

    def test_restricted_actor_id_is_not_hardcoded(self):
        errors = vr.relevancy_errors(self.server(3), self.reports(3, [1, 3], 1, [1]))
        self.assertEqual(errors, [])

    def test_non_owner_seeing_restricted_actor_fails(self):
        errors = vr.relevancy_errors(self.server(), self.reports(1, [1, 2], 2, [1, 2]))
        self.assertTrue(any("non-owner" in e for e in errors), errors)

    def test_owner_not_seeing_restricted_actor_fails(self):
        errors = vr.relevancy_errors(self.server(), self.reports(1, [1], 2, [1]))
        self.assertTrue(any("owner Client2 cannot see" in e for e in errors), errors)

    def test_missing_restricted_declaration_fails(self):
        for value in (0, None, "2", []):
            errors = vr.relevancy_errors(
                {"RelevancyRestrictedActor": value}, self.reports(1, [1], 2, [1, 2]))
            self.assertTrue(any("no owner-only actor" in e for e in errors), (value, errors))

    def test_no_owner_fails(self):
        errors = vr.relevancy_errors(self.server(), self.reports(1, [1], 1, [1]))
        self.assertTrue(any("not owned by exactly one client" in e for e in errors), errors)

    def test_two_owners_fails(self):
        errors = vr.relevancy_errors(self.server(), self.reports(2, [1, 2], 2, [1, 2]))
        self.assertTrue(any("not owned by exactly one client" in e for e in errors), errors)


if __name__ == "__main__":
    unittest.main()
