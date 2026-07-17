#include "Actors.h"

//Lock to don't process two times
static bool npcs_processed = false;

//Add KW to NPC based on TXT
static bool ModifyNPCWithSOS(RE::TESNPC* a_npc, RE::BGSKeyword* a_kwNone, const std::unordered_set<RE::BSFixedString>& a_whitelist) {

	if (!a_npc || !a_kwNone)
		return false;

	auto* file = a_npc->GetFile(0);
	if (!file)
		return false;

	RE::BSFixedString modName(file->GetFilename());
	bool isInList = a_whitelist.contains(modName);

	if (g_isWhitelistMode != isInList) 

		if (!a_npc->HasKeyword(a_kwNone)) {
			a_npc->AddKeyword(a_kwNone);
			return true;
		}

	return false;
}

std::uint8_t g_WhitelistCount = 0;

//JSON Shit
static RE::BSFixedString MakeNPCKey(RE::TESNPC* a_base) {

	if (!a_base)
		return {};

	RE::FormID formID = a_base->GetFormID();
	if ((formID >> 24) == 0xFF)
		return {};

	const RE::TESFile* file = a_base->GetFile(0);
	if (!file)
		return {};

	std::string pluginName{ file->GetFilename() };
	RE::FormID localID = a_base->GetLocalFormID();

	std::string fullName = a_base->GetFullName();
	if (fullName.empty()) fullName = "Unknown";

	std::string key = fmt::format("{:X}|{}|{}", localID, pluginName, fullName);

	return RE::BSFixedString(key.c_str());
}

static RE::FormID ResolveNPCKey(const RE::BSFixedString& a_key) {

	std::string keyStr = a_key.c_str();
	const auto separator = keyStr.find('|');

	if (separator == std::string::npos)
		return 0;

	std::string hexID = keyStr.substr(0, separator);
	std::string pluginName = keyStr.substr(separator + 1);

	try {
		RE::FormID localID = static_cast<RE::FormID>(std::stoul(hexID, nullptr, 16));

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (dataHandler)
			return dataHandler->LookupFormID(localID, pluginName);
	}
	catch (const std::exception& e) {
		SKSE::log::error("ResolveNPCKey: Failed to parse key {}. Error: {}", keyStr, e.what());
		return 0;
	}

	return 0;
}

std::unordered_map<RE::BSFixedString, Storage::NPCData> g_npcOverrides;

namespace Actors {

	static std::unordered_set<RE::BSFixedString> LoadNPCWhitelist() {

		const std::filesystem::path path{ WhitelistPath };

		if (!std::filesystem::exists(path)) {

			std::filesystem::create_directories(path.parent_path());
			std::ofstream out(path);

			if (out.is_open()) {
				for (const auto& mod : Util::GetDefaultConcealingPlugins()) {
					out << mod << '\n';
				}
				SKSE::log::info("SOS: Created default Whitelist file.");
			}
		}

		auto validatedList = Util::LoadValidatedModList(WhitelistPath, "NPC Whitelist");
		g_WhitelistCount = static_cast<std::uint16_t>(validatedList.size());

		return validatedList;
	}

	static void SaveNPCOverrides() {
		nlohmann::json j;

		for (const auto& [key, data] : g_npcOverrides) {
			std::string keyStr = key.c_str();

			std::string jsonKey = keyStr;
			std::string npcName = "Unknown";

			size_t firstPipe = keyStr.find('|');
			size_t lastPipe = keyStr.find_last_of('|');

			if (firstPipe != std::string::npos && lastPipe != std::string::npos && firstPipe != lastPipe) {
				jsonKey = keyStr.substr(0, lastPipe);
				npcName = keyStr.substr(lastPipe + 1);
			}

			j["npc_overrides"][jsonKey] = {
				{ "name", npcName },
				{ "addon", data.addonName.c_str() },
				{ "rank", data.rank }
			};
		}

		const std::filesystem::path path = NPCPath;
		std::filesystem::create_directories(path.parent_path());
		std::ofstream file(path);
		if (file.is_open()) {
			file << j.dump(4);
		}
	}

	void LoadNPCOverrides() {

		g_npcOverrides.clear();

		const std::filesystem::path path = NPCPath;
		std::ifstream file(path);
		if (!file.is_open())
			return;

		try {
			nlohmann::json j;
			file >> j;

			if (!j.contains("npc_overrides"))
				return;

			for (auto& [key, val] : j["npc_overrides"].items()) {
				if (val.contains("addon") && val.contains("rank")) {
					Storage::NPCData data;

					std::string addonStr = val["addon"].get<std::string>();
					data.addonName = RE::BSFixedString(addonStr.c_str());
					data.rank = val["rank"].get<std::uint8_t>();

					g_npcOverrides[RE::BSFixedString(key.c_str())] = data;
				}
			}

			SKSE::log::info("Loaded {} NPC overrides from JSON.", g_npcOverrides.size());
		}
		catch (const std::exception& e) {
			SKSE::log::error("Error parsing NPC overrides JSON: {}", e.what());
		}
	}

