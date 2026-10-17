#include "Menu.h"

#include "API.h"
#include "Actors.h"
#include "Armors.h"
#include "Input.h"
#include "SchlongLogic.h"
#include "Storage.h"
#include "util.h"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <filesystem>
#include <format>

#include "SKSEMenuFramework.h"

// The menu is drawn while the game renders, so it only READS the game from here.
// Everything that changes the game (equip, armors, overrides, animation) is queued to the game thread.
namespace Menu {

	namespace {

		namespace Gui = ImGuiMCP;

		// ===============================================================
		// STATE
		// ===============================================================

		// The addon data in memory has changes that are not in the JSON yet
		bool s_addonDataDirty = false;

		// Binding that is waiting for a key in the Controls page (-1 = none)
		int s_capturing = -1;

		// "Show all addons for the gender" of the NPC and Player pages
		bool s_showAllAddons = false;

		// Values of the "set for all" sliders
		int s_globalRank = Util::kDefaultRank;
		int s_globalChance = Util::kDefaultChance;
		int s_addonRank = Util::kDefaultRank;
		int s_addonChance = Util::kDefaultChance;

		// The uninstall button needs a second click
		bool s_confirmShutdown = false;

		// Result of the last maintenance button
		std::string s_status;

		// ===============================================================
		// KEYS
		// ===============================================================

		struct KeyInfo {

			Gui::ImGuiKey imguiKey; // To capture it in the menu
			int scanCode;           // DirectInput scan code, what the game reports and Settings::keys stores
			const char* name;
		};

