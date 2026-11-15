// Settings (phase 13): the file, applying it, the interface's size, the
// settings screen (from the title and the pause menu) and the Controls panel.
// The rules are core/Settings' and core/Bindings'; this is where they meet
// the window, the mixer and the keyboard.
#include "core/GameShared.h"

#include <cstdlib>

#include <stb/stb_image.h>

using namespace gameshared;

namespace
{
enum SettingsTab
{
    kTabSound,
    kTabScreen,
    kTabGame,
    kTabKeys,
    kTabCount
};
constexpr std::array<const char*, kTabCount> kTabNames = {"SOUND", "SCREEN", "GAME", "KEYS"};
constexpr std::array<int, kTabCount> kTabRows = {4, 6, 6, kRebindableCount + 1};
constexpr float kRowH = 30.0f;
constexpr float kNoteSeconds = 4.0f;

template <typename E>
E stepEnum(E value, int count, int delta)
{
    return static_cast<E>(((static_cast<int>(value) + delta) % count + count) % count);
}

std::string onOff(bool on)
{
    return on ? "On" : "Off";
}

std::string capital(const char* text)
{
    std::string s = text;
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = static_cast<char>(s[0] - 'a' + 'A');
    return s;
}

struct SettingsLayout
{
    glm::vec2 pos{0.0f};
    glm::vec2 size{0.0f};
    float tabsY = 0.0f;
    float rowsY = 0.0f;
    int visibleRows = 0;
};

SettingsLayout settingsLayout(int windowWidth, int windowHeight)
{
    SettingsLayout l;
    l.size = glm::vec2(std::min(720.0f, static_cast<float>(windowWidth) - 40.0f),
                       std::min(600.0f, static_cast<float>(windowHeight) - 40.0f));
    l.pos = glm::vec2(std::round((static_cast<float>(windowWidth) - l.size.x) * 0.5f),
                      std::round((static_cast<float>(windowHeight) - l.size.y) * 0.5f));
    l.tabsY = l.pos.y + 70.0f;
    l.rowsY = l.tabsY + 46.0f;
    l.visibleRows = std::max(3, static_cast<int>((l.pos.y + l.size.y - 70.0f - l.rowsY) / kRowH));
    return l;
}

// The Controls panel (F2), by situation (phase 13). A key is an action's
// binding, or a fixed name; "Hold " goes before it where holding matters.
enum class ControlKey
{
    Action,
    Move,   // the four movement keys
    Belt,   // the eight belt keys
    Zoom,   // minimap in and out
    Fixed
};

struct ControlRow
{
    ControlKey kind;
    Action action;
    const char* fixed; // the key's name when kind is Fixed
    bool hold;
    const char* text;
};

struct ControlSection
{
    const char* title;
    std::vector<ControlRow> rows;
};

const std::array<ControlSection, 5>& controlSections()
{
    static const std::array<ControlSection, 5> kSections = {{
        {"MOVING",
         {{ControlKey::Move, Action::Count, "", false, "Move; steer a vehicle"},
          {ControlKey::Action, Action::Run, "", false, "Run"},
          {ControlKey::Action, Action::Crouch, "", false, "Crouch; still: breathe"},
          {ControlKey::Action, Action::Crouch, "", false, "By a door, still: listen"},
          {ControlKey::Action, Action::Vehicle, "", false, "Get in, out of a vehicle"}}},
        {"HANDS",
         {{ControlKey::Action, Action::Use, "", false, "Use, open, search, refuel"},
          {ControlKey::Action, Action::Use, "", true, "Push (walk into it)"},
          {ControlKey::Action, Action::Board, "", false, "Board, tap, fill, reload"},
          {ControlKey::Action, Action::Pry, "", false, "Pry, break up, force"},
          {ControlKey::Action, Action::UseItem, "", false, "Use item; set down"},
          {ControlKey::Action, Action::Flashlight, "", false, "Flashlight; headlights"},
          {ControlKey::Belt, Action::Count, "", false, "Belt slot"},
          {ControlKey::Action, Action::Drop, "", false, "Drop (Shift: the stack)"}}},
        {"FIGHTING",
         {{ControlKey::Fixed, Action::Count, "Mouse", false, "Aim"},
          {ControlKey::Action, Action::Attack, "", false, "Attack, fire"},
          {ControlKey::Action, Action::TakeDown, "", false, "Take down from behind"},
          {ControlKey::Action, Action::Shove, "", false, "Shove; struggle free"},
          {ControlKey::Action, Action::Throw, "", true, "Aim a throw, let go"}}},
        {"HOME",
         {{ControlKey::Action, Action::Pack, "", false, "Pack, body"},
          {ControlKey::Action, Action::Make, "", false, "Make things"},
          {ControlKey::Action, Action::Build, "", false, "Build wheel"},
          {ControlKey::Fixed, Action::Count, "Arrows", false, "Pack cursor"},
          {ControlKey::Fixed, Action::Count, "Enter", false, "Swap to the belt"}}},
        {"LOOKING",
         {{ControlKey::Action, Action::Map, "", false, "Town map"},
          {ControlKey::Action, Action::Notebook, "", false, "Notebook (Left/Right)"},
          {ControlKey::Zoom, Action::Count, "", false, "Minimap zoom"},
          {ControlKey::Fixed, Action::Count, "Esc", false, "Pause, back"},
          {ControlKey::Action, Action::Controls, "", false, "Controls"},
          {ControlKey::Action, Action::DebugNoise, "", false, "Noise (dev)"},
          {ControlKey::Action, Action::DebugDistrict, "", false, "Infection (dev)"},
          {ControlKey::Action, Action::DebugDespawn, "", false, "Clear the dead (dev)"}}},
    }};
    return kSections;
}

bool devAction(Action action)
{
    return action == Action::DebugNoise || action == Action::DebugDistrict || action == Action::DebugDespawn;
}

std::string controlKeyLabel(const ControlRow& row, const bindings::Table& table)
{
    const bindings::Table defaults = bindings::defaults();
    std::string key;
    switch (row.kind)
    {
    case ControlKey::Action: key = bindings::label(table, row.action); break;
    case ControlKey::Fixed: key = row.fixed; break;
    case ControlKey::Move:
    {
        bool same = true;
        for (Action a : {Action::MoveUp, Action::MoveLeft, Action::MoveDown, Action::MoveRight})
        {
            same = same && table.codes[static_cast<std::size_t>(a)] == defaults.codes[static_cast<std::size_t>(a)];
        }
        key = same ? "WASD"
                   : bindings::label(table, Action::MoveUp) + bindings::label(table, Action::MoveLeft) +
                         bindings::label(table, Action::MoveDown) + bindings::label(table, Action::MoveRight);
        break;
    }
    case ControlKey::Belt:
    {
        bool same = true;
        for (int i = 0; i < 8; ++i)
        {
            const std::size_t a = static_cast<std::size_t>(Action::Belt1) + static_cast<std::size_t>(i);
            same = same && table.codes[a] == defaults.codes[a];
        }
        key = same ? "1-8" : bindings::label(table, Action::Belt1) + ".." + bindings::label(table, Action::Belt8);
        break;
    }
    case ControlKey::Zoom: key = bindings::label(table, Action::ZoomIn) + "/" + bindings::label(table, Action::ZoomOut); break;
    }
    return (row.hold ? "Hold " : "") + key;
}

struct ControlsGeometry
{
    glm::vec2 pos{0.0f};
    glm::vec2 size{0.0f};
    float columnW = 0.0f;
    float rowsY = 0.0f;
    // Rows at text size 2; on a short interface (a large interface size on a
    // small window) at text size 1, so the panel still fits.
    float row = 22.0f;
    float keyColumn = 136.0f;
    float text = 2.0f;
    static constexpr float kHead = 30.0f;
};

// Moving, Hands and Fighting on the left; Home and Looking on the right.
constexpr int kLeftSections = 3;

ControlsGeometry controlsGeometry(int windowWidth, int windowHeight, bool dev)
{
    ControlsGeometry g;
    const auto& sections = controlSections();
    const auto tallest = [&](float rowH)
    {
        float left = 0.0f;
        float right = 0.0f;
        for (int s = 0; s < static_cast<int>(sections.size()); ++s)
        {
            int rows = 0;
            for (const ControlRow& row : sections[static_cast<std::size_t>(s)].rows)
            {
                if (row.kind == ControlKey::Action && devAction(row.action) && !dev) continue;
                ++rows;
            }
            const float h = ControlsGeometry::kHead + static_cast<float>(rows) * rowH + 8.0f;
            (s < kLeftSections ? left : right) += h;
        }
        return std::max(left, right);
    };
    float columns = tallest(g.row);
    if (64.0f + columns + 26.0f > static_cast<float>(windowHeight) - 8.0f)
    {
        g.row = 13.0f;
        g.keyColumn = 72.0f;
        g.text = 1.0f;
        columns = tallest(g.row);
    }
    g.columnW = std::min(480.0f, std::floor((static_cast<float>(windowWidth) - 64.0f) * 0.5f));
    g.size = glm::vec2(g.columnW * 2.0f + 48.0f, 64.0f + columns + 26.0f);
    g.pos = glm::vec2(std::round((static_cast<float>(windowWidth) - g.size.x) * 0.5f),
                      std::round(std::max(4.0f, (static_cast<float>(windowHeight) - g.size.y) * 0.5f)));
    g.rowsY = g.pos.y + 64.0f;
    return g;
}
} // namespace

