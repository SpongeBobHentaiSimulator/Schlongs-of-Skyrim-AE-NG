#include "util.h"

#include <array>
#include <cctype>
#include <format>
#include <fstream>
#include <system_error>

RE::TESFaction* g_SexLabGenderFaction = nullptr;
RE::TESFaction* g_schlongifiedFaction = nullptr;
bool g_editorIDsAvailable = false;

namespace {

	// Section of the INI where all the settings live
	constexpr const char* kIniSection = "JSON";

	// INI names of the keys, in the same order as Settings::keys
	constexpr std::array<const char*, Binding::kCount> kKeyIniNames = {
		"PinKey",
		"BendUpKey",
		"BendDownKey",
		"PlayerBendUpKey",
		"PlayerBendDownKey"
	};

	// Forms of other plugins
	constexpr std::string_view kSexLabPlugin = "SexLab.esm";
	constexpr RE::FormID kSexLabGenderFactionID = 0x043A43;

	constexpr std::string_view kSOSPlugin = "Schlongs of Skyrim.esp";
	constexpr RE::FormID kSchlongifiedFactionID = 0x00AFF8;

	// Plugins used to create the default Blacklist and Whitelist files.
	// USSEP is not a dependency and can be removed from here; more files can be added.
	constexpr std::array<std::string_view, 11> kDefaultPlugins = {
		"Skyrim.esm",
		"Update.esm",
		"Dawnguard.esm",
		"HearthFires.esm",
		"Dragonborn.esm",
		"_ResourcePack.esl",
		"ccBGSSSE037-Curios.esl",
		"ccQDRSSE001-SurvivalMode.esl",
		"ccBGSSSE001-Fish.esm",
		"ccBGSSSE025-AdvDSGS.esm",
		"unofficial skyrim special edition patch.esp"
	};

	std::string Trim(const std::string& a_text) {

		const auto first = a_text.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			return {};

		const auto last = a_text.find_last_not_of(" \t\r\n");
		return a_text.substr(first, last - first + 1);
	}

	bool IsModLoaded(std::string_view a_modName) {

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		return dataHandler && dataHandler->LookupModByName(a_modName) != nullptr;
	}
}

// ===============================================================
// SETTINGS
// ===============================================================

void Settings::Load() {

	CSimpleIniA ini;
	ini.SetUnicode();

	// First run: the file is created with the defaults
	if (ini.LoadFile(INIPath) < 0) {
		Save();
		return;
	}

	enabled = ini.GetBoolValue(kIniSection, "Enabled", enabled);
	rank = static_cast<int>(ini.GetLongValue(kIniSection, "Rank", rank));
	chance = static_cast<int>(ini.GetLongValue(kIniSection, "Chance", chance));
	gaussianDeviation = static_cast<float>(ini.GetDoubleValue(kIniSection, "GaussianDeviation", static_cast<double>(gaussianDeviation)));
	whitelistMode = ini.GetBoolValue(kIniSection, "SOS_SchlongEligibleActorMods is a Whitelist", whitelistMode);
	sexLabForceMale = ini.GetBoolValue(kIniSection, "SexLabForceMaleOnSchlong", sexLabForceMale);

	// Scan codes go from 1 to 255, anything else is treated as unbound
	for (std::size_t i = 0; i < keys.size(); ++i) {

		const int key = static_cast<int>(ini.GetLongValue(kIniSection, kKeyIniNames[i], keys[i]));
		keys[i] = (key > 0 && key < 256) ? key : 0;
	}
}

void Settings::Save() const {

	CSimpleIniA ini;
	ini.SetUnicode();

	// Keeps whatever else the file has, a missing file is fine
	ini.LoadFile(INIPath);

	ini.SetBoolValue(kIniSection, "Enabled", enabled);
	ini.SetLongValue(kIniSection, "Rank", rank);
	ini.SetLongValue(kIniSection, "Chance", chance);
	ini.SetDoubleValue(kIniSection, "GaussianDeviation", static_cast<double>(gaussianDeviation));
	ini.SetBoolValue(kIniSection, "SOS_SchlongEligibleActorMods is a Whitelist", whitelistMode);
	ini.SetBoolValue(kIniSection, "SexLabForceMaleOnSchlong", sexLabForceMale);

	for (std::size_t i = 0; i < keys.size(); ++i)
		ini.SetLongValue(kIniSection, kKeyIniNames[i], keys[i]);

	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(INIPath).parent_path(), ec);

	if (ini.SaveFile(INIPath) < 0)
		SKSE::log::error("SOS: Couldn't save '{}'", INIPath);
}

