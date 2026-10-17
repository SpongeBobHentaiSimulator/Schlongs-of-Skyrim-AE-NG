#pragma once

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

// API of "Schlongs of Skyrim AE" for other SKSE plugins. Header only, nothing to link.
//
// - Everything is about the addon (schlong) of an actor. Actors are given by the FormID of the
//   reference (the player is 0x14).
// - The data is stored per NPC base, so every reference of the same base shares its addon and rank.
// - The functions can be called from any thread. The data changes right away, what the player sees
//   (equip, bones, factions) is applied a moment later on the game thread. An actor that is not
//   loaded gets it when it loads.
// - Call it from kPostLoad on (all the plugins are loaded by then). IsAvailable() tells if the mod
//   is there and compatible, so check it before anything else.
//
// Usage:
//   if (SOSAE_API::IsAvailable()) {
//       const std::string addon = SOSAE_API::GetActorAddonName(actorFormID);
//       SOSAE_API::SetActorRank(actorFormID, 15);
//   }
namespace SOSAE_API {

	// Oldest API version this header works with: major * 10000 + minor * 100 + patch (4.0.0 is 40000)
	inline constexpr std::uint32_t kRequiredVersion = 40000;

	namespace Detail {

		struct Exports {

			std::uint32_t (*getAPIVersion)();
			int (*getActorAddonName)(std::uint32_t, char*, int);
			int (*getActorRank)(std::uint32_t);
			std::uint32_t (*getActorAddonArmor)(std::uint32_t);
			bool (*setActorAddonName)(std::uint32_t, const char*);
			bool (*setActorRank)(std::uint32_t, int);
			bool (*setActorAddonArmor)(std::uint32_t, std::uint32_t);
		};

		template <class Function>
		inline Function Resolve(HMODULE a_dll, const char* a_name) {

			return reinterpret_cast<Function>(GetProcAddress(a_dll, a_name));
		}

		// Finds the exports the first time and keeps them. Nothing is kept while the DLL isn't there
		// or isn't compatible, so a later call tries again.
		inline const Exports* GetExports() {

			static Exports exports{};
			static std::atomic<bool> ready{ false };
			static std::mutex mutex;

			if (ready.load(std::memory_order_acquire))
				return &exports;

			std::lock_guard lock(mutex);
			if (ready.load(std::memory_order_relaxed))
				return &exports;

			const HMODULE dll = GetModuleHandleW(L"SchlongsOfSkyrim");
			if (!dll)
				return nullptr;

			exports.getAPIVersion = Resolve<std::uint32_t (*)()>(dll, "SOSAE_GetAPIVersion");
			exports.getActorAddonName = Resolve<int (*)(std::uint32_t, char*, int)>(dll, "SOSAE_GetActorAddonName");
			exports.getActorRank = Resolve<int (*)(std::uint32_t)>(dll, "SOSAE_GetActorRank");
			exports.getActorAddonArmor = Resolve<std::uint32_t (*)(std::uint32_t)>(dll, "SOSAE_GetActorAddonArmor");
			exports.setActorAddonName = Resolve<bool (*)(std::uint32_t, const char*)>(dll, "SOSAE_SetActorAddonName");
			exports.setActorRank = Resolve<bool (*)(std::uint32_t, int)>(dll, "SOSAE_SetActorRank");
			exports.setActorAddonArmor = Resolve<bool (*)(std::uint32_t, std::uint32_t)>(dll, "SOSAE_SetActorAddonArmor");

			const bool complete = exports.getAPIVersion && exports.getActorAddonName && exports.getActorRank &&
				exports.getActorAddonArmor && exports.setActorAddonName && exports.setActorRank && exports.setActorAddonArmor;

			if (!complete || exports.getAPIVersion() < kRequiredVersion)
				return nullptr;

			ready.store(true, std::memory_order_release);
			return &exports;
		}
	}

	// True if the mod is loaded and its API is compatible with this header
	inline bool IsAvailable() {

		return Detail::GetExports() != nullptr;
	}

	// Version of the API of the mod: major * 10000 + minor * 100 + patch. 0 if it isn't available.
	inline std::uint32_t GetVersion() {

		const auto* api = Detail::GetExports();
		return api ? api->getAPIVersion() : 0;
	}

	// ---------------------------------------------------------------
	// Getters: read only, they never roll or assign an addon
	// ---------------------------------------------------------------

	// Name of the addon of the actor, empty if it has none (or the API isn't available)
	inline std::string GetActorAddonName(std::uint32_t a_actorFormID) {

		const auto* api = Detail::GetExports();
		if (!api)
			return {};

		// A null buffer only asks for the length
		const int length = api->getActorAddonName(a_actorFormID, nullptr, 0);
		if (length <= 0)
			return {};

		std::string name(static_cast<std::size_t>(length), '\0');
		const int written = api->getActorAddonName(a_actorFormID, name.data(), length + 1);

		// The name may have changed between both calls
		name.resize(static_cast<std::size_t>(std::clamp(written, 0, length)));
		return name;
	}

	// Rank of the schlong, 1-20. 0 if the actor has no addon (or the API isn't available).
	inline int GetActorRank(std::uint32_t a_actorFormID) {

		const auto* api = Detail::GetExports();
		return api ? api->getActorRank(a_actorFormID) : 0;
	}

	// FormID of the addon armor of the actor. 0 if it has no addon, the armor doesn't exist or the API isn't available.
	inline std::uint32_t GetActorAddonArmor(std::uint32_t a_actorFormID) {

		const auto* api = Detail::GetExports();
		return api ? api->getActorAddonArmor(a_actorFormID) : 0;
	}

	// ---------------------------------------------------------------
	// Setters: return false if the change was rejected
	// ---------------------------------------------------------------

	// Sets the addon by name. A null, empty or "None" name removes it.
	// Gender and race are NOT checked: any addon that exists is accepted. Rejected if the addon doesn't exist.
	inline bool SetActorAddonName(std::uint32_t a_actorFormID, const char* a_addonName) {

		const auto* api = Detail::GetExports();
		return api && api->setActorAddonName(a_actorFormID, a_addonName);
	}

	// Sets the rank of the schlong, clamped to 1-20. Rejected if the actor has no addon.
	inline bool SetActorRank(std::uint32_t a_actorFormID, int a_rank) {

		const auto* api = Detail::GetExports();
		return api && api->setActorRank(a_actorFormID, a_rank);
	}

	// Sets the addon by its armor ("SOS_Addon_<name>_Genitals"). An armor FormID of 0 removes it.
	// Rejected if the armor isn't an addon of the mod.
	inline bool SetActorAddonArmor(std::uint32_t a_actorFormID, std::uint32_t a_armorFormID) {

		const auto* api = Detail::GetExports();
		return api && api->setActorAddonArmor(a_actorFormID, a_armorFormID);
	}
}
