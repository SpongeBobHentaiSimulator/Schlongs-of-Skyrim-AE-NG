#include "util.h"

RE::TESFaction* g_SexLabGenderFaction = nullptr;

//Trim the modname
static std::string Trim(const std::string& str) {

	const auto strBegin = str.find_first_not_of(" \t\r\n");
	if (strBegin == std::string::npos)
		return "";

	const auto strEnd = str.find_last_not_of(" \t\r\n");
	const auto strRange = strEnd - strBegin + 1;

	return str.substr(strBegin, strRange);
}

//CHeck if a mod is loaded
static bool IsModLoaded(std::string_view a_modName) {

	auto* dataHandler = RE::TESDataHandler::GetSingleton();
	if (!dataHandler)
		return false;

	return dataHandler->LookupModByName(a_modName) != nullptr;
}

/*Mods that in general will have things changed by default, Is just used when making the default files, UESP is not a dependency
and can be removed from here; more files can be added here*/
static constexpr std::array<std::string_view, 11> kDefaultConcealingPlugins = {
	"Skyrim.esm",
	"Update.esm",
	"Dawnguard.esm",
	"Hearthfires.esm",
	"Dragonborn.esm",
	"_ResourcePack.esl",
	"ccBGSSSE037-Curios.esl",
	"ccQDRSSE001-SurvivalMode.esl",
	"ccBGSSSE001-Fish.esm",
	"ccBGSSSE025-AdvDSGS.esm",
	"unofficial skyrim special edition patch.esp"
};

namespace Util {

	void CheckDependencies() {

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (dataHandler) {
			//Sexlab
			g_SexLabGenderFaction = dataHandler->LookupForm<RE::TESFaction>(0x043A43, "SexLab.esm");
			if (g_SexLabGenderFaction)
				SKSE::log::info("SOS AE - SexLab Compatibility Enabled (Faction cached)");
		}
	}

	std::unordered_set<RE::BSFixedString> LoadValidatedModList(const RE::BSFixedString& a_filePath, const RE::BSFixedString& a_listName) {

		std::unordered_set<RE::BSFixedString> validMods;
		std::vector<RE::BSFixedString> invalidMods;

		std::ifstream file(a_filePath.c_str());
		if (!file.is_open())
			return validMods;

		std::string line;
		while (std::getline(file, line)) {

			std::string modNameStr = Trim(line);
			if (modNameStr.empty())
				continue;

			if (IsModLoaded(modNameStr))
				validMods.insert(modNameStr);
			else
				invalidMods.push_back(modNameStr);
		}

		if (!invalidMods.empty()) {
			SKSE::log::warn("{} - Missing plugins in Load Order:", a_listName.c_str());
			for (const auto& mod : invalidMods) {
				SKSE::log::warn("  - '{}'", mod.c_str());
			}
		}

		return validMods;
	}

	const std::array<std::string_view, 11>& GetDefaultConcealingPlugins() {

		return kDefaultConcealingPlugins;
	}

	bool ArmorHasKeyword(RE::TESObjectARMO* a_armor, const char* a_keyword) {

		return a_armor && a_keyword && a_armor->HasKeywordString(a_keyword);
	}

	void RemoveSOSItemsFromInventory(RE::Actor* a_actor) {

		if (!a_actor)
			return;

		auto inventory = a_actor->GetInventory();

		for (auto& [form, data] : inventory) {

			if (!form)
				continue;

			auto* armor = form->As<RE::TESObjectARMO>();
			if (!armor)
				continue;

			if (!ArmorHasKeyword(armor, GenKW))
				continue;

			const auto count = data.first;
			if (count <= 0)
				continue;

			a_actor->RemoveItem(
				armor,
				count,
				RE::ITEM_REMOVE_REASON::kRemove,
				nullptr,
				nullptr);
		}
	}

	static bool IsArmorInSet(RE::TESObjectARMO* a_armor, const std::unordered_set<RE::BSFixedString>& a_plugins) {

		if (!a_armor) 
			return false;

		const auto* file = a_armor->GetFile(0);
		if (!file) 
			return false;

		RE::BSFixedString filename(file->GetFilename());

		return a_plugins.contains(filename);
	}

	std::string GetEditorIDSafe(const RE::TESForm* a_form) {

		if (!a_form)
			return {};

		static HMODULE po3Tweaks = GetModuleHandleW(L"po3_Tweaks");
		if (!po3Tweaks)
			return {};

		static auto func = reinterpret_cast<_GetFormEditorID>(GetProcAddress(po3Tweaks, "GetFormEditorID"));
		if (!func)
			return {};

		const char* id = func(a_form->GetFormID());
		return id ? id : "";
	}

	std::optional<std::string> GetSOSAddonName(RE::BGSListForm* a_list) {

		if (!a_list)
			return std::nullopt;

		const std::string editorID = Util::GetEditorIDSafe(a_list);
		if (editorID.empty())
			return std::nullopt;

		constexpr std::string_view kPrefix = "SOS_Addon_";
		constexpr std::string_view kSuffix = "_CompatibleRaces";

		std::string id{ editorID };

		if (id.starts_with(kPrefix) && id.ends_with(kSuffix)) {

			auto name = id.substr(kPrefix.size(), id.size() - kPrefix.size() - kSuffix.size());
			if (!name.empty()) {

				SKSE::log::debug("Addon found: {}", name);
				return std::string(name);
			}
		}

		return std::nullopt;
	}

