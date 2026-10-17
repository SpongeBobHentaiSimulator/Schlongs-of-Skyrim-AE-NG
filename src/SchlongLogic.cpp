#include "SchlongLogic.h"

#include "Storage.h"
#include "util.h"

#include <array>
#include <cmath>
#include <format>
#include <mutex>
#include <random>
#include <unordered_set>

namespace SchlongLogic {

	namespace {

		// Skeleton nodes of the genitals, in the same order as the bone scales of the addons
		constexpr std::array<const char*, 8> kBoneNodes = {
			"NPC GenitalsBase [GenBase]",
			"NPC GenitalsScrotum [GenScrot]",
			"NPC Genitals01 [Gen01]",
			"NPC Genitals02 [Gen02]",
			"NPC Genitals03 [Gen03]",
			"NPC Genitals04 [Gen04]",
			"NPC Genitals05 [Gen05]",
			"NPC Genitals06 [Gen06]"
		};

		// Editor ID of the armor of an addon: SOS_Addon_<name>_Genitals
		constexpr std::string_view kArmorPrefix = "SOS_Addon_";
		constexpr std::string_view kArmorSuffix = "_Genitals";

		// One generator per thread, the game events and the menu don't share a thread
		std::mt19937& Rng() {

			thread_local std::mt19937 rng{ std::random_device{}() };
			return rng;
		}

		// ===============================================================
		// ADDON SELECTION
		// ===============================================================

		struct AddonCandidate {

			std::string name;
			int chance;
			int targetRank;
		};

		// Addons that can be used by the NPC: right gender and enabled for its race
		std::vector<AddonCandidate> GetCompatibleCandidates(RE::TESNPC* a_base) {

			std::vector<AddonCandidate> candidates;

			auto* race = a_base ? a_base->GetRace() : nullptr;
			if (!race)
				return candidates;

			const auto addons = Storage::GetAddonNamesForSex(a_base->GetSex());
			if (addons.empty())
				return candidates;

			const std::string raceKey = Util::GetRaceKey(race);
			candidates.reserve(addons.size());

			for (const auto& addon : addons) {

				const auto raceData = Storage::GetRaceData(addon, raceKey);
				if (raceData && raceData->enabled)
					candidates.push_back({ addon, raceData->chance, raceData->rank });
			}

			return candidates;
		}

		// Rank of a new schlong: uniform in 1-20 if there is no target, otherwise Gaussian around the target
		std::uint8_t GenerateRank(int a_targetRank) {

			if (a_targetRank < 1)
				return static_cast<std::uint8_t>(std::uniform_int_distribution<int>(1, Util::kMaxRank)(Rng()));

			// A normal distribution needs a deviation above 0, without it the rank is always the target
			const float deviation = g_settings.gaussianDeviation;
			if (deviation <= 0.0f)
				return static_cast<std::uint8_t>(std::min(a_targetRank, Util::kMaxRank));

			std::normal_distribution<float> distribution(static_cast<float>(a_targetRank), deviation);
			const int rank = static_cast<int>(std::lround(distribution(Rng())));

			return static_cast<std::uint8_t>(std::clamp(rank, 1, Util::kMaxRank));
		}

		// ===============================================================
		// EQUIP GUARD
		// ===============================================================

		std::mutex s_equippingMutex;
		std::unordered_set<RE::FormID> s_equipping;

		// Marks an actor as busy while the plugin equips an addon on it
		class EquipGuard {

		public:
			explicit EquipGuard(RE::FormID a_actorID) : _actorID(a_actorID) {

				std::lock_guard lock(s_equippingMutex);
				s_equipping.insert(_actorID);
			}

			~EquipGuard() {

				std::lock_guard lock(s_equippingMutex);
				s_equipping.erase(_actorID);
			}

			EquipGuard(const EquipGuard&) = delete;
			EquipGuard& operator=(const EquipGuard&) = delete;

		private:
			RE::FormID _actorID;
		};

