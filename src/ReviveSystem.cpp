#include "TotalRestore.h"
#include "DeathTracker.h"
#include "ReviveRules.h"
#include <sstream>

/*
 * ReviveSystem Module
 * Hooks ForgottenGUI::selectedObjectsChanged
 * Adds Revive button to StatusPanel and performs resurrection.
 * The time window is measured from the recorded death time (DeathTracker),
 * keyed by the character handle, in in-game minutes.
 */

namespace TotalRestore
{
    static const char KR_REVIVE_PANEL[] = "TR_SW_RevivePanel";
    static const char KR_REVIVE_BUTTON[] = "TR_SW_ReviveButton";

    struct ReviveState
    {
        hand selectedDeadCharacter;
        hand selectedAliveCharacter;
        ReviveState() : selectedDeadCharacter(nullptr), selectedAliveCharacter(nullptr) {}
    };
    static ReviveState g_reviveState;

    static void (*g_origSelectedObjectsChanged)(ForgottenGUI*) = NULL;

    static MyGUI::Widget* FindWidgetBySuffix(MyGUI::EnumeratorWidgetPtr enumerator, const std::string& suffix)
    {
        while (enumerator.next())
        {
            std::string widgetName = enumerator.current()->getName();
            size_t splitPos = widgetName.find('_');
            if (splitPos != std::string::npos && widgetName.substr(splitPos + 1) == suffix)
            {
                return enumerator.current();
            }
            if (enumerator.current()->getChildCount() > 0)
            {
                MyGUI::Widget* child = FindWidgetBySuffix(enumerator.current()->getEnumerator(), suffix);
                if (child != NULL) return child;
            }
        }
        return NULL;
    }

    static void ScaleVitalsToFraction(MedicalSystem& medical, float fraction)
    {
        medical.blood *= fraction;

        const uint32_t anatomyCount = (uint32_t)medical.anatomy.size();
        for (uint32_t i = 0; i < anatomyCount; ++i)
        {
            MedicalSystem::HealthPartStatus* part = medical.anatomy[i];
            if (part == NULL) continue;

            part->flesh *= fraction;
            part->updateDerivedHealths();
        }
    }

    static void OnReviveButtonClick(MyGUI::WidgetPtr sender)
    {
        Character* deadChar = g_reviveState.selectedDeadCharacter.getCharacter();
        Character* aliveChar = g_reviveState.selectedAliveCharacter.getCharacter();

        if (deadChar == NULL || !deadChar->isDead())
        {
            return;
        }

        ActivePlatoon* platoon = (aliveChar != NULL) ? aliveChar->getPlatoon() : NULL;
        PlayerInterface* player_interface = (platoon != NULL) ? platoon->isPlayer : NULL;
        if (player_interface == NULL && ou != NULL)
        {
            player_interface = ou->player;
        }

        if (player_interface == NULL)
        {
            return;
        }

        // 1. Resurrect: clear dead flag before recruitment to avoid crash
        deadChar->medical.dead = false;

        // 2. Heal completely (clears wounds, stun, wear, bandaging), then scale every
        //    part and the blood down to ReviveHealthFraction
        deadChar->healCompletely();
        ScaleVitalsToFraction(deadChar->medical, ReviveRules::ClampReviveFraction(g_config.reviveHealthFraction));

        // 3. Recruit character into player squad (forget the death first: recruiting
        //    moves the character to another platoon, which changes its handle)
        ForgetDeath(deadChar);
        player_interface->recruit(deadChar, false);

        // Hide panel after revive
        MyGUI::Widget* panel = sender->getParent();
        if (panel != NULL)
        {
            panel->setVisible(false);
        }

        Log("Character revived successfully.");
    }

    static bool CanReviveCharacter(Character* deadChar)
    {
        if (deadChar == NULL || !deadChar->isDead()) return false;

        // Check if enemy revive allowed
        if (!g_config.allowRevivingEnemies && !deadChar->isPlayerCharacter())
        {
            return false;
        }

        // Check time limit if enabled
        double now = 0.0;
        if (g_config.enableReviveTimeLimit && CurrentInGameMinutes(now))
        {
            const double diedAt = ObserveDeathMinutes(deadChar, now);
            return ReviveRules::IsWithinReviveWindow(true, (double)g_config.maxReviveTimeMinutes, diedAt, now);
        }

        return true;
    }

    static void ForgottenGUI_selectedObjectsChanged_hook(ForgottenGUI* thisptr)
    {
        if (g_origSelectedObjectsChanged != NULL)
        {
            g_origSelectedObjectsChanged(thisptr);
        }

        if (thisptr == NULL) return;

        // Check if selected object is a dead character
        if (thisptr->selectedObject.type == ::CHARACTER)
        {
            Character* character = thisptr->selectedObject.getCharacter();
            if (CanReviveCharacter(character))
            {
                g_reviveState.selectedDeadCharacter = character->getHandle();
                g_reviveState.selectedAliveCharacter = thisptr->selectedPlayerCharacter;

                MyGUI::Gui* guiPtr = MyGUI::Gui::getInstancePtr();
                if (guiPtr != NULL)
                {
                    MyGUI::Widget* revivePanel = guiPtr->findWidget<MyGUI::Widget>(KR_REVIVE_PANEL, false);
                    if (revivePanel != NULL)
                    {
                        revivePanel->setVisible(true);
                    }
                    else
                    {
                        MyGUI::Widget* statusPanel = FindWidgetBySuffix(guiPtr->getEnumerator(), "StatusPanel");
                        if (statusPanel != NULL && statusPanel->getParent() != NULL)
                        {
                            MyGUI::Widget* newPanel = statusPanel->getParent()->createWidgetReal<MyGUI::Widget>(
                                "Kenshi_Panel", 0.75f, 0.0f, 0.25f, 0.15f, MyGUI::Align::Right, KR_REVIVE_PANEL);
                            MyGUI::Button* reviveButton = newPanel->createWidgetReal<MyGUI::Button>(
                                "Kenshi_Button1", 0.0f, 0.0f, 1.0f, 1.0f, MyGUI::Align::Left, KR_REVIVE_BUTTON);
                            reviveButton->setCaption("Revive");
                            reviveButton->eventMouseButtonClick += MyGUI::newDelegate(OnReviveButtonClick);
                        }
                    }
                }
                return;
            }
        }

        // Reset if selection changed away from dead character
        if (g_reviveState.selectedDeadCharacter.type == ::CHARACTER)
        {
            g_reviveState.selectedDeadCharacter = nullptr;
            g_reviveState.selectedAliveCharacter = nullptr;
            MyGUI::Gui* guiPtr = MyGUI::Gui::getInstancePtr();
            if (guiPtr != NULL)
            {
                MyGUI::Widget* revivePanel = guiPtr->findWidget<MyGUI::Widget>(KR_REVIVE_PANEL, false);
                if (revivePanel != NULL)
                {
                    revivePanel->setVisible(false);
                }
            }
        }
    }

    bool InitReviveSystem()
    {
        if (KenshiLib::SUCCESS != KenshiLib::AddHook(
                KenshiLib::GetRealAddress(&ForgottenGUI::selectedObjectsChanged),
                ForgottenGUI_selectedObjectsChanged_hook,
                &g_origSelectedObjectsChanged))
        {
            LogError("Could not install ForgottenGUI::selectedObjectsChanged hook for ReviveSystem.");
            return false;
        }

        InitDeathTracker();

        Log("ReviveSystem hook installed successfully.");
        return true;
    }
}
