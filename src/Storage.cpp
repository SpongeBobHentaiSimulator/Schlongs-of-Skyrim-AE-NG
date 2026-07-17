#include "Storage.h"
#include "Actors.h"
#include <format>

namespace JSON {

	static Json GetCompatibleRacesMap(RE::BGSListForm* a_list) {

		Json racesMap = Json::object();
		if (!a_list)
			return racesMap;

		Json cleanTemplate;
		cleanTemplate["Enabled"] = g_JSONDefaults.value("Enabled", true);
		cleanTemplate["Rank"] = g_JSONDefaults.value("Rank", 10);
		cleanTemplate["Chance"] = g_JSONDefaults.value("Chance", 15);

		for (auto* form : a_list->forms) {

			if (form && form->Is(RE::FormType::Race)) {
				const char* editorID = form->GetFormEditorID();
				std::string key = (editorID && strlen(editorID) > 0)
					? editorID
					: std::format("0x{:08X}", form->GetFormID());

				racesMap[key] = cleanTemplate;

				SKSE::log::debug("Added race: {} (FormID {:08X})", key, form->GetFormID());
			}
		}
		return racesMap;
	}

	void WriteDefaultJson() {

		Json root;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler)
			return;

		for (auto* list : dataHandler->GetFormArray<RE::BGSListForm>()) {

			if (auto nameOpt = Util::GetSOSAddonName(list)) {

				std::string addonName = *nameOpt;

				std::string genderGlobalName = std::format("SOS_Addon_{}_Gender", addonName);
				auto* g = RE::TESForm::LookupByEditorID<RE::TESGlobal>(genderGlobalName);

				int gender = g ? static_cast<int>(std::round(g->value)) : -1;
				bool assigned = false;

				if (gender == 0 || gender == 2) {
					root["gender_groups"]["male"].push_back(addonName);
					assigned = true;
				}

				if (gender == 1 || gender == 2) {
					root["gender_groups"]["female"].push_back(addonName);
					assigned = true;
				}

				if (!assigned)
					root["gender_groups"]["undefined"].push_back(addonName);

				std::string racesListName = std::format("SOS_Addon_{}_CompatibleRaces", addonName);
				auto* raceList = RE::TESForm::LookupByEditorID<RE::BGSListForm>(racesListName);

				root["addon_data"][addonName]["compatible_races"] = GetCompatibleRacesMap(raceList);
			}
		}

		std::ofstream file(JsonPath);
		if (file.is_open())
			file << root.dump(2);
	}

	static void SaveCurrentMemoryToJson() {

		Json root = Json::object();

		if (!Storage::s_genderGroups.male.empty())
			for (const auto& addon : Storage::s_genderGroups.male) {
				root["gender_groups"]["male"].push_back(addon.c_str());
			}
		else
			root["gender_groups"]["male"] = Json::array();

		if (!Storage::s_genderGroups.female.empty())
			for (const auto& addon : Storage::s_genderGroups.female) {
				root["gender_groups"]["female"].push_back(addon.c_str());
			}
		else
			root["gender_groups"]["female"] = Json::array();


		for (const auto& [addonName, addonData] : Storage::s_addons) {
			Json raceMap = Json::object();

			for (const auto& [raceName, raceData] : addonData.compatibleRaces) {
				Json jData;
				jData["Enabled"] = raceData.Enabled;
				jData["Rank"] = static_cast<int>(raceData.Rank);
				jData["Chance"] = static_cast<int>(raceData.Chance);

				raceMap[raceName.c_str()] = jData;
			}

			root["addon_data"][addonName.c_str()]["compatible_races"] = raceMap;
		}

		std::ofstream outFile(JsonPath);
		if (outFile.is_open()) {
			outFile << root.dump(4);
			SKSE::log::debug("SOS: JSON updated with MCM manual overrides (Gender Groups preserved).");
		}
	}
}

namespace Storage {

