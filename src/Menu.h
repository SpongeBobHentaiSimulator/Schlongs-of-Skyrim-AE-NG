#pragma once

namespace Menu {

	// Adds the pages of the mod to the SKSE Menu Framework. Does nothing if the framework isn't loaded.
	// Call it once the data is loaded (all the plugins are loaded by then).
	void Register();
}
