#include "DHControllerScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHCardButton.h"
#include "DHScreenStyle.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/ConfigCacheIni.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHControllerScreen)

namespace
{
	struct FRow
	{
		const TCHAR* Id;
		FText Name;
		TArray<const TCHAR*> Actions;  // asset paths; the first one shows the current button
	};

	const TArray<FRow>& Rows()
	{
		static const TArray<FRow> List = {
			{ TEXT("Fire"), NSLOCTEXT("DowntownHunting", "PadFire", "FIRE"), { TEXT("/Game/Input/Actions/IA_Weapon_Fire.IA_Weapon_Fire"), TEXT("/Game/Input/Actions/IA_Weapon_Fire_Auto.IA_Weapon_Fire_Auto") } },
			{ TEXT("Aim"), NSLOCTEXT("DowntownHunting", "PadAim", "AIM"), { TEXT("/ShooterCore/Input/Actions/IA_ADS.IA_ADS") } },
			{ TEXT("Jump"), NSLOCTEXT("DowntownHunting", "PadJump", "JUMP / WALL JUMP"), { TEXT("/Game/Input/Actions/IA_Jump.IA_Jump") } },
			{ TEXT("Crouch"), NSLOCTEXT("DowntownHunting", "PadCrouch", "CROUCH / SLIDE"), { TEXT("/Game/Input/Actions/IA_Crouch.IA_Crouch") } },
			{ TEXT("Reload"), NSLOCTEXT("DowntownHunting", "PadReload", "RELOAD"), { TEXT("/Game/Input/Actions/IA_Weapon_Reload.IA_Weapon_Reload") } },
			{ TEXT("Melee"), NSLOCTEXT("DowntownHunting", "PadMelee", "MELEE"), { TEXT("/ShooterCore/Input/Actions/IA_Melee.IA_Melee") } },
			{ TEXT("Grenade"), NSLOCTEXT("DowntownHunting", "PadGrenade", "GRENADE"), { TEXT("/ShooterCore/Input/Actions/IA_Grenade.IA_Grenade") } },
			{ TEXT("Switch"), NSLOCTEXT("DowntownHunting", "PadSwitch", "SWITCH WEAPON"), { TEXT("/ShooterCore/Input/Actions/IA_QuickSlot_CycleForward.IA_QuickSlot_CycleForward") } },
			{ TEXT("Ability"), NSLOCTEXT("DowntownHunting", "PadAbility", "ABILITY"), { TEXT("/Game/Input/Actions/IA_Ability_Dash.IA_Ability_Dash") } },
			{ TEXT("Ability2"), NSLOCTEXT("DowntownHunting", "PadAbility2", "2ND ABILITY"), { TEXT("/DowntownCore/Input/IA_DH_Ability2.IA_DH_Ability2") } },
			{ TEXT("Perspective"), NSLOCTEXT("DowntownHunting", "PadPerspective", "FIRST / THIRD PERSON"), { TEXT("/DowntownCore/Input/IA_DH_TogglePerspective.IA_DH_TogglePerspective") } },
			{ TEXT("Scoreboard"), NSLOCTEXT("DowntownHunting", "PadScoreboard", "SCOREBOARD"), { TEXT("/ShooterCore/Input/Actions/IA_ShowScoreboard.IA_ShowScoreboard") } },
		};
		return List;
	}

	// Highest priority first: DowntownCore's context wins over ShooterCore's and Lyra's (UDHPlayerRulesComponent).
	const TCHAR* Contexts[] = {
		TEXT("/DowntownCore/Input/IMC_DH_Perspective.IMC_DH_Perspective"),
		TEXT("/ShooterCore/Input/Mappings/IMC_ShooterGame.IMC_ShooterGame"),
		TEXT("/Game/Input/Mappings/IMC_Default.IMC_Default"),
	};

	const TCHAR* Section = TEXT("DowntownHunting");

	FString ConfigKey(const FRow& Row)
	{
		return FString(TEXT("Gamepad_")) + Row.Id;
	}

	FKey StoredKey(const FRow& Row)
	{
		FString Name;
		GConfig->GetString(Section, *ConfigKey(Row), Name, GGameUserSettingsIni);
		return Name.IsEmpty() ? FKey() : FKey(*Name);
	}

