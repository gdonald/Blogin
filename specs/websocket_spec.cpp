#include <format>
#include <string>

#include "support/spec.h"
#include "websocket.h"

using spec::expect;

namespace {

std::string hex(std::string_view digest) {
  std::string out;

  for (const char byte : digest) {
    out += std::format("{:02x}", static_cast<unsigned char>(byte));
  }

  return out;
}

// A client's frame is masked, and the server's is not. This builds one the way a
// browser would, so decoding is tested against what it will receive.
std::string masked_frame(blogin::websocket::Opcode opcode, std::string_view payload) {
  constexpr char mask[4] = {0x37, static_cast<char>(0xfa), 0x21, 0x3d};

  std::string frame;
  frame += static_cast<char>(0x80 | static_cast<unsigned char>(opcode));

  if (payload.size() < 126) {
    frame += static_cast<char>(0x80 | payload.size());
  } else {
    frame += static_cast<char>(0x80 | 126);
    frame += static_cast<char>((payload.size() >> 8) & 0xff);
    frame += static_cast<char>(payload.size() & 0xff);
  }

  frame.append(mask, 4);

  for (std::size_t index = 0; index < payload.size(); ++index) {
    frame += static_cast<char>(payload[index] ^ mask[index % 4]);
  }

  return frame;
}

}  // namespace

