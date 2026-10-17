#include "API.h"

#include "SchlongLogic.h"
#include "Storage.h"
#include "util.h"

#include <algorithm>
#include <cstring>

namespace API {

	// ===============================================================
	// IMPLEMENTATION
	// ===============================================================

	namespace {

		// The data is stored per NPC base. Only a form lookup is done here, which is safe from any thread.
		RE::TESNPC* GetBase(RE::FormID a_actorID) {

			if (a_actorID == 0)
				return nullptr;

			auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_actorID);
			return actor ? actor->GetActorBase() : nullptr;
		}
	}

	std::string GetActorAddonName(RE::FormID a_actorID) {

		auto* base = GetBase(a_actorID);
		if (!base)
			return {};

		const auto data = Storage::GetNPCData(base->GetFormID());
		return data ? data->addonName : std::string();
	}

	int GetActorRank(RE::FormID a_actorID) {

		auto* base = GetBase(a_actorID);
		if (!base)
			return 0;

		const auto data = Storage::GetNPCData(base->GetFormID());
		return (data && !data->addonName.empty()) ? data->rank : 0;
	}

	RE::FormID GetActorAddonArmor(RE::FormID a_actorID) {

		const auto name = GetActorAddonName(a_actorID);
		if (name.empty())
			return 0;

		auto* armor = SchlongLogic::FindAddonArmor(name);
		return armor ? armor->GetFormID() : 0;
	}

	bool SetActorAddonName(RE::FormID a_actorID, const std::string& a_addonName) {

		auto* base = GetBase(a_actorID);
		if (!base)
			return false;

		const RE::FormID baseID = base->GetFormID();

		if (a_addonName.empty() || a_addonName == "None") {

			// Same state as an NPC that rolled no addon
			Storage::SetNPCAddonData(baseID, "", 1);
		}
		else {

			// Gender and race are not checked on purpose: any addon that exists can be forced on any actor
			if (!Storage::HasAddon(a_addonName) || !SchlongLogic::FindAddonArmor(a_addonName))
				return false;

			Storage::SetNPCAddonName(baseID, a_addonName);
		}

		SchlongLogic::QueueRefresh(a_actorID);
		return true;
	}

	bool SetActorRank(RE::FormID a_actorID, int a_rank) {

		auto* base = GetBase(a_actorID);
		if (!base)
			return false;

		const auto rank = static_cast<std::uint8_t>(std::clamp(a_rank, 1, Util::kMaxRank));
		if (!Storage::SetNPCRank(base->GetFormID(), rank))
			return false;

		SchlongLogic::ScaleSchlongBones(a_actorID);
		return true;
	}

	bool SetActorAddonArmor(RE::FormID a_actorID, RE::FormID a_armorID) {

		if (a_armorID == 0)
			return SetActorAddonName(a_actorID, "");

		const auto name = SchlongLogic::GetAddonNameFromArmor(RE::TESForm::LookupByID<RE::TESObjectARMO>(a_armorID));
		return name && SetActorAddonName(a_actorID, *name);
	}

	// ===============================================================
	// PAPYRUS
	// ===============================================================

	namespace {

		std::string PapyrusGetActorAddonName(RE::StaticFunctionTag*, RE::Actor* a_actor) {

			return a_actor ? GetActorAddonName(a_actor->GetFormID()) : std::string();
		}

		std::int32_t PapyrusGetActorRank(RE::StaticFunctionTag*, RE::Actor* a_actor) {

			return a_actor ? GetActorRank(a_actor->GetFormID()) : 0;
		}

		RE::TESObjectARMO* PapyrusGetActorAddonArmor(RE::StaticFunctionTag*, RE::Actor* a_actor) {

			if (!a_actor)
				return nullptr;

			const RE::FormID armorID = GetActorAddonArmor(a_actor->GetFormID());
			return armorID != 0 ? RE::TESForm::LookupByID<RE::TESObjectARMO>(armorID) : nullptr;
		}

		bool PapyrusSetActorAddonName(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_addonName) {

			return a_actor && SetActorAddonName(a_actor->GetFormID(), Util::ToString(a_addonName));
		}

		bool PapyrusSetActorRank(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_rank) {

			return a_actor && SetActorRank(a_actor->GetFormID(), a_rank);
		}

		bool PapyrusSetActorAddonArmor(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::TESObjectARMO* a_armor) {

			return a_actor && SetActorAddonArmor(a_actor->GetFormID(), a_armor ? a_armor->GetFormID() : 0);
		}
	}

	void RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm) {

		a_vm->RegisterFunction("GetActorAddonName", SosPapyrusScript, PapyrusGetActorAddonName);
		a_vm->RegisterFunction("GetActorRank", SosPapyrusScript, PapyrusGetActorRank);
		a_vm->RegisterFunction("GetActorAddonArmor", SosPapyrusScript, PapyrusGetActorAddonArmor);

		a_vm->RegisterFunction("SetActorAddonName", SosPapyrusScript, PapyrusSetActorAddonName);
		a_vm->RegisterFunction("SetActorRank", SosPapyrusScript, PapyrusSetActorRank);
		a_vm->RegisterFunction("SetActorAddonArmor", SosPapyrusScript, PapyrusSetActorAddonArmor);
	}
}

