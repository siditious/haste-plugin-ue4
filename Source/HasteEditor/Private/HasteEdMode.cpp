// Copyright 2015-2016 Code Respawn Technologies. MIT License

#include "HasteEdMode.h"
#include "HasteEditorPrivatePCH.h"

#include "UnrealEd.h"
#include "StaticMeshResources.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"

#include "Modules/ModuleManager.h"

#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Toolkits/ToolkitManager.h"

#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#include "UnrealWidget.h"

//Slate dependencies
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Editor/LevelEditor/Public/SLevelViewport.h"
#include "ContentBrowserModule.h"
#include "HasteEdModeToolkit.h"
#include "HasteEdModeSettings.h"
#include "Transformer/HasteTransformLogic.h"

FEditorModeID FEdModeHaste::EM_Haste(TEXT("EM_Haste"));


#define LOCTEXT_NAMESPACE "HasteEdMode"
#define HASTE_SNAP_TRACE (10000.f)

DEFINE_LOG_CATEGORY(LogHasteMode);

#define BRUSH_RADIUS 100
//
// FEdModeHaste
//

const float ROTATION_SPEED = 10;

/** Constructor */
FEdModeHaste::FEdModeHaste()
	: FEdMode()
	, bToolActive(false)
	, bCanAltDrag(false)
	, bMeshRotating(false)
	, RotationOffset(FVector::ZeroVector)
	, UISettings(nullptr)
{
	// Load resources and construct brush component
	UMaterial* BrushMaterial = nullptr;
	if (!IsRunningCommandlet())
	{
		BrushMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EditorLandscapeResources/FoliageBrushSphereMaterial.FoliageBrushSphereMaterial"), nullptr, LOAD_None, nullptr);
		DefaultBrushMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineMeshes/Sphere.Sphere"), nullptr, LOAD_None, nullptr);
		ActiveBrushMesh = nullptr;
	}

	BrushMeshComponent = NewObject<UStaticMeshComponent>();
	BrushMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BrushMeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
	BrushMeshComponent->SetStaticMesh(DefaultBrushMesh);
	BrushMeshComponent->OverrideMaterials.Add(BrushMaterial);
	BrushMeshComponent->SetAbsolute(true, true, true);
	BrushMeshComponent->CastShadow = false;

	bBrushTraceValid = false;
	BrushLocation = FVector::ZeroVector;
	BrushScale = FVector(1);
	BrushRotation = FQuat::Identity;
	LastMousePosition = FIntVector::ZeroValue;

	ResetBrushMesh();
}


/** Destructor */
FEdModeHaste::~FEdModeHaste()
{
	// Save UI settings to config file
	FEditorDelegates::MapChange.RemoveAll(this);
}


/** FGCObject interface */
void FEdModeHaste::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Call parent implementation
	FEdMode::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(BrushMeshComponent);
	Collector.AddReferencedObject(UISettings);
}

/** FEdMode: Called when the mode is entered */
void FEdModeHaste::Enter()
{
	FEdMode::Enter();

	// Clear any selection 
	GEditor->SelectNone(false, true);

	if (!UISettings) {
		UISettings = NewObject<UHasteEdModeSettings>();
	}

	// Bind to editor callbacks
	FEditorDelegates::NewCurrentLevel.AddSP(this, &FEdModeHaste::NotifyNewCurrentLevel);

	// Force real-time viewports.  We'll back up the current viewport state so we can restore it when the
	// user exits this mode.
	const bool bWantRealTime = true;
	const bool bRememberCurrentState = true;
	ForceRealTimeViewports(bWantRealTime, bRememberCurrentState);

	/*
	if (!Toolkit.IsValid())
	{
		TSharedPtr<FHasteEdModeToolkit> HasteToolkit = MakeShareable(new FHasteEdModeToolkit);
		TSharedPtr<SHasteEditor> HasteEditor = SNew(SHasteEditor);
		HasteEditor->SetSettingsObject(UISettings);
		HasteToolkit->SetInlineContent(HasteEditor);
		Toolkit = HasteToolkit;
		Toolkit->Init(Owner->GetToolkitHost());
	}
	*/
	FEditorViewportClient* client = (FEditorViewportClient*)(GEditor->GetActiveViewport()->GetClient());
	hardCam = client->GetViewLocation();
	hardRot = client->GetViewRotation();

	// Listen for asset selection change events in the content browser
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		ContentBrowserSelectionChangeDelegate = ContentBrowserModule.GetOnAssetSelectionChanged().AddRaw(this, &FEdModeHaste::OnContentBrowserSelectionChanged);
	}
}


