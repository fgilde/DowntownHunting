#pragma once

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Fonts/SlateFontInfo.h"
#include "Math/Color.h"
#include "UObject/UObjectGlobals.h"

/** Look shared by the Downtown Hunting menu screens built in code. */
namespace DHScreenStyle
{
	// Arcade look: neon yellow for titles, bars and outlines, hot magenta for what is chosen, cyan as a third pop.
	inline const FLinearColor Accent(1.0f, 0.82f, 0.0f);
	inline const FLinearColor Selected(1.0f, 0.06f, 0.42f, 0.95f);
	inline const FLinearColor Cyan(0.0f, 0.85f, 1.0f);
	inline const FLinearColor Card(0.035f, 0.02f, 0.07f, 0.88f);
	inline const FLinearColor Focus(0.22f, 0.07f, 0.32f, 0.95f);  // keyboard/gamepad focus or mouse hover
	inline const FLinearColor Dim(0.02f, 0.0f, 0.05f, 0.78f);
	inline const FLinearColor Text(0.95f, 0.93f, 1.0f);

	/** Lyra's UI fonts: Orbitron for headings and buttons, NotoSans for running text. */
	inline FSlateFontInfo Font(const TCHAR* Name, int32 Size)
	{
		const FString Path = FString::Printf(TEXT("/Game/UI/Foundation/Fonts/%s.%s"), Name, Name);
		// Big Orbitron (titles) in its heaviest cut, arcade style.
		const FName Typeface = FCString::Strcmp(Name, TEXT("Orbitron")) == 0 && Size >= 28 ? FName(TEXT("Black")) : NAME_None;
		return FSlateFontInfo(LoadObject<UObject>(nullptr, *Path), Size, Typeface);
	}

	/** Text block in the screens' style. */
	inline UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* FontName, int32 Size, const FLinearColor& Colour)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetFont(Font(FontName, Size));
		Block->SetColorAndOpacity(Colour);
		if (Size >= 28)
		{
			// Arcade titles: hard magenta drop shadow.
			Block->SetShadowOffset(FVector2D(3.f, 3.f));
			Block->SetShadowColorAndOpacity(Selected);
		}
		return Block;
	}
}
