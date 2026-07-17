#include "Armors.h"

std::unordered_map<std::string, bool> g_armorOverrides;

//Lock for process
static bool g_processed = false;

//Modify the armor based on KW
static bool ModifyArmorWithSOS(RE::TESObjectARMO* a_armor, const std::unordered_set<RE::BSFixedString>& a_blacklist) {

	if (!a_armor)
		return false;

	auto* biped = a_armor->As<RE::BGSBipedObjectForm>();
	if (!biped)
		return false;

	SlotMask mask = static_cast<SlotMask>(biped->GetSlotMask());

	if ((mask & SLOT_32) == 0)
		return false;

	const bool hasSlot52 = (mask & SLOT_52) != 0;
	const bool revealing = Util::ArmorHasKeyword(a_armor, RevKW);
	const bool concealing = Util::ArmorHasKeyword(a_armor, ConKW);

	auto* file = a_armor->GetFile(0);
	RE::BSFixedString modName = file ? file->GetFilename() : "";
	const bool blacklisted = (a_blacklist.find(modName) != a_blacklist.end());

	bool shouldHaveSlot52 = false;

	if (revealing && !concealing)
		shouldHaveSlot52 = false;
	else if (concealing && !revealing)
		shouldHaveSlot52 = true;
	else if (blacklisted)
		shouldHaveSlot52 = true;
	else
		shouldHaveSlot52 = hasSlot52;

	if (shouldHaveSlot52 && !hasSlot52) {
		biped->SetSlotMask(static_cast<RE::BIPED_MODEL::BipedObjectSlot>(mask | SLOT_52));
		return true;
	}
	else if (!shouldHaveSlot52 && hasSlot52) {
		biped->SetSlotMask(static_cast<RE::BIPED_MODEL::BipedObjectSlot>(mask & ~SLOT_52));
		return true;
	}

	return false;
}

//JSON Shit
static RE::TESObjectARMO* ResolveArmorKey(const std::string& a_key) {

	const auto separator = a_key.find('|');
	if (separator == std::string::npos)
		return nullptr;

	const std::string plugin = a_key.substr(0, separator);
	const std::string editorID = a_key.substr(separator + 1);

	auto* armor = RE::TESForm::LookupByEditorID<RE::TESObjectARMO>(editorID);
	if (!armor)
		return nullptr;

	const auto* file = armor->GetFile(0);

	if (!file || plugin != file->GetFilename())
		return nullptr;

	return armor;
}
static inline std::string MakeArmorKey(const RE::TESObjectARMO* a_armor) {

	if (!a_armor)
		return {};

	std::string editorID = Util::GetEditorIDSafe(a_armor);
	if (editorID.empty())
		return {};

	const auto* file = a_armor->GetFile(0);

	if (!file)
		return {};

	return std::format("{}|{}", file->GetFilename(), editorID);
}
static void SaveArmorOverrides() {

	nlohmann::json j;
	for (const auto& [key, value] : g_armorOverrides) {
		j["armor_overrides"][key] = value;
	}

	const std::filesystem::path path = ARMOPath;
	std::ofstream file(path);
	if (!file.is_open())
		return;

	file << j.dump(4);
}

namespace Armors {

	std::uint8_t g_blacklistCount = 0;

	static std::unordered_set<RE::BSFixedString> LoadBlacklist() {

		const std::filesystem::path path{ BlacklistPath };
		if (!std::filesystem::exists(path)) {

			std::filesystem::create_directories(path.parent_path());
			std::ofstream out(path);
			if (out.is_open()) {

				for (const auto& mod : Util::GetDefaultConcealingPlugins()) {
					out << mod << '\n';
				}
				SKSE::log::info("SOS: Created default Blacklist file.");
			}
		}

		auto validatedList = Util::LoadValidatedModList(BlacklistPath, "Armor Blacklist");
		g_blacklistCount = static_cast<std::uint8_t>(validatedList.size());

		return validatedList;
	}

	void ApplyArmorOverrides() {

		static auto* revealingKW = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(RevKW);
		static auto* concealingKW = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(ConKW);

		if (!revealingKW || !concealingKW)
			return;

		for (const auto& [key, isRevealing] : g_armorOverrides) {
			auto* armor = ResolveArmorKey(key);
			if (!armor)
				continue;

			if (isRevealing) {
				armor->RemoveKeyword(concealingKW);
				armor->AddKeyword(revealingKW);
			}
			else {
				armor->RemoveKeyword(revealingKW);
				armor->AddKeyword(concealingKW);
			}
		}
	}

	void ProcessArmors() {

		if (g_processed)
			return;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler)
			return;

		auto blacklist = LoadBlacklist();

