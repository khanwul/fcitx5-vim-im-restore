#include "commands.h"
#include <algorithm>
#include <fcitx-config/iniparser.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/instance.h>

namespace {
using namespace fcitx;
constexpr char configPath[] = "conf/viescape.conf";

class Settings final : public Configuration {
public:
    const char *typeName() const override { return "ViEscapeConfig"; }
    Option<std::string> english{this, "EnglishInputMethod",
                                "English input method ID", "keyboard-us"};
    Option<std::vector<std::string>> excluded{
        this, "ExcludedPrograms", "Excluded program IDs", {}};
};

struct ContextState final : InputContextProperty {
    std::string saved;
    viescape::Commands commands;
    void clear() {
        saved.clear();
        commands = {};
    }
};

class ViEscape final : public AddonInstance {
public:
    explicit ViEscape(Instance &instance) : instance_(instance) {
        instance_.inputContextManager().registerProperty("viescape-state",
                                                         &factory_);
        readAsIni(settings_, configPath);
        result_ = instance_.connect<Instance::KeyEventResult>(
            [this](const KeyEvent &event) { handleKey(event); });
        switched_ =
            instance_.watchEvent<EventType::InputContextSwitchInputMethod>(
                EventWatcherPhase::PostInputMethod, [this](const auto &event) {
                    event.inputContext()->propertyFor(&factory_)->clear();
                });
    }

    const Configuration *getConfig() const override { return &settings_; }

    void reloadConfig() override {
        RawConfig raw;
        readAsIni(raw, configPath);
        applyConfig(raw, false);
    }

    void setConfig(const RawConfig &raw) override {
        applyConfig(raw, true);
        safeSaveAsIni(settings_, configPath);
    }

private:
    void applyConfig(const RawConfig &raw, bool partial) {
        const auto oldEnglish = *settings_.english;
        settings_.load(raw, partial);
        instance_.inputContextManager().foreach ([&](InputContext *ic) {
            if (oldEnglish != *settings_.english || excluded(ic)) {
                ic->propertyFor(&factory_)->clear();
            }
            return true;
        });
    }

    bool excluded(InputContext *ic) const {
        const auto &programs = *settings_.excluded;
        return std::find(programs.begin(), programs.end(), ic->program()) !=
               programs.end();
    }

    void handleKey(const KeyEvent &event) {
        auto *ic = event.inputContext();
        if (event.isRelease() || event.accepted() || event.filtered() ||
            !ic->hasFocus() || excluded(ic)) {
            return;
        }
        // This signal runs after the active engine's keyEvent and filterKey.
        // The frontend still delivers the original unconsumed command once.
        auto *state = ic->propertyFor(&factory_);
        const auto key = event.key();
        if (key.check(FcitxKey_Escape)) {
            state->commands = {};
            const auto current = instance_.inputMethod(ic);
            const auto &english = *settings_.english;
            if (current.empty() || current == english) {
                return;
            }
            state->clear();
            instance_.setCurrentInputMethod(ic, english, true);
            // Switching synchronously emits InputContextSwitchInputMethod.
            // Arm restoration only after the switch actually succeeded.
            if (instance_.inputMethod(ic) == english) {
                state->saved = current;
            }
        } else if (!state->saved.empty()) {
            if (instance_.inputMethod(ic) != *settings_.english) {
                state->clear();
            } else if (state->commands.entersInsert(key)) {
                const auto restore = state->saved;
                state->clear();
                instance_.setCurrentInputMethod(ic, restore, true);
            }
        }
    }

    Instance &instance_;
    Settings settings_;
    SimpleInputContextPropertyFactory<ContextState> factory_;
    ScopedConnection result_;
    std::unique_ptr<HandlerTableEntry<EventHandler>> switched_;
};

class Factory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new ViEscape(*manager->instance());
    }
};
} // namespace

FCITX_ADDON_FACTORY(Factory)
