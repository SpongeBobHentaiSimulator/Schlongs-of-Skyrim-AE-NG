#include "Actors.h"

#include "Storage.h"
#include "util.h"

#include <format>
#include <unordered_map>

namespace Actors {

	// ===============================================================
	// ELIGIBILITY
	// ===============================================================

	namespace {

		// ProcessNPCs only runs once
		bool s_processed = false;

		// Males are always eligible. Female NPCs are only eligible if their plugin passes the list
		// (whitelist: it has to be in it, blacklist: it can't be in it), the others get the "none by default" keyword.
		bool FlagIfNotEligible(RE::TESNPC* a_npc, RE::BGSKeyword* a_keyword, const Util::ModSet& a_mods) {

			if (!a_npc || !Util::IsFemale(a_npc) || a_npc->HasKeyword(a_keyword))
				return false;

			const auto* file = a_npc->GetFile(0);
			if (!file)
				return false;

			const bool listed = Util::ModSetContains(a_mods, file->GetFilename());
			if (listed == g_settings.whitelistMode)
				return false;

			a_npc->AddKeyword(a_keyword);
			return true;
		}
	}

	// ===============================================================
	// OVERRIDES (SOS_NPC_Override.JSON)
	// ===============================================================

	namespace {

		struct NPCOverride {

			Storage::NPCData data;
			std::string name; // Only to make the JSON easier to read
		};

		// Key: "<local FormID in hex>|<plugin>", the same one that is written in the JSON
		std::unordered_map<std::string, NPCOverride> s_overrides;

		std::string MakeNPCKey(RE::TESNPC* a_base) {

			// NPCs created while playing (0xFF) don't exist in the plugins, so they can't be saved
			if (!a_base || (a_base->GetFormID() >> 24) == 0xFF)
				return {};

			const auto* file = a_base->GetFile(0);
			if (!file)
				return {};

			return std::format("{:X}|{}", a_base->GetLocalFormID(), file->GetFilename());
		}

		RE::FormID ResolveNPCKey(const std::string& a_key) {

			const auto separator = a_key.find('|');
			if (separator == std::string::npos)
				return 0;

			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler)
				return 0;

			try {
				const auto localID = static_cast<RE::FormID>(std::stoul(a_key.substr(0, separator), nullptr, 16));
				return dataHandler->LookupFormID(localID, a_key.substr(separator + 1));
			}
			catch (const std::exception& e) {
				SKSE::log::error("SOS: Failed to parse the NPC key '{}': {}", a_key, e.what());
				return 0;
			}
		}

		void SaveOverrides() {

			Json root = Json::object();
			root["npc_overrides"] = Json::object();

			for (const auto& [key, entry] : s_overrides) {
				root["npc_overrides"][key] = {
					{ "name", entry.name },
					{ "addon", entry.data.addonName },
					{ "rank", static_cast<int>(entry.data.rank) }
				};
			}

			Util::SaveJson(NPCPath, root);
		}
	}

	void SaveOverride(RE::TESNPC* a_base) {

		if (!a_base)
			return;

		const auto key = MakeNPCKey(a_base);
		const auto data = Storage::GetNPCData(a_base->GetFormID());
		if (key.empty() || !data)
			return;

		const char* name = a_base->GetFullName();

		s_overrides[key] = { *data, (name && name[0] != '\0') ? name : "Unknown" };
		SaveOverrides();
	}

	void LoadOverrides() {

		s_overrides.clear();

		auto root = Util::LoadJson(NPCPath);
		if (!root || !root->contains("npc_overrides"))
			return;

		try {
			for (auto& [key, item] : (*root)["npc_overrides"].items()) {

				if (!item.contains("addon") || !item.contains("rank"))
					continue;

				NPCOverride entry;
				entry.data.addonName = item["addon"].get<std::string>();
				entry.data.rank = static_cast<std::uint8_t>(std::clamp(item["rank"].get<int>(), 1, Util::kMaxRank));
				entry.name = item.value("name", "Unknown");

				s_overrides[key] = std::move(entry);
			}

			SKSE::log::info("SOS: Loaded {} NPC overrides from the JSON.", s_overrides.size());
		}
		catch (const Json::exception& e) {
			SKSE::log::error("SOS: Error reading the NPC overrides JSON: {}", e.what());
		}
	}

	void ApplyOverrides() {

		for (const auto& [key, entry] : s_overrides) {

			const RE::FormID baseID = ResolveNPCKey(key);
			if (baseID != 0)
				Storage::SetNPCAddonData(baseID, entry.data.addonName, entry.data.rank);
		}
	}

	void ProcessNPCs() {

		if (s_processed)
			return;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		auto* keyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(NPCKW);

		if (!dataHandler || !keyword) {
			SKSE::log::warn("SOS: The keyword '{}' was not found, the NPCs were not processed.", NPCKW);
			return;
		}

		const auto mods = Util::LoadModList(WhitelistPath, "NPC Whitelist");

		std::uint32_t count = 0;
		for (auto* npc : dataHandler->GetFormArray<RE::TESNPC>()) {
			if (FlagIfNotEligible(npc, keyword, mods))
				++count;
		}

		s_processed = true;
		SKSE::log::info("SOS: Processed {} NPCs.", count);
	}
}
