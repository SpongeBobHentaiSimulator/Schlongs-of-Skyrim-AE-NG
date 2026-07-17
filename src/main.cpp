#include "Schlongification.h"
#include "util.h"
#include "Storage.h"
#include "SchlongLogic.h"
#include "Armors.h"
#include "Papyrus.h"
#include "Actors.h"

//log function, changing things here don't break the mod
static void SetupSimpleLog() {

	auto path = SKSE::log::log_directory();
	if (!path)
		return;

	*path /= "SOS_AE.log";

	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global", std::move(sink));

	log->set_level(spdlog::level::debug);
	log->flush_on(spdlog::level::debug);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%l] %v");
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {

	SKSE::Init(skse);

	//Log starters
	SetupSimpleLog();
	SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {

		if (message->type == SKSE::MessagingInterface::kDataLoaded) {

			//Read the armor changes from the JSON done by the MCM
			Armors::LoadArmorOverrides();
			Armors::ApplyArmorOverrides();
			Armors::ProcessArmors();

			//Ini Read
			Util::ReadJSONDefaultsFromINI();

			//Actors
			Actors::LoadNPCOverrides();
			Actors::ApplyNPCOverrides();
			Actors::ProcessNPCs();

			//Make sure the addon data is loaded
			if (Storage::EnsureAddonDataLoaded())
				RE::ConsoleLog::GetSingleton()->Print("[SOS-NG] JSON loaded.");
			else
				RE::ConsoleLog::GetSingleton()->Print("[SOS-NG] ERROR: JSON not loaded");

			Storage::LoadAddonBoneData();

			//Events
			SOS::RegisterActivationEvents();
			SOS::RegisterEquipEvent();
			//Register Functions
			SKSE::GetPapyrusInterface()->Register(SchlongLogic::RegisterFunctions);
			SKSE::GetPapyrusInterface()->Register(Armors::RegisterFunctions);
			SKSE::GetPapyrusInterface()->Register(Papyrus::RegisterFunctions);
			SKSE::GetPapyrusInterface()->Register(Storage::RegisterFunctions);
			SKSE::GetPapyrusInterface()->Register(Util::RegisterFunctions);
			SKSE::GetPapyrusInterface()->Register(Actors::RegisterFunctions);

			//Check dependencie
			Util::CheckDependencies();
		}
	});

	auto* serialization = SKSE::GetSerializationInterface();

	//SKSE Serialization
	if (serialization) {
		
		serialization->SetUniqueID('SOS');
		serialization->SetSaveCallback(Storage::SaveCallback);
		serialization->SetLoadCallback(Storage::LoadCallback);
		serialization->SetRevertCallback(Storage::RevertCallback);
	}

	return true;
}