// ===============================================================
// EXPORTS FOR OTHER SKSE PLUGINS
// ===============================================================
// Resolved by name with GetModuleHandle + GetProcAddress, see SOSAE_API.h.
// Only plain types cross the DLL boundary, never std::string or CommonLib types.

namespace {

	// Like snprintf: writes what fits (always with the final zero) and returns the full length of the text,
	// so the caller can tell if it was truncated. A null buffer only asks for the length.
	int CopyText(const std::string& a_text, char* a_buffer, int a_bufferSize) {

		if (a_buffer && a_bufferSize > 0) {

			const auto count = std::min(a_text.size(), static_cast<std::size_t>(a_bufferSize - 1));
			std::memcpy(a_buffer, a_text.data(), count);
			a_buffer[count] = '\0';
		}

		return static_cast<int>(a_text.size());
	}
}

extern "C" {

	// major * 10000 + minor * 100 + patch (4.0.0 is 40000)
	__declspec(dllexport) std::uint32_t SOSAE_GetAPIVersion() {

		return API::kVersion;
	}

	// Length of the name of the addon (0 if the actor has none). The name is copied to the buffer.
	__declspec(dllexport) int SOSAE_GetActorAddonName(std::uint32_t a_actorFormID, char* a_buffer, int a_bufferSize) {

		return CopyText(API::GetActorAddonName(a_actorFormID), a_buffer, a_bufferSize);
	}

	// 1-20, or 0 if the actor has no addon
	__declspec(dllexport) int SOSAE_GetActorRank(std::uint32_t a_actorFormID) {

		return API::GetActorRank(a_actorFormID);
	}

	// FormID of the addon armor, or 0
	__declspec(dllexport) std::uint32_t SOSAE_GetActorAddonArmor(std::uint32_t a_actorFormID) {

		return API::GetActorAddonArmor(a_actorFormID);
	}

	// A null, empty or "None" name removes the addon
	__declspec(dllexport) bool SOSAE_SetActorAddonName(std::uint32_t a_actorFormID, const char* a_addonName) {

		return API::SetActorAddonName(a_actorFormID, a_addonName ? a_addonName : "");
	}

	__declspec(dllexport) bool SOSAE_SetActorRank(std::uint32_t a_actorFormID, int a_rank) {

		return API::SetActorRank(a_actorFormID, a_rank);
	}

	// An armor FormID of 0 removes the addon
	__declspec(dllexport) bool SOSAE_SetActorAddonArmor(std::uint32_t a_actorFormID, std::uint32_t a_armorFormID) {

		return API::SetActorAddonArmor(a_actorFormID, a_armorFormID);
	}
}
