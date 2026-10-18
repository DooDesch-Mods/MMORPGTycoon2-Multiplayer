"""Prove two native chat messages and recipient acknowledgements on the owned peers."""
from __future__ import annotations

import time
import uuid

from tools.local_acceptance import AcceptanceError, extract_result

PEERS = ("host", "client-1")
PEER_IDS = {"host": 1, "client-1": 2}


def read(runner, peer, step, remaining=None):
    if remaining is None:
        return runner.single(step, peer, "game_read_chat")
    receipt = runner.job(step, peer, [{"tool": "game_read_chat", "arguments": {}}], remaining=remaining)
    return extract_result(receipt, "game_read_chat")


def connected(snapshots):
    sessions = set()
    for peer in PEERS:
        value = snapshots[peer]
        if (value.get("connected") is not True or value.get("local_peer") != PEER_IDS[peer]
                or type(value.get("session")) is not int or value["session"] <= 0
                or not isinstance(value.get("entries"), list)):
            raise AcceptanceError(f"native chat peer/session not ready: {peer}")
        sessions.add(value["session"])
    if len(sessions) != 1:
        raise AcceptanceError(f"native chat sessions differ: {sorted(sessions)}")
    return sessions.pop()


def matching(snapshot, origin, sequence, text):
    return [entry for entry in snapshot["entries"] if entry.get("origin") == origin
            and entry.get("sequence") == sequence and entry.get("text") == text]


def delivery(snapshots, sender, recipient, sequence, text):
    connected(snapshots)
    origin = PEER_IDS[sender]
    sent = matching(snapshots[sender], origin, sequence, text)
    received = matching(snapshots[recipient], origin, sequence, text)
    if len(sent) > 1 or len(received) > 1:
        raise AcceptanceError(f"native chat duplicate display sender={sender} sequence={sequence} counts={len(sent)}/{len(received)}")
    if sent and sent[0].get("delivery") == "failed":
        raise AcceptanceError(f"native chat send failed sender={sender} sequence={sequence}")
    if received and received[0].get("delivery") != "received":
        raise AcceptanceError(f"native chat recipient state differs recipient={recipient} sequence={sequence}")
    if not sent or not received or sent[0].get("delivery") != "confirmed":
        return False
    expected_mask = 1 << (PEER_IDS[recipient] - 1)
    if (sent[0].get("expected_peer_mask") != expected_mask
            or sent[0].get("ack_peer_mask") != expected_mask):
        raise AcceptanceError(f"native chat recipient ACK differs sender={sender} sequence={sequence} "
                              f"actual={sent[0].get('ack_peer_mask')} required={expected_mask}")
    return True


def ordered_delivery(snapshots, messages):
    orders = {}
    for peer in PEERS:
        selected = []
        for message in messages:
            rows = matching(snapshots[peer], PEER_IDS[message['sender']],
                            message['sequence'], message['text'])
            if len(rows) != 1 or type(rows[0].get('host_order')) is not int or rows[0]['host_order'] <= 0:
                raise AcceptanceError(f'native chat Host order absent or duplicated: {peer}')
            selected.append(rows[0])
        by_identity = {(row['origin'], row['sequence'], row['text']): row['host_order']
                       for row in selected}
        if len(set(by_identity.values())) != len(messages):
            raise AcceptanceError(f'native chat Host order reused: {peer}')
        displayed = [row['host_order'] for row in snapshots[peer]['entries']
                     if (row.get('origin'), row.get('sequence'), row.get('text')) in by_identity]
        if displayed != sorted(displayed):
            raise AcceptanceError(f'native chat transcript is not Host ordered: {peer}')
        orders[peer] = by_identity
    if orders['host'] != orders['client-1']:
        raise AcceptanceError('native chat Host orders differ between peers')
    return sorted(orders['host'].values())


def send_once(runner, sender, recipient, session, wait_seconds, now=time.monotonic):
    text = f"MT2 E2E {sender} {uuid.uuid4()}"
    before = {peer: read(runner, peer, f"chat-before-{sender}-{peer}") for peer in PEERS}
    if connected(before) != session:
        raise AcceptanceError("native chat session changed before send")
    if any(any(entry.get("text") == text for entry in before[peer]["entries"]) for peer in PEERS):
        raise AcceptanceError("native chat test text already exists before send")
    sent = runner.single(f"chat-send-once-{sender}", sender, "game_send_chat", {"text": text})
    sequence = sent.get("sequence")
    if sent.get("state") != "queued" or type(sequence) is not int or sequence <= 0:
        raise AcceptanceError(f"native chat queue acknowledgement differs sender={sender}: {sent}")
    runner.checked("chat-queued-" + sender, {"origin": PEER_IDS[sender], "sequence": sequence, "text": text,
                                            "delivery": "queued-only"})
    deadline, poll = now() + wait_seconds, 0
    while True:
        remaining = deadline - now()
        if remaining <= 2:
            raise AcceptanceError(f"native chat delivery timed out sender={sender} sequence={sequence} "
                                  f"required<={wait_seconds}s; send already attempted, no replay")
        poll += 1
        snapshots = {peer: read(runner, peer, f"chat-{sender}-{poll}-{peer}", remaining=deadline - now())
                     for peer in PEERS}
        if connected(snapshots) != session:
            raise AcceptanceError("native chat session changed during delivery")
        if delivery(snapshots, sender, recipient, sequence, text):
            runner.checked("chat-delivered-" + sender, {"origin": PEER_IDS[sender], "sequence": sequence,
                                                       "text": text, "recipient": recipient,
                                                       "recipient_ack_mask": 1 << (PEER_IDS[recipient] - 1),
                                                       "recipient_display_count": 1})
            return {"sender": sender, "recipient": recipient, "sequence": sequence, "text": text}


def run(runner, wait_seconds=45):
    initial = {peer: read(runner, peer, "chat-initial-" + peer) for peer in PEERS}
    session = connected(initial)
    runner.checked("chat-two-peer-session", {"session": session, "host": 1, "client": 2})
    messages = [send_once(runner, "client-1", "host", session, wait_seconds),
                send_once(runner, "host", "client-1", session, wait_seconds)]
    final = {peer: read(runner, peer, "chat-final-" + peer) for peer in PEERS}
    if connected(final) != session:
        raise AcceptanceError("native chat session changed after both sends")
    for message in messages:
        if not delivery(final, **message):
            raise AcceptanceError(f"native chat delivery lost after both sends: {message['sender']} sequence={message['sequence']}")
    host_orders = ordered_delivery(final, messages)
    runner.checked("chat-two-directions-final", {"session": session, "messages": messages,
                                                 "host_orders": host_orders,
                                                 "native_user_input": False})
    return messages
