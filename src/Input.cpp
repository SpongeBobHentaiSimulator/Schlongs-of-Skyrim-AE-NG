#include "Input.h"

#include "util.h"

#include <atomic>
#include <filesystem>
#include <format>
#include <mutex>
#include <unordered_map>

#include "SKSEMenuFramework.h"

namespace Input {

	// ===============================================================
	// STATE (runtime only, nothing is saved)
	// ===============================================================

	namespace {

		// FormID of the pinned NPC, 0 if there isn't one
		std::atomic<RE::FormID> s_pinnedID{ 0 };

		// Bend level of every actor, 0-20 (0 is flaccid)
		std::mutex s_bendMutex;
		std::unordered_map<RE::FormID, int> s_bends;
	}

	void SendNotification(const char* a_msg, const char* a_sound, bool a_cancelIfQueued) {

		if (!a_msg)
			return;

		using func_t = void(*)(const char*, const char*, bool);
		REL::Relocation<func_t> func{ RELOCATION_ID(52050, 52933) };
		func(a_msg, a_sound, a_cancelIfQueued);
	}

	// ===============================================================
	// BEND
	// ===============================================================

	namespace {

		// Plays the event of a bend level: 0 = flaccid, 1-20 = bend from -9 to 10 (events SOSBend-9 ... SOSBend10)
		void PlayBend(RE::FormID a_actorID, int a_level) {

			// The animation graph can only be touched from the game thread
			SKSE::GetTaskInterface()->AddTask([a_actorID, a_level]() {

				auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_actorID);
				if (!actor || !actor->Is3DLoaded())
					return;

				if (a_level <= 0)
					actor->NotifyAnimationGraph("SOSFlaccid");
				else
					actor->NotifyAnimationGraph(std::format("SOSBend{}", std::min(a_level, Util::kMaxRank) - 10));
			});
		}

		void StepBend(RE::FormID a_actorID, int a_step) {

			int level;
			{
				std::lock_guard lock(s_bendMutex);

				int& bend = s_bends[a_actorID];
				bend = std::clamp(bend + a_step, 0, Util::kMaxRank);
				level = bend;
			}

			PlayBend(a_actorID, level);
		}
	}

	// ===============================================================
	// PIN
	// ===============================================================

	namespace {

		// Pins the actor under the crosshair, nothing changes if there isn't one
		void PinActorUnderCrosshair() {

			auto* pickData = RE::CrosshairPickData::GetSingleton();
			if (!pickData)
				return;

			auto& target = pickData->target[0];
			if (!target)
				return;

			const auto ref = target.get();
			auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
			if (!actor)
				return;

			s_pinnedID = actor->GetFormID();

			auto* base = actor->GetActorBase();
			const char* name = base ? base->GetFullName() : nullptr;

			SendNotification(std::format("SOS: {} pinned", (name && name[0] != '\0') ? name : "NPC").c_str());
		}
	}

	RE::Actor* GetPinnedActor() {

		const RE::FormID pinnedID = s_pinnedID;
		return pinnedID != 0 ? RE::TESForm::LookupByID<RE::Actor>(pinnedID) : nullptr;
	}

	void ClearPinned() {

		s_pinnedID = 0;
	}

	void ClearState() {

		s_pinnedID = 0;

		{
			std::lock_guard lock(s_bendMutex);
			s_bends.clear();
		}

		if (const auto* player = RE::PlayerCharacter::GetSingleton())
			PlayBend(player->GetFormID(), 0);
	}

	// ===============================================================
	// KEYBOARD
	// ===============================================================

	namespace {

		// True if the keys of the mod should work now: playing, with no menu or console open
		bool IsGameplay() {

			if (SKSEMenuFramework::IsAnyBlockingWindowOpened())
				return false;

			auto* ui = RE::UI::GetSingleton();
			return ui && !ui->GameIsPaused() && !ui->IsMenuOpen("Console");
		}

		void HandleKey(std::uint32_t a_scanCode) {

			const int key = static_cast<int>(a_scanCode);
			if (key == 0 || !IsGameplay())
				return;

			const auto& keys = g_settings.keys;

			if (key == keys[Binding::kPin])
				PinActorUnderCrosshair();

			// The NPC bend keys act on the pinned NPC
			if (const RE::FormID pinnedID = s_pinnedID; pinnedID != 0) {

				if (key == keys[Binding::kBendUp])
					StepBend(pinnedID, 1);

				if (key == keys[Binding::kBendDown])
					StepBend(pinnedID, -1);
			}

			// The player keys always act on the player
			if (const auto* player = RE::PlayerCharacter::GetSingleton()) {

				if (key == keys[Binding::kPlayerBendUp])
					StepBend(player->GetFormID(), 1);

				if (key == keys[Binding::kPlayerBendDown])
					StepBend(player->GetFormID(), -1);
			}
		}

		class KeyListener : public RE::BSTEventSink<RE::InputEvent*> {

		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override {

				if (!a_event)
					return RE::BSEventNotifyControl::kContinue;

				for (auto* event = *a_event; event; event = event->next) {

					// Only the moment a key goes down, holding it doesn't repeat
					const auto* button = event->AsButtonEvent();
					if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown())
						continue;

					HandleKey(button->GetIDCode());
				}

				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Register() {

		auto* manager = RE::BSInputDeviceManager::GetSingleton();
		if (!manager) {
			SKSE::log::error("SOS: The input device manager is not available, the keys will not work.");
			return;
		}

		static KeyListener listener;
		manager->AddEventSink(&listener);

		SKSE::log::info("SOS: Keys registered (pin and bend)");
	}
}