/** FEdMode: Called when the mode is exited */
void FEdModeHaste::Exit()
{
	if (Toolkit.IsValid()) {
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}

	//
	FEditorDelegates::NewCurrentLevel.RemoveAll(this);

	// Remove the brush
	BrushMeshComponent->UnregisterComponent();

	// Restore real-time viewport state if we changed it
	const bool bWantRealTime = false;
	const bool bRememberCurrentState = false;
	ForceRealTimeViewports(bWantRealTime, bRememberCurrentState);

	// Unregister from the content browser
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		ContentBrowserModule.GetOnAssetSelectionChanged().Remove(ContentBrowserSelectionChangeDelegate);
	}

	// Call base Exit method to ensure proper cleanup
	FEdMode::Exit();
}

void FEdModeHaste::OnContentBrowserSelectionChanged(const TArray<FAssetData>& NewSelectedAssets, bool bIsPrimaryBrowser) {
	if (!bIsPrimaryBrowser) return;
	UE_LOG(LogHasteMode, Log, TEXT("Content Browser Selection Changed"));

	SelectedBrushMeshes.Reset();

	// Select the first static mesh we find from the list of selected assets
	for (const FAssetData& Asset : NewSelectedAssets) {
		UObject* AssetObj = Asset.GetAsset();
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetObj)) {
			SelectedBrushMeshes.Add(StaticMesh);
		}
	}
	RotationOffset = FVector::ZeroVector;
	ResetBrushMesh();
}

void FEdModeHaste::ResetBrushMesh()
{
	
	// Select a random brush mesh from the list
	UStaticMesh* RandomMesh = nullptr;
	if (SelectedBrushMeshes.Num() > 0) {
		int32 index = FMath::RandRange(0, SelectedBrushMeshes.Num() - 1);
		RandomMesh = SelectedBrushMeshes[index];
	}
	BrushMeshComponent->SetStaticMesh(RandomMesh ? RandomMesh : DefaultBrushMesh);
	ActiveBrushMesh = RandomMesh;
}

void FEdModeHaste::PostUndo()
{
	FEdMode::PostUndo();

	//StaticCastSharedPtr<FHasteEdModeToolkit>(Toolkit)->RefreshFullList();
}

/** When the user changes the active streaming level with the level browser */
void FEdModeHaste::NotifyNewCurrentLevel()
{

}

/** When the user changes the current tool in the UI */
void FEdModeHaste::NotifyToolChanged()
{

}

bool FEdModeHaste::DisallowMouseDeltaTracking() const
{
	// We never want to use the mouse delta tracker while painting
	return bToolActive;
}

float FEdModeHaste::biasedRoundDown(float input, float bias) {
	float result=std::fabs(std::floor(input + bias));
	return (input < 0.0) ? -result : result;
}

float FEdModeHaste::biasedRoundUp(float input, float bias) {
	float result = std::fabs(std::ceil(input - bias));
	return (input < 0.0) ? -result : result;
}