		// Makes the schlong grow or shrink from its base scale (1.0) according to the rank
		float Rescale(float a_scale, float a_factor) {

			return 1.0f + (a_scale - 1.0f) * a_factor;
		}
	}

	RE::TESObjectARMO* FindAddonArmor(const std::string& a_addonName) {

		return RE::TESForm::LookupByEditorID<RE::TESObjectARMO>(std::format("{}{}{}", kArmorPrefix, a_addonName, kArmorSuffix));
	}

	std::optional<std::string> GetAddonNameFromArmor(RE::TESObjectARMO* a_armor) {

		if (!a_armor)
			return std::nullopt;

		const std::string editorID = Util::GetEditorIDSafe(a_armor);

		if (editorID.size() <= kArmorPrefix.size() + kArmorSuffix.size() || !editorID.starts_with(kArmorPrefix) || !editorID.ends_with(kArmorSuffix))
			return std::nullopt;

		return editorID.substr(kArmorPrefix.size(), editorID.size() - kArmorPrefix.size() - kArmorSuffix.size());
	}

	std::vector<std::string> GetCompatibleAddonNames(RE::TESNPC* a_base) {

		std::vector<std::string> names;

		const auto candidates = GetCompatibleCandidates(a_base);
		names.reserve(candidates.size());

		for (const auto& candidate : candidates)
			names.push_back(candidate.name);

		return names;
	}

	RE::TESObjectARMO* ResolveCachedAddon(RE::FormID a_baseID) {

		const auto data = Storage::GetNPCData(a_baseID);
		if (!data || data->addonName.empty())
			return nullptr;

		auto* armor = FindAddonArmor(data->addonName);
		if (!armor) {
			SKSE::log::warn("SOS: Addon armor 'SOS_Addon_{}_Genitals' not found for NPC base {:08X}. Clearing its assignment.", data->addonName, a_baseID);
			Storage::ClearNPCData(a_baseID);
		}

		return armor;
	}

	RE::TESObjectARMO* DetermineWinningAddon(RE::Actor* a_actor) {

		auto* base = a_actor ? a_actor->GetActorBase() : nullptr;
		if (!base)
			return nullptr;

		const RE::FormID baseID = base->GetFormID();

		// Already decided: by a previous roll, by the menu, by an override or by the API
		if (Storage::HasNPCData(baseID))
			return ResolveCachedAddon(baseID);

		// The NPCs that are not eligible never get an addon by default
		if (base->HasKeywordString(NPCKW)) {
			Storage::TrySetNPCAddonData(baseID, "", 1);
			return nullptr;
		}

		const auto candidates = GetCompatibleCandidates(base);
		if (candidates.empty())
			return nullptr;

		int totalChance = 0;
		for (const auto& candidate : candidates)
			totalChance += candidate.chance;

		// Males always get one of the addons. Females only get one if the roll falls inside the
		// chances, the rest of the 100 means no addon.
		const int ceiling = Util::IsFemale(base) ? std::max(100, totalChance) : std::max(1, totalChance);
		const int roll = std::uniform_int_distribution<int>(1, ceiling)(Rng());

		// If another thread (the API) decided in the meantime, its choice stays: the Try functions don't overwrite
		if (roll > totalChance) {
			Storage::TrySetNPCAddonData(baseID, "", 1);
			return ResolveCachedAddon(baseID);
		}

		int accumulated = 0;
		for (const auto& candidate : candidates) {

			accumulated += candidate.chance;
			if (roll <= accumulated) {
				Storage::TrySetNPCAddonData(baseID, candidate.name, GenerateRank(candidate.targetRank));
				return ResolveCachedAddon(baseID);
			}
		}

		return nullptr;
	}

