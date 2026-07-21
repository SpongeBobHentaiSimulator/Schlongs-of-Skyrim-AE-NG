#include "SchlongLogic.h"

//Scale bones
static void SetNodeScaleImpl(RE::NiAVObject* a_root, const char* a_nodeName, float a_scale) {

	auto node = a_root->GetObjectByName(a_nodeName);
	if (node)
		node->local.scale = a_scale;
}

//Function to make the penis look good
inline static float Rescale(float scale, float factor) {

	return 1.0f + (scale - 1.0f) * factor;
}

static uint8_t GenerateGaussianRank(int target, std::mt19937& rng) {

	std::normal_distribution<float> dist(static_cast<float>(target), g_SOSGaussianDeviation);

	int rank = static_cast<int>(std::round(dist(rng)));

	return static_cast<uint8_t>(std::clamp(rank, 1, 20));
}

static std::uint8_t GenerateRandomRank(int a_targetSize) {

	if (a_targetSize == -1) {

		static std::uniform_int_distribution<int> dist(1, 20);
		return static_cast<std::uint8_t>(dist(g_rng));
	}
	return GenerateGaussianRank(a_targetSize, g_rng);
}

namespace SchlongLogic {

	void ScaleSchlongBones(RE::Actor* a_actor) {

		if (!a_actor)
			return;

		auto* root = a_actor->Get3D();
		if (!root)
			return;

		auto* base = a_actor->GetActorBase();
		if (!base)
			return;

		auto* npcData = Storage::GetNPCData(base->GetFormID());
		if (!npcData)
			return;

		const auto* addonData = Storage::GetAddonBoneData(npcData->addonName);
		if (!addonData)
			return;

		float size = static_cast<float>(npcData->rank) / 20.0f;
		float boost = 1.0f;

		for (std::size_t i = 0; i < 8; ++i) {

			float scale = addonData->bones[i];

			if (i == 0 && scale > 1.0f)
				scale = Rescale(scale, boost);

			scale = Rescale(scale, size);

			SetNodeScaleImpl(root, sNiNodes[i], scale);
		}
	}

	static void GetCompatibleCandidates(RE::TESNPC* a_base, std::vector<AddonCandidate>& outCandidates) {

		outCandidates.clear();

		if (!a_base)
			return;

		RE::TESRace* race = a_base->GetRace();
		if (!race)
			return;

		RE::BSFixedString raceName = race->GetFormEditorID();
		if (raceName.empty())
			raceName = std::format("0x{:08X}", race->GetFormID()).c_str();

		int sex = a_base->GetSex();
		const auto& genderGroups = Storage::GetGenderGroups();
		const std::vector<RE::BSFixedString>* targetGroup = nullptr;

		if (sex == RE::SEX::kMale)
			targetGroup = &genderGroups.male;
		else if (sex == RE::SEX::kFemale)
			targetGroup = &genderGroups.female;

		if (!targetGroup || targetGroup->empty())
			return;

		const auto& allAddons = Storage::GetAddons();
		outCandidates.reserve(targetGroup->size());

		for (const RE::BSFixedString& addonName : *targetGroup) {
			auto itAddon = allAddons.find(addonName);
			if (itAddon == allAddons.end())
				continue;

			const auto& raceMap = itAddon->second.compatibleRaces;
			auto itRace = raceMap.find(raceName);

			if (itRace != raceMap.end()) {
				const auto& raceData = itRace->second;
				if (raceData.Enabled) {
					outCandidates.emplace_back(addonName, raceData.Chance, raceData.Rank);
				}
			}
		}
	}

	RE::TESObjectARMO* ResolveCachedAddon(RE::FormID a_baseID) {

		auto* data = Storage::GetNPCData(a_baseID);
		if (!data || data->addonName.empty())
			return nullptr;

		std::string editorID = "SOS_Addon_";
		editorID += data->addonName.c_str();
		editorID += "_Genitals";

		auto* armor = RE::TESForm::LookupByEditorID<RE::TESObjectARMO>(editorID);

		if (!armor) {
			SKSE::log::warn("SOS: Addon '{}' missing for Base {:08X}. Cleared.", data->addonName, a_baseID);
			Storage::ClearNPCData(a_baseID);
			return nullptr;
		}

		return armor;
	}

