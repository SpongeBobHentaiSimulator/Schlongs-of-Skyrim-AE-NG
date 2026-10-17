#pragma once

namespace Actors {

	// Reads SOS_NPC_Override.JSON, the manual changes made through the menu
	void LoadOverrides();

	// Puts the loaded overrides in the NPC data
	void ApplyOverrides();

	// Gives the "none by default" keyword to the NPCs that are not eligible for an addon
	void ProcessNPCs();

	// Remembers the current addon and rank of an NPC in the overrides JSON, so they survive new games.
	// Only the menu calls it, and always from the game thread (the overrides are not locked).
	void SaveOverride(RE::TESNPC* a_base);
}