	void UpdateFactions(RE::Actor* a_actor, RE::TESObjectARMO* a_schlong, bool a_resetSexLab) {

		auto* base = a_actor ? a_actor->GetActorBase() : nullptr;
		if (!base)
			return;

		// The pubes addon is only hair, the actor doesn't count as schlongified
		const bool hasSchlong = a_schlong && !Util::ArmorHasKeyword(a_schlong, PubKW);

		if (g_schlongifiedFaction)
			a_actor->AddToFaction(g_schlongifiedFaction, hasSchlong ? 0 : -1);

		if (!g_SexLabGenderFaction || !Util::IsFemale(base))
			return;

		if (hasSchlong && g_settings.sexLabForceMale)
			a_actor->AddToFaction(g_SexLabGenderFaction, 0);
		else if (a_resetSexLab)
			a_actor->AddToFaction(g_SexLabGenderFaction, 1);
	}

	void EquipAddon(RE::Actor* a_actor, RE::TESObjectARMO* a_schlong, bool a_applyNow) {

		auto* equipManager = RE::ActorEquipManager::GetSingleton();
		if (!a_actor || !a_schlong || !equipManager)
			return;

		{
			// The equip events caused by this are not a change made by the player or the game
			const EquipGuard guard(a_actor->GetFormID());

			// Cleans any previous addon item so they don't pile up in the inventory
			Util::RemoveSOSItemsFromInventory(a_actor);
			a_actor->AddObjectToContainer(a_schlong, nullptr, 1, nullptr);

			// queueEquip, forceEquip, playSounds, applyNow
			equipManager->EquipObject(a_actor, a_schlong, nullptr, 1, nullptr, true, false, false, a_applyNow);
		}

		ScaleSchlongBones(a_actor->GetFormID());
	}

	bool IsEquippingAddon(RE::FormID a_actorID) {

		std::lock_guard lock(s_equippingMutex);
		return s_equipping.contains(a_actorID);
	}

	void ScaleSchlongBones(RE::FormID a_actorID) {

		// The nodes can only be touched from the main thread
		SKSE::GetTaskInterface()->AddTask([a_actorID]() {

			auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_actorID);
			if (!actor || !actor->Is3DLoaded())
				return;

			auto* root = actor->Get3D();
			auto* base = actor->GetActorBase();
			if (!root || !base)
				return;

			// Without an addon every bone goes back to its normal scale
			const auto npcData = Storage::GetNPCData(base->GetFormID());
			const std::optional<Storage::BoneScales> bones = npcData ? Storage::GetAddonBoneData(npcData->addonName) : std::nullopt;
			const float sizeFactor = npcData ? static_cast<float>(npcData->rank) / static_cast<float>(Util::kMaxRank) : 1.0f;

			for (std::size_t i = 0; i < kBoneNodes.size(); ++i) {

				if (auto* node = root->GetObjectByName(kBoneNodes[i]))
					node->local.scale = bones ? Rescale((*bones)[i], sizeFactor) : 1.0f;
			}
		});
	}

	// Game thread only: puts the actor in the state of its stored addon
	namespace {

		void RefreshActor(RE::Actor* a_actor) {

			auto* base = a_actor ? a_actor->GetActorBase() : nullptr;
			if (!base || !a_actor->Is3DLoaded())
				return;

			// If the armor of the addon doesn't exist the assignment is cleared here
			auto* schlong = ResolveCachedAddon(base->GetFormID());

			UpdateFactions(a_actor, schlong, true);

			// The old addon item goes away whatever the new state is
			Util::RemoveSOSItemsFromInventory(a_actor);

			// Armor in slot 52 or underwear hide the new addon, it is equipped when they are taken off
			if (schlong && !Util::HasSomethingInSlot52(a_actor) && !Util::ActorHasEquippedArmorWithKeyword(a_actor, UndwKW))
				EquipAddon(a_actor, schlong, false);
			else
				ScaleSchlongBones(a_actor->GetFormID());
		}
	}

	void QueueRefresh(RE::FormID a_actorID) {

		SKSE::GetTaskInterface()->AddTask([a_actorID]() {
			RefreshActor(RE::TESForm::LookupByID<RE::Actor>(a_actorID));
		});
	}
}
