#pragma once

#include <optional>
#include <string>
#include <vector>

namespace SchlongLogic {

	// Addon the actor should wear: the one already assigned to its NPC base, or a new random one that
	// gets stored. nullptr if it shouldn't have any. Only for the game thread, it can roll and assign.
	RE::TESObjectARMO* DetermineWinningAddon(RE::Actor* a_actor);

	// Addon armor assigned to an NPC base. If the armor doesn't exist anymore the assignment is cleared.
	RE::TESObjectARMO* ResolveCachedAddon(RE::FormID a_baseID);

	// Armor of an addon ("SOS_Addon_<name>_Genitals"), nullptr if it doesn't exist. Needs po3_Tweaks.
	RE::TESObjectARMO* FindAddonArmor(const std::string& a_addonName);

	// The opposite: name of the addon of an armor, nothing if the armor isn't an addon of the mod
	std::optional<std::string> GetAddonNameFromArmor(RE::TESObjectARMO* a_armor);

	// Addons the NPC can get according to its gender and race ("None" is not included)
	std::vector<std::string> GetCompatibleAddonNames(RE::TESNPC* a_base);

	// Updates the Schlongified and the SexLab gender factions according to the addon of the actor.
	// a_resetSexLab: females that don't get a schlong are also put back as female in SexLab.
	void UpdateFactions(RE::Actor* a_actor, RE::TESObjectARMO* a_schlong, bool a_resetSexLab = false);

	// Replaces the addon item of the actor with this one, equips it and updates the bones
	void EquipAddon(RE::Actor* a_actor, RE::TESObjectARMO* a_schlong, bool a_applyNow = true);

	// True while the plugin is equipping an addon on the actor, so the equip events it causes can be ignored
	bool IsEquippingAddon(RE::FormID a_actorID);

	// Any thread. Both run on the game thread later and do nothing if the actor isn't loaded.

	// Scales the genital bones according to the addon and the rank of the actor
	void ScaleSchlongBones(RE::FormID a_actorID);

	// Applies the addon stored for the actor: factions, item, equip and bones
	void QueueRefresh(RE::FormID a_actorID);
}
