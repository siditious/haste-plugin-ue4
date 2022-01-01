// Copyright 2015-2016 Code Respawn Technologies. MIT License

#pragma once

#include "UnrealEd.h"
#include "AssetToolsModule.h"
#include "PropertyEditorModule.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "HasteEdMode.h"
#include "Modules/ModuleInterface.h"


/**
 * The public interface to this module
 */
class IHasteEditorModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IHasteEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IHasteEditorModule >("HasteEditorModule");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "HasteEditorModule" );
	}
};

class FHasteEditorModule : public IHasteEditorModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;


	virtual void ShutdownModule() override;

	static TSharedPtr< class FSlateStyleSet > StyleSet;

	void RegisterStyleSet();
};



