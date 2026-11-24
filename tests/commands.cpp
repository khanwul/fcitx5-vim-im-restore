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
        {"i", 0},
        {"a", 0},
        {"o", 0},
        {"I", 0},
        {"A", 0},
        {"O", 0},
        {"R", 0},
        {"hjkl3i", 5},
        {"\ni", 1},
        {"d\ni", 2},
        {"diwi", 3},
        {"dawi", 3},
        {"d10iwA", 5},
        {"d0i", 2},
        {"fai", 2},
        {"dtii", 3},
        {"riA", 2},
        {"\"idiwo", 5},
        {"ggi", 2},
        {"/initial\ni", 9},
        {"?aoi\nR", 5},
        {":write initial\no", 15},
        {"viw", -1},
        {"ciao", -1},
        {"qiia", -1},
        {"@i", -1},
        {"gRi", -1},
        {"zzi", 2},
        {"ztzbo", 4},
        {"zfi", -1},
        {":a\bai\no", 6},
        {"/a\b\bi", 4},
        {":s/cat/c/g\na", 11},
        {":2,5substitute#x\\#y#c#gc\nynaR", 28},
        {":s/a/b/c\nqi", 10},
        {":s c\nlo", 6},
        {":%s/x/c/gnc\ni", 12},
        {":w /tmp/gc\ni", 11},
        {"3:\bi\no", 5},
        {"\"=input\npi", 9},
        {":s /[x/]/z/c\nao", 14},
        {":s/x/y/c\nydiwi", 13},
        {":%s/x/y/c\nynai", 13},
        {":s/x/y/c 2\nynai", 14},
        {":%s/x/y/c 1\nna", 13},
        {":sil! keepj keepp %s/x/y/gc\nyiao", 31},
        {":keepp s/x/y/c\nna", 16},
        {":keepp echo 's/x/y/c'\na", 22},
        {":s/x/y/gc\nyinao", 14},
        {":set number | %s/x/y/gc\nai", 25},
        {":set tw=72 | set nu | s/x/y/c\nna", 31},
        {":set titlestring=x\\|%s/x/y/gc\na", 30},
        {":s/x/y/ | s/z/t/c\nai", 19},
        {":s/x/y/c 1 | set nu\nyi", 21},
        {":s/missing/y/ce\ni", 16},
        {":s/x/y/ce\nai", 11},
    };
    for (const auto &[keys, restoreAt] : cases) {
        viescape::Commands commands;
        for (size_t i = 0; i < keys.size(); ++i) {
            auto sym = static_cast<fcitx::KeySym>(keys[i]);
            if (keys[i] == '\n') {
                sym = FcitxKey_Return;
            } else if (keys[i] == '\b') {
                sym = FcitxKey_BackSpace;
            }
            if (commands.entersInsert(fcitx::Key(sym)) !=
                (static_cast<int>(i) == restoreAt)) {
                std::cerr << "Unexpected restore decision in [" << keys
                          << "] at " << i << '\n';
                return 1;
            }
        }
    }
}
