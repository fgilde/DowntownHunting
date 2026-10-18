#pragma once

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Fonts/SlateFontInfo.h"
#include "Math/Color.h"
#include "UObject/UObjectGlobals.h"

/** Look shared by the Downtown Hunting menu screens built in code. */
namespace DHScreenStyle
{
	inline const FLinearColor Accent(1.0f, 0.42f, 0.05f);
	inline const FLinearColor Card(0.05f, 0.05f, 0.06f, 0.85f);
	inline const FLinearColor Focus(0.32f, 0.27f, 0.22f, 0.95f);  // keyboard/gamepad focus or mouse hover
	inline const FLinearColor Dim(0.f, 0.f, 0.f, 0.82f);
	inline const FLinearColor Text(0.85f, 0.82f, 0.78f);

	/** Lyra's UI fonts: Orbitron for headings and buttons, NotoSans for running text. */
	inline FSlateFontInfo Font(const TCHAR* Name, int32 Size)
	{
		const FString Path = FString::Printf(TEXT("/Game/UI/Foundation/Fonts/%s.%s"), Name, Name);
		return FSlateFontInfo(LoadObject<UObject>(nullptr, *Path), Size);
	}

	/** Text block in the screens' style. */
	inline UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* FontName, int32 Size, const FLinearColor& Colour)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetFont(Font(FontName, Size));
		Block->SetColorAndOpacity(Colour);
		return Block;
	}
}
