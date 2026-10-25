#include "DHAbilityHud.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHAbilities.h"
#include "DHControllerScreen.h"
#include "DHKillstreaks.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "DHLoadoutSettings.h"
#include "DHPlayerChoicesComponent.h"
#include "DHProgression.h"
#include "DHScreenStyle.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHAbilityHud)

namespace
{
	constexpr float TileSize = 92.f;
	constexpr float FrameWidth = 3.f;
	const TCHAR* Actions[2] = {
		TEXT("/Game/Input/Actions/IA_Ability_Dash.IA_Ability_Dash"),
		TEXT("/DowntownCore/Input/IA_DH_Ability2.IA_DH_Ability2"),
	};

	UTexture2D* AbilityIcon(const FString& Id)
	{
		return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/DH/UI/Abilities/T_DH_Ability_%s.T_DH_Ability_%s"), *Id, *Id));
	}

	/** White texture with an alpha mask from a function of the pixel's position (-1..1, centre 0). */
	template <typename TAlpha>
	UTexture2D* MakeMask(int32 Size, TAlpha Alpha)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
		FColor* Pixels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const float A = FMath::Clamp(Alpha(2.f * (X + 0.5f) / Size - 1.f, 2.f * (Y + 0.5f) / Size - 1.f), 0.f, 1.f);
				Pixels[Y * Size + X] = FColor(255, 255, 255, uint8(255.f * A));
			}
		}
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}

	UTextBlock* OutlinedText(UWidgetTree* Tree, const TCHAR* Font, int32 Size, const FLinearColor& Colour)
	{
		UTextBlock* Text = DHScreenStyle::MakeText(Tree, Font, Size, Colour);
		FSlateFontInfo Info = Text->GetFont();
		Info.OutlineSettings.OutlineSize = 2;
		Info.OutlineSettings.OutlineColor = FLinearColor::Black;
		Text->SetFont(Info);
		return Text;
	}
}

