// Copyright 2015-2016 Code Respawn Technologies. MIT License

#include "HasteEditorModule.h"
#include "HasteEditorPrivatePCH.h"

TSharedPtr< FSlateStyleSet > FHasteEditorModule::StyleSet = nullptr;

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

#define LOCTEXT_NAMESPACE "HasteEditorModule" TSharedPtr< FSlateStyleSet > ExampleEdModeTool::StyleSet = nullptr;

void FHasteEditorModule::StartupModule() {
	RegisterStyleSet();
	FEditorModeRegistry::Get().RegisterMode<FEdModeHaste>(
	FEdModeHaste::EM_Haste,
	NSLOCTEXT("EditorModes", "HasteMode", "Haste"),
	FSlateIcon(StyleSet->GetStyleSetName(), "ExampleEdMode", "ExampleEdMode.Small"),
	true, 400
	);
}

void FHasteEditorModule::ShutdownModule()
{
	FEditorModeRegistry::Get().UnregisterMode(FEdModeHaste::EM_Haste);
}

void FHasteEditorModule::RegisterStyleSet()
{
	// Const icon sizes
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}
	const FString BaseDir = IPluginManager::Get().FindPlugin("Haste")->GetBaseDir();
	StyleSet = MakeShareable(new FSlateStyleSet("ExampleEdModeToolStyle"));
	StyleSet->SetContentRoot(BaseDir / TEXT("/Haste/EditorResources"));
	StyleSet->SetCoreContentRoot(BaseDir / TEXT("/Haste/EditorResources"));

	// Spline editor
	{
		StyleSet->Set("ExampleEdMode", new IMAGE_BRUSH(TEXT("IconExampleEditorMode"), Icon40x40));
		StyleSet->Set("ExampleEdMode.Small", new IMAGE_BRUSH(TEXT("IconExampleEditorMode"), Icon20x20));
	}

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHasteEditorModule, HasteEditor)

