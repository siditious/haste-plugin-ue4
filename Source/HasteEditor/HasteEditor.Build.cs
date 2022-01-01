// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HasteEditor : ModuleRules
{
	public HasteEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
                	"HasteEditor/Private",
                    "Editor/GraphEditor/Private",
                    "Editor/GraphEditor/Private/KismetNodes",
                    "Editor/GraphEditor/Private/KismetPins",
            }
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "UnrealEd",
                "KismetWidgets",
                "GraphEditor",
                "EditorStyle",
                "Kismet",
                "InputCore"

				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{

                "Blutility",
                "UMG",
                "UMGEditor",
                "RenderCore",
                "EditorScriptingUtilities",
                    "PropertyEditor",
                    "WorkspaceMenuStructure",
                    "LevelEditor",
                    "EditorStyle",
                    "ContentBrowser",
                    "UnrealEd",
                    "EditorFramework",
                    "Projects"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