namespace Util {

	// ===============================================================
	// TEXT
	// ===============================================================

	std::string ToString(const RE::BSFixedString& a_text) {

		const char* text = a_text.c_str();
		return text ? text : "";
	}

	std::string ToLower(std::string a_text) {

		std::transform(a_text.begin(), a_text.end(), a_text.begin(),
			[](unsigned char a_char) { return static_cast<char>(std::tolower(a_char)); });

		return a_text;
	}

	// ===============================================================
	// FILES
	// ===============================================================

	std::optional<Json> LoadJson(const std::filesystem::path& a_path) {

		std::ifstream file(a_path);
		if (!file.is_open())
			return std::nullopt;

		try {
			Json json;
			file >> json;
			return json;
		}
		catch (const Json::exception& e) {
			SKSE::log::error("SOS: Error parsing '{}': {}", a_path.string(), e.what());
			return std::nullopt;
		}
	}

	bool SaveJson(const std::filesystem::path& a_path, const Json& a_json, int a_indent) {

		std::error_code ec;
		std::filesystem::create_directories(a_path.parent_path(), ec);

		std::ofstream file(a_path);
		if (!file.is_open()) {
			SKSE::log::error("SOS: Couldn't write '{}'", a_path.string());
			return false;
		}

		file << a_json.dump(a_indent);
		return true;
	}

	ModSet LoadModList(const std::filesystem::path& a_path, std::string_view a_label) {

		// First run: the file is created with the default plugins
		std::error_code ec;
		if (!std::filesystem::exists(a_path, ec)) {

			std::filesystem::create_directories(a_path.parent_path(), ec);

			std::ofstream out(a_path);
			if (out.is_open()) {
				for (const auto& mod : kDefaultPlugins)
					out << mod << '\n';

				SKSE::log::info("SOS: Created default {} file.", a_label);
			}
		}

		ModSet mods;
		std::vector<std::string> missing;

		std::ifstream file(a_path);
		std::string line;
		while (std::getline(file, line)) {

			const std::string modName = Trim(line);
			if (modName.empty())
				continue;

			if (IsModLoaded(modName))
				mods.insert(ToLower(modName));
			else
				missing.push_back(modName);
		}

		if (!missing.empty()) {
			SKSE::log::warn("{} - Missing plugins in Load Order:", a_label);
			for (const auto& modName : missing)
				SKSE::log::warn("  - '{}'", modName);
		}

		return mods;
	}

	bool ModSetContains(const ModSet& a_mods, std::string_view a_modName) {

		return a_mods.contains(ToLower(std::string(a_modName)));
	}

	// ===============================================================
	// FORMS
	// ===============================================================

	std::string GetEditorIDSafe(const RE::TESForm* a_form) {

		if (!a_form)
			return {};

		// Exported by po3_Tweaks, looked up once
		using GetFormEditorIDFunc = const char* (*)(RE::FormID);
		static const auto func = []() -> GetFormEditorIDFunc {
			const HMODULE po3Tweaks = GetModuleHandleW(L"po3_Tweaks");
			return po3Tweaks ? reinterpret_cast<GetFormEditorIDFunc>(GetProcAddress(po3Tweaks, "GetFormEditorID")) : nullptr;
		}();

		if (!func)
			return {};

		const char* editorID = func(a_form->GetFormID());
		return editorID ? editorID : "";
	}

	std::optional<std::string> GetSOSAddonName(RE::BGSListForm* a_list) {

		if (!a_list)
			return std::nullopt;

		const std::string editorID = GetEditorIDSafe(a_list);

		constexpr std::string_view kPrefix = "SOS_Addon_";
		constexpr std::string_view kSuffix = "_CompatibleRaces";

		if (editorID.size() <= kPrefix.size() + kSuffix.size() || !editorID.starts_with(kPrefix) || !editorID.ends_with(kSuffix))
			return std::nullopt;

		auto name = editorID.substr(kPrefix.size(), editorID.size() - kPrefix.size() - kSuffix.size());
		SKSE::log::debug("Addon found: {}", name);

		return name;
	}

	std::string GetRaceKey(RE::TESForm* a_race) {

		if (!a_race)
			return {};

		const char* editorID = a_race->GetFormEditorID();
		if (editorID && editorID[0] != '\0')
			return editorID;

		return std::format("0x{:08X}", a_race->GetFormID());
	}

	// ===============================================================
	// ARMORS AND ACTORS
	// ===============================================================

	bool ArmorHasKeyword(RE::TESObjectARMO* a_armor, const char* a_keyword) {

		return a_armor && a_keyword && a_armor->HasKeywordString(a_keyword);
	}