	std::unordered_map<RE::detail::BSFixedString<char>, AddonData> s_addons;
	GenderGroups s_genderGroups;

	//===============================================================JSON===============================================================

	//Race reading function
	static void ParseRaceEntry(const std::string& a_raceName, const Json& a_entry, AddonData& a_outData) {

		if (!a_entry.is_object()) {
			SKSE::log::warn("Race entry for '{}' is not a JSON object.", a_raceName);
			return;
		}

		try {
			RaceData rd;

			rd.Enabled = a_entry.value("Enabled", true);

			int rawRank = a_entry.value("Rank", 10);
			rd.Rank = (rawRank <= 0) ? -1 : std::clamp(rawRank, 1, 20);

			int rawProb = a_entry.value("Chance", 15);
			rd.Chance = std::clamp(rawProb, 0, 100);

			a_outData.compatibleRaces[a_raceName] = rd;

		}
		catch (const Json::exception& e) {
			SKSE::log::warn("Failed to parse race entry for '{}' - JSON error: {}", a_raceName, e.what());
		}
	}

	//Iterates over each addon entry
	static void ParseAddonData(const Json& a_root) {

		if (!a_root.contains("addon_data")) {
			SKSE::log::error("\"addon_data\" not present in the JSON");
			return;
		}

		for (auto& [addonName, addonObj] : a_root["addon_data"].items()) {

			AddonData data;
			if (addonObj.contains("compatible_races")) {
				for (auto& [raceName, raceArray] : addonObj["compatible_races"].items())
					ParseRaceEntry(raceName, raceArray, data);
			}
			s_addons[addonName] = std::move(data);
		}
	}

	//Same as above, but for gender
	static void ParseGenderGroups(const Json& a_root) {

		if (!a_root.contains("gender_groups")) {
			SKSE::log::error("\"gender_groups\" not present in the JSON");
			return;
		}

		auto& gg = a_root["gender_groups"];
		auto fillGroup = [&](const char* key, std::vector<RE::BSFixedString>& dest) {

			if (gg.contains(key) && gg[key].is_array()) {
				for (auto& item : gg[key]) {
					if (item.is_string()) {
						std::string str = item.get<std::string>();
						dest.emplace_back(str.c_str());
					}
				}
			}
			};

		fillGroup("male", s_genderGroups.male);
		fillGroup("female", s_genderGroups.female);
	}

	bool EnsureAddonDataLoaded() {

		const std::filesystem::path path = JsonPath;

		if (!std::filesystem::exists(path)) {
			SKSE::log::warn("JSON not found, creating default one...");
			JSON::WriteDefaultJson();
		}

		std::ifstream file(path);
		if (!file.is_open()) {
			SKSE::log::error("The JSON couldn't be loaded");
			return false;
		}

		Json root;
		try {
			file >> root;
		}
		catch (const Json::parse_error& e) {
			SKSE::log::critical("Error parsing JSON on byte {}: {}", e.byte, e.what());
			return false;
		}

		// Clear old JSON Data
		s_addons.clear();
		s_genderGroups = { {}, {} };

		// Build the data
		ParseGenderGroups(root);
		ParseAddonData(root);

		SKSE::log::info("Data correctly loaded:");
		SKSE::log::info(" > Addons: {}", s_addons.size());
		SKSE::log::info(" > Male Groups: {}", s_genderGroups.male.size());
		SKSE::log::info(" > Female Groups: {}", s_genderGroups.female.size());

		return true;
	}

	//===============================================================Bones===============================================================

	std::vector<SOSAddonBoneData> Storage::g_addonBoneData;

	//Function to get the bone scales from the globals in the esps and esls
	static std::vector<SOSAddonBoneData> GetBoneScales() {

		std::vector<SOSAddonBoneData> result;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler)
			return result;

