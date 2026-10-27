// src/tools/tool_registry.cpp
//
// Hash table that maps a tool's name to the tool, using separate chaining.
// The hash is 64-bit FNV-1a. The bucket count is always a power of two, so
// the index is hash & (size - 1). Before an add that would push the load
// factor over 1.0, the table doubles and every tool is moved into the new one.

#include "tools/tool_registry.h"
#include <cstdint>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(kStartingBuckets) {}

ToolRegistry::~ToolRegistry() = default;

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 14695981039346656037ULL;  // FNV-1a offset basis
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;                  // FNV prime
    }
    return static_cast<std::size_t>(h);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) return false;

    const std::string name = tool->name();
    if (find(name) != nullptr) return false;    // already there, new tool gets deleted

    // grow first, so the load factor never goes over the limit
    double load_after = static_cast<double>(count_ + 1) / static_cast<double>(buckets_.size());
    if (load_after > kMaxLoadFactor) rehash(buckets_.size() * 2);

    buckets_[index_for(name)].push_back(std::move(tool));
    ++count_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const Bucket& bucket = buckets_[index_for(name)];
    for (const std::unique_ptr<Tool>& tool : bucket) {
        if (tool->name() == name) return tool.get();
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return count_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const Bucket& bucket : buckets_){
        for (const std::unique_ptr<Tool>& tool : bucket){
            fn(*tool);
        }
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(count_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const Bucket& bucket : buckets_){
        if (bucket.size() > longest) longest = bucket.size();
    }
    return longest;
}

// Maps a name to its bucket. The bucket count is a power of two, so
// masking with (size - 1) gives the same result as hash % size.
std::size_t ToolRegistry::index_for(std::string_view key) const noexcept {
    return hash(key) & (buckets_.size() - 1);
}

// Builds a table with new_bucket_count buckets and moves every tool into it.
// Each tool's index is recomputed because it depends on the table size.
// Only the unique_ptrs move, so pointers from find() stay valid.
void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<Bucket> fresh(new_bucket_count);
    for (Bucket& old_bucket : buckets_) {
        for (std::unique_ptr<Tool>& tool : old_bucket) {
            std::size_t idx = hash(tool->name()) & (new_bucket_count - 1);
            fresh[idx].push_back(std::move(tool));
        }
    }
    buckets_ = std::move(fresh);
}