#include "commands.h"
#include <iostream>
#include <string_view>

int main() {
    // Each sequence starts in Normal after an unconsumed Escape.
    // The expected index is the only key allowed to restore the saved IM.
    struct Case {
        std::string_view keys;
        int restoreAt;
    };
    const Case cases[] = {
        {"i", 0},       {"a", 0},
        {"o", 0},       {"I", 0},
        {"A", 0},       {"O", 0},
        {"R", 0},       {"hjkl3i", 5},
        {"diwi", 3},    {"dawi", 3},
        {"d10iwA", 5},  {"d0i", 2},
        {"fai", 2},     {"dtii", 3},
        {"riA", 2},     {"\"idiwo", 5},
        {"ggi", 2},     {"/initial\ni", 9},
        {"?aoi\nR", 5}, {":write initial\no", 15},
        {"viw", -1},    {"ciao", -1},
        {"qiia", -1},   {"@i", -1},
        {"gRi", -1},
    };
    for (const auto &[keys, restoreAt] : cases) {
        viescape::Commands commands;
        for (size_t i = 0; i < keys.size(); ++i) {
            const auto sym = keys[i] == '\n'
                                 ? FcitxKey_Return
                                 : static_cast<fcitx::KeySym>(keys[i]);
            if (commands.entersInsert(fcitx::Key(sym)) !=
                (static_cast<int>(i) == restoreAt)) {
                std::cerr << "Unexpected restore decision in [" << keys
                          << "] at " << i << '\n';
                return 1;
            }
        }
    }
}
