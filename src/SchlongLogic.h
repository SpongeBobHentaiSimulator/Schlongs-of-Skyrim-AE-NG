#pragma once

#include "Storage.h"
#include "util.h"
#include <random>

//Global Random Number Generator
static std::mt19937 g_rng(std::random_device{}());

namespace SchlongLogic {

	//Structure to manage potential addon candidates during the selection process
	struct AddonCandidate {

		RE::BSFixedString name;
		std::uint8_t probability;;
		std::int8_t targetRank;

		AddonCandidate(const RE::BSFixedString& a_name, std::uint8_t a_prob, std::int8_t a_rank)
			: name(a_name), probability(a_prob), targetRank(a_rank) {
		}
	};

	void ScaleSchlongBones(RE::Actor* a_actor);
	RE::TESObjectARMO* DetermineWinningAddon(RE::Actor* a_actor);
	RE::TESObjectARMO* ResolveCachedAddon(RE::FormID a_baseID);

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm);
}