	void StoreKey(const FRow& Row, const FKey& Key)
	{
		if (Key.IsValid())
		{
			GConfig->SetString(Section, *ConfigKey(Row), *Key.GetFName().ToString(), GGameUserSettingsIni);
		}
		else
		{
			GConfig->RemoveKey(Section, *ConfigKey(Row), GGameUserSettingsIni);
		}
	}

	/** The mappings each context had when first seen: the assets' own layout. */
	TMap<TWeakObjectPtr<UInputMappingContext>, TArray<FEnhancedActionKeyMapping>>& Originals()
	{
		static TMap<TWeakObjectPtr<UInputMappingContext>, TArray<FEnhancedActionKeyMapping>> Map;
		return Map;
	}

	TArray<UInputMappingContext*> LoadContexts()
	{
		TArray<UInputMappingContext*> Loaded;
		for (const TCHAR* Path : Contexts)
		{
			if (UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, Path))
			{
				if (!Originals().Contains(Context))
				{
					// The second ability has only its gamepad button in the asset; F on the keyboard (free in Lyra).
					const UInputAction* Ability2 = LoadObject<UInputAction>(nullptr, TEXT("/DowntownCore/Input/IA_DH_Ability2.IA_DH_Ability2"));
					if (Ability2 && Context->HasMappingForInputAction(Ability2) && !Context->GetMappings().ContainsByPredicate(
						[Ability2](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == Ability2 && !Mapping.Key.IsGamepadKey(); }))
					{
						Context->MapKey(Ability2, EKeys::F);
					}
					Originals().Add(Context, Context->GetMappings());
				}
				Loaded.Add(Context);
			}
		}
		return Loaded;
	}

	/** The assets' button for a row. */
	FKey DefaultKey(const TArray<UInputMappingContext*>& Loaded, const FRow& Row)
	{
		const UInputAction* Action = LoadObject<UInputAction>(nullptr, Row.Actions[0]);
		for (UInputMappingContext* Context : Loaded)
		{
			for (const FEnhancedActionKeyMapping& Mapping : Originals()[Context])
			{
				if (Mapping.Action == Action && Mapping.Key.IsGamepadKey())
				{
					return Mapping.Key;
				}
			}
		}
		return FKey();
	}

	/** Every row's button: the stored choice or the assets' one. */
	TArray<FKey> CurrentKeys()
	{
		const TArray<UInputMappingContext*> Loaded = LoadContexts();
		TArray<FKey> Keys;
		for (const FRow& Row : Rows())
		{
			const FKey Stored = StoredKey(Row);
			Keys.Add(Stored.IsValid() ? Stored : DefaultKey(Loaded, Row));
		}
		return Keys;
	}

	FText KeyName(const FKey& Key)
	{
		static const TMap<FName, FText> Names = {
			{ EKeys::Gamepad_FaceButton_Bottom.GetFName(), FText::FromString(TEXT("A")) },
			{ EKeys::Gamepad_FaceButton_Right.GetFName(), FText::FromString(TEXT("B")) },
			{ EKeys::Gamepad_FaceButton_Left.GetFName(), FText::FromString(TEXT("X")) },
			{ EKeys::Gamepad_FaceButton_Top.GetFName(), FText::FromString(TEXT("Y")) },
			{ EKeys::Gamepad_LeftShoulder.GetFName(), FText::FromString(TEXT("LB")) },
			{ EKeys::Gamepad_RightShoulder.GetFName(), FText::FromString(TEXT("RB")) },
			{ EKeys::Gamepad_LeftTrigger.GetFName(), FText::FromString(TEXT("LT")) },
			{ EKeys::Gamepad_RightTrigger.GetFName(), FText::FromString(TEXT("RT")) },
			{ EKeys::Gamepad_LeftThumbstick.GetFName(), NSLOCTEXT("DowntownHunting", "PadLS", "LEFT STICK PRESS") },
			{ EKeys::Gamepad_RightThumbstick.GetFName(), NSLOCTEXT("DowntownHunting", "PadRS", "RIGHT STICK PRESS") },
			{ EKeys::Gamepad_DPad_Up.GetFName(), NSLOCTEXT("DowntownHunting", "PadUp", "D-PAD UP") },
			{ EKeys::Gamepad_DPad_Down.GetFName(), NSLOCTEXT("DowntownHunting", "PadDown", "D-PAD DOWN") },
			{ EKeys::Gamepad_DPad_Left.GetFName(), NSLOCTEXT("DowntownHunting", "PadLeft", "D-PAD LEFT") },
			{ EKeys::Gamepad_DPad_Right.GetFName(), NSLOCTEXT("DowntownHunting", "PadRight", "D-PAD RIGHT") },
			{ EKeys::Gamepad_Special_Left.GetFName(), NSLOCTEXT("DowntownHunting", "PadView", "VIEW / SELECT") },
		};
		if (!Key.IsValid())
		{
			return FText::FromString(TEXT("-"));
		}
		const FText* Name = Names.Find(Key.GetFName());
		return Name ? *Name : Key.GetDisplayName();
	}

	// Buttons a game action may take: no sticks (movement, looking) and not the pause button.
	bool IsBindable(const FKey& Key)
	{
		return Key.IsGamepadKey() && !Key.IsAxis1D() && !Key.IsAxis2D() && Key != EKeys::Gamepad_Special_Right
			&& !Key.GetFName().ToString().Contains(TEXT("Stick_"));
	}
}

