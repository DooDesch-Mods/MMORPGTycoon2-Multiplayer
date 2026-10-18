"""Offline two-peer chat orchestration tests using MCP-shaped readbacks."""
from __future__ import annotations

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.e2e import chat


class FakeRunner:
    def __init__(self, fault=None):
        self.fault = fault
        self.messages = []
        self.checks = []

    def snapshot(self, peer):
        local = chat.PEER_IDS[peer]
        entries = []
        for message in self.messages:
            recipient = 1 if message["origin"] == 2 else 2
            own = message["origin"] == local
            entry = {**message, "delivery": "confirmed" if own else "received",
                     "expected_peer_mask": 1 << (recipient - 1) if own else 0,
                     "ack_peer_mask": 1 << (recipient - 1) if own else 0}
            if self.fault == "ack" and own:
                entry["ack_peer_mask"] = 0
            if self.fault == "order_missing":
                entry.pop('host_order')
            if self.fault == "order_conflict" and peer == 'client-1':
                entry['host_order'] += 1
            if self.fault == "order_reused":
                entry['host_order'] = 1
            entries.append(entry)
            if self.fault == "duplicate" and not own:
                entries.append(dict(entry))
        if self.fault == 'order_display' and peer == 'client-1':
            entries.reverse()
        return {"connected": True, "session": 1234, "local_peer": local, "entries": entries}

    def single(self, step, peer, tool, arguments=None):
        if tool == "game_read_chat":
            return self.snapshot(peer)
        if tool == "game_send_chat":
            sequence = 1 + sum(message["origin"] == chat.PEER_IDS[peer] for message in self.messages)
            self.messages.append({"origin": chat.PEER_IDS[peer], "sequence": sequence,
                                  "text": arguments["text"], "host_order": len(self.messages) + 1})
            return {"state": "queued", "sequence": sequence, "chat": self.snapshot(peer)}
        raise AssertionError(f"unexpected MCP tool: {tool}")

    def job(self, step, peer, calls, remaining=None):
        self.checks.append(("poll", peer, remaining))
        return {"calls": [{"tool": "game_read_chat", "state": "confirmed",
                           "result": {"structuredContent": {"ok": True, "result": self.snapshot(peer)}}}]}

    def checked(self, name, value):
        self.checks.append((name, value))


class ChatTests(unittest.TestCase):
    def test_two_directions_ack_and_one_recipient_entry(self):
        runner = FakeRunner()
        messages = chat.run(runner, wait_seconds=10)
        self.assertEqual([(m["sender"], m["recipient"]) for m in messages],
                         [("client-1", "host"), ("host", "client-1")])
        self.assertEqual(len(runner.messages), 2)
        self.assertEqual(len(set(message["text"] for message in messages)), 2)
        final = {peer: runner.snapshot(peer) for peer in chat.PEERS}
        self.assertTrue(all(chat.delivery(final, **message) for message in messages))

    def test_duplicate_recipient_display_rejects(self):
        runner = FakeRunner(fault="duplicate")
        with self.assertRaisesRegex(chat.AcceptanceError, "duplicate display"):
            chat.run(runner, wait_seconds=10)
        self.assertEqual(len(runner.messages), 1)

    def test_missing_recipient_ack_rejects(self):
        runner = FakeRunner(fault="ack")
        with self.assertRaisesRegex(chat.AcceptanceError, "recipient ACK differs"):
            chat.run(runner, wait_seconds=10)
        self.assertEqual(len(runner.messages), 1)

    def test_host_order_missing_rejects(self):
        with self.assertRaisesRegex(chat.AcceptanceError, 'Host order absent'):
            chat.run(FakeRunner('order_missing'), wait_seconds=10)

    def test_host_order_disagreement_rejects(self):
        with self.assertRaisesRegex(chat.AcceptanceError, 'Host orders differ'):
            chat.run(FakeRunner('order_conflict'), wait_seconds=10)

    def test_host_order_reuse_rejects(self):
        with self.assertRaisesRegex(chat.AcceptanceError, 'Host order reused'):
            chat.run(FakeRunner('order_reused'), wait_seconds=10)

    def test_actual_transcript_order_rejects(self):
        with self.assertRaisesRegex(chat.AcceptanceError, 'transcript is not Host ordered'):
            chat.run(FakeRunner('order_display'), wait_seconds=10)


if __name__ == "__main__":
    unittest.main()