// ---------------------------------------------------------------------------
// The file

bool Game::devKeys()
{
    return !kPackagedBuild;
}

void Game::loadSettings()
{
    // A packaged build keeps its saves in the user's folder, unless a
    // portable.txt beside the game says to keep them here (phase 13).
    std::string root = "save";
    if (kPackagedBuild && !std::filesystem::exists(getExecutableDirectory() / "portable.txt"))
    {
        if (const char* appData = std::getenv("APPDATA"); appData != nullptr && appData[0] != '\0')
        {
            root = (std::filesystem::path(appData) / "DeadPixelSurvival").string();
        }
    }
    SaveManager::setSaveRoot(root);
    mSettingsPath = (std::filesystem::path(root) / "settings.json").string();
    mSettings = mHarness ? settings::Settings{} : settings::load(mSettingsPath);
    mTeachAllowed = !mHarness; // a script turns teaching on with `teach on`
    mAutosaveAllowed = !mHarness; // ... and the extra checkpoints with `autosave on`
    mDemo = mLaunchOptions.demo || std::filesystem::exists(getExecutableDirectory() / "demo.txt");
    if (mDemo) std::cout << "[Demo] a life ends at dawn on day 7\n";
}

void Game::applySettings()
{
    mAudioManager.setVolume(AudioManager::Bus::Master, settings::volume(mSettings.master));
    mAudioManager.setVolume(AudioManager::Bus::Effects, settings::volume(mSettings.effects));
    mAudioManager.setVolume(AudioManager::Bus::Ambience, settings::volume(mSettings.ambience));
    mAudioManager.setVolume(AudioManager::Bus::Music, settings::volume(mSettings.music));
    // The harness sets its own screen effects (`fx`), off by default.
    if (!mHarness) mScreenEffects = settings::screenFxLevel(mSettings.screenFx);
    mShakeScale = settings::shakeScale(mSettings.shake);
    mInput.setBindings(mSettings.keys);
    mInput.setToggle(Action::Crouch, mSettings.crouchToggle);
    mInput.setToggle(Action::Run, mSettings.runToggle);
    // Press once: the holds (push, hot-wire, aim a throw) run on after one
    // press and stop with the next (search, board and treat already do).
    // Push and hot-wire read mSettings.pressOnce where E is held; the
    // throw's aim is a plain switch.
    mInput.setToggle(Action::Throw, mSettings.pressOnce);
    onPixelSize(mPixelWidth, mPixelHeight); // the interface size
}