/** While a row waits for its button: the next gamepad button goes to the screen, Escape or B... is just a button. */
class FDHPadCapture : public IInputProcessor
{
public:
	explicit FDHPadCapture(UDHControllerScreen* InScreen) : Screen(InScreen) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		UDHControllerScreen* Target = Screen.Get();
		if (!Target || InKeyEvent.IsRepeat())
		{
			return true;
		}
		if (IsBindable(InKeyEvent.GetKey()))
		{
			Target->FinishCapture(InKeyEvent.GetKey());
		}
		else if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_Special_Right)
		{
			Target->FinishCapture(FKey());
		}
		return true;  // nothing else reacts while waiting
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("DHPadCapture"); }

private:
	TWeakObjectPtr<UDHControllerScreen> Screen;
};

UDHControllerScreen::UDHControllerScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	bIsModal = true;
}

void UDHControllerScreen::ApplyBindings(const ULocalPlayer* Player)
{
	const TArray<UInputMappingContext*> Loaded = LoadContexts();
	for (UInputMappingContext* Context : Loaded)
	{
		// Back to the assets' layout, the choices go on top.
		Context->UnmapAll();
		for (const FEnhancedActionKeyMapping& Mapping : Originals()[Context])
		{
			Context->MapKey(Mapping.Action, Mapping.Key) = Mapping;
		}
	}
	// Every gamepad mapping of a row's actions gets the row's button (the assets have second mappings, e.g. Lyra's
	// melee on RB besides ours on the right stick). Mappings of other actions on a chosen button are dropped.
	// Lyra's actions are player mappable: their keys come from the player's key profile (Enhanced Input user
	// settings), not from the asset, so those change there; DowntownCore's mappings change in the asset.
	const TArray<FKey> Keys = CurrentKeys();
	TSet<FKey> Chosen;
	for (const FRow& Row : Rows())
	{
		if (StoredKey(Row).IsValid())
		{
			Chosen.Add(StoredKey(Row));
		}
	}
	TArray<bool> Mapped;
	Mapped.Init(false, Rows().Num());
	TMap<FName, FKey> ProfileKeys;
	for (UInputMappingContext* Context : Loaded)
	{
		TArray<TPair<const UInputAction*, FKey>> Dropped;
		for (int32 Index = 0; Index < Context->GetMappings().Num(); ++Index)
		{
			FEnhancedActionKeyMapping& Mapping = Context->GetMapping(Index);
			if (!Mapping.Key.IsGamepadKey() || !Mapping.Action)
			{
				continue;
			}
			const FString Path = Mapping.Action->GetPathName();
			const int32 Row = Rows().IndexOfByPredicate([&Path](const FRow& Entry) { return Entry.Actions.Contains(Path); });
			FKey Key = Mapping.Key;
			if (Row != INDEX_NONE && Keys[Row].IsValid())
			{
				Key = Keys[Row];
				Mapped[Row] = true;
			}
			else if (Row == INDEX_NONE && Chosen.Contains(Mapping.Key))
			{
				Key = FKey();
			}
			if (Mapping.IsPlayerMappable())
			{
				ProfileKeys.Add(Mapping.GetMappingName(), Key);
			}
			else if (Key.IsValid())
			{
				Mapping.Key = Key;
			}
			else
			{
				Dropped.Emplace(Mapping.Action, Mapping.Key);
			}
		}
		for (const TPair<const UInputAction*, FKey>& Mapping : Dropped)
		{
			Context->UnmapKey(Mapping.Key, Mapping.Value);
		}
	}
	// A chosen button for an action the assets have no gamepad mapping for: one in DowntownCore's context.
	for (int32 Row = 0; Row < Rows().Num() && !Loaded.IsEmpty(); ++Row)
	{
		for (const TCHAR* Path : Mapped[Row] || !Keys[Row].IsValid() ? TArray<const TCHAR*>() : Rows()[Row].Actions)
		{
			if (const UInputAction* Action = LoadObject<UInputAction>(nullptr, Path))
			{
				Loaded[0]->MapKey(Action, Keys[Row]);
			}
		}
	}
	UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	UEnhancedInputUserSettings* Settings = Input ? Input->GetUserSettings() : nullptr;
	UEnhancedPlayerMappableKeyProfile* Profile = Settings ? Settings->GetActiveKeyProfile() : nullptr;
	for (const TPair<FName, FKey>& Entry : Profile ? ProfileKeys : TMap<FName, FKey>())
	{
		if (FKeyMappingRow* Row = Profile->FindKeyMappingRowMutable(Entry.Key))
		{
			for (FPlayerKeyMapping& Mapping : Row->Mappings)
			{
				if (Mapping.GetDefaultKey().IsGamepadKey())
				{
					Mapping.SetCurrentKey(Entry.Value);
				}
			}
		}
	}
	if (Input)
	{
		Input->RequestRebuildControlMappings();
	}
}

