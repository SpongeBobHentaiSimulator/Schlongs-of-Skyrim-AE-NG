#pragma once

namespace Input {

	// Starts listening to the keyboard: the pin key and the bend keys (see Settings::keys)
	void Register();

	// NPC pinned with the pin key (runtime only). nullptr if there isn't one or it doesn't exist anymore.
	RE::Actor* GetPinnedActor();
	void ClearPinned();

	// Forgets the pinned NPC and the bend levels, and straightens the player. Used before uninstalling the mod.
	void ClearState();

	void SendNotification(const char* a_msg, const char* a_sound = nullptr, bool a_cancelIfQueued = true);
}