	RE::TESObjectARMO* DetermineWinningAddon(RE::Actor* a_actor) {

		if (!a_actor)
			return nullptr;

		auto* npcBase = a_actor->GetActorBase();
		if (!npcBase)
			return nullptr;

		RE::FormID baseID = npcBase->GetFormID();

		if (Storage::HasNPCData(baseID))
			return ResolveCachedAddon(baseID);

		if (npcBase->HasKeywordString(NPCKW)) {
			Storage::SetNPCAddonData(baseID, "", 1);
			return nullptr;
		}

		std::vector<AddonCandidate> candidates;
		GetCompatibleCandidates(npcBase, candidates);

		if (candidates.empty())
			return nullptr;

		int totalWeight = 0;
		for (const auto& c : candidates)
			totalWeight += c.probability;

		int ceiling = std::max(100, totalWeight);

		static std::uniform_int_distribution<int> dist_100(1, 100);

		std::uniform_int_distribution<int> dist(1, ceiling);

		int roll = dist(g_rng);
		//SKSE::log::debug("Total Probability: {} for {}", roll, a_actor->GetName());
		if (roll > totalWeight) {
			Storage::SetNPCAddonData(baseID, "", 1);
			return nullptr;
		}

		int accum = 0;
		for (const auto& c : candidates) {
			accum += c.probability;

			if (roll <= accum) {

				std::uint8_t generatedRank = GenerateRandomRank(c.targetRank);
				Storage::SetNPCAddonData(baseID, c.name, generatedRank);

				std::string editorID = "SOS_Addon_";
				editorID += c.name.c_str();
				editorID += "_Genitals";

				return RE::TESForm::LookupByEditorID<RE::TESObjectARMO>(editorID);
			}
		}
		return nullptr;
	}

	static void ScaleSchlongBonesAE(RE::StaticFunctionTag*, RE::Actor* a_actor, float) {

		SchlongLogic::ScaleSchlongBones(a_actor);
	}

	static RE::TESObjectARMO* GetWinningAddon(RE::StaticFunctionTag*, RE::Actor* a_actor) {

		return SchlongLogic::DetermineWinningAddon(a_actor);
	}

	static std::vector<RE::BSFixedString> GetCompatibleAddonNames(RE::StaticFunctionTag*, RE::Actor* a_actor) {

		std::vector<RE::BSFixedString> names;

		if (!a_actor)
			return names;

		auto* base = a_actor->GetActorBase();
		if (!base)
			return names;

		names.push_back("None");

		std::vector<AddonCandidate> candidates;
		GetCompatibleCandidates(base, candidates);

		names.reserve(candidates.size() + 1);

		for (const auto& c : candidates) {
			names.push_back(c.name);
		}

		return names;
	}

	static std::vector<RE::BSFixedString> GetAllAddonNamesForGender(RE::StaticFunctionTag*, RE::Actor* a_actor) {

		std::vector<RE::BSFixedString> result;
		result.emplace_back("None");

		if (!a_actor)
			return result;

		auto* base = a_actor->GetActorBase();
		if (!base)
			return result;

		const auto& allAddons = Storage::GetAddons();
		const auto& genderGroups = Storage::GetGenderGroups();

		bool actorIsMale = (base->GetSex() == RE::SEX::kMale);
		const auto& targetGenderList = actorIsMale ? genderGroups.male : genderGroups.female;

		for (const auto& name : targetGenderList) {

			if (allAddons.contains(name))
				result.emplace_back(name);
		}

		return result;
	
	}

	static float GetGaussianDeviation(RE::StaticFunctionTag*) {
		
		return g_SOSGaussianDeviation;
	}

	static void SetGaussianDeviation(RE::StaticFunctionTag*, float a_value) {

		if (a_value < 0)
			return;

		g_SOSGaussianDeviation = a_value;

		g_JSONDefaults["GaussianDeviation"] = a_value;

		CSimpleIniA ini;
		ini.SetUnicode();

		if (ini.LoadFile(INIPath) >= 0) {
			ini.SetDoubleValue("JSON", "GaussianDeviation", static_cast<double>(a_value));
			ini.SaveFile(INIPath);
			SKSE::log::info("SOS: Gaussian Deviation updated to {} via MCM.", a_value);
		}
	}

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm) {

		if (!a_vm)
			return false;

		a_vm->RegisterFunction("GetWinningAddon", SosPapyrusScript, GetWinningAddon);
		a_vm->RegisterFunction("ScaleSchlongBonesAE", SosPapyrusScript, ScaleSchlongBonesAE);
		a_vm->RegisterFunction("GetCompatibleAddonNames", SosPapyrusScript, GetCompatibleAddonNames);
		a_vm->RegisterFunction("GetAllAddonNamesForGender", SosPapyrusScript, GetAllAddonNamesForGender);
		a_vm->RegisterFunction("GetGaussianDeviation", SosPapyrusScript, GetGaussianDeviation);
		a_vm->RegisterFunction("SetGaussianDeviation", SosPapyrusScript, SetGaussianDeviation);

		return true;
	}
}