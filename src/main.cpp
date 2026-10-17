#include "API.h"
#include "Actors.h"
#include "Armors.h"
#include "Input.h"
#include "Menu.h"
#include "Schlongification.h"
#include "Storage.h"
#include "util.h"

namespace {

	// Registers the native functions of the API
	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm) {

		if (!a_vm)
			return false;

		API::RegisterFunctions(a_vm);

		return true;
	}

	void PrintToConsole(const char* a_text) {

		if (auto* console = RE::ConsoleLog::GetSingleton())
			console->Print(a_text);
	}

	void OnDataLoaded() {

		// The settings go first, the other steps read them
		g_settings.Load();
		Util::CheckDependencies();

		// Armors: the menu overrides give their keywords, then the slot 52 changes are made
		Armors::LoadOverrides();
		Armors::ApplyOverrides();
		Armors::ProcessArmors();

		// Actors
		Actors::LoadOverrides();
		Actors::ApplyOverrides();
		Actors::ProcessNPCs();

		// Addon data
		if (Storage::EnsureAddonDataLoaded())
			PrintToConsole("[SOS-NG] JSON loaded.");
		else
			PrintToConsole("[SOS-NG] ERROR: JSON not loaded");

		Storage::LoadAddonBoneData();

		SOS::RegisterEvents();

		// Keys and menu, all the plugins are loaded at this point so the Menu Framework can be found
		Input::Register();
		Menu::Register();
	}

	void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message) {

		if (a_message->type == SKSE::MessagingInterface::kDataLoaded)
			OnDataLoaded();
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {

	// Starts CommonLib, which also creates the log file
	SKSE::Init(a_skse);

	SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage);
	SKSE::GetPapyrusInterface()->Register(RegisterPapyrusFunctions);

	// Cosave: the addons assigned to the NPCs. Don't change the ID, it identifies the existing saves.
	if (auto* serialization = SKSE::GetSerializationInterface()) {
		serialization->SetUniqueID('SOS');
		serialization->SetSaveCallback(Storage::SaveCallback);
		serialization->SetLoadCallback(Storage::LoadCallback);
		serialization->SetRevertCallback(Storage::RevertCallback);
	}

	return true;
}
