#pragma once

#include <fcitx-utils/key.h>
#include <string>

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
        Register,
        GPrefix,
        ZPrefix,
        CommandLine,
        SubstituteConfirm,
        Suspended
    };
    void editCommandLine(const fcitx::Key &key);
    State state_ = State::Normal;
    std::string commandLine_;
    char operator_ = 0;
    bool motionCount_ = false;
    int count_ = 0;
};

} // namespace viescape
