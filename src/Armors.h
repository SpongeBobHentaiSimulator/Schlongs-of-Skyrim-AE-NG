#pragma once

#include "util.h"

extern std::uint8_t g_blacklistCount;

namespace Armors {

	void LoadArmorOverrides();
	void ApplyArmorOverrides();
	void ProcessArmors();

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm);
}