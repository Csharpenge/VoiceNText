#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "Framework/Docking/TabManager.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenuDelegates.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "VoiceNTextSettings.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "VoiceNTextEditor"

namespace
{
	const FName VoiceNTextSettingsTabName(TEXT("VoiceNTextSettings"));
}

/**
 * Editor side of VoiceNText: adds a toolbar button next to Play and hosts the
 * settings tab. All configuration lives in UVoiceNTextSettings (config=Game).
 */
class FVoiceNTextEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()
			->RegisterNomadTabSpawner(
				VoiceNTextSettingsTabName,
				FOnSpawnTab::CreateRaw(this, &FVoiceNTextEditorModule::SpawnSettingsTab))
			.SetDisplayName(LOCTEXT("VoiceNTextTabTitle", "Voice N Text"))
			.SetTooltipText(LOCTEXT("VoiceNTextTabTooltip", "Speech to text / text to speech settings"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		StartupCallbackHandle = UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FVoiceNTextEditorModule::RegisterMenus));

		UE_LOG(LogTemp, Log, TEXT("VoiceNTextEditor module started."));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(StartupCallbackHandle);
		UToolMenus::UnregisterOwner(this);

		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(VoiceNTextSettingsTabName);
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);

		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		if (!ToolbarMenu)
		{
			return;
		}

		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Play");
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			"VoiceNTextSettings",
			FExecuteAction::CreateRaw(this, &FVoiceNTextEditorModule::OpenSettingsTab),
			LOCTEXT("VoiceNTextToolbarLabel", "Voice N Text"),
			LOCTEXT("VoiceNTextToolbarTooltip", "Open VoiceNText speech / text settings"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings")));

		UE_LOG(LogTemp, Log, TEXT("VoiceNTextEditor: toolbar button registered on LevelEditor.LevelEditorToolBar.PlayToolBar."));
	}

	void OpenSettingsTab()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(VoiceNTextSettingsTabName);
	}

	TSharedRef<SDockTab> SpawnSettingsTab(const FSpawnTabArgs& Args)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bHideSelectionTip = true;

		TSharedRef<IDetailsView> DetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);
		DetailsView->SetObject(UVoiceNTextSettings::GetMutable());
		DetailsView->OnFinishedChangingProperties().AddLambda([](const FPropertyChangedEvent&)
		{
			UVoiceNTextSettings::GetMutable()->SaveConfig();
		});

		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				.Padding(6.f)
				[
					DetailsView
				]
			];
	}

	FDelegateHandle StartupCallbackHandle;
};

IMPLEMENT_MODULE(FVoiceNTextEditorModule, VoiceNTextEditor)
