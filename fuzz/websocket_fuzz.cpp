#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "require.h"
#include "websocket.h"

// The one binary protocol blogin parses, and it arrives off a socket. A frame
// carries its payload length in a 7, 16, or 64 bit field that the sender picks,
// and a client's frame is masked with a key the sender also picks, so the
// decoder does length arithmetic on numbers it was handed. That is the
// shape of a buffer overrun.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string buffer(reinterpret_cast<const char*>(data), size);

  const blogin::websocket::Frame frame = blogin::websocket::decode_frame(buffer);

  if (frame.state == blogin::websocket::FrameState::ready) {
    // length is where the next frame in the buffer starts. Past the end and the
    // connection reads whatever follows the buffer. At zero it reads this same
    // frame forever.
    FUZZ_REQUIRE(frame.length > 0);
    FUZZ_REQUIRE(frame.length <= buffer.size());

    // The payload came out of those bytes, so it cannot be longer than the
    // frame that carried it. A 64 bit length field that was believed rather
    // than checked shows up here.
    FUZZ_REQUIRE(frame.payload.size() <= frame.length);
  }

  // Handing the same bytes back has to give the same answer.
  const blogin::websocket::Frame repeat = blogin::websocket::decode_frame(buffer);

  FUZZ_REQUIRE(repeat.state == frame.state);
  FUZZ_REQUIRE(repeat.length == frame.length);
  FUZZ_REQUIRE(repeat.payload == frame.payload);

  // A frame the server sends is built rather than parsed, and the payload is
  // whatever a rebuild wants to tell the page.
  const std::string encoded =
    blogin::websocket::encode_frame(blogin::websocket::Opcode::text, buffer);

  FUZZ_REQUIRE(encoded.size() > buffer.size());

  // Sec-WebSocket-Key is a header value, so this runs the hash and the base64
  // over bytes a client chose.
  const std::string accept = blogin::websocket::accept_key(buffer);

  // SHA-1 is 20 bytes, which is 28 base64 characters with one pad.
  FUZZ_REQUIRE(accept.size() == 28);

  FUZZ_REQUIRE(blogin::websocket::sha1(buffer).size() == 20);

  const std::string encoded_base64 = blogin::websocket::base64(buffer);

  FUZZ_REQUIRE(encoded_base64.size() == (buffer.size() + 2) / 3 * 4);

  return 0;
}
