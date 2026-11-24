#include "commands.h"
#include <algorithm>
#include <fcitx-config/iniparser.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/instance.h>
#include <utility>

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
    char pendingCommit = 0;
    // A switch invalidates pending restores in contexts sharing its IM state.
    // The saved IM and the command sequence belong to their original context.
    bool needCopy() const override { return true; }
    void copyTo(InputContextProperty *other) override {
        static_cast<ContextState *>(other)->clear();
    }
    void clear() {
        saved.clear();
        commands = {};
        pendingCommit = 0;
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
        commit_ = instance_.connect<Instance::CommitFilter>(
            [this](InputContext *ic, const std::string &text) {
                auto *state = ic->propertyFor(&factory_);
                const char expected = std::exchange(state->pendingCommit, 0);
                if (expected && text.size() == 1 && text.front() == expected &&
                    ic->hasFocus() &&
                    instance_.inputMethod(ic) == *settings_.english) {
                    restore(ic, *state);
                }
            });
        switched_ =
            instance_.watchEvent<EventType::InputContextSwitchInputMethod>(
                EventWatcherPhase::PostInputMethod, [this](const auto &event) {
                    auto *ic = event.inputContext();
                    ic->propertyFor(&factory_)->clear();
                    ic->updateProperty(&factory_);
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

    void restore(InputContext *ic, ContextState &state) {
        const auto saved = state.saved;
        state.clear();
        instance_.setCurrentInputMethod(ic, saved, true);
    }

    void handleKey(const KeyEvent &event) {
        auto *ic = event.inputContext();
        auto *state = ic->propertyFor(&factory_);
        state->pendingCommit = 0;
        if (event.isRelease() || event.accepted() || !ic->hasFocus() ||
            excluded(ic)) {
            return;
        }
        // filterKey can stop dispatch with filter() while still passing the
        // Escape to the client. Only accept() marks it as consumed.
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
        } else if (!event.filtered() && !state->saved.empty()) {
            if (instance_.inputMethod(ic) != *settings_.english) {
                state->clear();
            } else if (state->commands.entersInsert(key)) {
                // Fcitx uses the active layout again after KeyEventResult to
                // commit a translated character. Switch once it has that text.
                if (event.forward() &&
                    event.rawKey().sym() != event.origKey().sym()) {
                    state->pendingCommit = static_cast<char>(key.sym());
                } else {
                    restore(ic, *state);
                }
            }
        }
    }

    Instance &instance_;
    Settings settings_;
    SimpleInputContextPropertyFactory<ContextState> factory_;
    ScopedConnection result_;
    ScopedConnection commit_;
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