/** FEdMode: Called once per frame */
void FEdModeHaste::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	if (bToolActive)
	{
		//ApplyBrush(ViewportClient);
	}

	FEdMode::Tick(ViewportClient, DeltaTime);

	ViewportClient->SetViewLocation(hardCam);

	float axisLock=GEditor->GetRotGridSize().Yaw;
	FRotator delta = (ViewportClient->GetViewRotation() - hardRot);
	hardRot = hardRot + delta;
	FRotator tempRot = hardRot;
	if (delta.Yaw>0) tempRot.Yaw = biasedRoundDown(tempRot.Yaw / axisLock, (1.0f-1.0f/axisLock)*.8f)*axisLock;
	else tempRot.Yaw = biasedRoundUp(tempRot.Yaw / axisLock, (1.0f - 1.0f / axisLock)*.8f)*axisLock;
	if (delta.Pitch > 0) tempRot.Pitch = biasedRoundDown(tempRot.Pitch / axisLock, (1.0f - 1.0f / axisLock)*.8f)*axisLock;
	else tempRot.Pitch = biasedRoundUp(tempRot.Pitch / axisLock, (1.0f - 1.0f / axisLock)*.8f)*axisLock;
	tempRot.Roll = 0.0f;
	ViewportClient->SetViewRotation(tempRot);

	// Trace the brush
	
		HasteBrushTrace(ViewportClient, LastMousePosition.X, LastMousePosition.Y);


		// Update the position and size of the brush component
		if (bBrushTraceValid)
		{
			// Scale adjustment is due to default sphere SM size.
			BrushMeshComponent->SetRelativeTransform(BrushCursorTransform);

			if (!BrushMeshComponent->IsRegistered())
			{
				BrushMeshComponent->RegisterComponentWithWorld(ViewportClient->GetWorld());
			}
		}
		else
		{
			if (BrushMeshComponent->IsRegistered())
			{
				BrushMeshComponent->UnregisterComponent();
			}
		}

		FRotator viewRot = ViewportClient->GetViewRotation();
		FRotator sideViewRot = viewRot + FRotator(0.0f, 90.0f, 0.0f);
		FVector view = viewRot.Vector();
		FVector sideView = sideViewRot.Vector();
		float camSpeed = ViewportClient->GetCameraSpeed();
		if (ViewportClient->Viewport->KeyState(EKeys::W)) hardCam = (hardCam + FVector(view.X*camSpeed, view.Y*camSpeed, 0.0f));
		if (ViewportClient->Viewport->KeyState(EKeys::S)) hardCam = (hardCam - FVector(view.X*camSpeed, view.Y*camSpeed, 0.0f));
		
		if (ViewportClient->Viewport->KeyState(EKeys::A)) hardCam = (hardCam - FVector(sideView.X*camSpeed, sideView.Y*camSpeed, 0.0f));
		if (ViewportClient->Viewport->KeyState(EKeys::D)) hardCam = (hardCam + FVector(sideView.X*camSpeed, sideView.Y*camSpeed, 0.0f));

		if (ViewportClient->Viewport->KeyState(EKeys::Q)) hardCam = (hardCam + FVector(0.0f, 0.0f, -camSpeed));
		if (ViewportClient->Viewport->KeyState(EKeys::E)) hardCam = (hardCam + FVector(0.0f, 0.0f, camSpeed));

}

bool FEdModeHaste::HasteTrace(UWorld* InWorld, FHitResult& OutHit, FVector InStart, FVector InEnd, FName InTraceTag, bool InbReturnFaceIndex = false)
{
	FCollisionQueryParams QueryParams(InTraceTag, true);
	QueryParams.bReturnFaceIndex = InbReturnFaceIndex;

	bool bResult = true;
	while (true)
	{
		bResult = InWorld->LineTraceSingleByChannel(OutHit, InStart, InEnd, ECC_WorldStatic, QueryParams);
		if (bResult)
		{
			AActor* Actor = OutHit.GetActor();
			hitActor = Actor;
			// In the editor traces can hit "No Collision" type actors, so ugh.
			FBodyInstance* BodyInstance = OutHit.Component->GetBodyInstance();
			if (BodyInstance->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics || BodyInstance->GetResponseToChannel(ECC_WorldStatic) != ECR_Block)
			{
				if (Actor)
				{
					QueryParams.AddIgnoredActor(Actor);
				}
				InStart = OutHit.ImpactPoint;
				continue;
			}
		}
		break;
	}
	return bResult;
}

FVector PerformLocationSnap(const FVector& Location) {
	//ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	//int32 SnapWidth = ViewportSettings->GridEnabled;
	int32 SnapWidth = GEditor->GetGridSize();
	if (SnapWidth <= 0) {
		return Location;
	}
	float X = Location.X / SnapWidth;
	float Y = Location.Y / SnapWidth;
	float Z = Location.Z / SnapWidth;

	X = FMath::RoundToInt(X) * SnapWidth;
	Y = FMath::RoundToInt(Y) * SnapWidth;
	Z = FMath::RoundToInt(Z) * SnapWidth;
	return FVector(X, Y, Z);
}

