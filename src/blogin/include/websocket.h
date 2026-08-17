#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace blogin::websocket {

// RFC 6455 frame kinds, by the numbers the protocol gives them.
enum class Opcode : std::uint8_t {
  continuation = 0x0,
  text = 0x1,
  binary = 0x2,
  close = 0x8,
  ping = 0x9,
  pong = 0xa,
};

std::string sha1(std::string_view data);

std::string base64(std::string_view data);

// The value of Sec-WebSocket-Accept for a client's Sec-WebSocket-Key: the key,
// the protocol's fixed string, SHA-1, then base64. The magic string is the
// protocol's, not a choice.
std::string accept_key(std::string_view client_key);

// True when the request headers ask to become a WebSocket.
bool is_upgrade(std::string_view connection, std::string_view upgrade);

// A frame from the server, which is never masked.
std::string encode_frame(Opcode opcode, std::string_view payload);

enum class FrameState {
  incomplete,
  ready,
  malformed,
};

struct Frame {
  FrameState state = FrameState::incomplete;
  Opcode opcode = Opcode::text;
  bool final = true;
  std::string payload;

  // How many bytes of the buffer this frame occupied.
  std::size_t length = 0;
};

// Reads one frame from the front of the buffer. A frame from a client must be
// masked, and one that is not is refused.
Frame decode_frame(std::string_view buffer);

}  // namespace blogin::websocket
