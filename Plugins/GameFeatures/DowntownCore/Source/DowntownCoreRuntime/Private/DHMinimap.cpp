#include "DHMinimap.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "DHBombSite.h"
#include "DHFreeForAllScoring.h"
#include "DHScreenStyle.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Teams/LyraTeamDisplayAsset.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHMinimap)

namespace DHMinimapSettings
{
	constexpr float WidgetSize = 220.f;
	constexpr float ViewSize = 7000.f;  // world units shown across the minimap
	constexpr int32 Resolution = 1024;

	// One capture per map, shared by the minimaps of later pawns.
	TWeakObjectPtr<UWorld> CapturedWorld;
	TWeakObjectPtr<UTexture2D> CapturedTarget;
	FVector2D CapturedCentre;
	float CapturedExtent = 0.f;

	// Soft round dot (white, alpha), tinted per marker; the arrow is a triangle.
	UTexture2D* MakeShape(bool bArrow)
	{
		constexpr int32 Size = 32;
		UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
		FColor* Pixels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const float U = (X + 0.5f) / Size - 0.5f, V = (Y + 0.5f) / Size;
				float Alpha;
				if (bArrow)
				{
					// tip at the top, base at the bottom
					Alpha = (V > 0.08f && V < 0.92f && FMath::Abs(U) < 0.42f * (V - 0.08f) / 0.84f) ? 1.f : 0.f;
				}
				else
				{
					Alpha = FMath::Clamp((0.5f - FMath::Sqrt(U * U + (V - 0.5f) * (V - 0.5f))) * 12.f, 0.f, 1.f);
				}
				Pixels[Y * Size + X] = FColor(255, 255, 255, uint8(Alpha * 255.f));
			}
		}
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
}

void UDHMinimap::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	using namespace DHMinimapSettings;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
	Frame->SetBrushColor(DHScreenStyle::Accent);
	Frame->SetPadding(FMargin(2));
	UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame);
	FrameSlot->SetAnchors(FAnchors(1.f, 0.f));
	FrameSlot->SetAlignment(FVector2D(1.f, 0.f));
	FrameSlot->SetPosition(FVector2D(-28.f, 28.f));
	FrameSlot->SetSize(FVector2D(WidgetSize + 4.f, WidgetSize + 4.f));

	UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>();
	Stack->SetClipping(EWidgetClipping::ClipToBounds);
	Frame->SetContent(Stack);
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f));
	UOverlaySlot* BackgroundSlot = Stack->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	MapImage = WidgetTree->ConstructWidget<UImage>();
	MapImage->SetRenderOpacity(0.f);
	UOverlaySlot* MapSlot = Stack->AddChildToOverlay(MapImage);
	MapSlot->SetHorizontalAlignment(HAlign_Fill);
	MapSlot->SetVerticalAlignment(VAlign_Fill);
	Markers = WidgetTree->ConstructWidget<UCanvasPanel>();
	UOverlaySlot* MarkerSlot = Stack->AddChildToOverlay(Markers);
	MarkerSlot->SetHorizontalAlignment(HAlign_Fill);
	MarkerSlot->SetVerticalAlignment(VAlign_Fill);

	DotTexture = MakeShape(false);
	Arrow = MakeMarker(MakeShape(true), 18.f);
	Arrow->SetColorAndOpacity(DHScreenStyle::Accent);
	Arrow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

	// Reuse the capture of an earlier pawn on this map.
	if (CapturedWorld.Get() == GetWorld() && CapturedTarget.IsValid())
	{
		MapTexture = CapturedTarget.Get();
		MapImage->SetBrushResourceObject(MapTexture);
		Centre = CapturedCentre;
		Extent = CapturedExtent;
		Captures = 2;
	}
}

UImage* UDHMinimap::MakeMarker(UTexture2D* Texture, float Size)
{
	UImage* Marker = WidgetTree->ConstructWidget<UImage>();
	Marker->SetBrushFromTexture(Texture);
	UCanvasPanelSlot* MarkerCanvasSlot = Markers->AddChildToCanvas(Marker);
	MarkerCanvasSlot->SetSize(FVector2D(Size, Size));
	MarkerCanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	return Marker;
}

