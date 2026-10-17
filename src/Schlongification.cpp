#include "Schlongification.h"

#include "SchlongLogic.h"
#include "util.h"

namespace SOS {

	namespace {

		// True if the addon has to be equipped: it isn't worn yet and nothing hides it
		bool NeedsEquip(RE::Actor* a_actor, RE::TESObjectARMO* a_schlong) {

			// Already wearing it, or another armor in slot 52 hides it. Only an old addon
			// in slot 52 has to be replaced by the right one.
			auto* worn = Util::GetSlot52Armor(a_actor);
			if (worn && (worn == a_schlong || !Util::ArmorHasKeyword(worn, GenKW)))
				return false;

			// Underwear hides it too
			return !Util::ActorHasEquippedArmorWithKeyword(a_actor, UndwKW);
		}

		// An actor was loaded or initialized: gives it its addon
		void OnActorLoaded(RE::Actor* a_actor) {

			if (!a_actor || !a_actor->Is3DLoaded() || !a_actor->GetActorBase())
				return;

			auto* schlong = SchlongLogic::DetermineWinningAddon(a_actor);
			SchlongLogic::UpdateFactions(a_actor, schlong);

			if (!schlong)
				return;

			if (NeedsEquip(a_actor, schlong))
				SchlongLogic::EquipAddon(a_actor, schlong);
			else
				SchlongLogic::ScaleSchlongBones(a_actor->GetFormID());
		}

		// Underwear never uses slot 52: it hides the addon by removing its item and the addon comes back when it is taken off
		void HandleUnderwearChange(RE::Actor* a_actor, bool a_equipping) {

			if (a_equipping) {

				// Only the physical item is removed, the NPC data (addon and rank) stays untouched
				if (Util::HasSomethingInSlot52(a_actor))
					Util::RemoveSOSItemsFromInventory(a_actor);

				return;
			}

			// The addon only comes back if slot 52 is still free
			if (Util::HasSomethingInSlot52(a_actor))
				return;

			if (auto* schlong = SchlongLogic::DetermineWinningAddon(a_actor))
				SchlongLogic::EquipAddon(a_actor, schlong);
		}

		// An actor equipped or unequipped an armor
		void OnArmorChange(RE::Actor* a_actor, const RE::TESEquipEvent* a_event) {

			auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(a_event->baseObject);
			if (!armor)
				return;

			if (Util::ArmorHasKeyword(armor, UndwKW)) {
				HandleUnderwearChange(a_actor, a_event->equipped);
				return;
			}

			if ((Util::GetArmorSlots(armor) & SLOT_52) == 0)
				return;

			if (a_event->equipped) {
				SchlongLogic::ScaleSchlongBones(a_actor->GetFormID());
				return;
			}

			// Nothing to restore if it was the addon itself, or if underwear still hides it
			if (Util::ArmorHasKeyword(armor, GenKW) || Util::ActorHasEquippedArmorWithKeyword(a_actor, UndwKW))
				return;

			if (auto* schlong = SchlongLogic::DetermineWinningAddon(a_actor))
				SchlongLogic::EquipAddon(a_actor, schlong);
		}
	}

	RE::BSEventNotifyControl EventHandler::ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) {

		if (a_event && a_event->loaded)
			OnActorLoaded(RE::TESForm::LookupByID<RE::Actor>(a_event->formID));

		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl EventHandler::ProcessEvent(const RE::TESInitScriptEvent* a_event, RE::BSTEventSource<RE::TESInitScriptEvent>*) {

		if (a_event) {
			if (auto* refr = a_event->objectInitialized.get())
				OnActorLoaded(refr->As<RE::Actor>());
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl EventHandler::ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) {

		if (!a_event || !a_event->actor)
			return RE::BSEventNotifyControl::kContinue;

		auto* actor = a_event->actor->As<RE::Actor>();

		// The equip events caused by the plugin itself are ignored
		if (actor && !SchlongLogic::IsEquippingAddon(actor->GetFormID()))
			OnArmorChange(actor, a_event);

		return RE::BSEventNotifyControl::kContinue;
	}

	void RegisterEvents() {

		auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
		if (!holder) {
			SKSE::log::error("SOS: The script event source holder is not available, the events were not registered.");
			return;
		}

		static EventHandler handler;

		holder->AddEventSink<RE::TESObjectLoadedEvent>(&handler);
		holder->AddEventSink<RE::TESInitScriptEvent>(&handler);
		holder->AddEventSink<RE::TESEquipEvent>(&handler);

		SKSE::log::info("SOS: Events registered (ObjectLoaded, InitScript, Equip)");
	}
}
