#pragma once

#include "util.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Every function of this module takes the same mutex, so they can be called from any thread.
// Nothing returns a pointer or a reference to the inside of the storage, only copies.
namespace Storage {

	// ===============================================================
	// ADDON DATA (SOS_AddonData.json)
	// ===============================================================

	struct RaceData {

		std::int8_t rank{ Util::kDefaultRank };     // -1 for uniform, 1-20 for the median of the Gaussian
		std::uint8_t chance{ Util::kDefaultChance }; // 0-100
		bool enabled{ true };
	};

	// Loads the JSON, creating it from the plugins first if it doesn't exist
	bool EnsureAddonDataLoaded();

	// Writes the data in memory (with the menu changes) to the JSON
	void SaveAddonData();

	// Writes the JSON again from the plugins and loads it. The changes made in the menu are lost.
	bool RebuildAddonData();

	bool HasAddon(const std::string& a_addon);

	// Every loaded addon, sorted
	std::vector<std::string> GetAddonNames();

	// Addons of the gender group of a sex (only the ones with data), in the order of the JSON
	std::vector<std::string> GetAddonNamesForSex(RE::SEX a_sex);

	// Races of an addon with their values, sorted by name (editor ID or FormID of the race)
	std::vector<std::pair<std::string, RaceData>> GetAddonRaces(const std::string& a_addon);

	// Data of an addon for a race, nothing if the addon doesn't support it
	std::optional<RaceData> GetRaceData(const std::string& a_addon, const std::string& a_race);

	// Rank and chance are normalized. Returns false if the addon doesn't support the race.
	bool SetRaceValues(const std::string& a_addon, const std::string& a_race, int a_rank, int a_chance, bool a_enabled);

	// Values for many races at once. An empty addon name means every loaded addon.
	void SetRankForAll(const std::string& a_addon, int a_rank);
	void SetChanceForAll(const std::string& a_addon, int a_chance);
	void SetEnabledForAll(const std::string& a_addon, bool a_enabled);

	// ===============================================================
	// BONE SCALING DATA
	// ===============================================================

	// Scale of every genital bone: GenBase, GenScrotum, Gen01 ... Gen06
	using BoneScales = std::array<float, 8>;

	// Reads the bone scales of every addon from the globals of the plugins
	void LoadAddonBoneData();

	std::optional<BoneScales> GetAddonBoneData(const std::string& a_addon);

	// ===============================================================
	// NPC RUNTIME DATA
	// ===============================================================

	struct NPCData {

		std::string addonName; // Empty if the NPC has no addon
		std::uint8_t rank{ Util::kDefaultRank };
	};

	bool HasNPCData(RE::FormID a_baseID);
	std::optional<NPCData> GetNPCData(RE::FormID a_baseID);

	void SetNPCAddonData(RE::FormID a_baseID, const std::string& a_addonName, std::uint8_t a_rank);

	// Only if the NPC has no data yet, returns false if it already had
	bool TrySetNPCAddonData(RE::FormID a_baseID, const std::string& a_addonName, std::uint8_t a_rank);

	// Changes the addon and keeps the rank. An NPC that had no addon gets the default rank.
	void SetNPCAddonName(RE::FormID a_baseID, const std::string& a_addonName);

	// Only for NPCs that have an addon, returns false otherwise
	bool SetNPCRank(RE::FormID a_baseID, std::uint8_t a_rank);

	void ClearNPCData(RE::FormID a_baseID);
	void ClearAllNPCData();

	// Empties all the runtime state (addons, bones and NPCs)
	void ClearAll();

	// ===============================================================
	// SKSE
	// ===============================================================

	void SaveCallback(SKSE::SerializationInterface* a_intfc);
	void LoadCallback(SKSE::SerializationInterface* a_intfc);
	void RevertCallback(SKSE::SerializationInterface* a_intfc);
}
