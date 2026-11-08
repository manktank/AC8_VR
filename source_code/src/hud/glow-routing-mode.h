#pragma once
#include <atomic>
#include <cstdint>
// Shared by the feature's native modules. The renderer publishes this only
// after its owner/geometry contracts pass. This is NOT proof that a radar mask
// reached an actual submitted image. The accepted radar discovery stays active.
namespace glow_routing_mode {
inline std::atomic<bool> prepaint_ready{false};
inline std::atomic<uint64_t> portrait_requests{0},portrait_nodes{0},radar_walks_bypassed{0};
}