void UDHControllerScreen::LogBindings(const ULocalPlayer* Player)
{
	const UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	for (const FRow& Row : Rows())
	{
		FString Keys;
		for (const FKey& Key : Input ? Input->QueryKeysMappedToAction(LoadObject<UInputAction>(nullptr, Row.Actions[0])) : TArray<FKey>())
		{
			Keys += Key.GetFName().ToString() + TEXT(" ");
		}
		UE_LOG(LogTemp, Log, TEXT("DH pad: %s -> %s"), Row.Id, *Keys);
	}
}

void UDHControllerScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(DHScreenStyle::Dim);
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Center);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);

	UTextBlock* Title = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 34, DHScreenStyle::Accent);
	Title->SetText(NSLOCTEXT("DowntownHunting", "ControllerTitle", "CONTROLLER"));
	Column->AddChildToVerticalBox(Title);
	UTextBlock* Hint = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 14, DHScreenStyle::Text);
	Hint->SetText(NSLOCTEXT("DowntownHunting", "ControllerHint", "Pick an action, then press the button for it (Start cancels). A button another action has is swapped.\nMove and look stay on the sticks. Keyboard keys: Options > Mouse & Keyboard."));
	Column->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0, 4, 0, 14));

	auto AddButton = [this](UPanelWidget* Parent, const FString& Id, int32 Index, UWidget* Content) -> UDHCardButton*
	{
		UDHCardButton* Button = WidgetTree->ConstructWidget<UDHCardButton>();
		Button->SetBackgroundColor(DHScreenStyle::Card);
		Button->CardId = Id;
		Button->CardSlot = Index;
		Button->PopScale = 0.02f;  // wide rows
		Button->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);
		Button->AddChild(Content);
		Parent->AddChild(Button);
		Buttons.Add(Button);
		KeepAlive.Add(Button);
		return Button;
	};

	for (int32 Index = 0; Index < Rows().Num(); ++Index)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
		USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>();
		NameBox->SetWidthOverride(320.f);
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 15, DHScreenStyle::Text);
		Name->SetText(Rows()[Index].Name);
		NameBox->AddChild(Name);
		Line->AddChildToHorizontalBox(NameBox)->SetVerticalAlignment(VAlign_Center);
		USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>();
		KeyBox->SetWidthOverride(260.f);
		UTextBlock* KeyText = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 15, DHScreenStyle::Text);
		KeyBox->AddChild(KeyText);
		Line->AddChildToHorizontalBox(KeyBox);
		KeyTexts.Add(KeyText);
		UDHCardButton* Button = AddButton(Column, TEXT("Row"), Index, Line);
		Cast<UVerticalBoxSlot>(Button->Slot)->SetPadding(FMargin(0, 2));
	}

	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0, 16, 0, 0));
	for (const TPair<FString, FText>& Entry : { TPair<FString, FText>(TEXT("Restore"), NSLOCTEXT("DowntownHunting", "ControllerRestore", "RESTORE DEFAULTS")),
		TPair<FString, FText>(TEXT("Back"), NSLOCTEXT("DowntownHunting", "Back", "BACK")) })
	{
		UTextBlock* Label = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 18, DHScreenStyle::Text);
		Label->SetText(Entry.Value);
		UDHCardButton* Button = AddButton(Footer, Entry.Key, 0, Label);
		Cast<UHorizontalBoxSlot>(Button->Slot)->SetPadding(FMargin(0, 0, 16, 0));
	}
	Refresh();
}