/** Trace under the mouse cursor and update brush position */
void FEdModeHaste::HasteBrushTrace(FEditorViewportClient* ViewportClient, int32 MouseX, int32 MouseY)
{
	bBrushTraceValid = false;
	if (!ViewportClient->IsMovingCamera())
	{
		// Compute a world space ray from the screen space mouse coordinates
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			ViewportClient->Viewport,
			ViewportClient->GetScene(),
			ViewportClient->EngineShowFlags)
			.SetRealtimeUpdate(ViewportClient->IsRealtime()));
		FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
		FViewportCursorLocation MouseViewportRay(View, ViewportClient, MouseX, MouseY);

		FVector Start = MouseViewportRay.GetOrigin();
		BrushTraceDirection = MouseViewportRay.GetDirection();
		FVector End = Start + WORLD_MAX * BrushTraceDirection;

		FHitResult Hit;
		UWorld* World = ViewportClient->GetWorld();
		static FName NAME_HasteBrush = FName(TEXT("HasteBrush"));
		if (HasteTrace(World, Hit, Start, End, NAME_HasteBrush))
		{
			// Adjust the sphere brush
			BrushLocation = PerformLocationSnap(Hit.Location);

			// Find the rotation based on the normal
			LastHitImpact = Hit.ImpactNormal;
			UpdateBrushRotation();

			bBrushTraceValid = true;
		}
	}

	if (bBrushTraceValid) {
		BrushCursorTransform = FTransform(BrushRotation, BrushLocation, BrushScale);
		BrushCursorTransform = ApplyTransformers(BrushCursorTransform);
	}
}

FVector FEdModeHaste::HasteStretch(FEditorViewportClient* ViewportClient, int32 MouseX, int32 MouseY) {
	
		// Compute a world space ray from the screen space mouse coordinates
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			ViewportClient->Viewport,
			ViewportClient->GetScene(),
			ViewportClient->EngineShowFlags)
			.SetRealtimeUpdate(ViewportClient->IsRealtime()));
		FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
		FViewportCursorLocation MouseViewportRay(View, ViewportClient, MouseX, MouseY);

		FVector Start = MouseViewportRay.GetOrigin();
		FVector tempDirection = MouseViewportRay.GetDirection();
		FVector End = Start + WORLD_MAX * tempDirection;

		FHitResult Hit;
		UWorld* World = ViewportClient->GetWorld();
		static FName NAME_HasteBrush = FName(TEXT("HasteBrush"));
		if (HasteTrace(World, Hit, Start, End, NAME_HasteBrush))
		{
			// Adjust the sphere brush
			return PerformLocationSnap(Hit.Location);

		}
	return FVector(0);
}

float SnapRotation(float Value, float SnapWidth) {
	return FMath::RoundToInt(Value / SnapWidth) * SnapWidth;
}

void FEdModeHaste::UpdateBrushRotation()
{
	BrushRotation = FQuat::FindBetween(FVector(0, 0, 1), LastHitImpact);

	// Append the brush rotation
	
		float RotSnapWidth = GEditor->GetRotGridSize().Yaw;
		float SnappedOffsetX = SnapRotation(RotationOffset.X, RotSnapWidth);
		float SnappedOffsetY = SnapRotation(RotationOffset.Y, RotSnapWidth);
		float SnappedOffsetZ = SnapRotation(RotationOffset.Z, RotSnapWidth);
		BrushRotation = BrushRotation * FQuat(FVector(0, 0, 1), SnappedOffsetZ * PI / 180.0f);
	
}


/**
 * Called when the mouse is moved over the viewport
 *
 * @param	InViewportClient	Level editor viewport client that captured the mouse input
 * @param	InViewport			Viewport that captured the mouse input
 * @param	InMouseX			New mouse cursor X coordinate
 * @param	InMouseY			New mouse cursor Y coordinate
 *
 * @return	true if input was handled
 */
bool FEdModeHaste::MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 MouseX, int32 MouseY)
{
	FIntVector CurrentMousePosition(MouseX, MouseY, 0);
	if (LastMousePosition != CurrentMousePosition) {
		//UE_LOG(LogHasteMode, Log, TEXT("MouseMove (%d, %d)"), MouseX, MouseY);
		LastMousePosition = CurrentMousePosition;
		//HasteBrushTrace(ViewportClient, MouseX, MouseY);
		return bBrushTraceValid;
	}

	return false;
}

/**
 * Called when the mouse is moved while a window input capture is in effect
 *
 * @param	InViewportClient	Level editor viewport client that captured the mouse input
 * @param	InViewport			Viewport that captured the mouse input
 * @param	InMouseX			New mouse cursor X coordinate
 * @param	InMouseY			New mouse cursor Y coordinate
 *
 * @return	true if input was handled
 */