		constexpr KeyInfo kKeys[] = {
			{ Gui::ImGuiKey_A, 30, "A" }, { Gui::ImGuiKey_B, 48, "B" }, { Gui::ImGuiKey_C, 46, "C" },
			{ Gui::ImGuiKey_D, 32, "D" }, { Gui::ImGuiKey_E, 18, "E" }, { Gui::ImGuiKey_F, 33, "F" },
			{ Gui::ImGuiKey_G, 34, "G" }, { Gui::ImGuiKey_H, 35, "H" }, { Gui::ImGuiKey_I, 23, "I" },
			{ Gui::ImGuiKey_J, 36, "J" }, { Gui::ImGuiKey_K, 37, "K" }, { Gui::ImGuiKey_L, 38, "L" },
			{ Gui::ImGuiKey_M, 50, "M" }, { Gui::ImGuiKey_N, 49, "N" }, { Gui::ImGuiKey_O, 24, "O" },
			{ Gui::ImGuiKey_P, 25, "P" }, { Gui::ImGuiKey_Q, 16, "Q" }, { Gui::ImGuiKey_R, 19, "R" },
			{ Gui::ImGuiKey_S, 31, "S" }, { Gui::ImGuiKey_T, 20, "T" }, { Gui::ImGuiKey_U, 22, "U" },
			{ Gui::ImGuiKey_V, 47, "V" }, { Gui::ImGuiKey_W, 17, "W" }, { Gui::ImGuiKey_X, 45, "X" },
			{ Gui::ImGuiKey_Y, 21, "Y" }, { Gui::ImGuiKey_Z, 44, "Z" },

			{ Gui::ImGuiKey_1, 2, "1" }, { Gui::ImGuiKey_2, 3, "2" }, { Gui::ImGuiKey_3, 4, "3" },
			{ Gui::ImGuiKey_4, 5, "4" }, { Gui::ImGuiKey_5, 6, "5" }, { Gui::ImGuiKey_6, 7, "6" },
			{ Gui::ImGuiKey_7, 8, "7" }, { Gui::ImGuiKey_8, 9, "8" }, { Gui::ImGuiKey_9, 10, "9" },
			{ Gui::ImGuiKey_0, 11, "0" },

			{ Gui::ImGuiKey_F1, 59, "F1" }, { Gui::ImGuiKey_F2, 60, "F2" }, { Gui::ImGuiKey_F3, 61, "F3" },
			{ Gui::ImGuiKey_F4, 62, "F4" }, { Gui::ImGuiKey_F5, 63, "F5" }, { Gui::ImGuiKey_F6, 64, "F6" },
			{ Gui::ImGuiKey_F7, 65, "F7" }, { Gui::ImGuiKey_F8, 66, "F8" }, { Gui::ImGuiKey_F9, 67, "F9" },
			{ Gui::ImGuiKey_F10, 68, "F10" }, { Gui::ImGuiKey_F11, 87, "F11" }, { Gui::ImGuiKey_F12, 88, "F12" },

			{ Gui::ImGuiKey_UpArrow, 200, "Up" }, { Gui::ImGuiKey_DownArrow, 208, "Down" },
			{ Gui::ImGuiKey_LeftArrow, 203, "Left" }, { Gui::ImGuiKey_RightArrow, 205, "Right" },

			{ Gui::ImGuiKey_Insert, 210, "Insert" }, { Gui::ImGuiKey_Delete, 211, "Delete" },
			{ Gui::ImGuiKey_Home, 199, "Home" }, { Gui::ImGuiKey_End, 207, "End" },
			{ Gui::ImGuiKey_PageUp, 201, "Page Up" }, { Gui::ImGuiKey_PageDown, 209, "Page Down" },

			{ Gui::ImGuiKey_Space, 57, "Space" }, { Gui::ImGuiKey_Tab, 15, "Tab" },
			{ Gui::ImGuiKey_Backspace, 14, "Backspace" }, { Gui::ImGuiKey_Enter, 28, "Enter" },
			{ Gui::ImGuiKey_Minus, 12, "-" }, { Gui::ImGuiKey_Equal, 13, "=" },
			{ Gui::ImGuiKey_LeftBracket, 26, "[" }, { Gui::ImGuiKey_RightBracket, 27, "]" },
			{ Gui::ImGuiKey_Backslash, 43, "\\" }, { Gui::ImGuiKey_Semicolon, 39, ";" },
			{ Gui::ImGuiKey_Apostrophe, 40, "'" }, { Gui::ImGuiKey_GraveAccent, 41, "`" },
			{ Gui::ImGuiKey_Comma, 51, "," }, { Gui::ImGuiKey_Period, 52, "." }, { Gui::ImGuiKey_Slash, 53, "/" },

			{ Gui::ImGuiKey_Pause, 197, "Pause" }, { Gui::ImGuiKey_ScrollLock, 70, "Scroll Lock" },
			{ Gui::ImGuiKey_CapsLock, 58, "Caps Lock" }, { Gui::ImGuiKey_NumLock, 69, "Num Lock" },
			{ Gui::ImGuiKey_PrintScreen, 183, "Print Screen" },

			{ Gui::ImGuiKey_Keypad0, 82, "Num 0" }, { Gui::ImGuiKey_Keypad1, 79, "Num 1" },
			{ Gui::ImGuiKey_Keypad2, 80, "Num 2" }, { Gui::ImGuiKey_Keypad3, 81, "Num 3" },
			{ Gui::ImGuiKey_Keypad4, 75, "Num 4" }, { Gui::ImGuiKey_Keypad5, 76, "Num 5" },
			{ Gui::ImGuiKey_Keypad6, 77, "Num 6" }, { Gui::ImGuiKey_Keypad7, 71, "Num 7" },
			{ Gui::ImGuiKey_Keypad8, 72, "Num 8" }, { Gui::ImGuiKey_Keypad9, 73, "Num 9" },
			{ Gui::ImGuiKey_KeypadDecimal, 83, "Num ." }, { Gui::ImGuiKey_KeypadDivide, 181, "Num /" },
			{ Gui::ImGuiKey_KeypadMultiply, 55, "Num *" }, { Gui::ImGuiKey_KeypadSubtract, 74, "Num -" },
			{ Gui::ImGuiKey_KeypadAdd, 78, "Num +" }, { Gui::ImGuiKey_KeypadEnter, 156, "Num Enter" }
		};

		std::string GetKeyName(int a_scanCode) {

			if (a_scanCode == 0)
				return "Unbound";

			for (const auto& key : kKeys) {
				if (key.scanCode == a_scanCode)
					return key.name;
			}

			return std::format("Key {}", a_scanCode);
		}

		// ===============================================================
		// ACTIONS (queued to the game thread)
		// ===============================================================

		// Remembers the addon and rank of the actor in the overrides JSON, so they survive new games
		void QueueSaveOverride(RE::FormID a_actorID) {

			SKSE::GetTaskInterface()->AddTask([a_actorID]() {

				auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_actorID);
				Actors::SaveOverride(actor ? actor->GetActorBase() : nullptr);
			});
		}

