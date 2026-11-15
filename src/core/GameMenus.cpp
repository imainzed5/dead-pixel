// Input events, Escape, and the pause menu. The title is GameTitle.cpp's
// (phase 13), the settings screen GameSettings.cpp's.
#include "core/GameShared.h"

using namespace gameshared;

void Game::processInputEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        mInput.processEvent(event);

        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            int pw = event.window.data1;
            int ph = event.window.data2;
            if (mWindow != nullptr) SDL_GL_GetDrawableSize(mWindow, &pw, &ph);
            onPixelSize(pw, ph);
        }
        // Away from the window: the game waits (phase 13, a setting).
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST && !mHarness &&
            mSettings.autoPause && mRunState == RunState::Playing)
        {
            mRunState = RunState::Paused;
            mPauseSelection = 0;
            mQuitConfirm = false;
        }
    }

    if (!mHarness)
    {
        mInput.refreshMouseState();
    }

    if (mInput.quitRequested())
    {
        mIsRunning = false;
    }

    // Mouse wheel zoom (frame-level: scroll is consumed here, not per tick)
    if (const int scroll = mInput.consumeScroll(); scroll != 0)
    {
        const bool minimapZoomInput =
            mRunState == RunState::Playing &&
            !mShowWorldMap &&
            (mInput.down(Action::UiCtrl));

        if (minimapZoomInput)
        {
            const float zoomFactor = 1.0f + static_cast<float>(scroll) * kMinimapZoomStep;
            mMinimapZoom = std::clamp(mMinimapZoom * zoomFactor, kMinimapZoomMin, kMinimapZoomMax);
        }
        else
        {
            constexpr float kZoomStep = 0.15f;
            const float currentZoom = mCamera.zoom();
            const float newZoom = currentZoom * (1.0f + static_cast<float>(scroll) * kZoomStep);
            const glm::vec2 mouseScreen(static_cast<float>(mInput.mouseX()), static_cast<float>(mInput.mouseY()));
            mCamera.zoomToward(newZoom, mouseScreen);
            mCamera.clampToBounds(mTileMap.pixelSize());
        }
    }
}

void Game::handleEscapeKey()
{
    if (!mInput.pressed(Action::UiBack)) return;

    // Over everything: a key being bound, the settings, the Controls panel.
    if (mShowSettings)
    {
        if (mRebinding >= 0) mRebinding = -1;
        else mShowSettings = false;
        return;
    }
    if (mShowControls)
    {
        mShowControls = false;
        return;
    }

    // While playing, Escape closes the topmost open panel; it only pauses
    // when nothing is open. Panel handlers later in the tick see the same
    // press, but their panels are already closed so it is a no-op.
    if (mRunState == RunState::Playing)
    {
        if (mShowCrafting)
        {
            mShowCrafting = false;
        }
        else if (mContainerOpen)
        {
            mContainerOpen = false;
            mOpenContainerEntity = kInvalidEntity;
        }
        else if (mTalk.open)
        {
            // Out of trade or give first, then the talk.
            if (mTalk.view != TalkState::View::Talk) mTalk.view = TalkState::View::Talk;
            else closeTalk();
        }
        else if (mBuildWheelOpen || mBuildChoice >= 0)
        {
            mBuildWheelOpen = false;
            mBuildChoice = -1;
        }
        else if (mShowWorldMap)
        {
            mShowWorldMap = false;
        }
        else if (mShowNotebook)
        {
            mShowNotebook = false;
        }
        else if (mShowInventory && mBodyChoosing)
        {
            mBodyChoosing = false;
        }
        else if (mShowInventory)
        {
            mShowInventory = false;
        }
        else
        {
            mRunState = RunState::Paused;
            mPauseSelection = 0;
            mQuitConfirm = false;
        }
    }
    else if (mRunState == RunState::Paused)
    {
        if (mQuitConfirm) mQuitConfirm = false;
        else mRunState = RunState::Playing;
    }
    else if (mRunState == RunState::TitleScreen)
    {
        if (mTitleView == TitleView::Town && mTownReading) mTownReading = false;
        else if (mTitleView != TitleView::Main) mTitleView = TitleView::Main;
        else mIsRunning = false;
    }
}

// The pause menu (phase 13): Resume, Save, Settings, Controls, Stay here for
// good (from day ten), Save and quit to title, Quit to desktop (asked once).
std::vector<std::string> Game::pauseOptions() const
{
    std::vector<std::string> options = {"Resume", "Save", "Settings", "Controls"};
    if (mCurrentDay >= legacy::kStayFromDay && mRunState == RunState::Paused)
    {
        options.emplace_back(mStayConfirm == 1   ? "Stay for good? Enter again"
                             : mStayConfirm == 2 ? "It ends this life. Enter once more"
                                                 : "Stay here for good");
    }
    options.emplace_back("Save and quit to title");
    options.emplace_back(mQuitConfirm ? "Quit? Enter again" : "Quit to desktop");
    return options;
}

void Game::updatePauseMenu()
{
    const std::vector<std::string> options = pauseOptions();
    const int count = static_cast<int>(options.size());
    constexpr int kStayRow = 4;
    const bool stayOffered = count > 6;
    const auto moved = [&]()
    {
        mStayConfirm = 0;
        mStayWhy.clear();
        mQuitConfirm = false;
    };
    if (mInput.pressed(Action::UiUp) || mInput.pressed(Action::MoveUp))
    {
        mPauseSelection = (mPauseSelection + count - 1) % count;
        moved();
    }
    if (mInput.pressed(Action::UiDown) || mInput.pressed(Action::MoveDown))
    {
        mPauseSelection = (mPauseSelection + 1) % count;
        moved();
    }
    mPauseSelection = std::clamp(mPauseSelection, 0, count - 1);
    if (!(mInput.pressed(Action::UiConfirm) || mInput.pressed(Action::UiSpace))) return;

    int choice = mPauseSelection;
    if (stayOffered && choice == kStayRow)
    {
        mStayWhy = stayForGoodBlocker();
        if (!mStayWhy.empty())
        {
            mStayConfirm = 0;
            return;
        }
        if (++mStayConfirm >= 3) stayForGood();
        return;
    }
    if (stayOffered && choice > kStayRow) --choice;
    switch (choice)
    {
    case 0: mRunState = RunState::Playing; break;
    case 1: saveCurrentRunCheckpoint("Run checkpoint saved."); break;
    case 2: openSettings(); break;
    case 3: mShowControls = true; break;
    case 4: // Save and quit to title
        saveCurrentRunCheckpoint("Run checkpoint saved.");
        returnToTitleScreen();
        break;
    default: // Quit to desktop, asked once; the run is kept
        if (!mQuitConfirm)
        {
            mQuitConfirm = true;
            return;
        }
        saveCurrentRunCheckpoint("Run checkpoint saved.");
        mIsRunning = false;
        break;
    }
}
