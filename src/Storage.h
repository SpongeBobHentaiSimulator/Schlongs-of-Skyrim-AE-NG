#pragma once

#include "util.h"
#include "Papyrus.h"
#include "Schlongification.h"

namespace JSON {

	void WriteDefaultJson();
}

namespace Storage {

	// ===============================================================
	// JSON & CONFIGURATION DATA
	// ===============================================================

	struct RaceData {

		std::int8_t Rank{ 10 };    // -1 for uniform, 1-20 for Gaussian median
		std::uint8_t Chance{ 15 }; // 0-100 probability
		bool Enabled{ true };
	};

	struct GenderGroups {

		std::vector<RE::BSFixedString> male;
		std::vector<RE::BSFixedString> female;
	};

	struct AddonData {

		std::unordered_map<RE::BSFixedString, RaceData> compatibleRaces;
	};

	using AddonMap = std::unordered_map<RE::BSFixedString, AddonData>;

	extern AddonMap s_addons;
	extern GenderGroups s_genderGroups;

	// ===============================================================
	// BONE SCALING DATA
	// ===============================================================

	//Bone data and sufixes
	struct SOSAddonBoneData {
		std::string addonName;
		float bones[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
	};
	static constexpr std::array<const char*, 8> sBoneGlobalSuffixes = {
		"GenBase", "GenScrotum", "Gen01", "Gen02", "Gen03", "Gen04", "Gen05", "Gen06"
	};

	extern 	std::vector<SOSAddonBoneData> g_addonBoneData;

	const SOSAddonBoneData* GetAddonBoneData(RE::BSFixedString addonName);

	// ===============================================================
	// NPC RUNTIME DATA (STATE)
	// ===============================================================

	struct NPCData {

		RE::BSFixedString addonName{};
		std::uint8_t rank{ 10 };
	};

	extern std::unordered_map<RE::FormID, NPCData> s_npcData;

	bool HasNPCData(RE::FormID a_baseID);
	void SetNPCAddonData(RE::FormID a_baseID, const RE::BSFixedString& a_addonName, std::uint8_t a_rank);
	const RE::BSFixedString& GetNPCAddonName(RE::FormID a_baseID);
	NPCData* GetNPCData(RE::FormID a_baseID);
	std::uint8_t GetNPCRank(RE::FormID a_baseID);
	const AddonMap& GetAddons();
	const GenderGroups& GetGenderGroups();
	void ClearNPCData(RE::FormID a_baseID);
	void LoadAddonBoneData();
	bool EnsureAddonDataLoaded();

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm);

	void SaveCallback(SKSE::SerializationInterface* a_intfc);
	void LoadCallback(SKSE::SerializationInterface* a_intfc);
	void RevertCallback(SKSE::SerializationInterface*);
}