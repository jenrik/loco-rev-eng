// Status: TRANSCRIBED
/** DPlayConfig.h — Network lobby settings object at DAT_004FD3A8.
 *
 * Original constructor: GameConfig_constructor, 0x440C60.
 * The original x86 object persists exactly 0xAC settings bytes from +0x04,
 * plus its dispatch word. Host storage preserves those byte offsets for
 * untranslated callers while accessors keep initialization centralized.
 */
#pragma once

#include <cstdint>
#include <cstring>

class DPlayConfig {
public:
    static constexpr uint16_t k_format_marker = 0x006A;
    static constexpr uint32_t k_storage_size = 0xB0;

    DPlayConfig() { initialize_defaults(); }

    void initialize_defaults()
    {
        std::memset(storage_, 0, sizeof(storage_));
        write_u16(0x04, k_format_marker);
        storage_[0x07] = 1;
        write_u32(0x0C, 0x1E);
        write_u32(0x1C, 4);
        write_u32(0x20, 2);
        write_u32(0x28, 4);
        write_u32(0xAC, 2);
    }

    uint8_t* binary_data() { return storage_; }
    const uint8_t* binary_data() const { return storage_; }

private:
    void write_u16(uint32_t offset, uint16_t value) { std::memcpy(storage_ + offset, &value, sizeof(value)); }
    void write_u32(uint32_t offset, uint32_t value) { std::memcpy(storage_ + offset, &value, sizeof(value)); }

    uint8_t storage_[k_storage_size];
};
