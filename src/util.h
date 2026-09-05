#pragma once

#include <nlohmann/json.hpp>
#include <SimpleIni.h>

//Bool to control if females with a schlong get treated as male by SexLab
inline bool g_SexLabForceMaleOnSchlong = true;

//Keywords
constexpr const char* GenKW = "SOS_Genitals";
constexpr const char* NPCKW = "SOS_NoneDefault";
constexpr const char* RevKW = "SOS_Revealing";
constexpr const char* ConKW = "SOS_Concealing";
constexpr const char* PubKW = "SOS_Pubes";
constexpr const char* UndwKW = "SOS_Underwear";

//Shortname for Slotmask
using SlotMask = std::underlying_type_t<RE::BIPED_MODEL::BipedObjectSlot>;
//Slots to be used in general
constexpr SlotMask SLOT_32 = static_cast<SlotMask>(1 << 2);
constexpr SlotMask SLOT_52 = static_cast<SlotMask>(1 << 22);

//Bool to change how the Whitelist works
inline bool g_isWhitelistMode = true;

//Papyrus script
constexpr const char* SosPapyrusScript = "SOSAE_SKSE";

//Paths
constexpr const char* BlacklistPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_ConcealingArmorMods.txt";
constexpr const char* WhitelistPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_SchlongEligibleActorMods.txt";
constexpr const char* JsonPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_AddonData.json";
constexpr const char* INIPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_AE.ini";
constexpr const char* ARMOPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_ARMO_Override.JSON";
constexpr const char* NPCPath = "Data/SKSE/Plugins/SchlongsOfSkyrim/SOS_NPC_Override.JSON";

//Shortname for json
using Json = nlohmann::json;
//Default values for JSON
inline Json g_JSONDefaults = {

	{ "Chance", 20 },
	{ "Rank", 10 },
	{ "Enabled", true }
};

//Overrides
extern std::unordered_map<std::string, bool> g_armorOverrides;

//Gaussian Desviation
inline float g_SOSGaussianDeviation = 1.0f;

//Shortname for a funcion
using _GetFormEditorID = const char* (*)(RE::FormID a_formID);

//Name for bones
static constexpr std::array<const char*, 8> sNiNodes = {

	"NPC GenitalsBase [GenBase]",
	"NPC GenitalsScrotum [GenScrot]",
	"NPC Genitals01 [Gen01]",
	"NPC Genitals02 [Gen02]",
	"NPC Genitals03 [Gen03]",
	"NPC Genitals04 [Gen04]",
	"NPC Genitals05 [Gen05]",
	"NPC Genitals06 [Gen06]"
};

//=========================================Sexlab=============================================
//Keywords
constexpr const char* SLMaleKW = "SexLabTreatMale";
constexpr const char* SLFemaleKW = "SexLabTreatFemale";

//Gender Faction
extern RE::TESFaction* g_SexLabGenderFaction;

//Schlong Faction
extern RE::TESFaction* g_schlongifiedFaction;

namespace std {

	template <class CharT>
	struct hash<RE::detail::BSFixedString<CharT>> {

		size_t operator()(const RE::detail::BSFixedString<CharT>& a_string) const {

			auto vista_del_texto = std::basic_string_view<CharT>(a_string.data(), a_string.size());

			return std::hash<std::basic_string_view<CharT>>{}(vista_del_texto);
		}
	};
}

namespace Util {

	void CheckDependencies();

	void ReadJSONDefaultsFromINI();

	std::unordered_set<RE::BSFixedString> LoadValidatedModList(const RE::BSFixedString& a_filePath, const RE::BSFixedString& a_listName);

	bool ArmorHasKeyword(RE::TESObjectARMO* a_armor, const char* a_keyword);

	const std::array<std::string_view, 11>& GetDefaultConcealingPlugins();

	void RemoveSOSItemsFromInventory(RE::Actor* a_actor);

	std::string GetEditorIDSafe(const RE::TESForm* a_form);

	std::optional<std::string> GetSOSAddonName(RE::BGSListForm* a_list);

	bool HasSomethingInSlot52(RE::Actor* a_actor);

	bool IsFemale(RE::TESNPC* a_base);

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm);
}