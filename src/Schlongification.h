#pragma once

namespace SOS {

	// Listens to the events that decide when an addon has to be equipped or removed:
	// an actor loads, an actor is initialized and an actor equips or unequips something
	class EventHandler :
		public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
		public RE::BSTEventSink<RE::TESInitScriptEvent>,
		public RE::BSTEventSink<RE::TESEquipEvent> {

	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESObjectLoadedEvent* a_event,
			RE::BSTEventSource<RE::TESObjectLoadedEvent>* a_eventSource) override;

		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESInitScriptEvent* a_event,
			RE::BSTEventSource<RE::TESInitScriptEvent>* a_eventSource) override;

		RE::BSEventNotifyControl ProcessEvent(
			const RE::TESEquipEvent* a_event,
			RE::BSTEventSource<RE::TESEquipEvent>* a_eventSource) override;
	};

	void RegisterEvents();
}
