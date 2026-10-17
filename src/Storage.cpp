#include "Storage.h"

#include "Actors.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <mutex>
#include <unordered_map>

namespace Storage {

	// ===============================================================
	// STATE
	// ===============================================================

	namespace {

		struct AddonData {

			std::unordered_map<std::string, RaceData> compatibleRaces;
		};

		struct GenderGroups {

			std::vector<std::string> male;
			std::vector<std::string> female;
		};

		// Protects every variable below. The public functions take it, the helpers of the anonymous
		// namespaces expect the caller to already hold it (the mutex is not recursive).
		std::mutex s_mutex;

		// SOS_AddonData.json
		std::unordered_map<std::string, AddonData> s_addons;
		GenderGroups s_genderGroups;

		// Bone scales of every addon, taken from the globals of the plugins
		constexpr std::array<const char*, 8> kBoneGlobalSuffixes = {
			"GenBase", "GenScrotum", "Gen01", "Gen02", "Gen03", "Gen04", "Gen05", "Gen06"
		};
		std::unordered_map<std::string, BoneScales> s_bones;

		// Addon assigned to every NPC base, saved in the cosave
		std::unordered_map<RE::FormID, NPCData> s_npcData;
	}

	// ===============================================================
	// ADDON DATA (SOS_AddonData.json)
	// ===============================================================

	namespace {

		// One entry per race of the list of compatible races of an addon, all with the default values
		Json BuildRaceMap(RE::BGSListForm* a_list) {

			Json races = Json::object();
			if (!a_list)
				return races;

			const Json entry = {
				{ "Enabled", g_settings.enabled },
				{ "Rank", g_settings.rank },
				{ "Chance", g_settings.chance }
			};

			for (auto* form : a_list->forms) {
				if (form && form->Is(RE::FormType::Race))
					races[Util::GetRaceKey(form)] = entry;
			}

			return races;
		}

		// Creates the JSON with the addons found in the plugins
		void WriteDefaultJson() {

			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler)
				return;

			Json root = Json::object();
			root["gender_groups"] = { { "male", Json::array() }, { "female", Json::array() } };
			root["addon_data"] = Json::object();

			for (auto* list : dataHandler->GetFormArray<RE::BGSListForm>()) {

				const auto addonName = Util::GetSOSAddonName(list);
				if (!addonName)
					continue;

				// The gender is a global of the addon: 0 = male, 1 = female, 2 = both
				auto* genderGlobal = RE::TESForm::LookupByEditorID<RE::TESGlobal>(std::format("SOS_Addon_{}_Gender", *addonName));
				const int gender = genderGlobal ? static_cast<int>(std::round(genderGlobal->value)) : -1;

				const bool isMale = gender == 0 || gender == 2;
				const bool isFemale = gender == 1 || gender == 2;

				if (isMale)
					root["gender_groups"]["male"].push_back(*addonName);
				if (isFemale)
					root["gender_groups"]["female"].push_back(*addonName);
				if (!isMale && !isFemale)
					root["gender_groups"]["undefined"].push_back(*addonName);

				auto* races = RE::TESForm::LookupByEditorID<RE::BGSListForm>(std::format("SOS_Addon_{}_CompatibleRaces", *addonName));
				root["addon_data"][*addonName]["compatible_races"] = BuildRaceMap(races);
			}

			Util::SaveJson(JsonPath, root);
		}

		// Writes the data in memory (with the menu changes) to the JSON
		void WriteAddonData() {

			// Nothing to write after ClearAll(), the JSON on disk is worth more than an empty state
			if (s_addons.empty())
				return;

			// Starts from the current file so anything else in it (like the "undefined" group) is kept
			Json root = Util::LoadJson(JsonPath).value_or(Json::object());
			if (!root.is_object())
				root = Json::object();

			root["gender_groups"]["male"] = s_genderGroups.male;
			root["gender_groups"]["female"] = s_genderGroups.female;

			Json addons = Json::object();
			for (const auto& [addonName, addon] : s_addons) {

				Json races = Json::object();
				for (const auto& [raceName, race] : addon.compatibleRaces) {
					races[raceName] = {
						{ "Enabled", race.enabled },
						{ "Rank", static_cast<int>(race.rank) },
						{ "Chance", static_cast<int>(race.chance) }
					};
				}

				addons[addonName]["compatible_races"] = std::move(races);
			}
			root["addon_data"] = std::move(addons);

			if (Util::SaveJson(JsonPath, root))
				SKSE::log::debug("SOS: JSON updated with the menu changes.");
		}

