#pragma once

#include "util.h"
#include "Storage.h"

extern std::unordered_map<RE::BSFixedString, Storage::NPCData> g_npcOverrides;

namespace Actors {

	void ApplyNPCOverrides();
	void ProcessNPCs();
	void LoadNPCOverrides();

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm);
}