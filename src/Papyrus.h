#pragma once

#include "Armors.h"
#include "Storage.h"
#include <mutex>

// Global mutex for thread safety during shutdown
extern std::mutex g_shutdownMutex;

namespace Papyrus {

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm);
}