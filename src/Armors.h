#pragma once

namespace Armors {

	// Reads SOS_ARMO_Override.JSON, the manual changes made through the menu
	void LoadOverrides();

	// Gives the overrides their keyword (revealing or concealing)
	void ApplyOverrides();

	// Forgets the overrides loaded in memory, the JSON is not touched
	void ClearOverrides();

	// Adds or removes slot 52 from the body armors according to their keywords and the concealing plugins list
	void ProcessArmors();

	// Makes an armor revealing (no slot 52) or concealing (slot 52) and remembers it in the JSON.
	// Game thread only (the overrides are not locked).
	void SetOverride(RE::TESObjectARMO* a_armor, bool a_revealing);

	// Unequips what hides the addon: the armor in slot 52 (if it isn't the addon) and the underwear.
	// It exists for mods that replace all the outfits. Game thread only.
	void RemoveConcealing(RE::Actor* a_actor);
}
