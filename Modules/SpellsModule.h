#pragma once

#include "../Core/ModuleBase.h"
#include "../Core/SettingsStore.h"
#include "SpellPicker.h"

#include <mq/Plugin.h>

#include <string>
#include <vector>

class SpellsModule : public ModuleBase
{
public:
	const char* GetName() const override { return "Spells"; }

	void OnInit() override;
	void OnPulse() override;
	void OnRenderGUI() override;

private:
	enum class PickerTarget { None, LiveGem, SetSlot };

	int GemCount() const;
	void DrawOptionsButton();
	void DrawQuickSetMenu();
	void DrawSpellSetManager();

	void OpenPickerForGem(int gemIndex);
	void OpenPickerForSetSlot(int slot);

	std::vector<SpellSetRow> CurrentLoadout() const; // from live gems
	std::vector<SpellSetRow> EditorRows() const;     // from m_editSet
	void SeedEditorFromGems();
	void LoadSetIntoEditor(const std::string& name);
	void ApplyEditorToGame();
	void ApplyNamedSetToGame(const std::string& name);
	void ClearAllGems();

	void RefreshSetNames();
	void ProcessDbQueue();

	SpellPicker  m_picker;
	PickerTarget m_pickerTarget = PickerTarget::None;
	int          m_pickerSlot = 0;

	bool                     m_managerOpen = false;
	std::string              m_selectedSet;
	std::vector<int>         m_editSet; // index = gem slot, value = spellId (0 = empty)
	char                     m_newNameBuf[64] = {};
	bool                     m_confirmDelete = false;
	std::vector<std::string> m_setNames; // cached set-name list

	struct DbOp
	{
		bool                     isDelete = false;
		std::string              name;
		std::vector<SpellSetRow> rows;
	};
	std::vector<DbOp> m_dbQueue;
	bool              m_refreshNames = false;
};