		for (auto* list : dataHandler->GetFormArray<RE::BGSListForm>()) {

			auto nameOpt = Util::GetSOSAddonName(list);
			if (!nameOpt)
				continue;

			SOSAddonBoneData data;
			data.addonName = *nameOpt;

			for (std::size_t i = 0; i < sBoneGlobalSuffixes.size(); ++i) {

				std::string editorID = std::format("SOS_Addon_{}_{}", data.addonName, sBoneGlobalSuffixes[i]);

				if (auto* gv = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID))
					data.bones[i] = gv->value;
			}

			result.push_back(std::move(data));
		}

		return result;
	}

	void LoadAddonBoneData() {

		g_addonBoneData = GetBoneScales();
		SKSE::log::info("Loaded bone data for {} addons", g_addonBoneData.size());
	}

	const SOSAddonBoneData* GetAddonBoneData(RE::BSFixedString addonName) {

		for (const auto& data : g_addonBoneData) {
			if (data.addonName == addonName.c_str())
				return &data;
		}
		return nullptr;
	}

	//===============================================================NPCs===============================================================

	std::unordered_map<RE::FormID, NPCData> s_npcData;

	const AddonMap& GetAddons() {

		return s_addons;
	}

	const GenderGroups& GetGenderGroups() {

		return s_genderGroups;
	}

	bool HasNPCData(RE::FormID a_baseID) {

		return s_npcData.find(a_baseID) != s_npcData.end();
	}

	NPCData* GetNPCData(RE::FormID a_baseID) {

		auto it = s_npcData.find(a_baseID);
		if (it != s_npcData.end())
			return &(it->second);

		return nullptr;
	}

	void SetNPCAddonData(RE::FormID a_baseID, const RE::BSFixedString& a_addonName, std::uint8_t a_rank) {

		auto& data = s_npcData[a_baseID];
		data.addonName = a_addonName;
		data.rank = a_rank;
	}

	void ClearNPCData(RE::FormID a_baseID) {

		s_npcData.erase(a_baseID);
	}

	std::uint8_t GetNPCRank(RE::FormID a_baseID) {

		auto* data = GetNPCData(a_baseID);
		return data ? data->rank : 10;
	}

	const RE::BSFixedString& GetNPCAddonName(RE::FormID a_baseID) {

		static const RE::BSFixedString empty{};
		auto* data = GetNPCData(a_baseID);

		return data ? data->addonName : empty;
	}

	//==================================== Serialization ================================================================

	uint32_t kSerializationVersion = 3;
	uint32_t kDataKey = 'NPCs';

	void SaveCallback(SKSE::SerializationInterface* a_intfc) {

		if (!a_intfc->OpenRecord(kDataKey, kSerializationVersion)) {
			SKSE::log::error("The record could not be opened to save data");
			return;
		}

		uint32_t numNPCs = static_cast<uint32_t>(s_npcData.size());
		a_intfc->WriteRecordData(numNPCs);

		for (const auto& [baseID, data] : s_npcData) {

			a_intfc->WriteRecordData(baseID);

			const char* nameStr = data.addonName.c_str();
			uint32_t nameLen = static_cast<uint32_t>(nameStr ? strlen(nameStr) : 0);

			a_intfc->WriteRecordData(nameLen);
			if (nameLen > 0)
				a_intfc->WriteRecordData(nameStr, nameLen);

			a_intfc->WriteRecordData(data.rank);
		}

		SKSE::log::info("Saved {} NPCs in cosave", numNPCs);
	}

	void LoadCallback(SKSE::SerializationInterface* a_intfc) {

		uint32_t type, version, length;

		while (a_intfc->GetNextRecordInfo(type, version, length)) {

			if (type != kDataKey)
				continue;

			if (version != kSerializationVersion) {
				SKSE::log::warn("Unsupported serialization version: {}", version);
				continue;
			}

			uint32_t numNPCs;
			if (!a_intfc->ReadRecordData(numNPCs)) {
				SKSE::log::error("Failed to read number of NPCs");
				continue;
			}

			for (uint32_t i = 0; i < numNPCs; ++i) {

				RE::FormID oldBaseID;
				if (!a_intfc->ReadRecordData(oldBaseID)) break;

				uint32_t nameLen;
				if (!a_intfc->ReadRecordData(nameLen)) break;

				std::string addonName;
				if (nameLen > 0) {
					addonName.resize(nameLen);
					a_intfc->ReadRecordData(addonName.data(), nameLen);
				}

				uint8_t rank;
				if (!a_intfc->ReadRecordData(rank)) break;

				RE::FormID newBaseID;
				if (a_intfc->ResolveFormID(oldBaseID, newBaseID)) {
					s_npcData[newBaseID] = {
						RE::BSFixedString(addonName.c_str()),
						rank
					};
				}
			}
		}

		SKSE::log::info("LoadCallback: restored {} NPC entries from cosave", s_npcData.size());
		Actors::ApplyNPCOverrides();
		SKSE::log::info("LoadCallback: NPC overrides re-applied ({} entries)", g_npcOverrides.size());
	}

	void RevertCallback(SKSE::SerializationInterface*) {
		SKSE::log::info("Reverting Storage: clearing {} entries", s_npcData.size());
		s_npcData.clear();
	}

	//====================================Papyrus================================================================

	static std::uint8_t GetActorRank(RE::StaticFunctionTag*, RE::Actor* a_actor) {

		if (!a_actor)
			return 10;

		auto* base = a_actor->GetActorBase();
		return base ? Storage::GetNPCRank(base->GetFormID()) : 10;
	}

	static RE::BSFixedString GetActorAddonName(RE::StaticFunctionTag*, RE::Actor* a_actor) {

		if (!a_actor)
			return RE::BSFixedString("");

		auto* base = a_actor->GetActorBase();
		if (!base)
			return RE::BSFixedString("");

		return Storage::GetNPCAddonName(base->GetFormID());
	}

	static void ClearNPCDataAE(RE::StaticFunctionTag*, RE::Actor* a_actor) {

		if (!a_actor)
			return;

		auto* base = a_actor->GetActorBase();
		if (!base)
			return;

		RE::FormID baseID = base->GetFormID();
		Storage::ClearNPCData(baseID);

		SKSE::log::debug("NPC data cleared for Base: {:08X}", baseID);
	}

	static std::uint16_t GetStoredNPCCount(RE::StaticFunctionTag*) {

		return static_cast<std::uint16_t>(Storage::s_npcData.size());
	}

	static void RebuildAddonJson(RE::StaticFunctionTag*) {

		JSON::WriteDefaultJson();
		if (!EnsureAddonDataLoaded())
			return;

		LoadAddonBoneData();
		SKSE::log::info("MCM Request: Success. All addon data synchronized.");
	}

	static std::uint16_t GetMaleAddonCount(RE::StaticFunctionTag*) {

		return static_cast<std::uint16_t>(Storage::GetGenderGroups().male.size());
	}

	static std::uint16_t GetFemaleAddonCount(RE::StaticFunctionTag*) {

		return static_cast<std::uint16_t>(Storage::GetGenderGroups().female.size());
	}

	//This part is from 2.0 to have the MCM from the old SOS
	static std::vector<std::string> GetAddonCompatibleRaces(RE::StaticFunctionTag*, RE::BSFixedString a_addonName) {

		std::vector<std::string> results;

		auto itAddon = Storage::s_addons.find(a_addonName);
		if (itAddon != Storage::s_addons.end())
			for (const auto& [raceName, data] : itAddon->second.compatibleRaces) {
				results.push_back(raceName.c_str());
			}

		return results;
	}

	static int GetAddonRaceRank(RE::StaticFunctionTag*, RE::BSFixedString a_addon, RE::BSFixedString a_race) {

		if (Storage::s_addons.contains(a_addon) && Storage::s_addons[a_addon].compatibleRaces.contains(a_race))
			return Storage::s_addons[a_addon].compatibleRaces[a_race].Rank;
		return 10;
	}

	static int GetAddonRaceChance(RE::StaticFunctionTag*, RE::BSFixedString a_addon, RE::BSFixedString a_race) {

		if (Storage::s_addons.contains(a_addon) && Storage::s_addons[a_addon].compatibleRaces.contains(a_race))
			return Storage::s_addons[a_addon].compatibleRaces[a_race].Chance;
		return 0;
	}

	static bool GetAddonRaceEnabled(RE::StaticFunctionTag*, RE::BSFixedString a_addon, RE::BSFixedString a_race) {

		if (Storage::s_addons.contains(a_addon) && Storage::s_addons[a_addon].compatibleRaces.contains(a_race))
			return Storage::s_addons[a_addon].compatibleRaces[a_race].Enabled;
		return false;
	}

	static void SetAddonRaceValues(RE::StaticFunctionTag*, RE::BSFixedString a_addon, RE::BSFixedString a_race, int a_rank, int a_chance, bool a_enabled) {

		auto itAddon = Storage::s_addons.find(a_addon);
		if (itAddon != Storage::s_addons.end()) {

			auto itRace = itAddon->second.compatibleRaces.find(a_race);
			if (itRace != itAddon->second.compatibleRaces.end()) {

				itRace->second.Rank = static_cast<std::int8_t>(a_rank);
				itRace->second.Chance = static_cast<std::uint8_t>(a_chance);
				itRace->second.Enabled = a_enabled;

				SKSE::log::debug("MCM Update: {} for race {} -> Rank:{}, Chance:{}, Enabled:{}",
					a_addon.c_str(), a_race.c_str(), a_rank, a_chance, a_enabled);
			}
		}
	}

	static void SaveMCMChanges(RE::StaticFunctionTag*) {
		//Wrapper just in case
		JSON::SaveCurrentMemoryToJson();
	}

	static std::vector<RE::BSFixedString> GetLoadedAddonNames(RE::StaticFunctionTag*) {
		std::vector<RE::BSFixedString> names;
		for (const auto& [name, data] : Storage::s_addons) {
			names.push_back(name);
		}
		return names;
	}

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm) {

		if (!a_vm)
			return false;

		a_vm->RegisterFunction("GetActorRank", SosPapyrusScript, GetActorRank);
		a_vm->RegisterFunction("GetActorAddonName", SosPapyrusScript, GetActorAddonName);
		a_vm->RegisterFunction("ClearNPCDataAE", SosPapyrusScript, ClearNPCDataAE);
		a_vm->RegisterFunction("GetStoredNPCCount", SosPapyrusScript, GetStoredNPCCount);
		a_vm->RegisterFunction("RebuildAddonJson", SosPapyrusScript, RebuildAddonJson);

		a_vm->RegisterFunction("GetMaleAddonCount", SosPapyrusScript, GetMaleAddonCount);
		a_vm->RegisterFunction("GetFemaleAddonCount", SosPapyrusScript, GetFemaleAddonCount);
		a_vm->RegisterFunction("GetAddonCompatibleRaces", SosPapyrusScript, GetAddonCompatibleRaces);

		a_vm->RegisterFunction("GetAddonRaceRank", SosPapyrusScript, GetAddonRaceRank);
		a_vm->RegisterFunction("GetAddonRaceChance", SosPapyrusScript, GetAddonRaceChance);
		a_vm->RegisterFunction("GetAddonRaceEnabled", SosPapyrusScript, GetAddonRaceEnabled);

		a_vm->RegisterFunction("SetAddonRaceValues", SosPapyrusScript, SetAddonRaceValues);
		a_vm->RegisterFunction("SaveMCMChanges", SosPapyrusScript, SaveMCMChanges);
		a_vm->RegisterFunction("GetLoadedAddonNames", SosPapyrusScript, GetLoadedAddonNames);

		return true;
	}
}