void UDHMinimap::Capture()
{
	using namespace DHMinimapSettings;
	UWorld* World = GetWorld();

	// Area: all player starts (and bomb sites), with a margin.
	FBox Bounds(ForceInit);
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Bounds += It->GetActorLocation();
	}
	for (TActorIterator<ADHBombSite> It(World); It; ++It)
	{
		Bounds += It->GetActorLocation();
	}
	if (!Bounds.IsValid)
	{
		return;
	}
	Centre = FVector2D(Bounds.GetCenter());
	Extent = FMath::Max(8000.f, FMath::Max(Bounds.GetSize().X, Bounds.GetSize().Y) * 1.3f);

	if (!Target)
	{
		Target = NewObject<UTextureRenderTarget2D>(this);
		Target->RenderTargetFormat = RTF_RGBA8;
		Target->InitAutoFormat(Resolution, Resolution);
		Target->UpdateResourceImmediate(true);
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	ASceneCapture2D* Camera = World->SpawnActor<ASceneCapture2D>(FVector(Centre, Bounds.Max.Z + 25000.f), FRotator(-90.f, 0.f, 0.f), Params);
	USceneCaptureComponent2D* Component = Camera->GetCaptureComponent2D();
	Component->ProjectionType = ECameraProjectionMode::Orthographic;
	Component->OrthoWidth = Extent;
	Component->TextureTarget = Target;
	Component->CaptureSource = SCS_BaseColor;  // unlit colours: map-like, no exposure trouble from high up
	Component->bCaptureEveryFrame = false;
	Component->bCaptureOnMovement = false;
	Component->ShowFlags.SetFog(false);
	Component->ShowFlags.SetVolumetricFog(false);
	Component->ShowFlags.SetAtmosphere(false);
	Component->ShowFlags.SetCloud(false);
	Component->ShowFlags.SetSkyLighting(false);
	Component->CaptureScene();
	Camera->Destroy();

	// The capture leaves alpha at 0 (scene captures write inverted opacity): copy it into a plain opaque texture.
	TArray<FColor> Pixels;
	if (FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource())
	{
		Resource->ReadPixels(Pixels);
	}
	if (Pixels.Num() != Resolution * Resolution)
	{
		return;
	}
	MapTexture = UTexture2D::CreateTransient(Resolution, Resolution, PF_B8G8R8A8);
	FColor* Destination = static_cast<FColor*>(MapTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	for (int32 Index = 0; Index < Pixels.Num(); ++Index)
	{
		// base colours are dark: brighten for the small map
		const FColor& Pixel = Pixels[Index];
		Destination[Index] = FColor(uint8(FMath::Min(255, Pixel.R * 2 + 12)), uint8(FMath::Min(255, Pixel.G * 2 + 12)), uint8(FMath::Min(255, Pixel.B * 2 + 12)), 255);
	}
	MapTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	MapTexture->UpdateResource();
	UE_LOG(LogTemp, Log, TEXT("DH minimap: captured %.0f m around %s"), Extent / 100.f, *Centre.ToString());

	MapImage->SetBrushResourceObject(MapTexture);
	CapturedWorld = World;
	CapturedTarget = MapTexture;
	CapturedCentre = Centre;
	CapturedExtent = Extent;
}

FVector2D UDHMinimap::ToWidget(const FVector& World) const
{
	// Capture looks straight down with yaw 0: image up is +X, image right is +Y.
	const FVector2D UV(0.5f + (World.Y - Centre.Y) / Extent, 0.5f - (World.X - Centre.X) / Extent);
	return (UV - ViewOrigin) * (Extent / DHMinimapSettings::ViewSize) * DHMinimapSettings::WidgetSize;
}

void UDHMinimap::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	using namespace DHMinimapSettings;

	// Capture after the first seconds (streamed textures), and once more later for sharper textures.
	if (Captures < 2 && (CaptureDelay -= InDeltaTime) <= 0.f)
	{
		Capture();
		++Captures;
		CaptureDelay = 10.f;
	}
	const APlayerController* Player = GetOwningPlayer();
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Pawn || Extent <= 0.f)
	{
		return;
	}
	MapImage->SetRenderOpacity(1.f);

	// Region around the player, kept inside the capture.
	const float View = ViewSize / Extent;
	const FVector Location = Pawn->GetActorLocation();
	const FVector2D PlayerUV(0.5f + (Location.Y - Centre.Y) / Extent, 0.5f - (Location.X - Centre.X) / Extent);
	ViewOrigin = FVector2D(FMath::Clamp(PlayerUV.X - View * 0.5f, 0.f, 1.f - View), FMath::Clamp(PlayerUV.Y - View * 0.5f, 0.f, 1.f - View));
	FSlateBrush Brush = MapImage->GetBrush();
	Brush.SetUVRegion(FBox2D(ViewOrigin, ViewOrigin + FVector2D(View, View)));
	MapImage->SetBrush(Brush);

	auto Place = [](UWidget* Marker, const FVector2D& Position)
	{
		const bool bInside = Position.X >= 0.f && Position.Y >= 0.f && Position.X <= WidgetSize && Position.Y <= WidgetSize;
		Marker->SetVisibility(bInside ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* MarkerCanvasSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
		{
			MarkerCanvasSlot->SetPosition(Position);
		}
	};
	Place(Arrow, ToWidget(Location));
	Arrow->SetRenderTransformAngle(Player->GetControlRotation().Yaw);

	// Teammates.
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	int32 Used = 0;
	if (Teams && GameState)
	{
		const int32 OwnTeam = Teams->FindTeamFromObject(Player->PlayerState);
		for (const APlayerState* Other : GameState->PlayerArray)
		{
			const APawn* OtherPawn = Other->GetPawn();
			if (!OtherPawn || Other == Player->PlayerState || OwnTeam == INDEX_NONE || Teams->FindTeamFromObject(Other) != OwnTeam)
			{
				continue;
			}
			if (Dots.Num() <= Used)
			{
				Dots.Add(MakeMarker(DotTexture, 10.f));
			}
			UImage* Dot = Dots[Used++];
			const ULyraTeamDisplayAsset* Display = const_cast<ULyraTeamSubsystem*>(Teams)->GetTeamDisplayAsset(OwnTeam, INDEX_NONE);
			const FLinearColor* Colour = Display ? Display->ColorParameters.Find(TEXT("TeamColor")) : nullptr;
			Dot->SetColorAndOpacity(Colour ? *Colour : FLinearColor(0.2f, 0.6f, 1.f));
			Place(Dot, ToWidget(OtherPawn->GetActorLocation()));
		}
	}
	for (int32 Index = Used; Index < Dots.Num(); ++Index)
	{
		Dots[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Bomb sites.
	if (const UDHSearchAndDestroyScoring* Bomb = Cast<UDHSearchAndDestroyScoring>(UDHFreeForAllScoring::Find(this)))
	{
		const TArray<TObjectPtr<ADHBombSite>>& Sites = Bomb->GetSites();
		while (SiteLabels.Num() < Sites.Num())
		{
			UTextBlock* Label = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 16, DHScreenStyle::Accent);
			Label->SetShadowOffset(FVector2D(1, 1));
			UCanvasPanelSlot* LabelSlot = Markers->AddChildToCanvas(Label);
			LabelSlot->SetAutoSize(true);
			LabelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			SiteLabels.Add(Label);
		}
		for (int32 Index = 0; Index < Sites.Num(); ++Index)
		{
			if (Sites[Index])
			{
				SiteLabels[Index]->SetText(FText::FromString(Sites[Index]->Letter));
				SiteLabels[Index]->SetColorAndOpacity(Sites[Index] == Bomb->GetPlantedSite() ? FLinearColor(1.f, 0.1f, 0.05f) : DHScreenStyle::Accent);
				Place(SiteLabels[Index], ToWidget(Sites[Index]->GetActorLocation()));
			}
		}
	}
}