void UDHAbilityHud::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Covers the whole screen (effects): it must never take clicks or focus from the menus above the HUD.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	// Screen effects under the tiles.
	auto FullScreen = [this, Root](UTexture2D* Texture)
	{
		KeepAlive.Add(Texture);
		UImage* Image = WidgetTree->ConstructWidget<UImage>();
		Image->SetBrushFromTexture(Texture);
		Image->SetRenderOpacity(0.f);
		Image->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		UOverlaySlot* ImageSlot = Root->AddChildToOverlay(Image);
		ImageSlot->SetHorizontalAlignment(HAlign_Fill);
		ImageSlot->SetVerticalAlignment(VAlign_Fill);
		return Image;
	};
	Vignette = FullScreen(MakeMask(256, [](float X, float Y) { return FMath::SmoothStep(0.55f, 1.25f, FVector2f(X, Y).Size()); }));
	SpeedLines = FullScreen(MakeMask(512, [](float X, float Y)
	{
		// thin streaks pointing at the centre, only near the edges, at uneven angles
		const float Angle = FMath::Atan2(Y, X) / (2.f * PI) + 0.5f;
		const int32 Ray = FMath::FloorToInt(Angle * 90.f);
		const float Within = FMath::Frac(Angle * 90.f);
		const float Width = 0.12f + 0.25f * FMath::Frac(FMath::Sin(Ray * 12.9898f) * 43758.5453f);
		const bool bLine = FMath::Frac(FMath::Sin(Ray * 78.233f) * 12345.678f) > 0.45f && Within < Width;
		return bLine ? FMath::SmoothStep(0.62f, 1.1f, FVector2f(X, Y).Size()) : 0.f;
	}));
	Sonar = FullScreen(MakeMask(256, [](float X, float Y) { return FMath::Max(0.f, 1.f - FMath::Abs(FVector2f(X, Y).Size() - 0.9f) * 18.f); }));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlaySlot* RowSlot = Root->AddChildToOverlay(Row);
	RowSlot->SetHorizontalAlignment(HAlign_Center);
	RowSlot->SetVerticalAlignment(VAlign_Bottom);
	RowSlot->SetPadding(FMargin(0, 0, 0, 84));  // above Lyra's health bar

	for (FSlot& Item : Slots)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Row->AddChildToHorizontalBox(Column)->SetPadding(FMargin(18, 0));
		Item.Box = Column;

		// Tile: frame colour around a dark card, icon, recharge shade from the bottom, seconds; leaning like the menus.
		USizeBox* TileBox = WidgetTree->ConstructWidget<USizeBox>();
		TileBox->SetWidthOverride(TileSize);
		TileBox->SetHeightOverride(TileSize);
		UOverlay* Tile = WidgetTree->ConstructWidget<UOverlay>();
		TileBox->AddChild(Tile);
		TileBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Column->AddChildToVerticalBox(TileBox)->SetHorizontalAlignment(HAlign_Center);
		Item.Tile = TileBox;

		Item.Frame = WidgetTree->ConstructWidget<UBorder>();
		Item.Frame->SetPadding(FMargin(FrameWidth));
		UOverlaySlot* FrameSlot = Tile->AddChildToOverlay(Item.Frame);
		FrameSlot->SetHorizontalAlignment(HAlign_Fill);
		FrameSlot->SetVerticalAlignment(VAlign_Fill);
		UOverlay* Inner = WidgetTree->ConstructWidget<UOverlay>();
		Item.Frame->SetContent(Inner);
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
		Card->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		UOverlaySlot* CardSlot = Inner->AddChildToOverlay(Card);
		CardSlot->SetHorizontalAlignment(HAlign_Fill);
		CardSlot->SetVerticalAlignment(VAlign_Fill);
		Item.Icon = WidgetTree->ConstructWidget<UImage>();
		UOverlaySlot* IconSlot = Inner->AddChildToOverlay(Item.Icon);
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
		Item.Shade = WidgetTree->ConstructWidget<USizeBox>();
		UBorder* ShadeFill = WidgetTree->ConstructWidget<UBorder>();
		ShadeFill->SetBrushColor(FLinearColor(0.02f, 0.f, 0.05f, 0.78f));
		Item.Shade->AddChild(ShadeFill);
		UOverlaySlot* ShadeSlot = Inner->AddChildToOverlay(Item.Shade);
		ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
		ShadeSlot->SetVerticalAlignment(VAlign_Bottom);
		Item.Seconds = OutlinedText(WidgetTree, TEXT("Orbitron"), 30, FLinearColor::White);
		UOverlaySlot* SecondsSlot = Inner->AddChildToOverlay(Item.Seconds);
		SecondsSlot->SetHorizontalAlignment(HAlign_Center);
		SecondsSlot->SetVerticalAlignment(VAlign_Center);

		// Button badge above the tile (keyboard names can be long).
		UBorder* Badge = WidgetTree->ConstructWidget<UBorder>();
		Badge->SetBrushColor(DHScreenStyle::Accent);
		Badge->SetPadding(FMargin(7, 1));
		Item.Key = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 14, FLinearColor(0.02f, 0.01f, 0.03f));
		Badge->SetContent(Item.Key);
		Badge->SetRenderTransformAngle(-4.f);
		Column->InsertChildAt(0, Badge);
		UVerticalBoxSlot* BadgeSlot = Cast<UVerticalBoxSlot>(Badge->Slot);
		BadgeSlot->SetHorizontalAlignment(HAlign_Center);
		BadgeSlot->SetPadding(FMargin(0, 0, 0, 5));

		Item.Name = OutlinedText(WidgetTree, TEXT("Orbitron"), 13, DHScreenStyle::Text);
		Column->AddChildToVerticalBox(Item.Name)->SetHorizontalAlignment(HAlign_Center);
		Cast<UVerticalBoxSlot>(Item.Name->Slot)->SetPadding(FMargin(0, 6, 0, 0));
		Item.State = OutlinedText(WidgetTree, TEXT("NotoSans"), 12, DHScreenStyle::Text);
		Column->AddChildToVerticalBox(Item.State)->SetHorizontalAlignment(HAlign_Center);
	}

	if (FDHProgression::IsDevMode())
	{
		UTextBlock* Hint = OutlinedText(WidgetTree, TEXT("NotoSans"), 13, DHScreenStyle::Accent);
		Hint->SetText(NSLOCTEXT("DowntownHunting", "DevHudHint", "DEV MODE  ·  F10 or hold VIEW + Y: dev tools  ·  ^ (or `): console"));
		UOverlaySlot* HintSlot = Root->AddChildToOverlay(Hint);
		HintSlot->SetHorizontalAlignment(HAlign_Left);
		HintSlot->SetVerticalAlignment(VAlign_Top);
		HintSlot->SetPadding(FMargin(24, 16, 0, 0));
	}
}

