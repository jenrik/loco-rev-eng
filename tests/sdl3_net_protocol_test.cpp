#include "sdl3_net_protocol.h"

#ifndef _WIN32

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace lego_loco::network;

namespace {

TransportFrame ExampleFrame(std::uint32_t sequence = 7) {
    return {TransportFrameKind::LegacyPayload, 0x5a, 2, 1, sequence,
            {0x06, 0x00, 0x2c, 0x01, 0xaa, 0xbb}};
}

void TestEveryFragmentBoundary() {
    const std::vector<std::uint8_t> bytes = EncodeTransportFrame(ExampleFrame());
    assert(bytes.size() == kTransportHeaderSize + 6);
    for (std::size_t split = 0; split <= bytes.size(); ++split) {
        TransportFrameDecoder decoder;
        std::vector<TransportFrame> frames;
        std::string error;
        assert(decoder.Append(bytes.data(), split, &frames, &error));
        assert(frames.empty() == (split != bytes.size()));
        assert(decoder.Append(bytes.data() + split, bytes.size() - split, &frames, &error));
        assert(frames.size() == 1);
        assert(frames[0].kind == TransportFrameKind::LegacyPayload);
        assert(frames[0].source == 2);
        assert(frames[0].destination == 1);
        assert(frames[0].sequence == 7);
        assert(frames[0].payload == ExampleFrame().payload);
    }

    TransportFrameDecoder bytewise;
    std::vector<TransportFrame> frames;
    std::string error;
    assert(bytewise.Append(nullptr, 0, &frames, &error));
    for (std::uint8_t byte : bytes) {
        assert(bytewise.Append(&byte, 1, &frames, &error));
    }
    assert(frames.size() == 1);
}

void TestCoalescedFrames() {
    std::vector<std::uint8_t> bytes;
    for (std::uint32_t sequence = 1; sequence <= 3; ++sequence) {
        std::vector<std::uint8_t> encoded = EncodeTransportFrame(ExampleFrame(sequence));
        bytes.insert(bytes.end(), encoded.begin(), encoded.end());
    }
    TransportFrameDecoder decoder;
    std::vector<TransportFrame> frames;
    std::string error;
    assert(decoder.Append(bytes.data(), bytes.size(), &frames, &error));
    assert(frames.size() == 3);
    assert(frames[0].sequence == 1 && frames[1].sequence == 2 && frames[2].sequence == 3);
    assert(decoder.buffered_bytes() == 0);
}

void ExpectMalformed(std::vector<std::uint8_t> bytes, const std::string& expected) {
    TransportFrameDecoder decoder;
    std::vector<TransportFrame> frames;
    std::string error;
    assert(!decoder.Append(bytes.data(), bytes.size(), &frames, &error));
    assert(error.find(expected) != std::string::npos);
    assert(decoder.failed());
    assert(!decoder.Append(nullptr, 0, &frames, &error));
}

void TestMalformedHeaders() {
    const std::vector<std::uint8_t> good = EncodeTransportFrame(ExampleFrame());
    std::vector<std::uint8_t> bytes = good;
    bytes[0] = 'X';
    ExpectMalformed(bytes, "magic");
    bytes = good;
    bytes[4] = 2;
    ExpectMalformed(bytes, "version");
    bytes = good;
    bytes[6] = 0xff;
    ExpectMalformed(bytes, "kind");
    bytes = good;
    bytes[16] = 1;
    bytes[17] = 0;
    bytes[18] = 1;
    bytes[19] = 0;
    ExpectMalformed(bytes, "64 KiB");
}

void TestHandshakeCodecs() {
    ClientHello hello{"Client", "12345678-1234-1234-1234-123456789abc"};
    const std::vector<std::uint8_t> hello_bytes = EncodeClientHello(hello);
    ClientHello decoded_hello;
    std::string error;
    assert(DecodeClientHello(hello_bytes, &decoded_hello, &error));
    assert(decoded_hello.player_name == hello.player_name);
    assert(decoded_hello.expected_session_uuid == hello.expected_session_uuid);

    std::vector<std::uint8_t> wrong_app = hello_bytes;
    wrong_app[0] ^= 0xff;
    assert(!DecodeClientHello(wrong_app, &decoded_hello, &error));
    std::vector<std::uint8_t> wrong_legacy = hello_bytes;
    wrong_legacy[18] = 0;
    assert(!DecodeClientHello(wrong_legacy, &decoded_hello, &error));
    assert(EncodeClientHello({"player-name-too-long", {}}).empty());

    ServerWelcome welcome{2, 1, 2, 9, hello.expected_session_uuid, "Host"};
    ServerWelcome decoded_welcome;
    assert(DecodeServerWelcome(EncodeServerWelcome(welcome), &decoded_welcome, &error));
    assert(decoded_welcome.assigned_player_id == 2);
    assert(decoded_welcome.session_uuid == welcome.session_uuid);

    PlayerNotice notice{2, "Client"};
    PlayerNotice decoded_notice;
    assert(DecodePlayerNotice(EncodePlayerNotice(notice), &decoded_notice, &error));
    assert(decoded_notice.player_id == 2 && decoded_notice.player_name == "Client");
}

void TestLegacyTrainPositionAck() {
    const std::vector<std::uint8_t> good = {
        0xf7, 0x03, 0x2c, 0x01, 0x78, 0x56, 0x34, 0x12,
        8, 7, 0xaa, 0xbb,
    };
    LegacyTrainPositionAck acknowledgment;
    std::string error;
    assert(DecodeLegacyTrainPositionAck(good, &acknowledgment, &error));
    assert(acknowledgment.network_id == 0x12345678);
    assert(acknowledgment.slot_index == 8);
    assert(acknowledgment.peer_index == 7);
    assert(acknowledgment.reserved == 0xbbaa);

    std::vector<std::uint8_t> malformed = good;
    malformed.pop_back();
    assert(!DecodeLegacyTrainPositionAck(malformed, &acknowledgment, &error));
    malformed = good;
    malformed.push_back(0);
    assert(!DecodeLegacyTrainPositionAck(malformed, &acknowledgment, &error));
    malformed = good;
    malformed[0] = 0xf6;
    assert(!DecodeLegacyTrainPositionAck(malformed, &acknowledgment, &error));
    malformed = good;
    malformed[2] = 0;
    assert(!DecodeLegacyTrainPositionAck(malformed, &acknowledgment, &error));
    malformed = good;
    malformed[8] = 9;
    assert(!DecodeLegacyTrainPositionAck(malformed, &acknowledgment, &error));
    malformed = good;
    malformed[9] = 9;
    assert(!DecodeLegacyTrainPositionAck(malformed, &acknowledgment, &error));
    assert(!DecodeLegacyTrainPositionAck(good, nullptr, &error));
}

void TestLegacyValidationAndLimits() {
    std::string error;
    assert(ValidateLegacyPayload(ExampleFrame().payload, &error));
    assert(!ValidateLegacyPayload({0, 0, 0}, &error));
    assert(!ValidateLegacyPayload({0, 0, 0, 0}, &error));

    TransportFrame maximum = ExampleFrame();
    maximum.payload.assign(kMaximumTransportPayload, 0);
    maximum.payload[2] = 0x2c;
    maximum.payload[3] = 0x01;
    assert(!EncodeTransportFrame(maximum).empty());
    maximum.payload.push_back(0);
    assert(EncodeTransportFrame(maximum).empty());
}

}  // namespace

int main() {
    TestEveryFragmentBoundary();
    TestCoalescedFrames();
    TestMalformedHeaders();
    TestHandshakeCodecs();
    TestLegacyValidationAndLimits();
    TestLegacyTrainPositionAck();
    std::cout << "PASS: transport framing, fragmentation, handshake, and bounds validation\n";
    return 0;
}

#endif  // !_WIN32