	SlotMask GetArmorSlots(RE::TESObjectARMO* a_armor) {

		auto* biped = a_armor ? a_armor->As<RE::BGSBipedObjectForm>() : nullptr;
		return biped ? static_cast<SlotMask>(biped->GetSlotMask().get()) : 0;
	}

	void SetArmorSlots(RE::TESObjectARMO* a_armor, SlotMask a_mask) {

		if (auto* biped = a_armor ? a_armor->As<RE::BGSBipedObjectForm>() : nullptr)
			biped->SetSlotMask(static_cast<RE::BIPED_MODEL::BipedObjectSlot>(a_mask));
	}

	std::vector<RE::TESObjectARMO*> GetWornArmorsWithKeyword(RE::Actor* a_actor, const char* a_keyword) {

		std::vector<RE::TESObjectARMO*> worn;
		if (!a_actor || !a_keyword)
			return worn;

		// Only the armors with the keyword are collected from the inventory
		const auto inventory = a_actor->GetInventory([a_keyword](RE::TESBoundObject& a_object) {
			auto* armor = a_object.As<RE::TESObjectARMO>();
			return armor && armor->HasKeywordString(a_keyword);
		});

		for (const auto& [item, data] : inventory) {

			const auto& entry = data.second;
			if (entry && entry->IsWorn())
				if (auto* armor = item->As<RE::TESObjectARMO>())
					worn.push_back(armor);
		}

		return worn;
	}

	bool ActorHasEquippedArmorWithKeyword(RE::Actor* a_actor, const char* a_keyword) {

		return !GetWornArmorsWithKeyword(a_actor, a_keyword).empty();
	}

	void RemoveSOSItemsFromInventory(RE::Actor* a_actor) {

		if (!a_actor)
			return;

		const auto inventory = a_actor->GetInventory([](RE::TESBoundObject& a_object) {
			auto* armor = a_object.As<RE::TESObjectARMO>();
			return armor && armor->HasKeywordString(GenKW);
		});

		for (const auto& [item, data] : inventory) {

			const auto count = data.first;
			if (count > 0)
				a_actor->RemoveItem(item, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
		}
	}

	RE::TESObjectARMO* GetSlot52Armor(RE::Actor* a_actor) {

		return a_actor ? a_actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kModPelvisSecondary) : nullptr;
	}

	bool HasSomethingInSlot52(RE::Actor* a_actor) {

		return GetSlot52Armor(a_actor) != nullptr;
	}

	bool IsFemale(RE::TESNPC* a_base) {

		return a_base && a_base->GetSex() == RE::SEX::kFemale;
	}

	// ===============================================================
	// PLUGIN
	// ===============================================================

	void CheckDependencies() {

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			SKSE::log::error("SOS AE - Critical Error: DataHandler singleton is null.");
			return;
		}

		// Without it the addons can't be detected, because their lists have no editor ID at runtime
		if (!GetModuleHandleW(L"po3_Tweaks"))
			SKSE::log::error("SOS AE - Error: po3_Tweaks is not loaded, editor IDs are not available and no addon will be found.");

		// A keyword of the mod is the proof that the editor IDs were loaded ("Load EditorIDs" can be disabled in po3_Tweaks)
		g_editorIDsAvailable = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(GenKW) != nullptr;
		if (!g_editorIDsAvailable)
			SKSE::log::error("SOS AE - Error: the keyword '{}' was not found by editor ID. Check the 'Load EditorIDs' tweak and the load order.", GenKW);

		// SexLab (optional)
		g_SexLabGenderFaction = dataHandler->LookupForm<RE::TESFaction>(kSexLabGenderFactionID, kSexLabPlugin);
		if (g_SexLabGenderFaction)
			SKSE::log::info("SOS AE - SexLab Compatibility Enabled (Faction cached)");

		// Schlongs of Skyrim
		g_schlongifiedFaction = dataHandler->LookupForm<RE::TESFaction>(kSchlongifiedFactionID, kSOSPlugin);
		if (g_schlongifiedFaction)
			SKSE::log::info("SOS AE - SchlongifiedFaction cached successfully.");
		else if (dataHandler->LookupModByName(kSOSPlugin))
			SKSE::log::error("SOS AE - Error: '{}' is loaded, but FormID 0x{:06X} could not be found inside it. The plugin may be altered or corrupted.", kSOSPlugin, kSchlongifiedFactionID);
		else
			SKSE::log::warn("SOS AE - Warning: '{}' is missing from the current load order.", kSOSPlugin);
	}
}