	void ProcessNPCs() {

		if (npcs_processed)
			return;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		auto* kwNone = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(NPCKW);

		if (!dataHandler || !kwNone)
			return;

		auto whitelist = LoadNPCWhitelist();

		std::uint32_t count = 0;
		for (auto* npc : dataHandler->GetFormArray<RE::TESNPC>()) {
			if (ModifyNPCWithSOS(npc, kwNone, whitelist))
				count++;
		}

		npcs_processed = true;
		SKSE::log::info("SOS: Processed {} NPCs.", count);
	}

	void ApplyNPCOverrides() {

		if (g_npcOverrides.empty())
			return;

		for (const auto& [key, data] : g_npcOverrides) {

			RE::FormID resolvedID = ResolveNPCKey(key);

			if (resolvedID != 0)
				Storage::SetNPCAddonData(resolvedID, data.addonName, data.rank);
		}
	}

	//Papyrus

	static std::uint16_t GetSchlongEligibleActorModsCount(RE::StaticFunctionTag*) {

		return g_WhitelistCount;
	}

	static void SetActorRank(RE::StaticFunctionTag*, RE::Actor* a_actor, std::uint8_t a_rank) {

		if (!a_actor)
			return;

		auto* base = a_actor->GetActorBase();
		if (!base)
			return;

		RE::FormID baseFormID = base->GetFormID();

		Storage::SetNPCAddonData(baseFormID, Storage::GetNPCAddonName(baseFormID), a_rank);

		auto* data = Storage::GetNPCData(baseFormID);
		if (!data)
			return;

		RE::BSFixedString key = MakeNPCKey(base);
		if (!key.empty()) {
			g_npcOverrides[key] = *data;
			SaveNPCOverrides();
		}

		SchlongLogic::ScaleSchlongBones(a_actor);
	}

	static void SetActorSchlong(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_addonName) {

		if (!a_actor)
			return;

		auto* base = a_actor->GetActorBase();
		if (!base)
			return;

		RE::FormID baseID = base->GetFormID();
		std::string nameStr = a_addonName.c_str();

		std::uint8_t currentRank = Storage::GetNPCRank(baseID);

		if (nameStr == "None")
			Storage::SetNPCAddonData(baseID, "", 1);
		else
			Storage::SetNPCAddonData(baseID, a_addonName, currentRank);

		//JSON Part
		RE::BSFixedString key = MakeNPCKey(base);
		if (!key.empty()) {
			auto* data = Storage::GetNPCData(baseID);
			if (data) {
				g_npcOverrides[key] = *data;
				SaveNPCOverrides();
			}
		}

		RE::TESObjectARMO* schlong = SchlongLogic::ResolveCachedAddon(baseID);

		//Sexlab Part
		if (g_SexLabGenderFaction && Util::IsFemale(base)) {

			if (!schlong || Util::ArmorHasKeyword(schlong, PubKW))
				a_actor->AddToFaction(g_SexLabGenderFaction, 1);
			else
				a_actor->AddToFaction(g_SexLabGenderFaction, 0);
		}

		Util::RemoveSOSItemsFromInventory(a_actor);
		//If the penis is NONE or the Actor has something in the slot, do nothing
		if (!schlong || Util::HasSomethingInSlot52(a_actor))
			return;

		auto* equipManager = RE::ActorEquipManager::GetSingleton();
		if (equipManager) {
			a_actor->AddObjectToContainer(schlong, nullptr, 1, nullptr);
			equipManager->EquipObject(a_actor, schlong, nullptr, 1, nullptr, true, false, false, false);
		}

		SchlongLogic::ScaleSchlongBones(a_actor);
	}

	static void ClearNPCStorage(RE::StaticFunctionTag*) {

		Storage::s_npcData.clear();
		Actors::ApplyNPCOverrides();
	}

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm) {

		if (!a_vm)
			return false;
		a_vm->RegisterFunction("GetSchlongEligibleActorModsCount", SosPapyrusScript, GetSchlongEligibleActorModsCount);
		a_vm->RegisterFunction("SetActorRank", SosPapyrusScript, SetActorRank);
		a_vm->RegisterFunction("SetActorSchlong", SosPapyrusScript, SetActorSchlong);
		a_vm->RegisterFunction("ClearNPCStorage", SosPapyrusScript, ClearNPCStorage);
		return true;
	}
}