void Game::saveSettings()
{
    if (!settings::save(mSettingsPath, mSettings))
    {
        std::cerr << "Could not write " << mSettingsPath << "\n";
    }
}

void Game::applyWindowMode()
{
    if (mWindow == nullptr || mHarness) return; // a script's window stays 1280x720
    const int display = std::max(0, SDL_GetWindowDisplayIndex(mWindow));
    SDL_DisplayMode desktop{};
    SDL_GetDesktopDisplayMode(display, &desktop);
    const auto [rw, rh] = settings::resolutionSize(mSettings.resolution);
    const int w = rw > 0 ? rw : desktop.w;
    const int h = rh > 0 ? rh : desktop.h;
    switch (mSettings.fullscreen)
    {
    case settings::Fullscreen::Off:
        SDL_SetWindowFullscreen(mWindow, 0);
        if (rw > 0)
        {
            SDL_RestoreWindow(mWindow);
            SDL_SetWindowSize(mWindow, w, h);
            SDL_SetWindowPosition(mWindow, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display));
        }
        else
        {
            SDL_MaximizeWindow(mWindow); // native, windowed: as big as the desktop allows
        }
        break;
    case settings::Fullscreen::Borderless:
        SDL_SetWindowFullscreen(mWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
        break;
    case settings::Fullscreen::Exclusive:
    {
        SDL_DisplayMode want{};
        want.w = w;
        want.h = h;
        SDL_DisplayMode got{};
        if (SDL_GetClosestDisplayMode(display, &want, &got) != nullptr) SDL_SetWindowDisplayMode(mWindow, &got);
        SDL_SetWindowFullscreen(mWindow, SDL_WINDOW_FULLSCREEN);
        break;
    }
    }
    SDL_GL_SetSwapInterval(mSettings.vsync ? 1 : 0);
    int pw = 0;
    int ph = 0;
    SDL_GL_GetDrawableSize(mWindow, &pw, &ph);
    if (pw > 0 && ph > 0) onPixelSize(pw, ph);
}

