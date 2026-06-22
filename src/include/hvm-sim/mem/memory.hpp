#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <functional>
#include "hvm-sim/core/memory_access.hpp"

namespace hvm {



// Memory region types
enum class MemoryRegionType : uint8_t {
    kRam,
    kRom,
    kMmio,
    kReserved
};

// Memory region descriptor
struct MemoryRegion {
    uint64_t base;
    uint64_t size;
    MemoryRegionType type;
    std::vector<uint8_t> data;
    bool read_only;
};

// Main memory system class
class MemorySystem : public MemoryAccess {
public:
    MemorySystem();
    ~MemorySystem() override = default;

    // Add a memory region
    void add_region(uint64_t base, uint64_t size, MemoryRegionType type, bool read_only = false);

    // Add ROM data
    void add_rom(uint64_t base, const std::vector<uint8_t>& data);

    // Add RAM region
    void add_ram(uint64_t base, uint64_t size);

    // Add MMIO region with callback
    void add_mmio(uint64_t base, uint64_t size,
                 std::function<uint8_t(uint64_t)> read_fn,
                 std::function<void(uint64_t, uint8_t)> write_fn);

    // MemoryAccess interface implementation
    uint8_t read_byte(uint64_t addr) override;
    uint16_t read_half(uint64_t addr) override;
    uint32_t read_word(uint64_t addr) override;
    uint64_t read_dword(uint64_t addr) override;

    void write_byte(uint64_t addr, uint8_t val) override;
    void write_half(uint64_t addr, uint16_t val) override;
    void write_word(uint64_t addr, uint32_t val) override;
    void write_dword(uint64_t addr, uint64_t val) override;

    // Bulk memory operations
    void read_bytes(uint64_t addr, uint8_t* buf, size_t len);
    void write_bytes(uint64_t addr, const uint8_t* buf, size_t len);

    // Memory protection
    void set_read_only(uint64_t base, uint64_t size, bool read_only);

    // Clear memory
    void clear();

    // Get total memory size
    uint64_t get_total_size() const;

private:
    struct MMIOHandler {
        std::function<uint8_t(uint64_t)> read;
        std::function<void(uint64_t, uint8_t)> write;
    };

    std::vector<MemoryRegion> regions_;
    std::unordered_map<uint64_t, MMIOHandler> mmio_handlers_;

    MemoryRegion* find_region(uint64_t addr);
    const MemoryRegion* find_region(uint64_t addr) const;

    void bounds_check(uint64_t addr, size_t size) const;
};

// Simple flat memory implementation (for testing and initial use)
class FlatMemory : public MemoryAccess {
public:
    explicit FlatMemory(size_t size);
    ~FlatMemory() override = default;

    // MemoryAccess interface implementation
    uint8_t read_byte(uint64_t addr) override;
    uint16_t read_half(uint64_t addr) override;
    uint32_t read_word(uint64_t addr) override;
    uint64_t read_dword(uint64_t addr) override;

    void write_byte(uint64_t addr, uint8_t val) override;
    void write_half(uint64_t addr, uint16_t val) override;
    void write_word(uint64_t addr, uint32_t val) override;
    void write_dword(uint64_t addr, uint64_t val) override;

    // Additional operations
    uint8_t* get_data() { return data_.data(); }
    const uint8_t* get_data() const { return data_.data(); }
    size_t get_size() const { return data_.size(); }

    void load(uint64_t addr, const std::vector<uint8_t>& data);
    std::vector<uint8_t> dump(uint64_t addr, size_t len) const;

private:
    std::vector<uint8_t> data_;
    void check_bounds(uint64_t addr, size_t size = 1) const;
};

} // namespace hvm