bool FEdModeHaste::CapturedMouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 MouseX, int32 MouseY)
{
	CurrentMousePosition1=FIntVector(MouseX, MouseY, 0);
	if (LastMousePosition != CurrentMousePosition1 && !bMeshRotating) {
		//UE_LOG(LogHasteMode, Log, TEXT("CapturedMouseMove (%d, %d)"), MouseX, MouseY);
		LastMousePosition = CurrentMousePosition1;
		//HasteBrushTrace(ViewportClient, MouseX, MouseY);
		return bBrushTraceValid;
	}

	return false;
}

void FEdModeHaste::GetRandomVectorInBrush(FVector& OutStart, FVector& OutEnd)
{
	// Find Rx and Ry inside the unit circle
	float Ru = (2.f * FMath::FRand() - 1.f);
	float Rv = (2.f * FMath::FRand() - 1.f) * FMath::Sqrt(1.f - FMath::Square(Ru));

	// find random point in circle thru brush location parallel to screen surface
	FVector U, V;
	BrushTraceDirection.FindBestAxisVectors(U, V);
	FVector Point = Ru * U + Rv * V;

	// find distance to surface of sphere brush from this point
	float Rw = FMath::Sqrt(1.f - (FMath::Square(Ru) + FMath::Square(Rv)));

	OutStart = BrushLocation + BRUSH_RADIUS * (Ru * U + Rv * V - Rw * BrushTraceDirection);
	OutEnd = BrushLocation + BRUSH_RADIUS * (Ru * U + Rv * V + Rw * BrushTraceDirection);
}

void FEdModeHaste::ApplyBrush(FEditorViewportClient* ViewportClient)
{
	if (!bBrushTraceValid)
	{
		return;
	}
}


/** FEdMode: Called when a key is pressed */
bool FEdModeHaste::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	// Rotate if mouse wheel is scrolled
	if (Key == EKeys::X) BrushScale = FVector(1);

	if (Key == EKeys::RightMouseButton && Event == EInputEvent::IE_DoubleClick) hitActor->Destroy();

	if (Key == EKeys::LeftMouseButton && Event == EInputEvent::IE_Released)
	{
		bMeshRotating = false;
		scrollTicks = 0;
		if (ActiveBrushMesh) {
			AStaticMeshActor* MeshActor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass());

			// Rename the display name of the new actor in the editor to reflect the mesh that is being created from.
			FActorLabelUtilities::SetActorLabelUnique(MeshActor, ActiveBrushMesh->GetName());

			MeshActor->GetStaticMeshComponent()->SetStaticMesh(ActiveBrushMesh);
			MeshActor->ReregisterAllComponents();
			MeshActor->SetActorTransform(BrushCursorTransform);

			// Switch to another mesh from the list
			BrushRotation = FQuat::Identity;
			RotationOffset = FVector::ZeroVector;
			UpdateBrushRotation();
			ResetBrushMesh();

			BrushScale = FVector(1);
		}
	}

	if (Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown) {
		int32 WheelDelta = (Key == EKeys::MouseScrollUp) ? 1 : -1;
		const float ScrollSpeed = 1;
		float AngleDelta = WheelDelta * ROTATION_SPEED * ScrollSpeed;

		bool bShiftDown = IsShiftDown(Viewport);
		bool bCtrlDown = IsCtrlDown(Viewport);
		if (bCtrlDown) {
			RotationOffset.X += AngleDelta;
		} 
		else if (bShiftDown) {
			RotationOffset.Z += AngleDelta;
		} 
		else {
			if (WheelDelta>0) scrollTicks++;
			else scrollTicks--;
			float gridSize=GEditor->GetGridSize();
			if (ActiveBrushMesh) {
				FBox BoxExtent = ActiveBrushMesh->GetExtendedBounds().GetBox();
				BrushScale.Z = float(scrollTicks)*gridSize*.5 / FMath::Abs(BoxExtent.Max.Y - BoxExtent.Min.Y);
			}
		}
		return true;
	}
	
	return false;
}

/** FEdMode: Render the haste edit mode */
void FEdModeHaste::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	/** Call parent implementation */
	FEdMode::Render(View, Viewport, PDI);
}


/** FEdMode: Render HUD elements for this tool */
void FEdModeHaste::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
}

