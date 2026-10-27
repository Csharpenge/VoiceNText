#include "VoiceNText.h"

#include "Modules/ModuleManager.h"
#include "Providers/VoiceNTextNullProvider.h"
#include "Providers/VoiceNTextRestProvider.h"

void FVoiceNTextModule::StartupModule()
{
	RegisterProvider(TEXT("Null"), []() { return MakeUnique<FVoiceNTextNullProvider>(); });
	RegisterProvider(TEXT("Rest"), []() { return MakeUnique<FVoiceNTextRestProvider>(); });

	UE_LOG(LogTemp, Log, TEXT("VoiceNText module started (providers: Null, Rest)."));
}

void FVoiceNTextModule::ShutdownModule()
{
	Factories.Empty();
}

FVoiceNTextModule& FVoiceNTextModule::Get()
{
	return FModuleManager::LoadModuleChecked<FVoiceNTextModule>(TEXT("VoiceNText"));
}

void FVoiceNTextModule::RegisterProvider(FName Id, TFunction<TUniquePtr<IVoiceNTextProvider>()> Factory)
{
	Factories.Add(Id, MoveTemp(Factory));
}

void FVoiceNTextModule::UnregisterProvider(FName Id)
{
	Factories.Remove(Id);
}

TUniquePtr<IVoiceNTextProvider> FVoiceNTextModule::CreateProvider(FName Id) const
{
	if (const TFunction<TUniquePtr<IVoiceNTextProvider>()>* Factory = Factories.Find(Id))
	{
		return (*Factory)();
	}
	return nullptr;
}

TArray<FName> FVoiceNTextModule::GetProviderIds() const
{
	TArray<FName> Ids;
	Factories.GetKeys(Ids);
	return Ids;
}

IMPLEMENT_MODULE(FVoiceNTextModule, VoiceNText)
