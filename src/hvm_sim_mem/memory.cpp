#include "hvm-sim/mem/memory.hpp"
#include <stdexcept>
#include <algorithm>

namespace hvm {

// =============================================================================
// FlatMemory Implementation
// =============================================================================

FlatMemory::FlatMemory(size_t size) : data_(size, 0) {}

uint8_t FlatMemory::read_byte(uint64_t addr) {
    check_bounds(addr, 1);
    return data_[addr];
}

uint16_t FlatMemory::read_half(uint64_t addr) {
    check_bounds(addr, 2);
    return static_cast<uint16_t>(data_[addr]) | (static_cast<uint16_t>(data_[addr + 1]) << 8);
}

uint32_t FlatMemory::read_word(uint64_t addr) {
    check_bounds(addr, 4);
    return static_cast<uint32_t>(data_[addr]) |
           (static_cast<uint32_t>(data_[addr + 1]) << 8) |
           (static_cast<uint32_t>(data_[addr + 2]) << 16) |
           (static_cast<uint32_t>(data_[addr + 3]) << 24);
}

uint64_t FlatMemory::read_dword(uint64_t addr) {
    check_bounds(addr, 8);
    return static_cast<uint64_t>(data_[addr]) |
           (static_cast<uint64_t>(data_[addr + 1]) << 8) |
           (static_cast<uint64_t>(data_[addr + 2]) << 16) |
           (static_cast<uint64_t>(data_[addr + 3]) << 24) |
           (static_cast<uint64_t>(data_[addr + 4]) << 32) |
           (static_cast<uint64_t>(data_[addr + 5]) << 40) |
           (static_cast<uint64_t>(data_[addr + 6]) << 48) |
           (static_cast<uint64_t>(data_[addr + 7]) << 56);
}

void FlatMemory::write_byte(uint64_t addr, uint8_t val) {
    check_bounds(addr, 1);
    data_[addr] = val;
}

void FlatMemory::write_half(uint64_t addr, uint16_t val) {
    check_bounds(addr, 2);
    data_[addr] = static_cast<uint8_t>(val);
    data_[addr + 1] = static_cast<uint8_t>(val >> 8);
}

void FlatMemory::write_word(uint64_t addr, uint32_t val) {
    check_bounds(addr, 4);
    data_[addr] = static_cast<uint8_t>(val);
    data_[addr + 1] = static_cast<uint8_t>(val >> 8);
    data_[addr + 2] = static_cast<uint8_t>(val >> 16);
    data_[addr + 3] = static_cast<uint8_t>(val >> 24);
}

void FlatMemory::write_dword(uint64_t addr, uint64_t val) {
    check_bounds(addr, 8);
    data_[addr] = static_cast<uint8_t>(val);
    data_[addr + 1] = static_cast<uint8_t>(val >> 8);
    data_[addr + 2] = static_cast<uint8_t>(val >> 16);
    data_[addr + 3] = static_cast<uint8_t>(val >> 24);
    data_[addr + 4] = static_cast<uint8_t>(val >> 32);
    data_[addr + 5] = static_cast<uint8_t>(val >> 40);
    data_[addr + 6] = static_cast<uint8_t>(val >> 48);
    data_[addr + 7] = static_cast<uint8_t>(val >> 56);
}

void FlatMemory::load(uint64_t addr, const std::vector<uint8_t>& data) {
    check_bounds(addr, data.size());
    std::copy(data.begin(), data.end(), data_.begin() + addr);
}

std::vector<uint8_t> FlatMemory::dump(uint64_t addr, size_t len) const {
    check_bounds(addr, len);
    return std::vector<uint8_t>(data_.begin() + addr, data_.begin() + addr + len);
}

void FlatMemory::check_bounds(uint64_t addr, size_t size) const {
    if (addr >= data_.size() || addr + size > data_.size()) {
        throw std::out_of_range("Memory access out of bounds");
    }
}

// =============================================================================
// MemorySystem Implementation
// =============================================================================

MemorySystem::MemorySystem() = default;

void MemorySystem::add_region(uint64_t base, uint64_t size, MemoryRegionType type, bool read_only) {
    regions_.push_back({
        .base = base,
        .size = size,
        .type = type,
        .data = std::vector<uint8_t>(size, 0),
        .read_only = read_only
    });
}

void MemorySystem::add_rom(uint64_t base, const std::vector<uint8_t>& data) {
    regions_.push_back({
        .base = base,
        .size = static_cast<uint64_t>(data.size()),
        .type = MemoryRegionType::kRom,
        .data = data,
        .read_only = true
    });
}

void MemorySystem::add_ram(uint64_t base, uint64_t size) {
    regions_.push_back({
        .base = base,
        .size = size,
        .type = MemoryRegionType::kRam,
        .data = std::vector<uint8_t>(size, 0),
        .read_only = false
    });
}

void MemorySystem::add_mmio(uint64_t base, uint64_t size,
                         std::function<uint8_t(uint64_t)> read_fn,
                         std::function<void(uint64_t, uint8_t)> write_fn) {
    mmio_handlers_[base] = {.read = std::move(read_fn), .write = std::move(write_fn)};
    regions_.push_back({
        .base = base,
        .size = size,
        .type = MemoryRegionType::kMmio,
        .data = std::vector<uint8_t>(size, 0),
        .read_only = false
    });
}

MemoryRegion* MemorySystem::find_region(uint64_t addr) {
    for (auto& region : regions_) {
        if (addr >= region.base && addr < region.base + region.size) {
            return &region;
        }
    }
    return nullptr;
}

const MemoryRegion* MemorySystem::find_region(uint64_t addr) const {
    for (const auto& region : regions_) {
        if (addr >= region.base && addr < region.base + region.size) {
            return &region;
        }
    }
    return nullptr;
}

void MemorySystem::bounds_check(uint64_t addr, size_t size) const {
    const MemoryRegion* region = find_region(addr);
    if (!region) {
        throw std::out_of_range("Memory access to unmapped region");
    }
    uint64_t offset = addr - region->base;
    if (offset + size > region->size) {
        throw std::out_of_range("Memory access exceeds region size");
    }
}

uint8_t MemorySystem::read_byte(uint64_t addr) {
    const MemoryRegion* region = find_region(addr);
    if (!region) throw std::out_of_range("Read from unmapped memory");
    uint64_t offset = addr - region->base;
    
    if (region->type == MemoryRegionType::kMmio) {
        auto it = mmio_handlers_.find(region->base);
        if (it != mmio_handlers_.end()) {
            return it->second.read(addr);
        }
    }
    
    return region->data[offset];
}

uint16_t MemorySystem::read_half(uint64_t addr) {
    bounds_check(addr, 2);
    const MemoryRegion* region = find_region(addr);
    uint64_t offset = addr - region->base;
    return static_cast<uint16_t>(region->data[offset]) | 
           (static_cast<uint16_t>(region->data[offset + 1]) << 8);
}

uint32_t MemorySystem::read_word(uint64_t addr) {
    bounds_check(addr, 4);
    const MemoryRegion* region = find_region(addr);
    uint64_t offset = addr - region->base;
    return static_cast<uint32_t>(region->data[offset]) |
           (static_cast<uint32_t>(region->data[offset + 1]) << 8) |
           (static_cast<uint32_t>(region->data[offset + 2]) << 16) |
           (static_cast<uint32_t>(region->data[offset + 3]) << 24);
}

uint64_t MemorySystem::read_dword(uint64_t addr) {
    bounds_check(addr, 8);
    const MemoryRegion* region = find_region(addr);
    uint64_t offset = addr - region->base;
    return static_cast<uint64_t>(region->data[offset]) |
           (static_cast<uint64_t>(region->data[offset + 1]) << 8) |
           (static_cast<uint64_t>(region->data[offset + 2]) << 16) |
           (static_cast<uint64_t>(region->data[offset + 3]) << 24) |
           (static_cast<uint64_t>(region->data[offset + 4]) << 32) |
           (static_cast<uint64_t>(region->data[offset + 5]) << 40) |
           (static_cast<uint64_t>(region->data[offset + 6]) << 48) |
           (static_cast<uint64_t>(region->data[offset + 7]) << 56);
}

void MemorySystem::write_byte(uint64_t addr, uint8_t val) {
    MemoryRegion* region = find_region(addr);
    if (!region) throw std::out_of_range("Write to unmapped memory");
    
    if (region->read_only) {
        throw std::runtime_error("Write to read-only memory");
    }
    
    uint64_t offset = addr - region->base;
    
    if (region->type == MemoryRegionType::kMmio) {
        auto it = mmio_handlers_.find(region->base);
        if (it != mmio_handlers_.end()) {
            it->second.write(addr, val);
            return;
        }
    }
    
    region->data[offset] = val;
}

void MemorySystem::write_half(uint64_t addr, uint16_t val) {
    bounds_check(addr, 2);
    MemoryRegion* region = find_region(addr);
    if (region->read_only) throw std::runtime_error("Write to read-only memory");
    uint64_t offset = addr - region->base;
    region->data[offset] = static_cast<uint8_t>(val);
    region->data[offset + 1] = static_cast<uint8_t>(val >> 8);
}

void MemorySystem::write_word(uint64_t addr, uint32_t val) {
    bounds_check(addr, 4);
    MemoryRegion* region = find_region(addr);
    if (region->read_only) throw std::runtime_error("Write to read-only memory");
    uint64_t offset = addr - region->base;
    region->data[offset] = static_cast<uint8_t>(val);
    region->data[offset + 1] = static_cast<uint8_t>(val >> 8);
    region->data[offset + 2] = static_cast<uint8_t>(val >> 16);
    region->data[offset + 3] = static_cast<uint8_t>(val >> 24);
}

void MemorySystem::write_dword(uint64_t addr, uint64_t val) {
    bounds_check(addr, 8);
    MemoryRegion* region = find_region(addr);
    if (region->read_only) throw std::runtime_error("Write to read-only memory");
    uint64_t offset = addr - region->base;
    region->data[offset] = static_cast<uint8_t>(val);
    region->data[offset + 1] = static_cast<uint8_t>(val >> 8);
    region->data[offset + 2] = static_cast<uint8_t>(val >> 16);
    region->data[offset + 3] = static_cast<uint8_t>(val >> 24);
    region->data[offset + 4] = static_cast<uint8_t>(val >> 32);
    region->data[offset + 5] = static_cast<uint8_t>(val >> 40);
    region->data[offset + 6] = static_cast<uint8_t>(val >> 48);
    region->data[offset + 7] = static_cast<uint8_t>(val >> 56);
}

void MemorySystem::read_bytes(uint64_t addr, uint8_t* buf, size_t len) {
    bounds_check(addr, len);
    const MemoryRegion* region = find_region(addr);
    uint64_t offset = addr - region->base;
    std::copy(region->data.begin() + offset, region->data.begin() + offset + len, buf);
}

void MemorySystem::write_bytes(uint64_t addr, const uint8_t* buf, size_t len) {
    bounds_check(addr, len);
    MemoryRegion* region = find_region(addr);
    if (region->read_only) throw std::runtime_error("Write to read-only memory");
    uint64_t offset = addr - region->base;
    std::copy(buf, buf + len, region->data.begin() + offset);
}

void MemorySystem::set_read_only(uint64_t base, uint64_t size, bool read_only) {
    for (auto& region : regions_) {
        if (region.base == base && region.size == size) {
            region.read_only = read_only;
            return;
        }
    }
}

void MemorySystem::clear() {
    regions_.clear();
    mmio_handlers_.clear();
}

uint64_t MemorySystem::get_total_size() const {
    uint64_t total = 0;
    for (const auto& region : regions_) {
        total += region.size;
    }
    return total;
}

} // namespace hvm
