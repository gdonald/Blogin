#include "websocket.h"

#include <array>
#include <bit>
#include <cstring>

#include "sanitizer.h"
#include "text.h"

namespace blogin::websocket {
namespace {

// The protocol's own constant, appended to the client's key before hashing.
constexpr std::string_view websocket_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

constexpr std::string_view base64_alphabet =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// A rotate shifts bits off the top on purpose, and SHA-1's round additions wrap
// on purpose too. The integer sanitizer's checks are pointed away from the two
// functions that implement the algorithm, not from the file.
BLOGIN_WRAPS_ON_PURPOSE
std::uint32_t rotate_left(std::uint32_t value, int amount) {
  return (value << amount) | (value >> (32 - amount));
}

}  // namespace

BLOGIN_WRAPS_ON_PURPOSE
std::string sha1(std::string_view data) {
  std::uint32_t state[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};

  // The message, a 0x80 byte, zeroes, then the original length in bits as a
  // big-endian 64-bit number.
  std::string message(data);
  const std::uint64_t bits = static_cast<std::uint64_t>(data.size()) * 8;

  message += static_cast<char>(0x80);

  while (message.size() % 64 != 56) {
    message += '\0';
  }

  for (int index = 7; index >= 0; --index) {
    message += static_cast<char>((bits >> (index * 8)) & 0xff);
  }

  for (std::size_t offset = 0; offset < message.size(); offset += 64) {
    std::uint32_t words[80] = {};

    for (int index = 0; index < 16; ++index) {
      const auto* block = reinterpret_cast<const unsigned char*>(message.data() + offset +
                                                                 (static_cast<std::size_t>(index) * 4));

      words[index] = static_cast<std::uint32_t>(block[0]) << 24 |
                     static_cast<std::uint32_t>(block[1]) << 16 |
                     static_cast<std::uint32_t>(block[2]) << 8 | static_cast<std::uint32_t>(block[3]);
    }

    for (int index = 16; index < 80; ++index) {
      words[index] =
        rotate_left(words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);
    }

    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];

    for (int index = 0; index < 80; ++index) {
      std::uint32_t mixed = 0;
      std::uint32_t constant = 0;

      if (index < 20) {
        mixed = (b & c) | (~b & d);
        constant = 0x5a827999;
      } else if (index < 40) {
        mixed = b ^ c ^ d;
        constant = 0x6ed9eba1;
      } else if (index < 60) {
        mixed = (b & c) | (b & d) | (c & d);
        constant = 0x8f1bbcdc;
      } else {
        mixed = b ^ c ^ d;
        constant = 0xca62c1d6;
      }

      const std::uint32_t next = rotate_left(a, 5) + mixed + e + constant + words[index];

      e = d;
      d = c;
      c = rotate_left(b, 30);
      b = a;
      a = next;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
  }

  std::string digest;
  digest.reserve(20);

  for (const std::uint32_t word : state) {
    for (int index = 3; index >= 0; --index) {
      digest += static_cast<char>((word >> (index * 8)) & 0xff);
    }
  }

  return digest;
}

std::string base64(std::string_view data) {
  std::string out;
  out.reserve((data.size() + 2) / 3 * 4);

  std::size_t index = 0;

  while (index + 2 < data.size()) {
    const std::uint32_t chunk = static_cast<std::uint32_t>(static_cast<unsigned char>(data[index])) << 16 |
                                static_cast<std::uint32_t>(static_cast<unsigned char>(data[index + 1]))
                                  << 8 |
                                static_cast<std::uint32_t>(static_cast<unsigned char>(data[index + 2]));

    out += base64_alphabet[(chunk >> 18) & 0x3f];
    out += base64_alphabet[(chunk >> 12) & 0x3f];
    out += base64_alphabet[(chunk >> 6) & 0x3f];
    out += base64_alphabet[chunk & 0x3f];

    index += 3;
  }

  if (const std::size_t remaining = data.size() - index; remaining > 0) {
    std::uint32_t chunk = static_cast<std::uint32_t>(static_cast<unsigned char>(data[index])) << 16;

    if (remaining == 2) {
      chunk |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[index + 1])) << 8;
    }