/** FEdMode: Check to see if an actor can be selected in this mode - no side effects */
bool FEdModeHaste::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	return false;
}

/** FEdMode: Handling SelectActor */
bool FEdModeHaste::Select(AActor* InActor, bool bInSelected)
{
	// return true if you filter that selection
	// however - return false if we are trying to deselect so that it will infact do the deselection
	if (bInSelected == false)
	{
		return false;
	}
	return true;
}

/** FEdMode: Called when the currently selected actor has changed */
void FEdModeHaste::ActorSelectionChangeNotify()
{
}


/** Forces real-time perspective viewports */
void FEdModeHaste::ForceRealTimeViewports(const bool bEnable, const bool bStoreCurrentState)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr< IAssetViewport > ViewportWindow = LevelEditorModule.GetFirstActiveViewport();
	if (ViewportWindow.IsValid())
	{
		//Cast<SLevelViewport>(ViewportWindow.Get());
		FEditorViewportClient &Viewport = StaticCastSharedPtr<SLevelViewport>(ViewportWindow)->GetLevelViewportClient();
		if (Viewport.IsPerspective())
		{
			if (bEnable)
			{
				Viewport.SetRealtime(bEnable, bStoreCurrentState);
			}
			else
			{
				const bool bAllowDisable = true;
				Viewport.RestoreRealtime(bAllowDisable);
			}

		}
	}
}

bool FEdModeHaste::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{

	//return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
	return true;
}

FTransform FEdModeHaste::ApplyTransformers(const FTransform& BaseTransform)
{
	FTransform Transform = BaseTransform;

	return Transform;
}

FVector FEdModeHaste::GetWidgetLocation() const
{
	return FEdMode::GetWidgetLocation();
}

/** FEdMode: Called when a mouse button is pressed */
bool FEdModeHaste::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	bMeshRotating = true;

	return FEdMode::StartTracking(InViewportClient, InViewport);
}

/** FEdMode: Called when the a mouse button is released */
bool FEdModeHaste::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{

	return FEdMode::EndTracking(InViewportClient, InViewport);
}

/** FEdMode: Called when mouse drag input it applied */
bool FEdModeHaste::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (InViewportClient->Viewport->KeyState(EKeys::LeftMouseButton)) {
		if (bMeshRotating) {
			bool bShiftDown = IsShiftDown(InViewport);
			bool bCtrlDown = IsCtrlDown(InViewport);

			if (bShiftDown) {
				//RotationOffset.Z += AngleDelta;
				//scale in Z-dir here
			}
			//its woooooorkiinngg
			FVector stretchVector = HasteStretch(InViewportClient, CurrentMousePosition1.X, CurrentMousePosition1.Y);
			if (ActiveBrushMesh) {
				FBox BoxExtent = ActiveBrushMesh->GetExtendedBounds().GetBox();
				BrushScale.Y = -(BrushLocation.Y - stretchVector.Y) / FMath::Abs(BoxExtent.Max.Y - BoxExtent.Min.Y);
				BrushScale.X = -(BrushLocation.X - stretchVector.X) / FMath::Abs(BoxExtent.Max.X - BoxExtent.Min.X);
			}

			return true;
		}
	}
	else if (InViewportClient->Viewport->KeyState(EKeys::RightMouseButton)) {
		float axisLock = GEditor->GetRotGridSize().Yaw;
		hardRot.Yaw += InDrag.X*axisLock*.1;
		hardRot.Pitch += InDrag.Y*axisLock*3.0f;
		//hardRot.Pitch += InDrag.Z*axisLock*2.0f;
	}
	return FEdMode::InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
}

bool FEdModeHaste::AllowWidgetMove()
{
	return ShouldDrawWidget();
}

bool FEdModeHaste::UsesTransformWidget() const
{
	return ShouldDrawWidget();
}

bool FEdModeHaste::ShouldDrawWidget() const
{
	return true;
}

EAxisList::Type FEdModeHaste::GetWidgetAxisToDraw(UE::Widget::EWidgetMode InWidgetMode) const
{
	switch (InWidgetMode)
	{
	case UE::Widget::WM_Translate:
	case UE::Widget::WM_Rotate:
	case UE::Widget::WM_Scale:
		return EAxisList::XYZ;
	default:
		return EAxisList::None;
	}
}

#undef LOCTEXT_NAMESPACE