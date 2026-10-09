#pragma once
#include <atomic>
#include <cstdint>
namespace miniwindow {
class WebLifecycle {
public:
    uint64_t Begin() noexcept {return generation_.fetch_add(1)+1;}
    bool IsCurrent(uint64_t token) const noexcept {return token!=0 && token==generation_.load();}
    void Close() noexcept {generation_.fetch_add(1);}
private:
    std::atomic<uint64_t> generation_{0};
};
}
