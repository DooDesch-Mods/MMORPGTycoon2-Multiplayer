#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::live_host_state {
// Sidecar's existing queue is 8 MiB. Three peer copies of the maximum hex
// stream, including every envelope, fit with more than 1 MiB left for traffic.
constexpr size_t max_frame_payload=action_session::max_transport_payload;
constexpr size_t max_logical_frame_payload=1024*1024;
constexpr size_t frame_chunk_bytes=24*1024;
constexpr size_t frame_envelope_reserve=1024;
constexpr size_t max_frame_chunks=(max_logical_frame_payload+frame_chunk_bytes-1)/frame_chunk_bytes;
static_assert(3*(2*max_logical_frame_payload+max_frame_chunks*frame_envelope_reserve)<7*1024*1024);
static_assert(2*frame_chunk_bytes+frame_envelope_reserve<=action_session::max_transport_payload);
}