		void ParseRaces(const Json& a_races, AddonData& a_outData) {

			for (auto& [raceName, entry] : a_races.items()) {

				if (!entry.is_object()) {
					SKSE::log::warn("SOS: Race entry for '{}' is not a JSON object.", raceName);
					continue;
				}

				try {
					RaceData race;
					race.enabled = entry.value("Enabled", race.enabled);
					race.rank = Util::NormalizeRank(entry.value("Rank", static_cast<int>(race.rank)));
					race.chance = Util::NormalizeChance(entry.value("Chance", static_cast<int>(race.chance)));

					a_outData.compatibleRaces[raceName] = race;
				}
				catch (const Json::exception& e) {
					SKSE::log::warn("SOS: Failed to parse the race entry for '{}': {}", raceName, e.what());
				}
			}
		}

		void ParseAddonData(const Json& a_root) {

			if (!a_root.contains("addon_data")) {
				SKSE::log::error("SOS: \"addon_data\" not present in the JSON");
				return;
			}

			for (auto& [addonName, addonObj] : a_root["addon_data"].items()) {

				AddonData data;
				if (addonObj.contains("compatible_races"))
					ParseRaces(addonObj["compatible_races"], data);

				s_addons[addonName] = std::move(data);
			}
		}

		void ParseGenderGroups(const Json& a_root) {

			if (!a_root.contains("gender_groups")) {
				SKSE::log::error("SOS: \"gender_groups\" not present in the JSON");
				return;
			}

			const auto& groups = a_root["gender_groups"];

			auto fillGroup = [&groups](const char* a_key, std::vector<std::string>& a_out) {

				if (!groups.contains(a_key) || !groups[a_key].is_array())
					return;

				for (const auto& item : groups[a_key]) {
					if (item.is_string())
						a_out.push_back(item.get<std::string>());
				}
			};

			fillGroup("male", s_genderGroups.male);
			fillGroup("female", s_genderGroups.female);
		}

		// Reads the JSON into memory
		bool LoadAddonData() {

			const auto root = Util::LoadJson(JsonPath);
			if (!root) {
				SKSE::log::error("SOS: The JSON couldn't be loaded");
				return false;
			}

			s_addons.clear();
			s_genderGroups = {};

			ParseGenderGroups(*root);
			ParseAddonData(*root);

			SKSE::log::info("SOS: Data correctly loaded:");
			SKSE::log::info(" > Addons: {}", s_addons.size());
			SKSE::log::info(" > Male Groups: {}", s_genderGroups.male.size());
			SKSE::log::info(" > Female Groups: {}", s_genderGroups.female.size());

			return true;
		}

		// The scales are globals of each addon: SOS_Addon_<name>_<GenBase, GenScrotum, Gen01 ...>
		void LoadBones() {

			s_bones.clear();

			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler)
				return;

			for (auto* list : dataHandler->GetFormArray<RE::BGSListForm>()) {

				const auto addonName = Util::GetSOSAddonName(list);
				if (!addonName)
					continue;

				BoneScales bones;
				bones.fill(1.0f);

				for (std::size_t i = 0; i < bones.size(); ++i) {

					const auto editorID = std::format("SOS_Addon_{}_{}", *addonName, kBoneGlobalSuffixes[i]);
					if (auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID))
						bones[i] = global->value;
				}

