#include "UI/HGCodexWidget.h"

#include "Engine/DataTable.h"
#include "Game/HGGameInstance.h"
#include "Game/HGIngredientTypes.h"
#include "World/Planet/HGPlanetTypes.h"
#include "Rendering/DrawElements.h"
#include "Shop/Furniture/HGFurnitureData.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor CodexPanel(0.02f, 0.03f, 0.05f, 0.82f);
	const FLinearColor CodexTitle(0.45f, 0.85f, 0.95f, 1.f);
	const FLinearColor CodexText(0.92f, 0.94f, 0.97f, 1.f);
	const FLinearColor CodexDim(0.5f, 0.52f, 0.58f, 1.f);
	const FLinearColor CodexGold(1.f, 0.75f, 0.25f, 1.f);

	const TCHAR* TimeName(EHGIngredientTime Time)
	{
		switch (Time)
		{
		case EHGIngredientTime::Day:
			return TEXT("낮");
		case EHGIngredientTime::Night:
			return TEXT("밤");
		default:
			return TEXT("언제나");
		}
	}
}

void UHGCodexWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 보기만 하고 입력은 받지 않는다 (게임 조작을 막지 않는다).
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

int32 UHGCodexWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (!GI)
	{
		return Layer;
	}

	// 줄 목록을 먼저 만들고(글자, 색, 앞 점 색), 그 높이에 맞춰 패널을 그린 뒤 글자를 얹는다.
	struct FLine
	{
		FString Text;
		FLinearColor Color;
		bool bTitle = false;
		bool bDot = false;
		FLinearColor DotColor = FLinearColor::White;
	};
	TArray<FLine> Lines;
	auto Add = [&Lines](const FString& Text, const FLinearColor& Color, bool bTitle = false)
	{
		FLine& L = Lines.AddDefaulted_GetRef();
		L.Text = Text;
		L.Color = Color;
		L.bTitle = bTitle;
	};

	// 재료
	if (const UDataTable* Table = GI->GetIngredientTable())
	{
		const TArray<FName> Ids = Table->GetRowNames();
		int32 Found = 0;
		for (const FName& Id : Ids)
		{
			Found += GI->IsIngredientDiscovered(Id) ? 1 : 0;
		}
		Add(FString::Printf(TEXT("재료  %d / %d"), Found, Ids.Num()), CodexTitle, true);
		for (const FName& Id : Ids)
		{
			const FHGIngredientRow* Row = Table->FindRow<FHGIngredientRow>(Id, TEXT("Codex"));
			FLine& L = Lines.AddDefaulted_GetRef();
			L.bDot = true;
			if (Row && GI->IsIngredientDiscovered(Id))
			{
				L.Text = FString::Printf(TEXT("%s   (%s, %s 행성, 캔 횟수 %d)"), *Row->DisplayName.ToString(), TimeName(Row->AvailableAt),
					HGPlanet::BiomeName(Row->HomeBiome), GI->GetHarvestTotal(Id));
				L.Color = CodexText;
				L.DotColor = Row->Color;
			}
			else
			{
				L.Text = TEXT("???   (스캔하거나 캐면 기록된다)");
				L.Color = CodexDim;
				L.DotColor = CodexDim;
			}
		}
	}

	// 장소 (군계 x 종류)
	static const TCHAR* KindNames[] = { TEXT("행성"), TEXT("소행성 지대"), TEXT("폐우주선") };
	int32 Places = 0;
	for (int32 Biome = 0; Biome < 3; ++Biome)
	{
		for (int32 Kind = 0; Kind < 3; ++Kind)
		{
			Places += GI->IsPlaceDiscovered(UHGGameInstance::PlaceKey(Biome, Kind)) ? 1 : 0;
		}
	}
	Add(TEXT(""), CodexText);
	Add(FString::Printf(TEXT("장소  %d / 9"), Places), CodexTitle, true);
	for (int32 Biome = 0; Biome < 3; ++Biome)
	{
		FString Row = FString::Printf(TEXT("%s:  "), HGPlanet::BiomeName(static_cast<EHGPlanetBiome>(Biome)));
		for (int32 Kind = 0; Kind < 3; ++Kind)
		{
			const bool bSeen = GI->IsPlaceDiscovered(UHGGameInstance::PlaceKey(Biome, Kind));
			Row += FString::Printf(TEXT("%s %s    "), bSeen ? TEXT("[O]") : TEXT("[ ]"), bSeen ? KindNames[Kind] : TEXT("???"));
		}
		Add(Row, CodexText);
	}

	// 가게
	const FIntPoint Furniture = HGFurnitureUnlock::CountUnlocked(GI);
	const UDataTable* Combos = GI->GetComboTable();
	Add(TEXT(""), CodexText);
	Add(TEXT("가게"), CodexTitle, true);
	Add(FString::Printf(TEXT("발견한 콤보 %d / %d    열린 가구 %d / %d    찾은 설계도 %d"),
		GI->GetDiscoveredCombos().Num(), Combos ? Combos->GetRowNames().Num() : 0, Furniture.X, Furniture.Y, GI->GetBlueprintsFound()), CodexGold);

	// 패널: 화면 왼쪽, 위에서 260 내려온 곳 (왼쪽 위의 안내 글자 줄들과 겹치지 않게). 줄 높이 18, 제목 줄은 24.
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 11);
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	float Height = 44.f;
	for (const FLine& L : Lines)
	{
		Height += L.bTitle ? 24.f : 18.f;
	}
	const FVector2f Origin(24.f, 260.f);
	const float Width = 560.f;
	auto Box = [&AllottedGeometry](const FVector2f& TopLeft, const FVector2f& Size)
	{
		return AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft));
	};
	++Layer;
	FSlateDrawElement::MakeBox(OutDrawElements, Layer, Box(Origin, FVector2f(Width, Height)), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, CodexPanel);
	++Layer;
	FSlateDrawElement::MakeText(OutDrawElements, Layer, Box(Origin + FVector2f(14.f, 10.f), FVector2f(Width, 24.f)), FString(TEXT("도감  (J 닫기)")),
		FCoreStyle::GetDefaultFontStyle("Bold", 15), ESlateDrawEffect::None, CodexTitle);
	float Y = Origin.Y + 40.f;
	for (const FLine& L : Lines)
	{
		const float LineH = L.bTitle ? 24.f : 18.f;
		float X = Origin.X + 16.f;
		if (L.bDot)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, Layer, Box(FVector2f(X + 4.f, Y + 5.f), FVector2f(8.f, 8.f)), FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None, L.DotColor);
			X += 20.f;
		}
		FSlateDrawElement::MakeText(OutDrawElements, Layer, Box(FVector2f(X, Y), FVector2f(Width - 30.f, LineH)), L.Text,
			L.bTitle ? TitleFont : Font, ESlateDrawEffect::None, L.Color);
		Y += LineH;
	}
	return Layer;
}
