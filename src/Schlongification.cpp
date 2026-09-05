#include "Schlongification.h"

namespace SOS {

	inline std::unordered_set<RE::FormID> g_activeActors;

	static void OnActorActivated(RE::Actor* a_actor) {

		if (!a_actor || !a_actor->Is3DLoaded())
			return;

		auto* base = a_actor->GetActorBase();
		if (!base)
			return;

		auto* schlong = SchlongLogic::DetermineWinningAddon(a_actor);
		if (!schlong) {
			a_actor->AddToFaction(g_schlongifiedFaction, -1);
			return;
		}

		bool isValidSchlong = !Util::ArmorHasKeyword(schlong, PubKW);

		if (isValidSchlong) {
			a_actor->AddToFaction(g_schlongifiedFaction, 0);

			if (g_SexLabGenderFaction && Util::IsFemale(base) && g_SexLabForceMaleOnSchlong)
				a_actor->AddToFaction(g_SexLabGenderFaction, 0);
		}
		else
			a_actor->AddToFaction(g_schlongifiedFaction, -1);

		//Slot52 occupied OR Underwear equipped: don't equip the Addon, just scale bones
		if (Util::HasSomethingInSlot52(a_actor) || Util::ActorHasEquippedArmorWithKeyword(a_actor, UndwKW)) {
			SchlongLogic::ScaleSchlongBones(a_actor);
			return;
		}

		auto* equipManager = RE::ActorEquipManager::GetSingleton();
		if (!equipManager)
			return;

		Util::RemoveSOSItemsFromInventory(a_actor);

		a_actor->AddObjectToContainer(schlong, nullptr, 1, nullptr);

		equipManager->EquipObject(
			a_actor,
			schlong,
			nullptr,
			1,
			nullptr,
			true,  // preventUnequip
			false, // playSound
			true,  // immediate
			true   // applyNow
		);

		SchlongLogic::ScaleSchlongBones(a_actor);
	}

	//Handles equip/unequip of armor with the Underwear keyword.
//It hides or restores the Addon without ever touching Slot52 itself.
	static void HandleUnderwearChange(RE::Actor* a_actor, bool is_equipping) {

		if (!a_actor)
			return;

		if (is_equipping) {

			//Nothing to hide if the Slot52 is already empty
			if (!Util::HasSomethingInSlot52(a_actor))
				return;

			//Only removes the physical Addon item, NPC data (rank/addon) stays untouched
			Util::RemoveSOSItemsFromInventory(a_actor);
			return;
		}

		//Underwear removed: only restore the Addon if Slot52 is still empty
		if (Util::HasSomethingInSlot52(a_actor))
			return;

		auto* schlong = SchlongLogic::DetermineWinningAddon(a_actor);
		if (!schlong)
			return;

		auto* equipManager = RE::ActorEquipManager::GetSingleton();
		if (!equipManager)
			return;

		//Cleanup any leftover unequipped Addon items before adding it back
		Util::RemoveSOSItemsFromInventory(a_actor);
		a_actor->AddObjectToContainer(schlong, nullptr, 1, nullptr);

		RE::FormID refID = a_actor->GetFormID();
		g_activeActors.insert(refID);

		equipManager->EquipObject(
			a_actor,
			schlong,
			nullptr,
			1,
			nullptr,
			true,  // preventUnequip
			false, // playSound
			true,  // immediate
			true   // applyNow
		);

		SchlongLogic::ScaleSchlongBones(a_actor);
		g_activeActors.erase(refID);
	}

	static void OnWearChange(RE::Actor* a_actor, const RE::TESEquipEvent* a_event, bool is_equipping) {

		auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(a_event->baseObject);
		if (!armor)
			return;

		auto* biped = armor->As<RE::BGSBipedObjectForm>();
		if (!biped)
			return;

		//Underwear doesn't touch Slot52, so it's handled in its own separate branch
		if (Util::ArmorHasKeyword(armor, UndwKW)) {
			HandleUnderwearChange(a_actor, is_equipping);
			return;
		}

		SlotMask mask = static_cast<SlotMask>(biped->GetSlotMask().get());
		if ((mask & SLOT_52) == 0)
			return;

		auto* base = a_actor->GetActorBase();
		if (!base)
			return;

		if (is_equipping)
			SchlongLogic::ScaleSchlongBones(a_actor);
		else {

			if (Util::ArmorHasKeyword(armor, GenKW))
				return;

			//If Underwear is still equipped, the Addon must stay hidden even though Slot52 is now free
			if (Util::ActorHasEquippedArmorWithKeyword(a_actor, UndwKW))
				return;

			auto* schlong = SchlongLogic::DetermineWinningAddon(a_actor);
			if (!schlong)
				return;

			auto* equipManager = RE::ActorEquipManager::GetSingleton();
			if (!equipManager)
				return;

			a_actor->AddObjectToContainer(schlong, nullptr, 1, nullptr);

			RE::FormID refID = a_actor->GetFormID();
			g_activeActors.insert(refID);

			equipManager->EquipObject(
				a_actor,
				schlong,
				nullptr,
				1,
				nullptr,
				true,  // preventUnequip
				false, // playSound
				true,  // immediate
				true   // applyNow
			);

			SchlongLogic::ScaleSchlongBones(a_actor);
			g_activeActors.erase(refID);
		}
	}

	RE::BSEventNotifyControl ObjectLoadedHandler::ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) {

		if (!a_event || !a_event->loaded)
			return RE::BSEventNotifyControl::kContinue;

		auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_event->formID);
		if (actor)
			OnActorActivated(actor);

		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl InitScriptHandler::ProcessEvent(const RE::TESInitScriptEvent* a_event, RE::BSTEventSource<RE::TESInitScriptEvent>*) {

		if (!a_event)
			return RE::BSEventNotifyControl::kContinue;

		auto* refr = a_event->objectInitialized.get();
		if (refr) {
			auto* actor = refr->As<RE::Actor>();
			if (actor) {
				OnActorActivated(actor);
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl EquipEventHandler::ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) {

		if (!a_event || !a_event->actor)
			return RE::BSEventNotifyControl::kContinue;

		RE::FormID refID = a_event->actor->GetFormID();

		if (g_activeActors.contains(refID))
			return RE::BSEventNotifyControl::kContinue;

		auto* actor = a_event->actor->As<RE::Actor>();
		if (actor)
			OnWearChange(actor, a_event, a_event->equipped);

		return RE::BSEventNotifyControl::kContinue;
	}

	void RegisterActivationEvents() {
		auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
		if (holder) {
			static ObjectLoadedHandler g_objectLoadedHandler;
			static InitScriptHandler g_initScriptHandler;

			holder->AddEventSink(&g_objectLoadedHandler);
			holder->AddEventSink(&g_initScriptHandler);
			SKSE::log::info("SOS: Activation Events (ObjectLoad + InitScript) Registered");
		}
	}

	void RegisterEquipEvent() {
		auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
		if (holder) {
			static EquipEventHandler g_equipEventHandler;
			holder->AddEventSink(&g_equipEventHandler);
			SKSE::log::info("SOS: EquipEvent registered");
		}
	}
}