void UDHControllerScreen::NativeOnDeactivated()
{
	StopCapture();
	Super::NativeOnDeactivated();
}

UWidget* UDHControllerScreen::NativeGetDesiredFocusTarget() const
{
	return Buttons.IsEmpty() ? nullptr : Buttons[0].Get();
}

void UDHControllerScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		const bool bFocused = Buttons[Index]->HasAnyUserFocus() || Buttons[Index]->IsHovered();
		const bool bWaiting = Buttons[Index]->CardId == TEXT("Row") && Buttons[Index]->CardSlot == CapturingRow;
		Buttons[Index]->SetBackgroundColor(bWaiting ? DHScreenStyle::Selected : bFocused ? DHScreenStyle::Focus : DHScreenStyle::Card);
		Buttons[Index]->Animate(InDeltaTime);
	}
}

void UDHControllerScreen::HandleCardClicked(UDHCardButton* Card)
{
	if (CapturingRow != INDEX_NONE)
	{
		return;
	}
	if (Card->CardId == TEXT("Back"))
	{
		DeactivateWidget();
	}
	else if (Card->CardId == TEXT("Restore"))
	{
		for (const FRow& Row : Rows())
		{
			StoreKey(Row, FKey());
		}
		GConfig->Flush(false, GGameUserSettingsIni);
		ApplyBindings(GetOwningLocalPlayer());
		Refresh();
	}
	else if (Rows().IsValidIndex(Card->CardSlot) && FSlateApplication::IsInitialized())
	{
		CapturingRow = Card->CardSlot;
		KeyTexts[CapturingRow]->SetText(NSLOCTEXT("DowntownHunting", "ControllerPress", "PRESS A BUTTON..."));
		Capture = MakeShared<FDHPadCapture>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(Capture, 0);
	}
}

void UDHControllerScreen::FinishCapture(const FKey& Key)
{
	const int32 Row = CapturingRow;
	StopCapture();
	if (Rows().IsValidIndex(Row) && Key.IsValid())
	{
		// Swap with the action that had the button.
		const TArray<FKey> Keys = CurrentKeys();
		const int32 Other = Keys.IndexOfByKey(Key);
		if (Other != INDEX_NONE && Other != Row)
		{
			StoreKey(Rows()[Other], Keys[Row]);
		}
		StoreKey(Rows()[Row], Key);
		GConfig->Flush(false, GGameUserSettingsIni);
		ApplyBindings(GetOwningLocalPlayer());
	}
	Refresh();
}

void UDHControllerScreen::StopCapture()
{
	if (Capture.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Capture);
	}
	Capture.Reset();
	CapturingRow = INDEX_NONE;
}

void UDHControllerScreen::Refresh()
{
	const TArray<FKey> Keys = CurrentKeys();
	for (int32 Index = 0; Index < KeyTexts.Num(); ++Index)
	{
		KeyTexts[Index]->SetText(KeyName(Keys[Index]));
	}
}
