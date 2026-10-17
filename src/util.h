#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <SimpleIni.h>
#include <nlohmann/json.hpp>

// Shorthand for the JSON library
using Json = nlohmann::json;

// ===============================================================
// CONSTANTS
// ===============================================================

// Keywords
constexpr const char* GenKW = "SOS_Genitals";    // The addon armor itself
constexpr const char* NPCKW = "SOS_NoneDefault"; // NPC that doesn't get an addon by default
constexpr const char* RevKW = "SOS_Revealing";   // Armor that shows the addon (no slot 52)
constexpr const char* ConKW = "SOS_Concealing";  // Armor that hides the addon (slot 52)
constexpr const char* PubKW = "SOS_Pubes";       // Addon that is only pubic hair, not a real schlong
constexpr const char* UndwKW = "SOS_Underwear";  // Underwear: hides the addon without using slot 52

// Biped slots
using SlotMask = std::underlying_type_t<RE::BIPED_MODEL::BipedObjectSlot>;
constexpr SlotMask SLOT_32 = static_cast<SlotMask>(1 << 2);  // Body
constexpr SlotMask SLOT_52 = static_cast<SlotMask>(1 << 22); // Where the addon lives

// Papyrus script that holds the native functions
constexpr const char* SosPapyrusScript = "SOSAE_SKSE";

// Files
constexpr const char* BlacklistPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_ConcealingArmorMods.txt";
constexpr const char* WhitelistPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_SchlongEligibleActorMods.txt";
constexpr const char* JsonPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_AddonData.json";
constexpr const char* INIPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_AE.ini";
constexpr const char* ARMOPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_ARMO_Override.JSON";
constexpr const char* NPCPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_NPC_Override.JSON";

// Ranks and chances, the single source of the default values
namespace Util {

	inline constexpr int kMaxRank = 20;
	inline constexpr int kDefaultRank = 10;
	inline constexpr int kDefaultChance = 20;
}

// ===============================================================
// SETTINGS (SOS_AE.ini)
// ===============================================================

// Keys that can be rebound in the menu, in the same order as Settings::keys
namespace Binding {

	enum : std::size_t {
		kPin,            // Pins the NPC under the crosshair
		kBendUp,         // Bend of the pinned NPC
		kBendDown,
		kPlayerBendUp,   // Bend of the player
		kPlayerBendDown,
		kCount
	};
}

struct Settings {

	bool enabled{ true };                      // Default "Enabled" of the new race entries
	int rank{ Util::kDefaultRank };            // Default "Rank" of the new race entries (-1 = uniform random)
	int chance{ Util::kDefaultChance };        // Default "Chance" of the new race entries (0-100)
	float gaussianDeviation{ 1.0f };           // Spread of the random ranks around the target rank
	bool whitelistMode{ true };                // true: only female NPCs of the listed mods are eligible, false: all except the listed ones
	bool sexLabForceMale{ true };              // Females with a schlong are treated as male by SexLab

	// DirectInput scan codes, 0 = unbound. Pin: N. Bend of the pinned NPC: Page Up / Page Down. Bend of the player: unbound.
	std::array<int, Binding::kCount> keys{ 49, 201, 209, 0, 0 };

	// Reads the INI, creating it with the current values if it doesn't exist
	void Load();

	void Save() const;
};

inline Settings g_settings;

// ===============================================================
// OTHER PLUGINS
// ===============================================================

// SexLab gender faction (rank 0 = male, 1 = female)
extern RE::TESFaction* g_SexLabGenderFaction;

// Schlongs of Skyrim faction (rank 0 = has a schlong, -1 = hasn't)
extern RE::TESFaction* g_schlongifiedFaction;

// False if the editor IDs can't be read (po3_Tweaks missing or without "Load EditorIDs"), the addons can't work then
extern bool g_editorIDsAvailable;

// ===============================================================
// HELPERS
// ===============================================================

namespace Util {

	// Rank: 0 or less means uniform random (-1), otherwise the median of the Gaussian (1-20)
	constexpr std::int8_t NormalizeRank(int a_rank) {

		return a_rank <= 0 ? static_cast<std::int8_t>(-1) : static_cast<std::int8_t>(std::clamp(a_rank, 1, kMaxRank));
	}

	// Chance: 0-100
	constexpr std::uint8_t NormalizeChance(int a_chance) {

		return static_cast<std::uint8_t>(std::clamp(a_chance, 0, 100));
	}

	// ---------------------------------------------------------------
	// Text
	// ---------------------------------------------------------------

	std::string ToString(const RE::BSFixedString& a_text);

	std::string ToLower(std::string a_text);

	// ---------------------------------------------------------------
	// Files
	// ---------------------------------------------------------------

	// Reads a JSON file, returns nothing if it is missing or malformed
	std::optional<Json> LoadJson(const std::filesystem::path& a_path);

	// Writes a JSON file, the folder is created if needed
	bool SaveJson(const std::filesystem::path& a_path, const Json& a_json, int a_indent = 4);

	// Plugin names are not case sensitive, so the lists store them in lowercase
	using ModSet = std::unordered_set<std::string>;

	// Reads a list of plugin names (one per line), creating the default file if it doesn't exist.
	// The plugins that are not in the load order are skipped and reported in the log.
	ModSet LoadModList(const std::filesystem::path& a_path, std::string_view a_label);

	bool ModSetContains(const ModSet& a_mods, std::string_view a_modName);

	// ---------------------------------------------------------------
	// Forms
	// ---------------------------------------------------------------

	// Editor ID of any form (needs po3_Tweaks), empty if it isn't available
	std::string GetEditorIDSafe(const RE::TESForm* a_form);

	// "SOS_Addon_<name>_CompatibleRaces" -> "<name>"
	std::optional<std::string> GetSOSAddonName(RE::BGSListForm* a_list);

	// Editor ID of a race, or its FormID if it doesn't have one
	std::string GetRaceKey(RE::TESForm* a_race);

	// ---------------------------------------------------------------
	// Armors and actors
	// ---------------------------------------------------------------

	bool ArmorHasKeyword(RE::TESObjectARMO* a_armor, const char* a_keyword);

	SlotMask GetArmorSlots(RE::TESObjectARMO* a_armor);
	void SetArmorSlots(RE::TESObjectARMO* a_armor, SlotMask a_mask);

	std::vector<RE::TESObjectARMO*> GetWornArmorsWithKeyword(RE::Actor* a_actor, const char* a_keyword);
	bool ActorHasEquippedArmorWithKeyword(RE::Actor* a_actor, const char* a_keyword);

	void RemoveSOSItemsFromInventory(RE::Actor* a_actor);

	// Armor worn in slot 52, the addon itself or something that hides it
	RE::TESObjectARMO* GetSlot52Armor(RE::Actor* a_actor);
	bool HasSomethingInSlot52(RE::Actor* a_actor);

	bool IsFemale(RE::TESNPC* a_base);

	// ---------------------------------------------------------------
	// Plugin
	// ---------------------------------------------------------------

	void CheckDependencies();
}