void Game::onPixelSize(int width, int height)
{
    mPixelWidth = std::max(1, width);
    mPixelHeight = std::max(1, height);
    mUiScale = settings::uiScaleFor(mSettings.uiSize, mPixelHeight);
    mWindowWidth = std::max(1, static_cast<int>(std::lround(static_cast<float>(mPixelWidth) / mUiScale)));
    mWindowHeight = std::max(1, static_cast<int>(std::lround(static_cast<float>(mPixelHeight) / mUiScale)));
    mProjection = buildProjection(mPixelWidth, mPixelHeight);
    mCamera.setViewportSize(static_cast<float>(mPixelWidth), static_cast<float>(mPixelHeight));
    if (mTileMap.width() > 0) mCamera.clampToBounds(mTileMap.pixelSize());
}

glm::vec2 Game::uiAt(const glm::vec2& world) const
{
    return mCamera.worldToScreen(world) / mUiScale;
}

float Game::uiZoom() const
{
    return mCamera.zoom() / mUiScale;
}

glm::vec2 Game::uiMouse() const
{
    return glm::vec2(static_cast<float>(mInput.mouseX()), static_cast<float>(mInput.mouseY())) / mUiScale;
}

double Game::hoursPerSecond() const
{
    return daylength::hoursPerSecond(mSaveManager.presentationSettings().dayMinutes);
}

// ---------------------------------------------------------------------------
// The settings screen

void Game::openSettings()
{
    mShowSettings = true;
    mShowControls = false;
    mSettingsRow = -1;
    mSettingsScroll = 0;
    mRebinding = -1;
    mSettingsNote.clear();
}

int Game::settingsRows() const
{
    return kTabRows[static_cast<std::size_t>(std::clamp(mSettingsTab, 0, kTabCount - 1))];
}

