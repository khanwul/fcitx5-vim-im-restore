#if __has_include(<fcitx-utils/standardpaths.h>)
#include <fcitx-utils/standardpaths.h>
#else
#include <fcitx-utils/standardpath.h>
#endif
#include <fcitx-utils/testing.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <testfrontend_public.h>
#include <testim_public.h>

namespace {
using namespace fcitx;
// TestIM exposes its keyEvent handler. This engine exercises the later
// filterKey hook, just before Fcitx publishes KeyEventResult.
class EscapeFilter final : public InputMethodEngine {
    void keyEvent(const InputMethodEntry &, KeyEvent &) override {}
    void filterKey(const InputMethodEntry &, KeyEvent &event) override {
        if (event.key().check(FcitxKey_Escape)) {
            event.filter();
        }
    }
};

class EscapeFilterFactory final : public AddonFactory {
    AddonInstance *create(AddonManager *) override { return new EscapeFilter; }
};

void require(bool ok, const std::string &message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

class Client {
public:
    Client(Instance &instance, AddonInstance &frontend,
           const std::string &program)
        : instance_(instance), frontend_(frontend),
          uuid_(frontend.call<ITestFrontend::createInputContext>(program)),
          ic_(instance.inputContextManager().findByUUID(uuid_)) {
        ic_->focusIn();
    }
    ~Client() { frontend_.call<ITestFrontend::destroyInputContext>(uuid_); }
    void select(const std::string &im) {
        instance_.setCurrentInputMethod(ic_, im, true);
        expect(im);
    }
    void expect(const std::string &im) const {
        const auto actual = instance_.inputMethod(ic_);
        require(actual == im, "expected IM " + im + ", got " + actual);
    }
    bool key(KeySym sym, bool release = false) {
        return key(Key(sym), release);
    }
    bool key(const Key &key, bool release = false) {
        return frontend_.call<ITestFrontend::sendKeyEvent>(uuid_, key, release);
    }
    void pass(KeySym sym) {
        require(!key(sym), "client did not receive " + Key(sym).toString());
    }
    void pass(std::string_view text) {
        for (const char c : text) {
            pass(static_cast<KeySym>(c));
        }
    }
    InputContext *context() const { return ic_; }

private:
    Instance &instance_;
    AddonInstance &frontend_;
    ICUUID uuid_;
    InputContext *ic_;
};

struct CompositionCase {
    std::string addon;
    std::string inputMethod;
    std::string keys;
    std::string text;
};

int run(const std::string &scenario, Instance &instance,
        const CompositionCase &composition) {
    auto &manager = instance.addonManager();
    auto *addon = manager.addon("viescape");
    auto *frontend = manager.addon("testfrontend");
    auto *engine = manager.addon("testim");
    require(addon && frontend && engine,
            "could not load the addon and test modules");
    auto &methods = instance.inputMethodManager();
    InputMethodGroup group(methods.currentGroup().name());
    group.setDefaultLayout(scenario == "translated-command" ? "fr" : "us");
    for (const char *name :
         {"keyboard-us", "test-primary", "test-secondary", "test-command"}) {
        group.inputMethodList().emplace_back(name);
    }
    if (scenario == "composition") {
        require(manager.addon(composition.addon, true),
                "could not load the requested engine: " + composition.addon);
        require(methods.entry(composition.inputMethod),
                "unknown input method: " + composition.inputMethod);
        group.inputMethodList().emplace_back(composition.inputMethod);
    } else if (scenario == "filtered-escape") {
        require(manager.addon("escapefilter", true),
                "could not load Escape filter");
        group.inputMethodList().emplace_back("test-filter");
    }
    group.setDefaultInputMethod("test-primary");
    methods.setGroup(std::move(group));
    Client client(instance, *frontend, "editor");

    if (scenario == "switching") {
        // Count both ways a frontend can get a command: an unconsumed
        // original event (pass) and any additional synthetic forward.
        unsigned forwards = 0;
        auto watch = instance.watchEvent<EventType::InputContextForwardKey>(
            EventWatcherPhase::PostInputMethod,
            [&](const auto &) { ++forwards; });
        std::string received;
        engine->call<ITestIM::setHandler>(
            [&](const InputMethodEntry &entry, KeyEvent &event) {
                if (!event.isRelease() && event.key().check(FcitxKey_r)) {
                    received = entry.uniqueName();
                    event.filterAndAccept();
                }
            });
        client.select("test-primary");
        client.pass(FcitxKey_Escape);
        client.expect("keyboard-us");
        client.pass(FcitxKey_Escape);
        for (auto sym : {FcitxKey_h, FcitxKey_d, FcitxKey_i, FcitxKey_w,
                         FcitxKey_f, FcitxKey_a}) {
            client.pass(sym);
            client.expect("keyboard-us");
        }
        client.pass(FcitxKey_i);
        client.expect("test-primary");
        require(client.key(FcitxKey_r) && received == "test-primary",
                "the first Insert keystroke missed the restored engine");
        require(forwards == 0, "duplicate command event forwarded");
        engine->call<ITestIM::setHandler>(nullptr);
    } else if (scenario == "translated-command") {
        client.select("test-primary");
        client.pass(FcitxKey_Escape);
        client.expect("keyboard-us");
        // XKB keycode 38 produces q on French AZERTY and a on US QWERTY.
        const Key azertyQ(FcitxKey_q, KeyStates{}, 38);
        frontend->call<ITestFrontend::pushCommitExpectation>("a");
        client.context()->setBlockEventToClient(true);
        require(client.key(azertyQ), "the original q reached the client");
        client.expect("test-primary");
        client.context()->setBlockEventToClient(false);
        client.key(azertyQ, true);
        require(!client.key(azertyQ), "the next q missed the restored layout");
    } else if (scenario == "scroll-restore") {
        client.select("test-secondary");
        client.pass(FcitxKey_Escape);
        require(!client.key(Key(FcitxKey_d, KeyState::Ctrl)),
                "Ctrl+D was consumed");
        client.pass(FcitxKey_z);
        client.pass(FcitxKey_z);
        client.pass(FcitxKey_Return);
        client.expect("keyboard-us");
        client.pass(FcitxKey_i);
        client.expect("test-secondary");
    } else if (scenario == "command-line-restore") {
        client.select("test-primary");
        client.pass(FcitxKey_Escape);
        client.pass(FcitxKey_colon);
        client.pass(FcitxKey_BackSpace);
        client.pass(FcitxKey_i);
        client.expect("test-primary");
        client.pass(FcitxKey_Escape);
        client.pass(":%s/x/y/gc");
        client.pass(FcitxKey_Return);
        client.pass(FcitxKey_a);
        client.expect("keyboard-us");
        client.pass(FcitxKey_i);
        client.expect("test-primary");
    } else if (scenario == "search-history-restore") {
        client.select("test-primary");
        client.pass(FcitxKey_Escape);
        client.pass("/x");
        client.pass(FcitxKey_Return);
        client.pass(FcitxKey_slash);
        client.pass(FcitxKey_Up);
        client.pass(FcitxKey_Return);
        client.pass(FcitxKey_i);
        client.expect("test-primary");
        client.pass(FcitxKey_Escape);
        client.pass(FcitxKey_question);
        client.pass(FcitxKey_Up);
        client.pass(FcitxKey_BackSpace);
        client.pass(FcitxKey_i);
        client.expect("keyboard-us");
        require(!client.key(Key(FcitxKey_u, KeyState::Ctrl)),
                "Ctrl+U was consumed");
        client.pass(FcitxKey_x);
        client.pass(FcitxKey_Return);
        client.pass(FcitxKey_i);
        client.expect("test-primary");
    } else if (scenario == "edited-search-restore") {
        client.select("test-primary");
        client.pass(FcitxKey_Escape);
        client.pass("/ab");
        client.pass(FcitxKey_Left);
        client.pass(FcitxKey_i);
        client.expect("keyboard-us");
        client.pass(FcitxKey_Return);
        client.pass(FcitxKey_i);
        client.expect("test-primary");
        client.pass(FcitxKey_Escape);
        client.pass("?ab");
        require(!client.key(Key(FcitxKey_c, KeyState::Ctrl)),
                "Ctrl+C was consumed");
        client.pass(FcitxKey_i);
        client.expect("test-primary");
    } else if (scenario == "shared-override") {
        for (auto policy :
             {PropertyPropagatePolicy::All, PropertyPropagatePolicy::Program}) {
            instance.inputContextManager().setPropertyPropagatePolicy(policy);
            client.select("test-primary");
            client.pass(FcitxKey_Escape);
            client.expect("keyboard-us");
            client.context()->focusOut();
            Client other(instance, *frontend,
                         policy == PropertyPropagatePolicy::All ? "browser"
                                                                : "editor");
            other.select("test-secondary");
            other.select("keyboard-us");
            other.context()->focusOut();
            client.pass(FcitxKey_i);
            client.expect("keyboard-us");
        }
        // Program sharing still isolates a different program's manual switch.
        client.select("test-primary");
        client.pass(FcitxKey_Escape);
        client.context()->focusOut();
        Client browser(instance, *frontend, "browser");
        browser.select("test-secondary");
        browser.select("keyboard-us");
        browser.context()->focusOut();
        client.pass(FcitxKey_i);
        client.expect("test-primary");
    } else if (scenario == "single-substitution-restore") {
        client.select("test-primary");
        for (auto reply : {FcitxKey_y, FcitxKey_n}) {
            client.pass(FcitxKey_Escape);
            client.pass(":s/x/y/c");
            client.pass(FcitxKey_Return);
            client.pass(reply);
            client.expect("keyboard-us");
            client.pass(FcitxKey_i);
            client.expect("test-primary");
        }
    } else if (scenario == "modified-substitution-confirm") {
        client.select("test-primary");
        client.pass(FcitxKey_Escape);
        client.pass(":set number | keeppatterns %s/x/y/gc");
        client.pass(FcitxKey_Return);
        client.pass(FcitxKey_a);
        client.expect("keyboard-us");
        client.pass(FcitxKey_i);
        client.expect("test-primary");
    } else if (scenario == "filtered-escape") {
        client.select("test-filter");
        client.pass(FcitxKey_Escape);
        client.expect("keyboard-us");
        client.pass(FcitxKey_i);
        client.expect("test-filter");
    } else if (scenario == "consumed-escape") {
        bool candidateOpen = true;
        engine->call<ITestIM::setHandler>([&](const auto &, KeyEvent &event) {
            if (candidateOpen && event.key().check(FcitxKey_Escape) &&
                !event.isRelease()) {
                candidateOpen = false;
                event.filterAndAccept();
            }
        });
        client.select("test-secondary");
        require(client.key(FcitxKey_Escape),
                "candidate should consume the first Escape");
        client.expect("test-secondary");
        client.key(FcitxKey_Escape, true);
        client.expect("test-secondary");
        client.pass(FcitxKey_Escape);
        client.expect("keyboard-us");
        client.pass(FcitxKey_A);
        client.expect("test-secondary");
        engine->call<ITestIM::setHandler>(nullptr);
    } else if (scenario == "overrides") {
        client.select("test-primary");
        client.pass(FcitxKey_Escape);
        client.select("test-secondary");
        client.select("keyboard-us");
        client.pass(FcitxKey_i);
        client.expect("keyboard-us");
        RawConfig config;
        config["EnglishInputMethod"] = "test-command";
        config["ExcludedPrograms/0"] = "browser";
        addon->setConfig(config);
        client.select("test-secondary");
        client.pass(FcitxKey_Escape);
        client.expect("test-command");
        client.pass(FcitxKey_o);
        client.expect("test-secondary");
        Client excluded(instance, *frontend, "browser");
        excluded.select("test-primary");
        excluded.pass(FcitxKey_Escape);
        excluded.expect("test-primary");
        config["EnglishInputMethod"] = "missing";
        addon->setConfig(config);
        client.select("test-secondary");
        client.pass(FcitxKey_Escape);
        client.expect("test-secondary");
    } else if (scenario == "contexts") {
        instance.inputContextManager().setPropertyPropagatePolicy(
            PropertyPropagatePolicy::No);
        client.select("test-primary");
        client.pass(FcitxKey_Escape);
        client.context()->focusOut();
        Client other(instance, *frontend, "editor");
        other.select("test-secondary");
        other.pass(FcitxKey_Escape);
        other.context()->focusOut();
        client.pass(FcitxKey_i);
        client.expect("test-primary");
        client.context()->focusOut();
        other.pass(FcitxKey_R);
        other.expect("test-secondary");
    } else if (scenario == "composition") {
        auto composeAndExit = [&] {
            client.expect(composition.inputMethod);
            frontend->call<ITestFrontend::pushCommitExpectation>(
                composition.text);
            for (unsigned char key : composition.keys) {
                require(client.key(static_cast<KeySym>(key)),
                        "the engine did not handle a composition keystroke");
            }
            client.pass(FcitxKey_Escape);
            client.expect("keyboard-us");
        };
        client.select(composition.inputMethod);
        composeAndExit();
        client.pass(FcitxKey_h);
        client.pass(FcitxKey_i);
        composeAndExit();
    } else {
        throw std::runtime_error("unknown test scenario " + scenario);
    }
    return 0;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2 || argc != (std::string(argv[1]) == "composition" ? 9 : 5)) {
        std::cerr << "usage: test-integration SCENARIO MODULE_DIR METADATA_DIR "
                     "FIXTURE_DIR [ADDON IM KEYS TEXT]\n";
        return 1;
    }
    try {
        using namespace fcitx;
#if __has_include(<fcitx-utils/standardpaths.h>)
        auto dataDir = StandardPaths::fcitxPath("pkgdatadir").string();
#else
        auto dataDir = StandardPath::fcitxPath("pkgdatadir");
#endif
        setupTestingEnvironment("", {argv[2]}, {argv[3], argv[4], dataDir});
        char program[] = "test-integration";
        char disabled[] = "--disable=all";
        std::string enabled = "--enable=testfrontend,testim,viescape";
        if (std::string_view(argv[1]) == "filtered-escape") {
            enabled += ",escapefilter";
        }
        CompositionCase composition;
        if (argc == 9) {
            composition = {argv[5], argv[6], argv[7], argv[8]};
            require(!composition.keys.empty() && !composition.text.empty(),
                    "composition keystrokes and expected text are required");
            enabled += "," + composition.addon;
        }
        char *args[] = {program, disabled, enabled.data(), nullptr};
        EscapeFilterFactory filterFactory;
        StaticAddonRegistry registry{{"escapefilter", &filterFactory}};
        Instance instance(3, args);
        instance.addonManager().registerDefaultLoader(&registry);
        instance.initialize();
        return run(argv[1], instance, composition);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
