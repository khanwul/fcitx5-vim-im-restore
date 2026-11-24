#include "commands.h"
#include <algorithm>
#include <string_view>
#include <utility>

namespace viescape {
namespace {
bool contains(std::string_view chars, char c) {
    return chars.find(c) != std::string_view::npos;
}

bool skipSubstituteField(std::string_view &line, char delimiter, bool pattern) {
    bool inClass = false;
    while (!line.empty()) {
        const char c = line.front();
        line.remove_prefix(1);
        if (!inClass && c == delimiter) {
            return true;
        }
        if (c == '\\' && !line.empty()) {
            line.remove_prefix(1);
        } else if (pattern && (c == '[' || c == ']')) {
            inClass = c == '[';
        }
    }
    return false;
}

bool isCommandModifier(std::string_view name) {
    struct Modifier {
        std::string_view name;
        size_t minLength;
    };
    static constexpr Modifier modifiers[] = {
        {"aboveleft", 3},  {"belowright", 3},   {"botright", 2},
        {"browse", 3},     {"confirm", 4},      {"hide", 3},
        {"horizontal", 3}, {"keepalt", 5},      {"keepjumps", 5},
        {"keepmarks", 3},  {"keeppatterns", 5}, {"leftabove", 5},
        {"lockmarks", 3},  {"noautocmd", 3},    {"noswapfile", 3},
        {"rightbelow", 6}, {"sandbox", 3},      {"silent", 3},
        {"tab", 3},        {"topleft", 2},      {"unsilent", 3},
        {"verbose", 4},    {"vertical", 4},
    };
    return std::ranges::any_of(modifiers, [name](const Modifier &modifier) {
        return name.size() >= modifier.minLength &&
               modifier.name.starts_with(name);
    });
}
} // namespace

Commands::State Commands::afterCommandLine(std::string_view line) {
    if (!line.starts_with(':')) {
        return State::Normal;
    }
    bool multipleLines = false;
    for (;;) {
        // Skip the range, including marks such as '<,'>.
        while (!line.empty()) {
            if (line.starts_with('\'') && line.size() >= 2) {
                line.remove_prefix(2);
            } else if (contains(": %.$0123456789,;+-", line.front())) {
                multipleLines |= contains("%,;", line.front());
                line.remove_prefix(1);
            } else {
                break;
            }
        }
        const auto name = line.substr(
            0, line.find_first_not_of("abcdefghijklmnopqrstuvwxyz"));
        line.remove_prefix(name.size());
        if (!name.empty() && std::string_view("substitute").starts_with(name)) {
            break;
        }
        if (!isCommandModifier(name)) {
            return State::Normal;
        }
        if (std::string_view("silent").starts_with(name) &&
            line.starts_with('!')) {
            line.remove_prefix(1);
        }
    }
    while (line.starts_with(' ')) {
        line.remove_prefix(1);
    }
    // With only flags (e.g. :s c), Vim repeats the last substitution.
    if (!line.empty() && !contains("cegiInplr", line.front())) {
        // In :s/pattern/replacement/flags the delimiter is user-selected.
        const char delimiter = line.front();
        line.remove_prefix(1);
        if (!skipSubstituteField(line, delimiter, true) ||
            !skipSubstituteField(line, delimiter, false)) {
            return State::Normal;
        }
    }
    const auto flags = line.substr(0, line.find_first_not_of("&cegiInplr#"));
    if (!contains(flags, 'c') || contains(flags, 'n')) {
        return State::Normal;
    }
    line.remove_prefix(flags.size());
    const auto countStart = line.find_first_not_of(' ');
    if (countStart != std::string_view::npos) {
        // A trailing count starts at the last address in the range.
        multipleLines =
            line[countStart] != '1' || countStart != line.find_last_not_of(' ');
    }
    // With Vim's default 'nogdefault', at most one match per addressed
    // line is confirmed unless flags request or inherit a global replacement.
    return multipleLines || contains(flags, 'g') || contains(flags, '&')
               ? State::ConfirmMultiple
               : State::ConfirmOnce;
}

void Commands::editCommandLine(const fcitx::Key &key) {
    if (key.check(FcitxKey_Return) || key.check(FcitxKey_KP_Enter)) {
        state_ = afterCommandLine(commandLine_);
    } else if ((commandLine_.starts_with('/') ||
                commandLine_.starts_with('?')) &&
               (key.check(FcitxKey_Up) || key.check(FcitxKey_Down) ||
                key.check(FcitxKey_P, fcitx::KeyState::Ctrl) ||
                key.check(FcitxKey_N, fcitx::KeyState::Ctrl))) {
        // Vim owns the recalled text. Keep tracking the suffix typed after it.
        commandLine_.resize(1);
        recalledSearch_ = true;
        return;
    } else if (key.check(FcitxKey_U, fcitx::KeyState::Ctrl)) {
        // All tracked edits leave the cursor at the end of the command line.
        commandLine_.resize(1);
        recalledSearch_ = false;
        return;
    } else if (key.check(FcitxKey_BackSpace) ||
               key.check(FcitxKey_H, fcitx::KeyState::Ctrl)) {
        if (commandLine_.size() > 1) {
            commandLine_.pop_back();
            return;
        }
        if (recalledSearch_) {
            return;
        }
        state_ = State::Normal;
    } else {
        const auto ch = fcitx::Key::keySymToUnicode(key.sym());
        if (!key.states() && ch >= ' ' && ch != 127) {
            // One byte per typed character keeps Backspace accurate for UTF-8.
            commandLine_ += ch < 128 ? static_cast<char>(ch) : '\x7f';
            return;
        }
        // History, completion and quoted keys can replace unseen text.
        state_ = State::Suspended;
    }
    commandLine_.clear();
    recalledSearch_ = false;
}

bool Commands::entersInsert(const fcitx::Key &key) {
    if (key.isModifier() || state_ == State::Suspended) {
        return false;
    }
    if (state_ == State::CommandLine) {
        editCommandLine(key);
        return false;
    }
    const auto sym = key.sym();
    const char c = sym >= 32 && sym <= 126 ? static_cast<char>(sym) : '\0';
    if (state_ == State::Normal && !key.states() && c >= '0' && c <= '9') {
        // Vim caps a Normal-mode count at 999999999.
        count_ = static_cast<int>(
            std::min(count_ * 10LL + (c - '0'), 999'999'999LL));
        return false;
    }
    const int count = std::exchange(count_, 0);
    if (state_ == State::ConfirmOnce || state_ == State::ConfirmMultiple) {
        if (!key.states() &&
            contains(state_ == State::ConfirmOnce ? "aqlyn" : "aql", c)) {
            state_ = State::Normal;
        }
        return false;
    }
    if (key.states()) {
        if (state_ != State::Normal || key.states() != fcitx::KeyState::Ctrl ||
            !contains("DBFUEY", c)) {
            state_ = State::Suspended;
        }
        return false;
    }
    if (state_ == State::Register && c == '=') {
        commandLine_ = "=";
        state_ = State::CommandLine;
        return false;
    }
    if (state_ == State::Argument || state_ == State::Register) {
        state_ = State::Normal;
        return false;
    }
    if (state_ == State::GPrefix) {
        state_ =
            contains("geEgGjJkK_^$0", c) ? State::Normal : State::Suspended;
        return false;
    }
    if (state_ == State::ZPrefix) {
        state_ = contains("ztb.^+-hHlL", c) || sym == FcitxKey_Return
                     ? State::Normal
                     : State::Suspended;
        return false;
    }
    if (contains(":/?", c)) {
        commandLine_.assign(1, c);
        if (c == ':' && count > 0) {
            commandLine_ += '.';
            if (count > 1) {
                commandLine_ += ",.+" + std::to_string(count - 1);
            }
        }
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
    } else if (contains("fFtTrm'`", c)) {
        state_ = State::Argument;
    } else if (c == '"') {
        state_ = State::Register;
    } else if (c == 'g') {
        state_ = State::GPrefix;
    } else if (c == 'z') {
        state_ = State::ZPrefix;
    } else if (!contains(motions, c) && !contains("xXpPuUJDY~.nN*", c) &&
               sym != FcitxKey_Left && sym != FcitxKey_Right &&
               sym != FcitxKey_Up && sym != FcitxKey_Down) {
        // Includes Visual, change commands, macros, and unknown sequences.
        state_ = State::Suspended;
    }
    return false;
}

} // namespace viescape