void Game::settingsChange(int delta)
{
    settings::Settings& s = mSettings;
    const auto level = [&](int& value) { value = std::clamp(value + delta, 0, settings::kLevelMax); };
    bool windowChanged = false;
    switch (mSettingsTab)
    {
    case kTabSound:
        switch (mSettingsRow)
        {
        case 0: level(s.master); break;
        case 1: level(s.effects); break;
        case 2: level(s.ambience); break;
        case 3: level(s.music); break;
        default: return;
        }
        break;
    case kTabScreen:
        switch (mSettingsRow)
        {
        case 0:
            s.fullscreen = stepEnum(s.fullscreen, 3, delta);
            windowChanged = true;
            break;
        case 1:
            s.resolution = stepEnum(s.resolution, 4, delta);
            windowChanged = true;
            break;
        case 2:
            s.vsync = !s.vsync;
            windowChanged = true;
            break;
        case 3: s.uiSize = stepEnum(s.uiSize, 4, delta); break;
        case 4:
            s.screenFx = stepEnum(s.screenFx, 3, delta);
            mScreenEffects = settings::screenFxLevel(s.screenFx); // the harness too: it was asked for
            break;
        case 5: level(s.shake); break;
        default: return;
        }
        break;
    case kTabGame:
        switch (mSettingsRow)
        {
        case 0: s.autoPause = !s.autoPause; break;
        case 1: s.crouchToggle = !s.crouchToggle; break;
        case 2: s.runToggle = !s.runToggle; break;
        case 3: s.teaching = !s.teaching; break;
        case 4: s.pressOnce = !s.pressOnce; break;
        default: return; // day length is the world's
        }
        break;
    default: return;
    }
    applySettings();
    if (windowChanged) applyWindowMode();
    saveSettings();
}

void Game::updateSettingsScreen(float dtSeconds)
{
    mSettingsNoteTimer = std::max(0.0f, mSettingsNoteTimer - dtSeconds);

    // Waiting for a key: the next key or button pressed is the binding.
    if (mRebinding >= 0)
    {
        const int code = mInput.codePressed();
        if (code == bindings::kNone) return;
        const Action action = static_cast<Action>(mRebinding);
        const bindings::RebindResult result = bindings::rebind(mSettings.keys, action, code);
        mRebinding = -1;
        if (!result.ok)
        {
            mSettingsNote = bindings::codeLabel(code) + " stays the menus' key.";
        }
        else
        {
            mSettingsNote = std::string(bindings::actionLabel(action)) + " is " + bindings::label(mSettings.keys, action) + " now.";
            if (result.swappedWith)
            {
                mSettingsNote += std::string(" ") + bindings::actionLabel(*result.swappedWith) + " took " +
                                 bindings::label(mSettings.keys, *result.swappedWith) + ".";
            }
            applySettings();
            saveSettings();
            std::cout << "[Settings] " << mSettingsNote << '\n';
        }
        mSettingsNoteTimer = kNoteSeconds;
        return;
    }

    const int rows = settingsRows();
    const bool up = mInput.pressed(Action::UiUp) || mInput.pressed(Action::MoveUp);
    const bool down = mInput.pressed(Action::UiDown) || mInput.pressed(Action::MoveDown);
    const bool left = mInput.pressed(Action::UiLeft) || mInput.pressed(Action::MoveLeft);
    const bool right = mInput.pressed(Action::UiRight) || mInput.pressed(Action::MoveRight);
    const bool confirm = mInput.pressed(Action::UiConfirm);
    if (up) mSettingsRow = mSettingsRow == -1 ? rows - 1 : mSettingsRow - 1; // up from the tabs: the last row
    if (down) mSettingsRow = std::min(rows - 1, mSettingsRow + 1);

    // The mouse: a tab, a row, a click on the chosen row changes it.
    const SettingsLayout layout = settingsLayout(mWindowWidth, mWindowHeight);
    if (mInput.pressed(Action::UiClick))
    {
        const glm::vec2 m = uiMouse();
        if (m.y >= layout.tabsY - 6.0f && m.y < layout.tabsY + 30.0f)
        {
            const float tabW = (layout.size.x - 48.0f) / static_cast<float>(kTabCount);
            const int tab = static_cast<int>((m.x - layout.pos.x - 24.0f) / tabW);
            if (tab >= 0 && tab < kTabCount)
            {
                mSettingsTab = tab;
                mSettingsRow = -1;
                mSettingsScroll = 0;
            }
        }
        else if (m.y >= layout.rowsY - 6.0f && m.x >= layout.pos.x && m.x < layout.pos.x + layout.size.x)
        {
            const int row = mSettingsScroll + static_cast<int>((m.y - layout.rowsY + 6.0f) / kRowH);
            if (row >= 0 && row < rows && row - mSettingsScroll < layout.visibleRows)
            {
                if (row == mSettingsRow)
                {
                    if (mSettingsTab == kTabKeys) mRebinding = row < kRebindableCount ? row : -1;
                    else settingsChange(1);
                    if (mSettingsTab == kTabKeys && row == kRebindableCount)
                    {
                        mSettings.keys = bindings::defaults();
                        applySettings();
                        saveSettings();
                        mSettingsNote = "Keys reset.";
                        mSettingsNoteTimer = kNoteSeconds;
                    }
                }
                mSettingsRow = row;
            }
        }
    }

    if (mSettingsRow < 0)
    {
        if (left) mSettingsTab = (mSettingsTab + kTabCount - 1) % kTabCount;
        if (right) mSettingsTab = (mSettingsTab + 1) % kTabCount;
        if (left || right) mSettingsScroll = 0;
        if (confirm) mSettingsRow = 0;
    }
    else if (mSettingsTab == kTabKeys)
    {
        if (confirm && mSettingsRow < kRebindableCount)
        {
            mRebinding = mSettingsRow;
        }
        else if (confirm)
        {
            mSettings.keys = bindings::defaults();
            applySettings();
            saveSettings();
            mSettingsNote = "Keys reset.";
            mSettingsNoteTimer = kNoteSeconds;
        }
    }
    else
    {
        if (left) settingsChange(-1);
        if (right || confirm) settingsChange(1);
    }

    // Keep the chosen row in sight.
    if (mSettingsRow >= 0)
    {
        if (mSettingsRow < mSettingsScroll) mSettingsScroll = mSettingsRow;
        if (mSettingsRow >= mSettingsScroll + layout.visibleRows) mSettingsScroll = mSettingsRow - layout.visibleRows + 1;
    }
    else
    {
        mSettingsScroll = 0;
    }
}