SPEC {
  // Checked against the published values, not against itself, since an
  // implementation that agrees only with its own specs is no use to a browser.
  spec::describe("sha1", [] {
    spec::it("hashes the empty string", [] {
      expect(hex(blogin::websocket::sha1(""))).to_eq("da39a3ee5e6b4b0d3255bfef95601890afd80709");
    });

    spec::it("hashes abc", [] {
      expect(hex(blogin::websocket::sha1("abc"))).to_eq("a9993e364706816aba3e25717850c26c9cd0d89d");
    });

    // Long enough to need a second block, where the padding is easy to
    // get wrong.
    spec::it("hashes a message spanning two blocks", [] {
      expect(hex(blogin::websocket::sha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")))
        .to_eq("84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    });

    spec::it("hashes a message exactly one block long", [] {
      expect(hex(blogin::websocket::sha1(std::string(64, 'a'))))
        .to_eq("0098ba824b5c16427bd7a1122a5a442a25ec644d");
    });
  });

  spec::describe("base64", [] {
    spec::it("encodes a length divisible by three", [] {
      expect(blogin::websocket::base64("abc")).to_eq("YWJj");
    });

    spec::it("pads a length one short", [] { expect(blogin::websocket::base64("ab")).to_eq("YWI="); });

    spec::it("pads a length two short", [] { expect(blogin::websocket::base64("a")).to_eq("YQ=="); });

    spec::it("encodes nothing as nothing", [] { expect(blogin::websocket::base64("")).to_eq(""); });
  });

  spec::describe("the handshake", [] {
    // The example from RFC 6455 itself.
    spec::it("answers the key from the specification with its published value", [] {
      expect(blogin::websocket::accept_key("dGhlIHNhbXBsZSBub25jZQ=="))
        .to_eq("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    });

    spec::it("recognises an upgrade request", [] {
      expect(blogin::websocket::is_upgrade("Upgrade", "websocket")).to_be_true();
    });

    spec::it("recognises an upgrade named alongside something else", [] {
      expect(blogin::websocket::is_upgrade("keep-alive, Upgrade", "WebSocket")).to_be_true();
    });

    spec::it("refuses an ordinary request", [] {
      expect(blogin::websocket::is_upgrade("keep-alive", "")).to_be_false();
    });
  });

  spec::describe("framing", [] {
    spec::context("what the server sends", [] {
      spec::it("marks a text frame final", [] {
        const std::string frame = blogin::websocket::encode_frame(blogin::websocket::Opcode::text, "hi");

        expect(static_cast<unsigned char>(frame[0])).to_eq(static_cast<unsigned char>(0x81));
      });

      spec::it("writes a short length in one byte", [] {
        const std::string frame = blogin::websocket::encode_frame(blogin::websocket::Opcode::text, "hi");

        expect(static_cast<unsigned char>(frame[1])).to_eq(static_cast<unsigned char>(2));
      });

      // A server frame is never masked, so the length byte carries no mask bit.
      spec::it("does not mask", [] {
        const std::string frame = blogin::websocket::encode_frame(blogin::websocket::Opcode::text, "hi");

        expect((static_cast<unsigned char>(frame[1]) & 0x80) == 0).to_be_true();
      });

      spec::it("writes a medium length in two extra bytes", [] {
        const std::string frame =
          blogin::websocket::encode_frame(blogin::websocket::Opcode::text, std::string(200, 'x'));

        expect(frame.size()).to_eq(std::size_t{204});
      });

      spec::it("writes a long length in eight extra bytes", [] {
        const std::string frame =
          blogin::websocket::encode_frame(blogin::websocket::Opcode::text, std::string(70000, 'x'));

        expect(frame.size()).to_eq(std::size_t{70010});
      });
    });

    spec::context("what the client sends", [] {
      spec::it("unmasks the payload", [] {
        const auto frame = blogin::websocket::decode_frame(
          masked_frame(blogin::websocket::Opcode::text, "hello"));

        expect(frame.payload).to_eq("hello");
      });

      spec::it("reads the opcode", [] {
        const auto frame =
          blogin::websocket::decode_frame(masked_frame(blogin::websocket::Opcode::ping, ""));

        expect(frame.opcode == blogin::websocket::Opcode::ping).to_be_true();
      });

      spec::it("reports how many bytes the frame took", [] {
        const auto frame =
          blogin::websocket::decode_frame(masked_frame(blogin::websocket::Opcode::text, "hello"));

        expect(frame.length).to_eq(std::size_t{11});
      });

      spec::it("reads a payload needing two length bytes", [] {
        const auto frame = blogin::websocket::decode_frame(
          masked_frame(blogin::websocket::Opcode::text, std::string(300, 'y')));

        expect(frame.payload.size()).to_eq(std::size_t{300});
      });

      spec::it("waits for a frame that has not all arrived", [] {
        const std::string whole = masked_frame(blogin::websocket::Opcode::text, "hello");

        expect(blogin::websocket::decode_frame(whole.substr(0, 6)).state ==
               blogin::websocket::FrameState::incomplete)
          .to_be_true();
      });

      spec::it("leaves the bytes after a frame alone", [] {
        const std::string two = masked_frame(blogin::websocket::Opcode::text, "one") +
                                masked_frame(blogin::websocket::Opcode::text, "two");

        const auto first = blogin::websocket::decode_frame(two);
        const auto second = blogin::websocket::decode_frame(std::string_view(two).substr(first.length));

        expect(second.payload).to_eq("two");
      });

      spec::it("waits for a frame carrying only one byte", [] {
        expect(blogin::websocket::decode_frame("\x81").state ==
               blogin::websocket::FrameState::incomplete)
          .to_be_true();
      });

      spec::it("waits for the two length bytes it was promised", [] {
        expect(blogin::websocket::decode_frame(std::string("\x81\xfe", 2)).state ==
               blogin::websocket::FrameState::incomplete)
          .to_be_true();
      });

      spec::it("waits for the eight length bytes it was promised", [] {
        expect(blogin::websocket::decode_frame(std::string("\x81\xff", 2)).state ==
               blogin::websocket::FrameState::incomplete)
          .to_be_true();
      });

      spec::it("reads a payload needing eight length bytes", [] {
        std::string frame("\x81\xff", 2);

        for (const int shift : {56, 48, 40, 32, 24, 16, 8, 0}) {
          frame += static_cast<char>((std::size_t{4} >> shift) & 0xff);
        }

        frame.append(4, '\0');
        frame += "abcd";

        expect(blogin::websocket::decode_frame(frame).payload).to_eq("abcd");
      });

      spec::it("waits for the mask it was promised", [] {
        expect(blogin::websocket::decode_frame(std::string("\x81\x84", 2)).state ==
               blogin::websocket::FrameState::incomplete)
          .to_be_true();
      });

      // The length is whatever the sender wrote in the field, and adding it to
      // the read position wraps a 64 bit number around. A frame claiming the
      // largest length there is came out looking smaller than the buffer, so
      // the decoder went on to reserve that much and the process died on the
      // length_error nobody catches.
      spec::context("a length field larger than the buffer could hold", [] {
        auto frame = spec::let([] {
          std::string bytes("\x81\xff", 2);

          bytes.append(8, '\xff');
          bytes.append(4, '\xff');
          bytes.append(16, '\xff');

          return bytes;
        });

        spec::it("waits rather than believing the length", [=] {
          expect(blogin::websocket::decode_frame(frame()).state ==
                 blogin::websocket::FrameState::incomplete)
            .to_be_true();
        });

        spec::it("reads no payload out of it", [=] {
          expect(blogin::websocket::decode_frame(frame()).payload).to_eq("");
        });
      });

      // Everything a client sends must be masked. One that is not is a protocol
      // error, not something to read anyway.
      spec::it("refuses an unmasked frame", [] {
        expect(blogin::websocket::decode_frame(
                 blogin::websocket::encode_frame(blogin::websocket::Opcode::text, "hi"))
                 .state == blogin::websocket::FrameState::malformed)
          .to_be_true();
      });
    });
  });
}
