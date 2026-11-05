// Title screen and pause menu input.
#include "core/GameShared.h"

using namespace gameshared;

// Title screen hub: briefing menu, mode, and save-slot selection.
void Game::updateTitleScreen(float dtSeconds)
{
    mTitlePulseTimer += dtSeconds;
    mTitleConfirmNewWorldTimer = std::max(0.0f, mTitleConfirmNewWorldTimer - dtSeconds);
    if (mTitleConfirmNewWorldTimer <= 0.0f)
    {
        mTitleConfirmNewWorld = false;
    }

    mTitleStatusTimer = std::max(0.0f, mTitleStatusTimer - dtSeconds);
    if (mTitleStatusTimer <= 0.0f)
    {
        mTitleStatusMessage.clear();
    }

    // The checkpoint lives on disk: re-read it after input (slot changes,
    // deletes) or once a second, not every tick.
    mTitleCheckpointRefreshTimer -= dtSeconds;
    if (mTitleCheckpointRefreshTimer <= 0.0f || mInput.anyKeyPressed())
    {
        refreshTitleCheckpoint();
    }

    const int menuCount = static_cast<int>(kTitleMenuItems.size());
    if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
    {
        const int previousSelection = mTitleMenuSelection;
        mTitleMenuSelection = wrapIndex(mTitleMenuSelection - 1, menuCount);
        if (mTitleMenuSelection != previousSelection)
        {
            mTitleConfirmNewWorld = false;
        }
    }
    if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
    {
        const int previousSelection = mTitleMenuSelection;
        mTitleMenuSelection = wrapIndex(mTitleMenuSelection + 1, menuCount);
        if (mTitleMenuSelection != previousSelection)
        {
            mTitleConfirmNewWorld = false;
        }
    }

    if (mTitleMenuSelection != 2)
    {
        mTitleConfirmNewWorld = false;
    }

    auto adjustMenuValue = [&](int delta)
    {
        if (mTitleMenuSelection == 3) // Save Slot
        {
            const int oldIndex = mTitleSaveSlotIndex;
            const int newIndex = wrapIndex(mTitleSaveSlotIndex + delta, static_cast<int>(kTitleSaveSlots.size()));
            if (newIndex == oldIndex)
            {
                return;
            }

            if (loadSlotAndWorld(kTitleSaveSlots[static_cast<std::size_t>(newIndex)]))
            {
                mTitleSaveSlotIndex = newIndex;
                mTitleStatusMessage = std::string("Active slot: ") + kTitleSaveLabels[static_cast<std::size_t>(newIndex)];
                mTitleStatusTimer = 2.0f;
            }
            else
            {
                mTitleStatusMessage = "Unable to load slot data.";
                mTitleStatusTimer = 2.5f;
            }
            return;
        }

        if (mTitleMenuSelection == 4) // Game Mode
        {
            mTitleGameModeIndex = wrapIndex(mTitleGameModeIndex + delta, static_cast<int>(kTitleGameModeNames.size()));
            mTitleStatusMessage = std::string("Game Mode: ") + kTitleGameModeNames[static_cast<std::size_t>(mTitleGameModeIndex)];
            mTitleStatusTimer = 2.0f;
            syncPresentationSettingsToSave();
            return;
        }

        if (mTitleMenuSelection == 5) // Threat
        {
            mTitleModeIndex = wrapIndex(mTitleModeIndex + delta, static_cast<int>(kTitleModeNames.size()));
            mTitleStatusMessage = std::string("Threat: ") + kTitleModeNames[static_cast<std::size_t>(mTitleModeIndex)];
            mTitleStatusTimer = 2.0f;
            syncPresentationSettingsToSave();
            return;
        }

        if (mTitleMenuSelection == 6) // Tone
        {
            mTitleToneIndex = wrapIndex(mTitleToneIndex + delta, static_cast<int>(kTitleToneNames.size()));
            mTitleStatusMessage = std::string("Tone: ") + kTitleToneNames[static_cast<std::size_t>(mTitleToneIndex)];
            mTitleStatusTimer = 2.0f;
            syncPresentationSettingsToSave();
        }
    };

    if (mInput.wasKeyPressed(SDL_SCANCODE_LEFT) || mInput.wasKeyPressed(SDL_SCANCODE_A))
    {
        adjustMenuValue(-1);
    }
    if (mInput.wasKeyPressed(SDL_SCANCODE_RIGHT) || mInput.wasKeyPressed(SDL_SCANCODE_D))
    {
        adjustMenuValue(1);
    }

    const bool activate =
        mInput.wasKeyPressed(SDL_SCANCODE_RETURN) ||
        mInput.wasKeyPressed(SDL_SCANCODE_SPACE);

    if (activate)
    {
        switch (mTitleMenuSelection)
        {
        case 0: // Continue Run
            if (mTitleHasCheckpoint)
            {
                mIgnoreCheckpointRestoreOnce = false;
                startNewRun();
            }
            else
            {
                mTitleStatusMessage = "No checkpoint found for this slot.";
                mTitleStatusTimer = 2.5f;
            }
            break;

        case 1: // Start New Run
            mSaveManager.deleteCheckpoint();
            mIgnoreCheckpointRestoreOnce = true;
            syncPresentationSettingsToSave();
            startNewRun();
            break;

        case 2: // Start New World
        {
            if (!mTitleConfirmNewWorld)
            {
                mTitleConfirmNewWorld = true;
                mTitleConfirmNewWorldTimer = 4.0f;
                mTitleStatusMessage = "Press Enter again to confirm: this resets world and history for this slot.";
                mTitleStatusTimer = 3.5f;
            }
            else
            {
                mTitleConfirmNewWorld = false;
                mTitleConfirmNewWorldTimer = 0.0f;

                if (!mSaveManager.resetCurrentSlotWorld(true))
                {
                    mTitleStatusMessage = "Failed to reset world for this slot.";
                    mTitleStatusTimer = 3.0f;
                    break;
                }

                if (!loadSlotAndWorld(kTitleSaveSlots[static_cast<std::size_t>(mTitleSaveSlotIndex)]))
                {
                    mTitleStatusMessage = "World reset complete, but reload failed.";
                    mTitleStatusTimer = 3.0f;
                    break;
                }

                mIgnoreCheckpointRestoreOnce = true;
                syncPresentationSettingsToSave();
                startNewRun();
            }
            break;
        }

        case 3: // Save Slot
            adjustMenuValue(1);
            break;

        case 4: // Game Mode
            adjustMenuValue(1);
            break;

        case 5: // Threat
            adjustMenuValue(1);
            break;

        case 6: // Tone
            adjustMenuValue(1);
            break;

        case 7: // Options
            mShowControls = !mShowControls;
            if (mShowControls)
            {
                mShowTitleCredits = false;
            }
            break;

        case 8: // Credits
            mShowTitleCredits = !mShowTitleCredits;
            if (mShowTitleCredits)
            {
                mShowControls = false;
            }
            break;

        case 9: // Quit
            mIsRunning = false;
            break;

        default:
            break;
        }
    }
}

// Pause menu navigation and actions.
void Game::updatePauseMenu()
{
    if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
    {
        mPauseSelection = (mPauseSelection + 4) % 5;
    }
    if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
    {
        mPauseSelection = (mPauseSelection + 1) % 5;
    }
    if (mInput.wasKeyPressed(SDL_SCANCODE_RETURN) || mInput.wasKeyPressed(SDL_SCANCODE_SPACE))
    {
        if (mPauseSelection == 0) // Resume
        {
            mRunState = RunState::Playing;
        }
        else if (mPauseSelection == 1) // Save
        {
            saveCurrentRunCheckpoint("Run checkpoint saved.");
        }
        else if (mPauseSelection == 2) // Back to Menu
        {
            returnToTitleScreen();
        }
        else if (mPauseSelection == 3) // Restart
        {
            startNewRun();
        }
        else // Quit
        {
            mIsRunning = false;
        }
    }
}
