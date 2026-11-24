#include "commands.h"
#include <string_view>

namespace viescape {
namespace {
bool contains(std::string_view chars, char c) {
    return chars.find(c) != std::string_view::npos;
}
} // namespace

bool Commands::entersInsert(const fcitx::Key &key) {
    if (key.isModifier() || state_ == State::Suspended) {
        return false;
    }
    if (state_ == State::CommandLine) {
        if (key.check(FcitxKey_Return) || key.check(FcitxKey_KP_Enter)) {
            state_ = State::Normal;
        } else if (key.states()) {
            // A quoted Return or a register can change the command line.
            state_ = State::Suspended;
        }
        return false;
    }
    if (key.states()) {
        state_ = State::Suspended;
        return false;
    }
    if (state_ == State::Argument) {
        state_ = State::Normal;
        return false;
    }
    const auto sym = key.sym();
    const char c = sym >= 32 && sym <= 126 ? static_cast<char>(sym) : '\0';
    if (state_ == State::GPrefix) {
        state_ =
            contains("geEgGjJkK_^$0", c) ? State::Normal : State::Suspended;
        return false;
    }
    if (contains(":/?", c)) {
        state_ = State::CommandLine;
        return false;
    }
    constexpr std::string_view motions = "hjklwWbBeE0^$G|%(){};,+- ";
    if (state_ == State::Operator) {
        if ((c >= '1' && c <= '9') || (motionCount_ && c == '0')) {
            motionCount_ = true;
        } else if (contains("iafFtT'`", c)) {
            state_ = State::Argument;
        } else {
            state_ = c == operator_ || contains(motions, c) ? State::Normal
                                                            : State::Suspended;
        }
        return false;
    }
    if (contains("iaoIAOR", c)) {
        return true;
    }
    if (contains("dy", c)) {
        operator_ = c;
        motionCount_ = false;
        state_ = State::Operator;
    } else if (contains("fFtTrm'`\"", c)) {
        state_ = State::Argument;
    } else if (c == 'g') {
        state_ = State::GPrefix;
    } else if (!contains(motions, c) &&
               !contains("123456789xXpPuUJDY~.nN*", c) &&
               sym != FcitxKey_Left && sym != FcitxKey_Right &&
               sym != FcitxKey_Up && sym != FcitxKey_Down) {
        // Includes Visual, change commands, macros, and unknown sequences.
        state_ = State::Suspended;
    }
    return false;
}

} // namespace viescape
