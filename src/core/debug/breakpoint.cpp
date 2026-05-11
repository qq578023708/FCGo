#include "breakpoint.h"

int BreakpointManager::add(const Breakpoint& bp) {
    if (breakpoints_.size() >= kMaxBreakpoints) return -1;
    breakpoints_.push_back(bp);
    return static_cast<int>(breakpoints_.size()) - 1;
}

bool BreakpointManager::remove(int index) {
    if (index < 0 || index >= static_cast<int>(breakpoints_.size())) return false;
    breakpoints_.erase(breakpoints_.begin() + index);
    return true;
}

void BreakpointManager::clear() {
    breakpoints_.clear();
}

int BreakpointManager::toggle(uint16_t addr, uint16_t typeFlags) {
    // Check if a breakpoint with same address and type already exists
    for (int i = 0; i < static_cast<int>(breakpoints_.size()); i++) {
        if (breakpoints_[i].address == addr && (breakpoints_[i].flags & typeFlags)) {
            // Remove existing
            breakpoints_.erase(breakpoints_.begin() + i);
            return -1;
        }
    }
    // Add new
    Breakpoint bp;
    bp.address = addr;
    bp.flags = BP_ENABLE | typeFlags;
    return add(bp);
}

bool BreakpointManager::checkMatch(uint16_t addr, uint16_t accessType, int& hitIndex) {
    for (int i = 0; i < static_cast<int>(breakpoints_.size()); i++) {
        Breakpoint& bp = breakpoints_[i];
        if (!bp.isEnabled()) continue;
        if (!(bp.flags & accessType)) continue;
        if (bp.matchesAddr(addr)) {
            bp.hitCount++;
            hitIndex = i;
            return true;
        }
    }
    return false;
}

bool BreakpointManager::checkExec(uint16_t pc, int& hitIndex) {
    return checkMatch(pc, BP_EXEC, hitIndex);
}

bool BreakpointManager::checkRead(uint16_t addr, int& hitIndex) {
    return checkMatch(addr, BP_READ, hitIndex);
}

bool BreakpointManager::checkWrite(uint16_t addr, int& hitIndex) {
    return checkMatch(addr, BP_WRITE, hitIndex);
}

void BreakpointManager::setAllEnabled(bool enabled) {
    for (auto& bp : breakpoints_) {
        if (enabled) bp.flags |= BP_ENABLE;
        else        bp.flags &= ~BP_ENABLE;
    }
}
