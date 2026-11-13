#pragma once

// GUIDs are derived from stable interface names at compile time (RFC 4122
// UUIDv5, URL namespace). The name is part of the ABI: never rename or move an
// identifier once it has been released.

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace helios::abi {

struct guid {
    std::uint32_t data1{};
    std::uint16_t data2{};
    std::uint16_t data3{};
    std::uint8_t data4[8]{};

    friend constexpr bool operator==(const guid&, const guid&) noexcept = default;
};

inline constexpr guid uuid_namespace_url{
        0x6ba7b811u, 0x9dadu, 0x11d1u, {0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8}};

namespace detail {

inline constexpr std::size_t max_guid_name = 200;

constexpr std::uint32_t load_be32(const std::uint8_t* data) noexcept {
    return (static_cast<std::uint32_t>(data[0]) << 24)
         | (static_cast<std::uint32_t>(data[1]) << 16)
         | (static_cast<std::uint32_t>(data[2]) << 8)
         | static_cast<std::uint32_t>(data[3]);
}

constexpr void store_be32(std::uint8_t* data, std::uint32_t value) noexcept {
    data[0] = static_cast<std::uint8_t>(value >> 24);
    data[1] = static_cast<std::uint8_t>(value >> 16);
    data[2] = static_cast<std::uint8_t>(value >> 8);
    data[3] = static_cast<std::uint8_t>(value);
}

constexpr std::array<std::uint8_t, 20> sha1(std::span<const std::uint8_t> message) noexcept {
    std::array<std::uint8_t, 256> buffer{};
    for (std::size_t i = 0; i < message.size(); ++i) buffer[i] = message[i];
    buffer[message.size()] = 0x80;

    const std::size_t padded_size = ((message.size() + 9 + 63) / 64) * 64;
    const std::uint64_t bit_count = static_cast<std::uint64_t>(message.size()) * 8;
    for (std::size_t i = 0; i < 8; ++i)
        buffer[padded_size - 1 - i] = static_cast<std::uint8_t>(bit_count >> (8 * i));

    std::array<std::uint32_t, 5> hash{
            0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};

    for (std::size_t offset = 0; offset < padded_size; offset += 64) {
        std::array<std::uint32_t, 80> words{};
        for (std::size_t i = 0; i < 16; ++i)
            words[i] = load_be32(buffer.data() + offset + i * 4);
        for (std::size_t i = 16; i < 80; ++i)
            words[i] = std::rotl(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);

        std::uint32_t a = hash[0];
        std::uint32_t b = hash[1];
        std::uint32_t c = hash[2];
        std::uint32_t d = hash[3];
        std::uint32_t e = hash[4];

        for (std::size_t i = 0; i < 80; ++i) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999u;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }

            const std::uint32_t temp = std::rotl(a, 5) + f + e + k + words[i];
            e = d;
            d = c;
            c = std::rotl(b, 30);
            b = a;
            a = temp;
        }

        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
    }

    std::array<std::uint8_t, 20> digest{};
    for (std::size_t i = 0; i < 5; ++i) store_be32(digest.data() + i * 4, hash[i]);
    return digest;
}

}  // namespace detail

// UUIDv5 from the URL namespace. The same name always produces the same GUID;
// a name longer than detail::max_guid_name produces a zero GUID, which the
// interface_base static assertion rejects when used as an interface identifier.
// constexpr: ABI identifiers are evaluated at compile time; runtime calls are
// allowed for dynamic identifiers.
constexpr guid make_guid(std::string_view name) noexcept {
    if (name.size() > detail::max_guid_name) return {};

    constexpr std::array<std::uint8_t, 16> namespace_bytes{
            0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1,
            0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8};

    std::array<std::uint8_t, 16 + detail::max_guid_name> message{};
    for (std::size_t i = 0; i < namespace_bytes.size(); ++i) message[i] = namespace_bytes[i];
    for (std::size_t i = 0; i < name.size(); ++i)
        message[namespace_bytes.size() + i] = static_cast<std::uint8_t>(name[i]);

    auto digest = detail::sha1(
            std::span<const std::uint8_t>(message).first(namespace_bytes.size() + name.size()));

    digest[6] = static_cast<std::uint8_t>((digest[6] & 0x0F) | 0x50);
    digest[8] = static_cast<std::uint8_t>((digest[8] & 0x3F) | 0x80);

    return guid{
            (static_cast<std::uint32_t>(digest[0]) << 24)
                    | (static_cast<std::uint32_t>(digest[1]) << 16)
                    | (static_cast<std::uint32_t>(digest[2]) << 8)
                    | static_cast<std::uint32_t>(digest[3]),
            static_cast<std::uint16_t>((digest[4] << 8) | digest[5]),
            static_cast<std::uint16_t>((digest[6] << 8) | digest[7]),
            {digest[8], digest[9], digest[10], digest[11],
             digest[12], digest[13], digest[14], digest[15]},
    };
}

}  // namespace helios::abi