				s_bones[*addonName] = bones;
			}

			SKSE::log::info("SOS: Loaded bone data for {} addons", s_bones.size());
		}

		RaceData* FindRace(const std::string& a_addon, const std::string& a_race) {

			const auto addon = s_addons.find(a_addon);
			if (addon == s_addons.end())
				return nullptr;

			const auto race = addon->second.compatibleRaces.find(a_race);
			return race != addon->second.compatibleRaces.end() ? &race->second : nullptr;
		}

		// Runs a_function on every race of an addon, or of every addon if the name is empty
		template <class Function>
		void ForEachRace(const std::string& a_addon, Function a_function) {

			if (a_addon.empty()) {
				for (auto& [addonName, addon] : s_addons)
					for (auto& [raceName, race] : addon.compatibleRaces)
						a_function(race);

				return;
			}

			const auto it = s_addons.find(a_addon);
			if (it == s_addons.end())
				return;

			for (auto& [raceName, race] : it->second.compatibleRaces)
				a_function(race);
		}
	}

	bool EnsureAddonDataLoaded() {

		std::lock_guard lock(s_mutex);

		std::error_code ec;
		if (!std::filesystem::exists(JsonPath, ec)) {
			SKSE::log::warn("SOS: JSON not found, creating the default one...");
			WriteDefaultJson();
		}

		return LoadAddonData();
	}

	void SaveAddonData() {

		std::lock_guard lock(s_mutex);
		WriteAddonData();
	}

	bool RebuildAddonData() {

		std::lock_guard lock(s_mutex);

		WriteDefaultJson();
		if (!LoadAddonData())
			return false;

		LoadBones();
		SKSE::log::info("SOS: All the addon data was synchronized with the plugins.");
		return true;
	}

	bool HasAddon(const std::string& a_addon) {

		std::lock_guard lock(s_mutex);
		return s_addons.contains(a_addon);
	}

	std::vector<std::string> GetAddonNames() {

		std::lock_guard lock(s_mutex);

		std::vector<std::string> names;
		names.reserve(s_addons.size());

		for (const auto& [name, addon] : s_addons)
			names.push_back(name);

		std::sort(names.begin(), names.end());
		return names;
	}

	std::vector<std::string> GetAddonNamesForSex(RE::SEX a_sex) {

		std::lock_guard lock(s_mutex);

		std::vector<std::string> names;

		const std::vector<std::string>* group = nullptr;
		if (a_sex == RE::SEX::kMale)
			group = &s_genderGroups.male;
		else if (a_sex == RE::SEX::kFemale)
			group = &s_genderGroups.female;

		if (!group)
			return names;

		for (const auto& name : *group) {
			if (s_addons.contains(name))
				names.push_back(name);
		}

		return names;
	}

	std::vector<std::pair<std::string, RaceData>> GetAddonRaces(const std::string& a_addon) {

		std::lock_guard lock(s_mutex);

		std::vector<std::pair<std::string, RaceData>> races;

		const auto it = s_addons.find(a_addon);
		if (it == s_addons.end())
			return races;

		races.reserve(it->second.compatibleRaces.size());
		for (const auto& [raceName, race] : it->second.compatibleRaces)
			races.emplace_back(raceName, race);

		std::sort(races.begin(), races.end(), [](const auto& a_left, const auto& a_right) {
			return a_left.first < a_right.first;
		});

		return races;
	}

	std::optional<RaceData> GetRaceData(const std::string& a_addon, const std::string& a_race) {

		std::lock_guard lock(s_mutex);

		const auto* race = FindRace(a_addon, a_race);
		if (!race)
			return std::nullopt;

		return *race;
	}

	bool SetRaceValues(const std::string& a_addon, const std::string& a_race, int a_rank, int a_chance, bool a_enabled) {

		std::lock_guard lock(s_mutex);

		auto* race = FindRace(a_addon, a_race);
		if (!race)
			return false;

		race->rank = Util::NormalizeRank(a_rank);
		race->chance = Util::NormalizeChance(a_chance);
		race->enabled = a_enabled;
		return true;
	}

	void SetRankForAll(const std::string& a_addon, int a_rank) {

		std::lock_guard lock(s_mutex);

		const auto rank = Util::NormalizeRank(a_rank);
		ForEachRace(a_addon, [rank](RaceData& a_race) { a_race.rank = rank; });

		SKSE::log::debug("SOS: Rank set to {} for '{}' via the menu.", rank, a_addon.empty() ? "every addon" : a_addon);
	}

	void SetChanceForAll(const std::string& a_addon, int a_chance) {

		std::lock_guard lock(s_mutex);

		const auto chance = Util::NormalizeChance(a_chance);
		ForEachRace(a_addon, [chance](RaceData& a_race) { a_race.chance = chance; });

		SKSE::log::debug("SOS: Chance set to {}% for '{}' via the menu.", chance, a_addon.empty() ? "every addon" : a_addon);
	}

	void SetEnabledForAll(const std::string& a_addon, bool a_enabled) {

		std::lock_guard lock(s_mutex);

		ForEachRace(a_addon, [a_enabled](RaceData& a_race) { a_race.enabled = a_enabled; });

		SKSE::log::debug("SOS: Enabled set to {} for '{}' via the menu.", a_enabled, a_addon.empty() ? "every addon" : a_addon);
	}

	// ===============================================================
	// BONE SCALING DATA
	// ===============================================================

	void LoadAddonBoneData() {

		std::lock_guard lock(s_mutex);
		LoadBones();
	}

	std::optional<BoneScales> GetAddonBoneData(const std::string& a_addon) {

		std::lock_guard lock(s_mutex);

		const auto it = s_bones.find(a_addon);
		if (it == s_bones.end())
			return std::nullopt;

		return it->second;
	}

	// ===============================================================
	// NPC RUNTIME DATA
	// ===============================================================

	bool HasNPCData(RE::FormID a_baseID) {

		std::lock_guard lock(s_mutex);
		return s_npcData.contains(a_baseID);
	}

	std::optional<NPCData> GetNPCData(RE::FormID a_baseID) {

		std::lock_guard lock(s_mutex);

		const auto it = s_npcData.find(a_baseID);
		if (it == s_npcData.end())
			return std::nullopt;

		return it->second;
	}

	void SetNPCAddonData(RE::FormID a_baseID, const std::string& a_addonName, std::uint8_t a_rank) {

		std::lock_guard lock(s_mutex);

		auto& data = s_npcData[a_baseID];
		data.addonName = a_addonName;
		data.rank = a_rank;
	}

	bool TrySetNPCAddonData(RE::FormID a_baseID, const std::string& a_addonName, std::uint8_t a_rank) {

		std::lock_guard lock(s_mutex);
		return s_npcData.try_emplace(a_baseID, NPCData{ a_addonName, a_rank }).second;
	}

	void SetNPCAddonName(RE::FormID a_baseID, const std::string& a_addonName) {

		std::lock_guard lock(s_mutex);

		auto& data = s_npcData[a_baseID];

		// An entry without addon only has a placeholder rank
		if (data.addonName.empty())
			data.rank = static_cast<std::uint8_t>(Util::kDefaultRank);

		data.addonName = a_addonName;
	}

	bool SetNPCRank(RE::FormID a_baseID, std::uint8_t a_rank) {

		std::lock_guard lock(s_mutex);

		const auto it = s_npcData.find(a_baseID);
		if (it == s_npcData.end() || it->second.addonName.empty())
			return false;

		it->second.rank = a_rank;
		return true;
	}

	void ClearNPCData(RE::FormID a_baseID) {

		std::lock_guard lock(s_mutex);
		s_npcData.erase(a_baseID);
	}

	void ClearAllNPCData() {

		std::lock_guard lock(s_mutex);
		s_npcData.clear();
	}

	void ClearAll() {

		std::lock_guard lock(s_mutex);

		s_addons.clear();
		s_genderGroups = {};
		s_bones.clear();
		s_npcData.clear();
	}

	// ===============================================================
	// SERIALIZATION
	// ===============================================================

	namespace {

		// Don't change these, they identify the data already saved in the cosaves
		constexpr std::uint32_t kSerializationVersion = 3;
		constexpr std::uint32_t kNPCRecord = 'NPCs';

		// Sanity limit when reading, a longer name means the record is corrupt
		constexpr std::uint32_t kMaxAddonNameLength = 1024;
	}

	void SaveCallback(SKSE::SerializationInterface* a_intfc) {

		std::lock_guard lock(s_mutex);

		if (!a_intfc->OpenRecord(kNPCRecord, kSerializationVersion)) {
			SKSE::log::error("SOS: The record could not be opened to save the data");
			return;
		}

		const auto numNPCs = static_cast<std::uint32_t>(s_npcData.size());
		a_intfc->WriteRecordData(numNPCs);

		for (const auto& [baseID, data] : s_npcData) {

			a_intfc->WriteRecordData(baseID);

			const auto nameLength = static_cast<std::uint32_t>(data.addonName.size());
			a_intfc->WriteRecordData(nameLength);
			if (nameLength > 0)
				a_intfc->WriteRecordData(data.addonName.data(), nameLength);

			a_intfc->WriteRecordData(data.rank);
		}

		SKSE::log::info("SOS: Saved {} NPCs in the cosave", numNPCs);
	}

	void LoadCallback(SKSE::SerializationInterface* a_intfc) {

		std::uint32_t type, version, length;

		while (a_intfc->GetNextRecordInfo(type, version, length)) {

			if (type != kNPCRecord)
				continue;

			if (version != kSerializationVersion) {
				SKSE::log::warn("SOS: Unsupported serialization version: {}", version);
				continue;
			}

			std::uint32_t numNPCs;
			if (!a_intfc->ReadRecordData(numNPCs)) {
				SKSE::log::error("SOS: Failed to read the number of NPCs");
				continue;
			}

			for (std::uint32_t i = 0; i < numNPCs; ++i) {

				RE::FormID oldBaseID;
				std::uint32_t nameLength;
				if (!a_intfc->ReadRecordData(oldBaseID) || !a_intfc->ReadRecordData(nameLength) || nameLength > kMaxAddonNameLength)
					break;

				std::string addonName(nameLength, '\0');
				if (nameLength > 0 && !a_intfc->ReadRecordData(addonName.data(), nameLength))
					break;

				std::uint8_t rank;
				if (!a_intfc->ReadRecordData(rank))
					break;

				// The plugin of the NPC may not be loaded anymore
				RE::FormID newBaseID;
				if (a_intfc->ResolveFormID(oldBaseID, newBaseID)) {

					std::lock_guard lock(s_mutex);
					s_npcData[newBaseID] = { std::move(addonName), rank };
				}
			}
		}

		std::lock_guard lock(s_mutex);
		SKSE::log::info("SOS: Restored {} NPC entries from the cosave", s_npcData.size());
	}

	void RevertCallback(SKSE::SerializationInterface*) {

		SKSE::log::info("SOS: Reverting storage, clearing the NPC entries");

		ClearAllNPCData();
		Actors::ApplyOverrides();
	}
}