		std::uint32_t count = 0;
		for (auto* armor : dataHandler->GetFormArray<RE::TESObjectARMO>()) {
			if (ModifyArmorWithSOS(armor, blacklist)) {
				count++;
			}
		}

		g_processed = true;
		SKSE::log::info("SOS: Processed {} armors.", count);
	}

	void LoadArmorOverrides() {

		g_armorOverrides.clear();
		const std::filesystem::path path = ARMOPath;

		std::ifstream file(path);
		if (!file.is_open())
			return;

		try {
			nlohmann::json j;
			file >> j;

			if (!j.contains("armor_overrides"))
				return;

			for (auto& [key, value] : j["armor_overrides"].items()) {
				if (!value.is_boolean())
					continue;
				g_armorOverrides[key] = value.get<bool>();
			}
		}
		catch (...) {
			SKSE::log::error("SOS: Failed to parse Armor Overrides JSON");
		}
	}

	static bool ApplyArmorLogic(RE::TESObjectARMO* a_armor) {

		auto* biped = a_armor->As<RE::BGSBipedObjectForm>();
		if (!biped)
			return false;

		SlotMask mask = static_cast<SlotMask>(biped->GetSlotMask());
		if ((mask & SLOT_32) == 0)
			return false;

		if (Util::ArmorHasKeyword(a_armor, RevKW) && (mask & SLOT_52)) {
			biped->SetSlotMask(static_cast<RE::BIPED_MODEL::BipedObjectSlot>(mask & ~SLOT_52));
			return true;
		}

		if (Util::ArmorHasKeyword(a_armor, ConKW) && !(mask & SLOT_52)) {
			biped->SetSlotMask(static_cast<RE::BIPED_MODEL::BipedObjectSlot>(mask | SLOT_52));
			return true;
		}

		return false;
	}

	//Papyrus
	static bool IsRevealingAE(RE::StaticFunctionTag*, RE::TESObjectARMO* a_armor) {

		if (!a_armor)
			return false;
		auto* biped = a_armor->As<RE::BGSBipedObjectForm>();
		return biped && (static_cast<SlotMask>(biped->GetSlotMask()) & SLOT_52) == 0;
	}

	static bool IsConcealingAE(RE::StaticFunctionTag*, RE::TESObjectARMO* a_armor) {

		if (!a_armor)
			return false;
		auto* biped = a_armor->As<RE::BGSBipedObjectForm>();
		return biped && (static_cast<SlotMask>(biped->GetSlotMask()) & SLOT_52) != 0;
	}

	static void SetRevealingAE(RE::StaticFunctionTag*, RE::TESObjectARMO* a_armor) {

		if (!a_armor)
			return;

		const auto key = MakeArmorKey(a_armor);
		if (key.empty())
			return;

		g_armorOverrides[key] = true;

		static auto* concealingKW = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(ConKW);
		static auto* revealingKW = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(RevKW);

		if (concealingKW)
			a_armor->RemoveKeyword(concealingKW);
		if (revealingKW)
			a_armor->AddKeyword(revealingKW);

		SaveArmorOverrides();
		ApplyArmorLogic(a_armor);
	}

	static void SetConcealingAE(RE::StaticFunctionTag*, RE::TESObjectARMO* a_armor) {

		if (!a_armor)
			return;

		const auto key = MakeArmorKey(a_armor);
		if (key.empty())
			return;

		g_armorOverrides[key] = false;

		static auto* concealingKW = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(ConKW);
		static auto* revealingKW = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(RevKW);

		if (concealingKW) a_armor->AddKeyword(concealingKW);
		if (revealingKW) a_armor->RemoveKeyword(revealingKW);

		SaveArmorOverrides();
		ApplyArmorLogic(a_armor);
	}

	static std::uint8_t GetConcealingArmorModsCount(RE::StaticFunctionTag*) {

		return g_blacklistCount;
	}

	static bool FixCustomArmorAE(RE::StaticFunctionTag*, RE::TESObjectARMO* a_armor) {

		return ApplyArmorLogic(a_armor);
	}

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm) {

		if (!a_vm)
			return false;

		a_vm->RegisterFunction("IsRevealingAE", SosPapyrusScript, IsRevealingAE);
		a_vm->RegisterFunction("IsConcealingAE", SosPapyrusScript, IsConcealingAE);
		a_vm->RegisterFunction("FixCustomArmorAE", SosPapyrusScript, FixCustomArmorAE);
		a_vm->RegisterFunction("SetRevealingAE", SosPapyrusScript, SetRevealingAE);
		a_vm->RegisterFunction("SetConcealingAE", SosPapyrusScript, SetConcealingAE);
		a_vm->RegisterFunction("GetConcealingArmorModsCount", SosPapyrusScript, GetConcealingArmorModsCount);
		return true;
	}
}