void Game::renderSettingsShapes()
{
    if (!mShowSettings) return;
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    mSpriteBatch.draw(glm::vec2(0.0f), glm::vec2(w, h), uiUv, glm::vec4(0.0f, 0.0f, 0.0f, 0.55f));
    const SettingsLayout l = settingsLayout(mWindowWidth, mWindowHeight);
    mSpriteBatch.draw(l.pos, l.size, uiUv, glm::vec4(kPanelBack.r, kPanelBack.g, kPanelBack.b, 1.0f));
    const float left = l.pos.x + 24.0f;
    const float right = l.pos.x + l.size.x - 24.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(left, l.pos.y + 20.0f), "SETTINGS", kPanelInk, 3.0f);
    mFont.drawText(mSpriteBatch, glm::vec2(right, l.pos.y + 28.0f), std::string("version ") + kGameVersion, kPanelDim, 1.0f,
                   TextAlign::Right);

    // The tabs: the chosen one inverted; when the cursor is on them, amber.
    const float tabW = (l.size.x - 48.0f) / static_cast<float>(kTabCount);
    for (int t = 0; t < kTabCount; ++t)
    {
        const float x = left + tabW * static_cast<float>(t);
        const bool chosen = t == mSettingsTab;
        if (chosen)
        {
            mSpriteBatch.draw(glm::vec2(x - 6.0f, l.tabsY - 6.0f), glm::vec2(tabW - 8.0f, 28.0f), uiUv,
                              mSettingsRow < 0 ? kPanelInk : kPanelDim);
        }
        mFont.drawText(mSpriteBatch, glm::vec2(x, l.tabsY), kTabNames[static_cast<std::size_t>(t)],
                       chosen ? kPanelOnInk : kPanelDim, 2.0f);
    }
    mSpriteBatch.draw(glm::vec2(left, l.tabsY + 32.0f), glm::vec2(right - left, 1.0f), uiUv, kPanelRule);

    // The rows: the name at the left, the value at the right.
    const auto row = [&](int index, const std::string& name, const std::string& value, bool readOnly)
    {
        const int shown = index - mSettingsScroll;
        if (shown < 0 || shown >= l.visibleRows) return;
        const float y = l.rowsY + static_cast<float>(shown) * kRowH;
        const bool chosen = index == mSettingsRow;
        if (chosen) mSpriteBatch.draw(glm::vec2(left - 8.0f, y - 6.0f), glm::vec2(right - left + 16.0f, kRowH - 2.0f), uiUv, kPanelInk);
        const glm::vec4 ink = chosen ? kPanelOnInk : (readOnly ? kPanelDim : kPanelInk);
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), name, ink, 2.0f);
        const std::string shownValue = chosen && !readOnly && mSettingsTab != kTabKeys ? "< " + value + " >" : value;
        mFont.drawText(mSpriteBatch, glm::vec2(right, y), shownValue, chosen ? kPanelOnInk : (readOnly ? kPanelDim : kPanelAmber),
                       2.0f, TextAlign::Right);
    };
    const settings::Settings& s = mSettings;
    const auto levelText = [](int v) { return std::to_string(v); };
    switch (mSettingsTab)
    {
    case kTabSound:
        row(0, "Master", levelText(s.master), false);
        row(1, "Effects", levelText(s.effects), false);
        row(2, "Ambience", levelText(s.ambience), false);
        row(3, "Music", levelText(s.music), false);
        break;
    case kTabScreen:
        row(0, "Fullscreen", capital(settings::name(s.fullscreen)), false);
        row(1, "Resolution", capital(settings::name(s.resolution)), false);
        row(2, "VSync", onOff(s.vsync), false);
        row(3, "Interface size",
            s.uiSize == settings::UiSize::Auto
                ? "Auto (" + std::string(mUiScale >= 1.99f ? "2x" : mUiScale >= 1.49f ? "1.5x" : "1x") + ")"
                : std::string(settings::name(s.uiSize)),
            false);
        row(4, "Screen effects", capital(settings::name(s.screenFx)), false);
        row(5, "Screen shake", levelText(s.shake), false);
        break;
    case kTabGame:
    {
        row(0, "Pause when the window loses focus", onOff(s.autoPause), false);
        row(1, "Crouch", s.crouchToggle ? "Toggle" : "Hold", false);
        row(2, "Run", s.runToggle ? "Toggle" : "Hold", false);
        row(3, "Teaching prompts", onOff(s.teaching), false);
        row(4, "Hold to act", s.pressOnce ? "Press once" : "Hold", false);
        const int minutes = mSaveManager.presentationSettings().dayMinutes;
        row(5, "Day length (this world)", std::to_string(minutes) + " min", true);
        break;
    }
    case kTabKeys:
        for (int i = 0; i < kRebindableCount; ++i)
        {
            const Action action = static_cast<Action>(i);
            std::string value = i == mRebinding ? "Press a key" : bindings::label(s.keys, action, true);
            row(i, bindings::actionLabel(action), value, false);
        }
        row(kRebindableCount, "Reset keys", s.keys == bindings::defaults() ? "" : "Enter", false);
        break;
    default: break;
    }
    // More above or below.
    if (mSettingsScroll > 0)
    {
        mFont.drawText(mSpriteBatch, glm::vec2(right, l.rowsY - 22.0f), "more above", kPanelDim, 1.0f, TextAlign::Right);
    }
    if (mSettingsScroll + l.visibleRows < settingsRows())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(right, l.rowsY + static_cast<float>(l.visibleRows) * kRowH - 4.0f),
                       "more below", kPanelDim, 1.0f, TextAlign::Right);
    }

    const float footY = l.pos.y + l.size.y - 28.0f;
    mSpriteBatch.draw(glm::vec2(left, footY - 30.0f), glm::vec2(right - left, 1.0f), uiUv, kPanelRule);
    if (!mSettingsNote.empty() && mSettingsNoteTimer > 0.0f)
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, footY - 22.0f), mSettingsNote, kPanelAmber, 1.0f);
    }
    std::string hint;
    if (mRebinding >= 0) hint = "Press the new key or mouse button   Esc: keep the old one";
    else if (mSettingsRow < 0) hint = "Left/Right: group   Down: settings   Esc: back";
    else if (mSettingsTab == kTabKeys) hint = "Up/Down: choose   Enter: rebind   Esc: back";
    else hint = "Up/Down: choose   Left/Right: change   Esc: back";
    mFont.drawText(mSpriteBatch, glm::vec2(left, footY), hint, kPanelDim, 1.0f);
}

