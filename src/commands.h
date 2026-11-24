#pragma once

#include <fcitx-utils/key.h>

namespace viescape {

// Only called between an unconsumed Escape and the next direct entry to Insert.
class Commands {
public:
    bool entersInsert(const fcitx::Key &key);

private:
    enum class State {
        Normal,
        Operator,
        Argument,
        GPrefix,
        CommandLine,
        Suspended
    };
    State state_ = State::Normal;
    char operator_ = 0;
    bool motionCount_ = false;
};

} // namespace viescape