void UDHAbilityHud::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APlayerController* PC = GetOwningPlayer();
	UpdateScreenEffects(InDeltaTime);
	const UDHPlayerChoicesComponent* Choices = PC ? PC->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
	// The button names follow the device in use; looked up twice a second.
	KeyRefresh -= InDeltaTime;
	const bool bRefreshKeys = KeyRefresh <= 0.f;
	if (bRefreshKeys)
	{
		KeyRefresh = 0.5f;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FSlot& Item = Slots[Index];
		const FString Id = Choices ? Choices->GetAbility(Index) : FString();
		const FDHAbilityEntry* Ability = UDHLoadoutSettings::FindAbility(Id);
		Item.Box->SetVisibility(Ability ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!Ability)
		{
			continue;
		}
		if (Item.ShownAbility != Id)
		{
			Item.ShownAbility = Id;
			Item.Name->SetText(Ability->Name);
			if (UTexture2D* Texture = AbilityIcon(Id))
			{
				Item.Icon->SetBrushFromTexture(Texture);
			}
		}
		if (bRefreshKeys)
		{
			const FText Key = UDHControllerScreen::ActionKeyLabel(PC ? PC->GetLocalPlayer() : nullptr, Actions[Index], /*bShort=*/ true);
			Item.Key->SetText(Key);
			Item.Key->GetParent()->SetVisibility(Key.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}

		const float Remaining = Choices->GetRemaining(Index);
		const float Cooldown = FMath::Max(0.1f, Choices->GetCooldown(Index));
		const float Active = Remaining - (Cooldown - Choices->GetDuration(Index));  // effect time left
		const bool bReady = Remaining <= 0.f;
		if (bReady && !Item.bWasReady)
		{
			Item.PopVelocity += 9.f;
			if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Sounds/UI/sfx_UI_MainMenu_Select_nl_meta_Preset.sfx_UI_MainMenu_Select_nl_meta_Preset")))
			{
				UGameplayStatics::PlaySound2D(this, Sound, 0.5f, 1.4f);
			}
		}
		Item.bWasReady = bReady;

		// Recharge: the shade drains from the top; the effect running: pulsing cyan frame.
		Item.Shade->SetHeightOverride((TileSize - 2.f * FrameWidth) * FMath::Clamp(Remaining / Cooldown, 0.f, 1.f));
		Item.Icon->SetColorAndOpacity(bReady ? FLinearColor::White : FLinearColor(0.55f, 0.55f, 0.6f));
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * 10.0);
		Item.Frame->SetBrushColor(Active > 0.f ? FMath::Lerp(DHScreenStyle::Cyan, FLinearColor::White, Pulse)
			: bReady ? DHScreenStyle::Accent : FLinearColor(0.25f, 0.2f, 0.32f, 0.9f));
		Item.Seconds->SetText(bReady ? FText::GetEmpty() : FText::AsNumber(FMath::CeilToInt(Active > 0.f ? Active : Remaining)));
		Item.Seconds->SetColorAndOpacity(Active > 0.f ? DHScreenStyle::Cyan : FLinearColor::White);
		Item.Name->SetColorAndOpacity(bReady ? DHScreenStyle::Accent : DHScreenStyle::Text);
		Item.State->SetText(Active > 0.f ? NSLOCTEXT("DowntownHunting", "AbilityActiveHud", "ACTIVE")
			: bReady ? NSLOCTEXT("DowntownHunting", "AbilityReady", "READY") : NSLOCTEXT("DowntownHunting", "AbilityCharging", "CHARGING"));

		// Underdamped spring for the ready pop, like the menu cards.
		const float Step = FMath::Min(InDeltaTime, 1.f / 30.f);
		Item.PopVelocity += -Item.Pop * 300.f * Step - Item.PopVelocity * 12.f * Step;
		Item.Pop += Item.PopVelocity * Step;
		FWidgetTransform Transform;
		Transform.Shear = FVector2D(-8.f, 0.f);
		Transform.Scale = FVector2D(1.f + 0.12f * Item.Pop);
		Transform.Angle = -3.f * Item.Pop;
		Item.Tile->SetRenderTransform(Transform);
	}
}