		void ChangeAddon(RE::FormID a_actorID, const std::string& a_addonName) {

			if (API::SetActorAddonName(a_actorID, a_addonName))
				QueueSaveOverride(a_actorID);
		}

		// Makes an armor revealing or concealing and updates the addon of the actor that wears it
		void QueueSetArmorOverride(RE::FormID a_actorID, RE::FormID a_armorID, bool a_revealing) {

			SKSE::GetTaskInterface()->AddTask([a_actorID, a_armorID, a_revealing]() {

				auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_actorID);
				auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(a_armorID);
				if (!actor || !armor)
					return;

				Armors::SetOverride(armor, a_revealing);

				// The armor hides the addon now: its item goes away
				if (!a_revealing) {
					Util::RemoveSOSItemsFromInventory(actor);
					return;
				}

				// The armor shows the addon now, unless underwear still hides it
				auto* schlong = SchlongLogic::DetermineWinningAddon(actor);
				SchlongLogic::UpdateFactions(actor, schlong);

				if (schlong && !Util::ActorHasEquippedArmorWithKeyword(actor, UndwKW))
					SchlongLogic::EquipAddon(actor, schlong);
			});
		}

		void QueueRemoveConcealing(RE::FormID a_actorID) {

			SKSE::GetTaskInterface()->AddTask([a_actorID]() {
				Armors::RemoveConcealing(RE::TESForm::LookupByID<RE::Actor>(a_actorID));
			});
		}

		void QueueClearNPCData() {

			SKSE::GetTaskInterface()->AddTask([]() {

				Storage::ClearAllNPCData();
				Actors::ApplyOverrides();
			});
		}

		void QueueRebuildAddonData() {

			SKSE::GetTaskInterface()->AddTask([]() {
				Storage::RebuildAddonData();
			});
		}

		// Clears all the runtime state, used before uninstalling or updating the mod
		void QueuePrepareForShutdown() {

			SKSE::GetTaskInterface()->AddTask([]() {

				Armors::ClearOverrides();
				Storage::ClearAll();
				Input::ClearState();

				SKSE::log::info("SOS: Runtime state cleared for shutdown.");
			});
		}

		// ===============================================================
		// WIDGETS
		// ===============================================================

		struct Edit {

			bool changed;   // The value changed this frame: apply it right away
			bool committed; // The user let go of the widget after changing it: save it
		};

		Edit SliderIntEdit(const char* a_label, int* a_value, int a_min, int a_max, const char* a_format = "%d") {

			Edit edit{};
			edit.changed = Gui::SliderInt(a_label, a_value, a_min, a_max, a_format);
			edit.committed = Gui::IsItemDeactivatedAfterEdit();
			return edit;
		}

		Edit SliderFloatEdit(const char* a_label, float* a_value, float a_min, float a_max, const char* a_format) {

			Edit edit{};
			edit.changed = Gui::SliderFloat(a_label, a_value, a_min, a_max, a_format);
			edit.committed = Gui::IsItemDeactivatedAfterEdit();
			return edit;
		}

		void Tooltip(const char* a_text) {

			if (Gui::IsItemHovered())
				Gui::SetTooltip("%s", a_text);
		}

		// Writes the addon data to the JSON if the menu changed it
		void SaveAddonDataIfDirty() {

			if (!s_addonDataDirty)
				return;

			s_addonDataDirty = false;
			Storage::SaveAddonData();
		}

		// Shown at the top of every page when the addons can't work
		void RenderBanner() {

			if (g_editorIDsAvailable)
				return;

			Gui::TextColored(Gui::ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f }, "Editor IDs are not available.");
			Gui::TextWrapped("This mod needs powerof3's Tweaks with the 'Load EditorIDs' tweak enabled and Schlongs of Skyrim.esp in the load order. Addons and races will not work correctly without them.");
			Gui::Separator();
		}

		// ===============================================================
		// ACTOR SECTION (NPC and Player pages)
		// ===============================================================

		// Information, schlong type, rank and armor tools of an actor
		void RenderActorSection(RE::Actor* a_actor) {

			auto* base = a_actor ? a_actor->GetActorBase() : nullptr;
			if (!base)
				return;

			const RE::FormID actorID = a_actor->GetFormID();
			const auto data = Storage::GetNPCData(base->GetFormID());
			const std::string addon = data ? data->addonName : std::string();

			Gui::PushID(static_cast<int>(actorID));

			// ---- Information ----

			Gui::SeparatorText("Information");

			const char* name = base->GetFullName();
			Gui::Text("Name: %s", (name && name[0] != '\0') ? name : "Unknown");

			auto* race = base->GetRace();
			const std::string raceKey = race ? Util::GetRaceKey(race) : "Unknown";
			Gui::Text("Race: %s", raceKey.c_str());

			if (addon.empty()) {
				Gui::Text("Status: No schlong");
			}
			else {
				Gui::Text("Type: %s", addon.c_str());
				Gui::Text("Rank: %d", static_cast<int>(data->rank));
			}

			if (!a_actor->Is3DLoaded())
				Gui::TextDisabled("The actor is not loaded, the changes are applied when it loads.");

			// ---- Schlong type ----

			Gui::SeparatorText("Schlong Type");

			auto options = s_showAllAddons ? Storage::GetAddonNamesForSex(base->GetSex()) : SchlongLogic::GetCompatibleAddonNames(base);

			// The current addon is always in the list, even if it isn't compatible
			if (!addon.empty() && std::find(options.begin(), options.end(), addon) == options.end())
				options.insert(options.begin(), addon);

			if (Gui::Selectable("None", addon.empty()))
				ChangeAddon(actorID, "");

			for (const auto& option : options) {

				if (Gui::Selectable(option.c_str(), option == addon))
					ChangeAddon(actorID, option);
			}

			Gui::Checkbox("Show all addons for the gender", &s_showAllAddons);
			Tooltip("CAUTION: use this for custom races that are not detected automatically.");

			// ---- Rank ----

			if (!addon.empty()) {

				Gui::SeparatorText("Rank");

				int rank = data->rank;
				const Edit edit = SliderIntEdit("Change Rank", &rank, 1, Util::kMaxRank);

				if (edit.changed)
					API::SetActorRank(actorID, rank);

				if (edit.committed)
					QueueSaveOverride(actorID);
			}

			// ---- Armor ----

			Gui::SeparatorText("Armor");

			auto* armor = a_actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kBody);
			if (!armor) {
				Gui::TextDisabled("No slot 32 armor equipped");
			}
			else {
				const bool concealing = (Util::GetArmorSlots(armor) & SLOT_52) != 0;
				const char* armorName = armor->GetFullName();

				Gui::Text("Equipped armor: %s", (armorName && armorName[0] != '\0') ? armorName : "Unknown");
				Gui::Text("Is: %s", concealing ? "Concealing" : "Revealing");

				if (concealing) {

					if (Gui::Button("Set Revealing"))
						QueueSetArmorOverride(actorID, armor->GetFormID(), true);
				}
				else {

					if (Gui::Button("Set Concealing"))
						QueueSetArmorOverride(actorID, armor->GetFormID(), false);
				}
			}

			if (Gui::Button("Remove Slot52 and Underwear"))
				QueueRemoveConcealing(actorID);

			Tooltip("Unequips the armor in slot 52 and the underwear so the schlong shows. Use it if Set Revealing doesn't work.");

			Gui::PopID();
		}

		// ===============================================================
		// PAGES
		// ===============================================================

		void __stdcall RenderNPCPage() {

			RenderBanner();

			Gui::SeparatorText("Pinned NPC");

			const std::string pinKey = GetKeyName(g_settings.keys[Binding::kPin]);
			Gui::TextWrapped("Look at an NPC and press the pin key (%s) to pin it. The bend keys act on the pinned NPC.", pinKey.c_str());

			auto* actor = Input::GetPinnedActor();
			if (!actor) {
				Gui::TextDisabled("No NPC pinned.");
				return;
			}

			if (Gui::Button("Unpin")) {
				Input::ClearPinned();
				return;
			}

			RenderActorSection(actor);
		}

		void __stdcall RenderPlayerPage() {

			RenderBanner();
			RenderActorSection(RE::PlayerCharacter::GetSingleton());
		}

		// Race table of an addon: enabled, rank and chance of every race
		void RenderRaceTable(const std::string& a_addon) {

			const auto races = Storage::GetAddonRaces(a_addon);
			if (races.empty()) {
				Gui::TextDisabled("This addon has no races.");
				return;
			}

			constexpr int flags = Gui::ImGuiTableFlags_RowBg | Gui::ImGuiTableFlags_Borders | Gui::ImGuiTableFlags_SizingStretchProp;
			if (!Gui::BeginTable("races", 4, flags))
				return;

			Gui::TableSetupColumn("Race", Gui::ImGuiTableColumnFlags_WidthStretch, 2.0f);
			Gui::TableSetupColumn("Enabled", Gui::ImGuiTableColumnFlags_WidthFixed, 70.0f);
			Gui::TableSetupColumn("Rank", Gui::ImGuiTableColumnFlags_WidthStretch, 3.0f);
			Gui::TableSetupColumn("Chance", Gui::ImGuiTableColumnFlags_WidthStretch, 3.0f);
			Gui::TableHeadersRow();

			for (const auto& [raceName, race] : races) {

				bool enabled = race.enabled;
				int rank = race.rank;
				int chance = race.chance;

				Gui::PushID(raceName.c_str());
				Gui::TableNextRow();

				Gui::TableNextColumn();
				Gui::TextUnformatted(raceName.c_str());

				Gui::TableNextColumn();
				const bool toggled = Gui::Checkbox("##enabled", &enabled);

				Gui::TableNextColumn();
				Gui::SetNextItemWidth(-FLT_MIN);
				const Edit rankEdit = SliderIntEdit("##rank", &rank, -1, Util::kMaxRank);

				Gui::TableNextColumn();
				Gui::SetNextItemWidth(-FLT_MIN);
				const Edit chanceEdit = SliderIntEdit("##chance", &chance, 0, 100, "%d%%");

				Gui::PopID();

				if (toggled || rankEdit.changed || chanceEdit.changed) {
					Storage::SetRaceValues(a_addon, raceName, rank, chance, enabled);
					s_addonDataDirty = true;
				}

				if (toggled || rankEdit.committed || chanceEdit.committed)
					SaveAddonDataIfDirty();
			}

			Gui::EndTable();
		}

		void RenderAddonTab(const std::string& a_addon) {

			Gui::SeparatorText("All races in this addon");

			Gui::SetNextItemWidth(250.0f);
			SliderIntEdit("Rank##all", &s_addonRank, -1, Util::kMaxRank);
			Gui::SameLine();
			if (Gui::Button("Apply##allrank")) {
				Storage::SetRankForAll(a_addon, s_addonRank);
				s_addonDataDirty = true;
				SaveAddonDataIfDirty();
			}

			Gui::SetNextItemWidth(250.0f);
			SliderIntEdit("Chance##all", &s_addonChance, 0, 100, "%d%%");
			Gui::SameLine();
			if (Gui::Button("Apply##allchance")) {
				Storage::SetChanceForAll(a_addon, s_addonChance);
				s_addonDataDirty = true;
				SaveAddonDataIfDirty();
			}

			if (Gui::Button("Enable all races")) {
				Storage::SetEnabledForAll(a_addon, true);
				s_addonDataDirty = true;
				SaveAddonDataIfDirty();
			}

			Gui::SameLine();
			if (Gui::Button("Disable all races")) {
				Storage::SetEnabledForAll(a_addon, false);
				s_addonDataDirty = true;
				SaveAddonDataIfDirty();
			}

			Gui::SeparatorText("Races");
			RenderRaceTable(a_addon);
		}

		// One tab per addon
		void __stdcall RenderAddonsPage() {

			RenderBanner();

			const auto addons = Storage::GetAddonNames();
			if (addons.empty()) {
				Gui::TextDisabled("No addons loaded.");
				return;
			}

			if (!Gui::BeginTabBar("addons", Gui::ImGuiTabBarFlags_FittingPolicyScroll))
				return;

			for (const auto& addon : addons) {

				if (Gui::BeginTabItem(addon.c_str())) {
					RenderAddonTab(addon);
					Gui::EndTabItem();
				}
			}

			Gui::EndTabBar();
		}

		void __stdcall RenderSettingsPage() {

			RenderBanner();

			// ---- INI defaults ----

			Gui::SeparatorText("JSON and INI Options");

			if (Gui::Checkbox("All addons will be enabled", &g_settings.enabled))
				g_settings.Save();

			Tooltip("Makes the new race entries of the JSON enabled or disabled by default.");

			Gui::SetNextItemWidth(250.0f);
			const Edit defaultRank = SliderIntEdit("Default rank for addons", &g_settings.rank, -1, Util::kMaxRank);
			Tooltip("-1 is a uniform random rank, any other value is the center of the Gaussian distribution.");

			Gui::SetNextItemWidth(250.0f);
			const Edit defaultChance = SliderIntEdit("Default chance for addons", &g_settings.chance, 0, 100, "%d%%");

			if (defaultRank.committed || defaultChance.committed)
				g_settings.Save();

			if (Gui::Button("Rebuild the JSON")) {
				s_addonDataDirty = false;
				QueueRebuildAddonData();
				s_status = "JSON rebuild requested, see the log for the result.";
			}

			Tooltip("Rebuilds the JSON with the current INI settings and addons. The changes made in the menu are lost.");

			// ---- Deviation ----

			Gui::SeparatorText("Rank Standard Deviation");

			Gui::SetNextItemWidth(250.0f);
			const Edit deviation = SliderFloatEdit("Standard deviation", &g_settings.gaussianDeviation, 0.0f, 20.0f, "%.1f");
			Tooltip("Spread of the random ranks around the rank of the race. 0 gives always that rank.");

			if (deviation.committed)
				g_settings.Save();

			// ---- Global overrides ----

			Gui::SeparatorText("Global Overrides");
			Gui::TextWrapped("Apply a value to every race of every loaded addon, without touching the other values.");

			Gui::SetNextItemWidth(250.0f);
			SliderIntEdit("Rank##global", &s_globalRank, -1, Util::kMaxRank);
			Gui::SameLine();
			if (Gui::Button("Apply##globalrank")) {
				Storage::SetRankForAll("", s_globalRank);
				s_addonDataDirty = true;
				SaveAddonDataIfDirty();
				s_status = "Rank applied to every race in every loaded addon.";
			}

			Gui::SetNextItemWidth(250.0f);
			SliderIntEdit("Chance##global", &s_globalChance, 0, 100, "%d%%");
			Gui::SameLine();
			if (Gui::Button("Apply##globalchance")) {
				Storage::SetChanceForAll("", s_globalChance);
				s_addonDataDirty = true;
				SaveAddonDataIfDirty();
				s_status = "Chance applied to every race in every loaded addon.";
			}

			// ---- SexLab ----

			Gui::SeparatorText("SexLab");

			if (Gui::Checkbox("Force male treatment on female schlong", &g_settings.sexLabForceMale))
				g_settings.Save();

			Tooltip("If disabled, females keep being treated as female by SexLab even with a schlong equipped.");

			if (!s_status.empty()) {
				Gui::Separator();
				Gui::TextWrapped("%s", s_status.c_str());
			}
		}

		// Waits for the next key while a binding is being captured. Escape cancels.
		void UpdateKeyCapture() {

			if (s_capturing < 0)
				return;

			if (Gui::IsKeyPressed(Gui::ImGuiKey_Escape, false)) {
				s_capturing = -1;
				return;
			}

			for (const auto& key : kKeys) {

				if (!Gui::IsKeyPressed(key.imguiKey, false))
					continue;

				g_settings.keys[static_cast<std::size_t>(s_capturing)] = key.scanCode;
				g_settings.Save();
				s_capturing = -1;
				return;
			}
		}

		void RenderKeyBinding(const char* a_label, std::size_t a_binding) {

			Gui::PushID(static_cast<int>(a_binding));

			const bool capturing = s_capturing == static_cast<int>(a_binding);
			const std::string text = capturing ? std::string("Press a key...") : GetKeyName(g_settings.keys[a_binding]);

			// The ### keeps the ID of the button when its text changes
			if (Gui::Button(std::format("{}###bind", text).c_str(), Gui::ImVec2{ 150.0f, 0.0f }))
				s_capturing = static_cast<int>(a_binding);

			Gui::SameLine();

			if (Gui::SmallButton("Clear")) {

				g_settings.keys[a_binding] = 0;
				g_settings.Save();

				if (capturing)
					s_capturing = -1;
			}

			Gui::SameLine();
			Gui::Text("%s", a_label);

			Gui::PopID();
		}

		void __stdcall RenderControlsPage() {

			RenderBanner();

			// Before the buttons, so the click that starts the capture is not read as the new key
			UpdateKeyCapture();

			Gui::TextWrapped("Click a key to change it and press the new one. Escape cancels, Clear unbinds it.");

			Gui::SeparatorText("Pin");
			RenderKeyBinding("Pin the NPC under the crosshair", Binding::kPin);

			Gui::SeparatorText("Bend - Pinned NPC");
			RenderKeyBinding("Bend Up", Binding::kBendUp);
			RenderKeyBinding("Bend Down", Binding::kBendDown);

			Gui::SeparatorText("Bend - Player (Self)");
			RenderKeyBinding("Player Bend Up", Binding::kPlayerBendUp);
			RenderKeyBinding("Player Bend Down", Binding::kPlayerBendDown);

			// The same key in two actions makes both of them happen
			bool repeated = false;
			for (std::size_t i = 0; i < g_settings.keys.size(); ++i) {
				for (std::size_t j = i + 1; j < g_settings.keys.size(); ++j) {

					if (g_settings.keys[i] != 0 && g_settings.keys[i] == g_settings.keys[j])
						repeated = true;
				}
			}

			if (repeated) {
				Gui::Separator();
				Gui::TextColored(Gui::ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f }, "The same key is used by more than one action.");
			}
		}

		void __stdcall RenderDebugPage() {

			RenderBanner();

			Gui::Text("Version %u.%u.%u", API::kVersion / 10000, (API::kVersion / 100) % 100, API::kVersion % 100);

			Gui::SeparatorText("NPC Data");

			if (Gui::Button("Clear NPC data and load the saved one")) {
				QueueClearNPCData();
				s_status = "NPC data cleared and the NPC JSON data reapplied.";
			}

			Tooltip("Clears the NPC data of this game and loads the data saved in the NPC JSON. That JSON is written when an actor is changed in this menu.");

			Gui::SeparatorText("Uninstall / Update");

			if (!s_confirmShutdown) {

				if (Gui::Button("Uninstall or prepare for update"))
					s_confirmShutdown = true;

				Tooltip("Clears the runtime state of the mod so it can be removed or updated.");
			}
			else {

				Gui::TextWrapped("This clears all the runtime data of the mod. Continue?");

				if (Gui::Button("Yes, clear it")) {
					s_confirmShutdown = false;
					s_addonDataDirty = false;
					QueuePrepareForShutdown();
					s_status = "Save the game and exit it, remove or update the mod and load again.";
				}

				Gui::SameLine();
				if (Gui::Button("Cancel"))
					s_confirmShutdown = false;
			}

			if (!s_status.empty()) {
				Gui::Separator();
				Gui::TextWrapped("%s", s_status.c_str());
			}
		}

		// ===============================================================
		// EVENTS
		// ===============================================================

		void __stdcall OnMenuEvent(SKSEMenuFramework::Model::EventType a_type) {

			if (a_type != SKSEMenuFramework::Model::kCloseMenu)
				return;

			// Safety net: whatever was not saved yet is written when the menu closes
			SaveAddonDataIfDirty();

			s_capturing = -1;
			s_confirmShutdown = false;
		}
	}

	void Register() {

		if (!GetMenuFrameworkModule()) {
			SKSE::log::warn("SOS: SKSE Menu Framework is not loaded, the menu is not available.");
			return;
		}

		SKSEMenuFramework::SetSection("Schlongs of Skyrim AE");
		SKSEMenuFramework::AddSectionItem("NPC", RenderNPCPage);
		SKSEMenuFramework::AddSectionItem("Player", RenderPlayerPage);
		SKSEMenuFramework::AddSectionItem("Addons", RenderAddonsPage);
		SKSEMenuFramework::AddSectionItem("Settings", RenderSettingsPage);
		SKSEMenuFramework::AddSectionItem("Controls", RenderControlsPage);
		SKSEMenuFramework::AddSectionItem("Debug", RenderDebugPage);
		SKSEMenuFramework::AddEvent(OnMenuEvent, 0.0f);

		SKSE::log::info("SOS: Menu registered in SKSE Menu Framework.");
	}
}
