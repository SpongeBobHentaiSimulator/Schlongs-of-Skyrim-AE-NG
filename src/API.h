#pragma once

#include <cstdint>
#include <string>

// The public surface of the mod: 3 getters and 3 setters about the addon of an actor.
// There is ONE implementation (this namespace). The Papyrus natives, the extern "C" exports for other
// SKSE plugins (see SOSAE_API.h) and the menu all call it.
//
// Every function can be called from any thread:
//  - the Storage is written right away (it has its own mutex)
//  - what the player sees (equip, bones, factions) is queued to the game thread
//  - game objects are never touched, the actors are only looked up by FormID
//
// The actors are references (the player is 0x14), but the data is stored per NPC base.
namespace API {

	// major * 10000 + minor * 100 + patch (4.0.0)
	inline constexpr std::uint32_t kVersion = 40000;

	// ---------------------------------------------------------------
	// Getters: read only, they never roll or assign an addon
	// ---------------------------------------------------------------

	// Empty if the actor has no addon
	std::string GetActorAddonName(RE::FormID a_actorID);

	// 1-20, or 0 if the actor has no addon
	int GetActorRank(RE::FormID a_actorID);

	// FormID of the addon armor, or 0 if the actor has no addon or its armor doesn't exist
	RE::FormID GetActorAddonArmor(RE::FormID a_actorID);

	// ---------------------------------------------------------------
	// Setters: return false if the change was rejected
	// ---------------------------------------------------------------

	// Empty or "None" removes the addon. Gender and race are not checked, any addon that exists is accepted.
	bool SetActorAddonName(RE::FormID a_actorID, const std::string& a_addonName);

	// Clamped to 1-20. Rejected if the actor has no addon.
	bool SetActorRank(RE::FormID a_actorID, int a_rank);

	// 0 removes the addon. Rejected if the armor isn't an addon of the mod ("SOS_Addon_<name>_Genitals").
	bool SetActorAddonArmor(RE::FormID a_actorID, RE::FormID a_armorID);

	void RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm);
}