void UDHAbilityHud::UpdateScreenEffects(float DeltaTime)
{
	EffectTime += DeltaTime;
	const APawn* Pawn = GetOwningPlayerPawn();
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	const APlayerState* Player = Pawn ? Pawn->GetPlayerState() : nullptr;
	const bool bCloak = DHAbilities::IsCloaked(Player);
	const bool bAdrenaline = DHAbilities::HasAdrenaline(Player);
	const bool bRecon = DHKillstreaks::IsActive(Player, DHKillstreaks::RadarTag());

	// Cloak: shimmering cyan frame. Adrenaline: red frame beating like a heart (about 140 bpm).
	const float Beat = FMath::Pow(FMath::Max(0.f, FMath::Sin(EffectTime * 14.6f)), 6.f);
	const float Shimmer = 0.5f + 0.5f * FMath::Sin(EffectTime * 6.f);
	if (bCloak)
	{
		Vignette->SetColorAndOpacity(FLinearColor(0.1f, 0.85f, 1.f));
		Vignette->SetRenderOpacity(0.45f + 0.2f * Shimmer);
	}
	else if (bAdrenaline)
	{
		Vignette->SetColorAndOpacity(FLinearColor(1.f, 0.08f, 0.05f));
		Vignette->SetRenderOpacity(0.22f + 0.4f * Beat);
	}
	else
	{
		Vignette->SetRenderOpacity(FMath::FInterpTo(Vignette->GetRenderOpacity(), 0.f, DeltaTime, 6.f));
	}

	// Speed lines while sprinting (more with adrenaline), turning a little so they flicker.
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const float Walk = Movement ? CastChecked<UCharacterMovementComponent>(Movement->GetArchetype())->MaxWalkSpeed : 600.f;
	const float Fast = Movement && Movement->IsMovingOnGround() ? FMath::GetMappedRangeValueClamped(FVector2f(Walk * 1.1f, Walk * 1.7f), FVector2f(0.f, 1.f), Character->GetVelocity().Size2D()) : 0.f;
	Speed = FMath::FInterpTo(Speed, Fast, DeltaTime, 6.f);
	SpeedLines->SetColorAndOpacity(bAdrenaline ? FLinearColor(1.f, 0.6f, 0.5f) : FLinearColor(1.f, 1.f, 1.f));
	SpeedLines->SetRenderOpacity(0.5f * Speed);
	SpeedLines->SetRenderTransformAngle(FMath::FRandRange(-1.5f, 1.5f));
	SpeedLines->SetRenderScale(FVector2D(1.f + 0.04f * FMath::Sin(EffectTime * 30.f)));

	// Recon: a ring rushes out from the middle, again every second while it runs.
	if (bRecon && (!bWasRecon || SonarTime > 1.f))
	{
		SonarTime = 0.f;
	}
	bWasRecon = bRecon;
	SonarTime += DeltaTime;
	const float Ring = FMath::Clamp(SonarTime / 0.9f, 0.f, 1.f);
	Sonar->SetColorAndOpacity(FLinearColor(0.2f, 1.f, 0.6f));
	Sonar->SetRenderOpacity(SonarTime < 0.9f ? 0.8f * (1.f - Ring) : 0.f);
	Sonar->SetRenderScale(FVector2D(0.1f + 1.4f * FMath::InterpEaseOut(0.f, 1.f, Ring, 2.f)));
}
