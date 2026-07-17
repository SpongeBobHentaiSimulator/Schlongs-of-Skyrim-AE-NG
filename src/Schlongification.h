#pragma once

#include "SchlongLogic.h"
#include "util.h"

namespace SOS {

	class ObjectLoadedHandler : public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESObjectLoadedEvent* a_event,
			RE::BSTEventSource<RE::TESObjectLoadedEvent>* a_eventSource) override;
	};

	class InitScriptHandler : public RE::BSTEventSink<RE::TESInitScriptEvent> {
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESInitScriptEvent* a_event,
			RE::BSTEventSource<RE::TESInitScriptEvent>* a_eventSource) override;
	};

	class EquipEventHandler : public RE::BSTEventSink<RE::TESEquipEvent> {
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESEquipEvent* a_event,
			RE::BSTEventSource<RE::TESEquipEvent>* a_source) override;
	};

	void RegisterActivationEvents();

	void RegisterEquipEvent();
}