// ---------------------------------------------------------------------------
// The Controls panel (F2): by situation, two columns, the keys bound now.

void Game::renderControlsPanel()
{
    if (!mShowControls) return;
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const ControlsGeometry g = controlsGeometry(mWindowWidth, mWindowHeight, devKeys());
    mSpriteBatch.draw(g.pos, g.size, uiUv, glm::vec4(kPanelBack.r, kPanelBack.g, kPanelBack.b, 1.0f));
    mSpriteBatch.draw(glm::vec2(g.pos.x + g.size.x * 0.5f, g.rowsY), glm::vec2(1.0f, g.size.y - 64.0f - 30.0f), uiUv,
                      kPanelRule);
    mFont.drawText(mSpriteBatch, glm::vec2(g.pos.x + g.size.x * 0.5f, g.pos.y + 18.0f), "CONTROLS", kPanelInk, 3.0f,
                   TextAlign::Center);

    const auto& sections = controlSections();
    float y[2] = {g.rowsY, g.rowsY};
    for (int s = 0; s < static_cast<int>(sections.size()); ++s)
    {
        const int column = s < kLeftSections ? 0 : 1;
        const float x = g.pos.x + 24.0f + static_cast<float>(column) * (g.columnW + 8.0f);
        const ControlSection& section = sections[static_cast<std::size_t>(s)];
        mFont.drawText(mSpriteBatch, glm::vec2(x, y[column]), section.title, kPanelDim, 1.0f);
        mSpriteBatch.draw(glm::vec2(x, y[column] + 12.0f), glm::vec2(g.columnW - 24.0f, 1.0f), uiUv, kPanelRule);
        y[column] += ControlsGeometry::kHead - 10.0f;
        for (const ControlRow& row : section.rows)
        {
            if (row.kind == ControlKey::Action && devAction(row.action) && !devKeys()) continue;
            mFont.drawText(mSpriteBatch, glm::vec2(x, y[column]), controlKeyLabel(row, mInput.bindings()), kPanelAmber, g.text);
            mFont.drawText(mSpriteBatch, glm::vec2(x + g.keyColumn, y[column]), row.text, kPanelInk, g.text);
            y[column] += g.row;
        }
        y[column] += 18.0f;
    }
    const std::string foot = "Keys can be changed in Settings, Keys.   " + bindings::label(mInput.bindings(), Action::Controls) +
                             " or Esc: close";
    mFont.drawText(mSpriteBatch, glm::vec2(g.pos.x + g.size.x * 0.5f, g.pos.y + g.size.y - 22.0f), foot, kPanelDim, 1.0f,
                   TextAlign::Center);
}

// ---------------------------------------------------------------------------
// The window's icon (phase 13): a boarded door, drawn by tools/generate_icon.py.

void Game::setWindowIcon(const std::filesystem::path& assetsRoot)
{
    if (mWindow == nullptr) return;
    const std::string path = (assetsRoot / "icons" / "icon_32.png").string();
    int w = 0;
    int h = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (pixels == nullptr) return; // no icon: the system's own
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    if (surface != nullptr)
    {
        SDL_SetWindowIcon(mWindow, surface);
        SDL_FreeSurface(surface);
    }
    stbi_image_free(pixels);
}