    out += base64_alphabet[(chunk >> 18) & 0x3f];
    out += base64_alphabet[(chunk >> 12) & 0x3f];
    out += remaining == 2 ? base64_alphabet[(chunk >> 6) & 0x3f] : '=';
    out += '=';
  }

  return out;
}

std::string accept_key(std::string_view client_key) {
  return base64(sha1(std::string(client_key) + std::string(websocket_guid)));
}

bool is_upgrade(std::string_view connection, std::string_view upgrade) {
  return blogin::text::to_lower_ascii(connection).contains("upgrade") &&
         blogin::text::to_lower_ascii(upgrade) == "websocket";
}

std::string encode_frame(Opcode opcode, std::string_view payload) {
  std::string frame;

  frame += static_cast<char>(0x80 | static_cast<std::uint8_t>(opcode));

  // A server never masks, so the length byte carries no mask bit.
  if (payload.size() < 126) {
    frame += static_cast<char>(payload.size());
  } else if (payload.size() <= 0xffff) {
    frame += static_cast<char>(126);
    frame += static_cast<char>((payload.size() >> 8) & 0xff);
    frame += static_cast<char>(payload.size() & 0xff);
  } else {
    frame += static_cast<char>(127);

    for (int index = 7; index >= 0; --index) {
      frame += static_cast<char>((payload.size() >> (index * 8)) & 0xff);
    }
  }

  frame += payload;

  return frame;
}

Frame decode_frame(std::string_view buffer) {
  Frame frame;

  if (buffer.size() < 2) {
    return frame;
  }

  const auto byte = [&buffer](std::size_t index) {
    return static_cast<std::uint8_t>(static_cast<unsigned char>(buffer[index]));
  };

  frame.final = (byte(0) & 0x80) != 0;
  frame.opcode = static_cast<Opcode>(byte(0) & 0x0f);

  const bool masked = (byte(1) & 0x80) != 0;
  std::uint64_t length = byte(1) & 0x7f;
  std::size_t cursor = 2;

  if (length == 126) {
    if (buffer.size() < cursor + 2) {
      return frame;
    }

    length = static_cast<std::uint64_t>(byte(cursor)) << 8 | byte(cursor + 1);
    cursor += 2;
  } else if (length == 127) {
    if (buffer.size() < cursor + 8) {
      return frame;
    }

    length = 0;

    for (std::size_t index = 0; index < 8; ++index) {
      length = (length << 8) | byte(cursor + index);
    }

    cursor += 8;
  }

  // Everything a client sends must be masked. One that is not is a protocol
  // error rather than something to read anyway.
  if (!masked) {
    frame.state = FrameState::malformed;

    return frame;
  }

  if (buffer.size() < cursor + 4) {
    return frame;
  }

  const std::array<std::uint8_t, 4> mask{byte(cursor), byte(cursor + 1), byte(cursor + 2),
                                         byte(cursor + 3)};
  cursor += 4;

  // Written as a subtraction rather than as buffer.size() < cursor + length,
  // because length is whatever the sender put in the 64 bit field and adding it
  // to the cursor wraps: a frame claiming 2^64 - 1 bytes came out looking
  // smaller than the buffer, passed this check, and reached reserve, which
  // threw length_error and took the process down. cursor is known to be inside
  // the buffer by here, so the subtraction cannot wrap.
  if (length > buffer.size() - cursor) {
    return frame;
  }

  frame.payload.reserve(static_cast<std::size_t>(length));

  for (std::uint64_t index = 0; index < length; ++index) {
    frame.payload += static_cast<char>(byte(cursor + static_cast<std::size_t>(index)) ^
                                       mask[static_cast<std::size_t>(index % 4)]);
  }

  frame.state = FrameState::ready;
  frame.length = cursor + static_cast<std::size_t>(length);

  return frame;
}

}  // namespace blogin::websocket