	bool HasSomethingInSlot52(RE::Actor* a_actor) {

		if (!a_actor)
			return false;

		return a_actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kModPelvisSecondary) != nullptr;
	}

	bool IsMale(RE::TESNPC* a_base) {

		if (!a_base)
			return false;

		return a_base->GetSex() == RE::SEX::kMale;
	}

	bool IsFemale(RE::TESNPC* a_base) {

		if (!a_base)
			return false;

		return a_base->GetSex() == RE::SEX::kFemale;
	}

	void ReadJSONDefaultsFromINI() {

		CSimpleIniA ini;
		ini.SetUnicode();

		bool defaultEnabled = g_JSONDefaults.value("Enabled", true);
		int defaultRank = g_JSONDefaults.value("Rank", 10);
		int defaultChance = g_JSONDefaults.value("Chance", 15);
		float defaultDeviation = g_JSONDefaults.value("GaussianDeviation", 1.0f);
		bool defaultWhiteList = g_JSONDefaults.value("SOS_SchlongEligibleActorMods is a Whitelist", true);

		if (ini.LoadFile(INIPath) < 0) {

			ini.SetBoolValue("JSON", "Enabled", defaultEnabled);
			ini.SetLongValue("JSON", "Rank", defaultRank);
			ini.SetLongValue("JSON", "Chance", defaultChance);
			ini.SetDoubleValue("JSON", "GaussianDeviation", static_cast<double>(defaultDeviation));
			ini.SetBoolValue("JSON", "SOS_SchlongEligibleActorMods is a Whitelist", defaultWhiteList);

			ini.SaveFile(INIPath);

			g_isWhitelistMode = defaultWhiteList;
			g_SOSGaussianDeviation = defaultDeviation;
			return;
		}

		g_JSONDefaults["Enabled"] = ini.GetBoolValue("JSON", "Enabled", defaultEnabled);
		g_JSONDefaults["Rank"] = static_cast<int>(ini.GetLongValue("JSON", "Rank", defaultRank));
		g_JSONDefaults["Chance"] = static_cast<int>(ini.GetLongValue("JSON", "Chance", defaultChance));

		double readValue = ini.GetDoubleValue("JSON", "GaussianDeviation", static_cast<double>(defaultDeviation));
		g_SOSGaussianDeviation = static_cast<float>(readValue);
		g_JSONDefaults["GaussianDeviation"] = g_SOSGaussianDeviation;

		g_isWhitelistMode = ini.GetBoolValue("JSON", "SOS_SchlongEligibleActorMods is a Whitelist", defaultWhiteList);
		g_JSONDefaults["SOS_SchlongEligibleActorMods is a Whitelist"] = g_isWhitelistMode;
	}

	//Papyrus functions
	static void RemoveSOSItems(RE::StaticFunctionTag*, RE::Actor* a_actor) {
		//This is a wrapper for papyrus
		return RemoveSOSItemsFromInventory(a_actor);
	}

	static bool GetDefaultEnabled(RE::StaticFunctionTag*) {
		return g_JSONDefaults.value("Enabled", true);
	}

	static int GetDefaultRank(RE::StaticFunctionTag*) {
		return g_JSONDefaults.value("Rank", 10);
	}

	static int GetDefaultChance(RE::StaticFunctionTag*) {
		return g_JSONDefaults.value("Chance", 20);
	}

	static void SetDefaultConfig(RE::StaticFunctionTag*, bool a_enabled, int a_rank, int a_chance) {

		g_JSONDefaults["Enabled"] = a_enabled;
		g_JSONDefaults["Rank"] = a_rank;
		g_JSONDefaults["Chance"] = a_chance;

		CSimpleIniA ini;
		ini.SetUnicode();
		if (ini.LoadFile(INIPath) >= 0) {

			ini.SetBoolValue("JSON", "Enabled", a_enabled);
			ini.SetLongValue("JSON", "Rank", a_rank);
			ini.SetLongValue("JSON", "Chance", a_chance);
			ini.SaveFile(INIPath);
			SKSE::log::info("SOS: Global defaults updated via MCM (Enabled: {}, Rank: {}, Chance: {}).", a_enabled, a_rank, a_chance);
		}
	}
	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm) {

		if (!a_vm)
			return false;

		a_vm->RegisterFunction("RemoveSOSItems", SosPapyrusScript, RemoveSOSItems);

		a_vm->RegisterFunction("GetDefaultEnabled", SosPapyrusScript, GetDefaultEnabled);
		a_vm->RegisterFunction("GetDefaultRank", SosPapyrusScript, GetDefaultRank);
		a_vm->RegisterFunction("GetDefaultChance", SosPapyrusScript, GetDefaultChance);

		a_vm->RegisterFunction("SetDefaultConfig", SosPapyrusScript, SetDefaultConfig);
		return true;
	}
}