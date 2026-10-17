#include "Armors.h"

#include "util.h"

#include <format>
#include <unordered_map>

namespace Armors {

	namespace {

		// ProcessArmors only runs once
		bool s_processed = false;

		// Manual changes made through the menu. Key: "<plugin>|<editor ID>", value: true = revealing, false = concealing
		std::unordered_map<std::string, bool> s_overrides;

		// ===============================================================
		// SLOT 52
		// ===============================================================

		// Adds or removes slot 52 from a body armor according to its keywords. Armors with neither (or both)
		// keywords are concealing if their plugin is in the list, otherwise they are left as they are.
		// Returns true if the armor was changed.
		bool ApplySlot52Rules(RE::TESObjectARMO* a_armor, const Util::ModSet* a_concealingMods = nullptr) {

			if (!a_armor)
				return false;

			const SlotMask mask = Util::GetArmorSlots(a_armor);

			// Only body armors
			if ((mask & SLOT_32) == 0)
				return false;

			const bool hasSlot52 = (mask & SLOT_52) != 0;
			const bool revealing = Util::ArmorHasKeyword(a_armor, RevKW);
			const bool concealing = Util::ArmorHasKeyword(a_armor, ConKW);

			bool shouldHaveSlot52 = hasSlot52;

			if (revealing != concealing)
				shouldHaveSlot52 = concealing;
			else if (a_concealingMods) {
				const auto* file = a_armor->GetFile(0);
				if (file && Util::ModSetContains(*a_concealingMods, file->GetFilename()))
					shouldHaveSlot52 = true;
			}

			if (shouldHaveSlot52 == hasSlot52)
				return false;

			Util::SetArmorSlots(a_armor, shouldHaveSlot52 ? (mask | SLOT_52) : (mask & ~SLOT_52));
			return true;
		}

		// ===============================================================
		// OVERRIDES (SOS_ARMO_Override.JSON)
		// ===============================================================

		std::string MakeArmorKey(const RE::TESObjectARMO* a_armor) {

			if (!a_armor)
				return {};

			const std::string editorID = Util::GetEditorIDSafe(a_armor);
			const auto* file = a_armor->GetFile(0);
			if (editorID.empty() || !file)
				return {};

			return std::format("{}|{}", file->GetFilename(), editorID);
		}

		RE::TESObjectARMO* ResolveArmorKey(const std::string& a_key) {

			const auto separator = a_key.find('|');
			if (separator == std::string::npos)
				return nullptr;

			const std::string plugin = a_key.substr(0, separator);
			const std::string editorID = a_key.substr(separator + 1);

			auto* armor = RE::TESForm::LookupByEditorID<RE::TESObjectARMO>(editorID);
			if (!armor)
				return nullptr;

			// The editor ID has to belong to the plugin of the key
			const auto* file = armor->GetFile(0);
			return file && plugin == file->GetFilename() ? armor : nullptr;
		}

		void SaveOverrides() {

			Json root = Json::object();
			root["armor_overrides"] = Json::object();

			for (const auto& [key, revealing] : s_overrides)
				root["armor_overrides"][key] = revealing;

			Util::SaveJson(ARMOPath, root);
		}

		// Gives the armor the keyword of its override and takes the opposite one away
		void SetKeywords(RE::TESObjectARMO* a_armor, bool a_revealing) {

			if (auto* opposite = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(a_revealing ? ConKW : RevKW))
				a_armor->RemoveKeyword(opposite);

			if (auto* keyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(a_revealing ? RevKW : ConKW))
				a_armor->AddKeyword(keyword);
		}
	}

	void SetOverride(RE::TESObjectARMO* a_armor, bool a_revealing) {

		const auto key = MakeArmorKey(a_armor);
		if (key.empty())
			return;

		s_overrides[key] = a_revealing;
		SetKeywords(a_armor, a_revealing);
		SaveOverrides();

		ApplySlot52Rules(a_armor);
	}

	void LoadOverrides() {

		s_overrides.clear();

		auto root = Util::LoadJson(ARMOPath);
		if (!root || !root->contains("armor_overrides"))
			return;

		for (auto& [key, value] : (*root)["armor_overrides"].items()) {
			if (value.is_boolean())
				s_overrides[key] = value.get<bool>();
		}

		SKSE::log::info("SOS: Loaded {} armor overrides from the JSON.", s_overrides.size());
	}

	void ApplyOverrides() {

		for (const auto& [key, revealing] : s_overrides) {
			if (auto* armor = ResolveArmorKey(key))
				SetKeywords(armor, revealing);
		}
	}

	void ClearOverrides() {

		s_overrides.clear();
	}

	void ProcessArmors() {

		if (s_processed)
			return;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler)
			return;

		const auto concealingMods = Util::LoadModList(BlacklistPath, "Armor Blacklist");

		std::uint32_t count = 0;
		for (auto* armor : dataHandler->GetFormArray<RE::TESObjectARMO>()) {
			if (ApplySlot52Rules(armor, &concealingMods))
				++count;
		}

		s_processed = true;
		SKSE::log::info("SOS: Processed {} armors.", count);
	}

	void RemoveConcealing(RE::Actor* a_actor) {

		auto* equipManager = RE::ActorEquipManager::GetSingleton();
		if (!a_actor || !equipManager)
			return;

		auto* slot52Armor = Util::GetSlot52Armor(a_actor);
		if (slot52Armor && !Util::ArmorHasKeyword(slot52Armor, GenKW))
			equipManager->UnequipObject(a_actor, slot52Armor);

		for (auto* underwear : Util::GetWornArmorsWithKeyword(a_actor, UndwKW))
			equipManager->UnequipObject(a_actor, underwear);
	}
}
