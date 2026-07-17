#include "Papyrus.h"

std::mutex g_shutdownMutex;

namespace Papyrus {

	static RE::Actor* GetActorUnderCrosshair(RE::StaticFunctionTag*) {

		auto* pickData = RE::CrosshairPickData::GetSingleton();
		if (!pickData)
			return RE::PlayerCharacter::GetSingleton();

		auto target = pickData->targetActor;
		if (target)
			if (auto ref = target.get())
				if (auto* actor = ref->As<RE::Actor>())
					return actor;

		return RE::PlayerCharacter::GetSingleton();
	}

	static RE::TESObjectARMO* GetEquippedBodyArmor(RE::StaticFunctionTag*, RE::Actor* a_actor) {

		if (!a_actor)
			return nullptr;

		return a_actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kBody);
	}

	static void ExecuteErectionCycle(RE::StaticFunctionTag*, RE::Actor* a_actor, uint32_t a_durationSeconds, bool Fast) {

		if (!a_actor || !a_actor->Is3DLoaded())
			return;

		a_actor->SetGraphVariableFloat("SOS_Erection", 1.0f);

		if (Fast)
			a_actor->NotifyAnimationGraph("SOSFastErect");
		else
			a_actor->NotifyAnimationGraph("SOSSlowErect");

		auto actorHandle = a_actor->GetHandle();

		std::thread([actorHandle, a_durationSeconds]() {
			std::this_thread::sleep_for(std::chrono::seconds(a_durationSeconds));

			SKSE::GetTaskInterface()->AddTask([actorHandle]() {
				if (auto actor = actorHandle.get()) {
					if (actor->Is3DLoaded()) {
						actor->SetGraphVariableFloat("SOS_Erection", 0.0f);
						actor->NotifyAnimationGraph("SOSFlaccid");
					}
				}
				});
			}).detach();
	}

	static void PrepareForShutdown(RE::StaticFunctionTag*) {

		std::lock_guard<std::mutex> lock(g_shutdownMutex);

		g_armorOverrides.clear();
		Storage::s_addons.clear();
		Storage::s_genderGroups.male.clear();
		Storage::s_genderGroups.female.clear();
		Storage::g_addonBoneData.clear();
		Storage::s_npcData.clear();


		auto* player = RE::PlayerCharacter::GetSingleton();
		if (player && player->Is3DLoaded()) {
			player->SetGraphVariableFloat("SOS_Erection", 0.0f);
			player->NotifyAnimationGraph("SOSFlaccid");
		}

		SKSE::log::info("SOS: Runtime state cleared for shutdown.");
	}

	static void SetSchlongBend(RE::StaticFunctionTag*, RE::Actor* a_actor, int32_t a_value) {

		if (!a_actor || !a_actor->Is3DLoaded())
			return;

		if (a_value < 0)
			a_actor->NotifyAnimationGraph("SOSFlaccid");
		else {

			int32_t input = std::clamp(a_value, 0, 20);
			int32_t pos = input - 10;

			if (pos > 10)
				pos = 10;

			std::string eventName = "SOSBend" + std::to_string(pos);
			a_actor->NotifyAnimationGraph(eventName);
		}
	}

	static std::string GetActorRaceEditorID(RE::StaticFunctionTag*, RE::Actor* a_actor) {

		if (!a_actor)
			return "None";

		auto* base = a_actor->GetActorBase();
		auto* race = base ? base->GetRace() : a_actor->GetRace();

		if (!race)
			return "Unknown Race";

		const char* editorID = race->GetFormEditorID();

		if (editorID && editorID[0] != '\0')
			return editorID;

		return std::format("0x{:08X}", race->GetFormID());
	}

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm) {

		if (!a_vm)
			return false;

		a_vm->RegisterFunction("GetActorUnderCrosshair", SosPapyrusScript, GetActorUnderCrosshair);
		a_vm->RegisterFunction("GetEquippedBodyArmor", SosPapyrusScript, GetEquippedBodyArmor);
		a_vm->RegisterFunction("SetSchlongBend", SosPapyrusScript, SetSchlongBend);
		a_vm->RegisterFunction("ExecuteErectionCycle", SosPapyrusScript, ExecuteErectionCycle);
		a_vm->RegisterFunction("PrepareForShutdown", SosPapyrusScript, PrepareForShutdown);
		a_vm->RegisterFunction("GetActorRaceEditorID", SosPapyrusScript, GetActorRaceEditorID);

		return true;
	}
}