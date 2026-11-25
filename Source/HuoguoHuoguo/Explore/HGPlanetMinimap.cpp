#include "Explore/HGPlanetMinimap.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Explore/HGExplorerCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HuoguoHuoguo.h"
#include "Planet/HGPlanetGenerator.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor BackgroundColor(0.02f, 0.03f, 0.05f, 0.72f);
	const FLinearColor RimColor(0.45f, 0.85f, 0.95f, 0.85f);
	const FLinearColor GuideColor(0.45f, 0.85f, 0.95f, 0.25f);
	const FLinearColor PadColor(1.0f, 0.70f, 0.18f, 1.0f);
	const FLinearColor PlayerColor(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor RadarRockColor(0.75f, 0.72f, 0.85f, 0.55f);
	const FLinearColor RadarCurrentRockColor(1.0f, 1.0f, 1.0f, 0.95f);
	const FLinearColor ShipColor(0.65f, 0.75f, 0.9f, 0.9f);
	const FLinearColor DoorColor(0.3f, 1.0f, 0.9f, 1.0f);
	const FLinearColor TerminalColor(1.0f, 0.75f, 0.2f, 1.0f);

	// 정사각형 영역에 그리면 원이 되는 붓 (모서리를 높이의 절반만큼 둥글린다).
	const FSlateBrush* CircleBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White);
		return &Brush;
	}

	// 스캐너 파동이 지나간 재료: 4초 동안 점 둘레로 퍼지며 옅어지는 고리 (Age = 지나간 뒤 경과 초).
	TArray<FVector2f> CirclePoints(const FVector2f& Center, float Radius, int32 Segments = 64);
	void DrawPing(FSlateWindowElementList& Out, int32 Layer, const FPaintGeometry& Whole, const FVector2f& Pos, float Age, const FLinearColor& Color)
	{
		if (Age < 0.f || Age > 4.f)
		{
			return;
		}
		// 1초마다 한 번씩 퍼지는 고리를 되풀이한다 (깜빡임).
		const float Phase = FMath::Fmod(Age, 1.f);
		FLinearColor C = Color;
		C.A = (1.f - Phase) * (1.f - Age / 4.f);
		FSlateDrawElement::MakeLines(Out, Layer, Whole, CirclePoints(Pos, 5.f + Phase * 9.f, 20), ESlateDrawEffect::None, C, true, 1.5f);
	}

	// 중심 Center, 반지름 Radius인 원 둘레 점들 (MakeLines용, 처음 점으로 닫는다).
	TArray<FVector2f> CirclePoints(const FVector2f& Center, float Radius, int32 Segments)
	{
		TArray<FVector2f> Points;
		Points.Reserve(Segments + 1);
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float A = UE_TWO_PI * i / Segments;
			Points.Add(Center + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius);
		}
		return Points;
	}
}

void UHGPlanetMinimap::Init(AHGPlanetGenerator* InPlanet, APawn* InViewer)
{
	Planet = InPlanet;
	Viewer = InViewer;
	// 그리기만 하고 마우스/키 입력은 받지 않는다.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

FVector UHGPlanetMinimap::GetViewForward() const
{
	if (const APlayerController* PC = GetOwningPlayer())
	{
		return PC->GetControlRotation().Vector();
	}
	return Viewer.IsValid() ? Viewer->GetActorForwardVector() : FVector::ForwardVector;
}

FVector2D UHGPlanetMinimap::ProjectToMap(const FVector& PlanetCenter, float PlanetRadius, const FVector& ViewerLocation, const FVector& ViewForward, const FVector& Point)
{
	// 사장님 발밑의 "위"(행성 바깥 방향)와, 그 위에 수직인 접평면의 앞/오른쪽.
	const FVector Up = (ViewerLocation - PlanetCenter).GetSafeNormal();
	FVector Forward = ViewForward - Up * (ViewForward | Up);
	if (!Forward.Normalize())
	{
		// 카메라가 똑바로 위나 아래를 보면 접평면 성분이 없다. 아무 접선이나 앞으로 쓴다.
		Forward = FVector::CrossProduct(Up, FMath::Abs(Up.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
	}
	// 언리얼은 왼손 좌표계: 위(Z) x 앞(X) = 오른쪽(Y).
	const FVector Right = FVector::CrossProduct(Up, Forward);

	// theta = 중심에서 본 두 방향 사이 각도. 표면 거리 = theta * 반지름.
	const FVector Dir = (Point - PlanetCenter).GetSafeNormal();
	const double CosTheta = FMath::Clamp(Up | Dir, -1.0, 1.0);
	const double Theta = FMath::Acos(CosTheta);

	// P 쪽으로 출발하는 방향 = P 방향에서 "위" 성분을 뺀 접평면 성분.
	FVector Toward = Dir - Up * CosTheta;
	if (!Toward.Normalize())
	{
		// 바로 발밑이거나 정반대편 한 점. 방향이 정해지지 않으므로 앞쪽에 둔다.
		return FVector2D(0.0, Theta * PlanetRadius);
	}

	const double Distance = Theta * PlanetRadius;
	return FVector2D((Toward | Right) * Distance, (Toward | Forward) * Distance);
}

bool UHGPlanetMinimap::IsRadar() const
{
	return Planet.IsValid() && (Planet->GetPlanet().bAsteroidField || Planet->GetPlanet().bDerelict);
}

FVector UHGPlanetMinimap::GetRadarUp() const
{
	if (const AHGExplorerCharacter* Explorer = Cast<AHGExplorerCharacter>(Viewer.Get()))
	{
		return Explorer->GetGravityFrame().GetUpVector();
	}
	return FVector::UpVector;
}

FVector2D UHGPlanetMinimap::ProjectToRadar(const FVector& ViewerLocation, const FVector& Up, const FVector& ViewForward, const FVector& Point, float* OutHeight)
{
	// 기준 평면의 앞/오른쪽 (ProjectToMap과 같은 방식으로 카메라 방향을 평면에 눕힌다).
	FVector Forward = ViewForward - Up * (ViewForward | Up);
	if (!Forward.Normalize())
	{
		Forward = FVector::CrossProduct(Up, FMath::Abs(Up.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
	}
	const FVector Right = FVector::CrossProduct(Up, Forward);
	const FVector Rel = Point - ViewerLocation;
	if (OutHeight)
	{
		*OutHeight = static_cast<float>(Rel | Up);
	}
	return FVector2D(Rel | Right, Rel | Forward);
}

bool UHGPlanetMinimap::GetReturnPadOnMap(FVector2D& OutMapCm) const
{
	FVector PadLocation;
	if (!Planet.IsValid() || !Viewer.IsValid() || !Planet->GetReturnPadLocation(PadLocation))
	{
		return false;
	}
	OutMapCm = IsRadar()
		? ProjectToRadar(Viewer->GetActorLocation(), GetRadarUp(), GetViewForward(), PadLocation)
		: ProjectToMap(Planet->GetGravityCenter(), Planet->GetPlanet().Radius, Viewer->GetActorLocation(), GetViewForward(), PadLocation);
	return true;
}

int32 UHGPlanetMinimap::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bLoggedFirstPaint)
	{
		bLoggedFirstPaint = true;
		UE_LOG(LogHuoguo, Log, TEXT("[Map] 미니맵 첫 그리기: 화면 위치 %s, 크기 %s, 행성 %s"),
			*AllottedGeometry.GetAbsolutePosition().ToString(), *AllottedGeometry.GetLocalSize().ToString(), Planet.IsValid() ? TEXT("있음") : TEXT("없음"));
	}
	if (!Planet.IsValid() || !Viewer.IsValid())
	{
		return Layer;
	}

	const AHGPlanetGenerator* P = Planet.Get();
	const FVector Center = P->GetGravityCenter();
	const float PlanetRadius = P->GetPlanet().Radius;
	const FVector ViewerLocation = Viewer->GetActorLocation();
	const FVector Forward = GetViewForward();

	// 지도 원: 지름 D, 왼쪽 위 꼭짓점 Origin(화면 오른쪽 위 모서리 기준), 중심 Mid. 점들은 테두리에서 조금 안쪽(Inner)까지만 쓴다.
	const float D = MapDiameter;
	const FVector2f ScreenSize = AllottedGeometry.GetLocalSize();
	const FVector2f Origin(ScreenSize.X - D - ScreenMargin, ScreenMargin);
	const FVector2f Mid = Origin + FVector2f(D * 0.5f, D * 0.5f);
	const float Inner = D * 0.5f - 8.f;
	const double RangeCm = bWholePlanet ? UE_PI * PlanetRadius : LocalRangeCm;
	const double PixelsPerCm = Inner / RangeCm;

	const FPaintGeometry Whole = AllottedGeometry.ToPaintGeometry();
	auto Box = [&AllottedGeometry](const FVector2f& TopLeft, const FVector2f& Size)
	{
		return AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft));
	};
	// 지도 좌표(cm, X 오른쪽 / Y 앞쪽) -> 위젯 좌표(px, Y 아래쪽).
	auto ToWidget = [&Mid, PixelsPerCm](const FVector2D& MapCm)
	{
		return Mid + FVector2f(static_cast<float>(MapCm.X * PixelsPerCm), static_cast<float>(-MapCm.Y * PixelsPerCm));
	};

	// 배경과 테두리
	++Layer;
	FSlateDrawElement::MakeBox(OutDrawElements, Layer, Box(Origin, FVector2f(D, D)), CircleBrush(), ESlateDrawEffect::None, BackgroundColor);
	++Layer;
	FSlateDrawElement::MakeLines(OutDrawElements, Layer, Whole, CirclePoints(Mid, D * 0.5f - 1.f), ESlateDrawEffect::None, RimColor, true, 2.f);
	// 안내 원: 근거리면 범위의 절반, 행성 전체면 90도 선(행성 반대쪽 반구의 시작). 레이더면 반경의 절반.
	FSlateDrawElement::MakeLines(OutDrawElements, Layer, Whole, CirclePoints(Mid, Inner * 0.5f), ESlateDrawEffect::None, GuideColor, true, 1.f);
	if (IsRadar())
	{
		return PaintRadar(AllottedGeometry, OutDrawElements, Layer, Origin, Mid, D, Inner);
	}

	// 재료 표식 (캔 것은 그리지 않는다)
	++Layer;
	constexpr float DotSize = 6.f;
	for (const FHGIngredientMarker& Marker : P->GetIngredientMarkers())
	{
		if (!Marker.IsAvailable())
		{
			continue;
		}
		const FVector2D MapCm = ProjectToMap(Center, PlanetRadius, ViewerLocation, Forward, Marker.Location);
		if (MapCm.Size() > RangeCm)
		{
			continue;
		}
		const FVector2f Pos = ToWidget(MapCm);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, Box(Pos - FVector2f(DotSize * 0.5f), FVector2f(DotSize)),
			CircleBrush(), ESlateDrawEffect::None, P->GetIngredientColor(Marker.Kind));
		DrawPing(OutDrawElements, Layer, Whole, Pos, static_cast<float>(P->GetWorld()->GetTimeSeconds()) - Marker.PingTime, P->GetIngredientColor(Marker.Kind));
	}

	// 귀환 발판: 범위 안이면 금색 원, 밖이면 테두리에 바깥을 가리키는 금색 삼각형.
	++Layer;
	FVector2D PadMapCm;
	const bool bHasPad = GetReturnPadOnMap(PadMapCm);
	if (bHasPad)
	{
		const double PadDistance = PadMapCm.Size();
		if (PadDistance <= RangeCm)
		{
			const FVector2f Pos = ToWidget(PadMapCm);
			constexpr float PadSize = 12.f;
			FSlateDrawElement::MakeBox(OutDrawElements, Layer, Box(Pos - FVector2f(PadSize * 0.5f), FVector2f(PadSize)),
				CircleBrush(), ESlateDrawEffect::None, PadColor);
		}
		else
		{
			// 화면 방향 단위 벡터 (Y는 아래쪽이 +).
			const FVector2f Dir = FVector2f(static_cast<float>(PadMapCm.X), static_cast<float>(-PadMapCm.Y)).GetSafeNormal();
			const FVector2f Side(-Dir.Y, Dir.X);
			const FVector2f Tip = Mid + Dir * (D * 0.5f - 3.f);
			const FVector2f Base = Tip - Dir * 12.f;
			TArray<FVector2f> Arrow = { Tip, Base + Side * 7.f, Base - Side * 7.f, Tip };
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, Whole, Arrow, ESlateDrawEffect::None, PadColor, true, 3.f);
		}
	}

	// 사장님: 중심의 흰 화살표. 지도 위쪽이 카메라 방향이라 늘 위를 가리킨다.
	++Layer;
	TArray<FVector2f> PlayerArrow = { Mid + FVector2f(0.f, -9.f), Mid + FVector2f(6.f, 7.f), Mid + FVector2f(0.f, 3.f), Mid + FVector2f(-6.f, 7.f), Mid + FVector2f(0.f, -9.f) };
	FSlateDrawElement::MakeLines(OutDrawElements, Layer, Whole, PlayerArrow, ESlateDrawEffect::None, PlayerColor, true, 2.f);

	// 아래 글자: 귀환 발판 거리, 범위 안내.
	++Layer;
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	const FString PadText = bHasPad
		? FString::Printf(TEXT("귀환 발판 %.0fm"), PadMapCm.Size() / 100.0)
		: FString(TEXT("귀환 발판 없음"));
	FSlateDrawElement::MakeText(OutDrawElements, Layer, Box(Origin + FVector2f(4.f, D + 2.f), FVector2f(D, 18.f)), PadText, Font, ESlateDrawEffect::None, PadColor);
	const FString RangeText = bWholePlanet
		? FString::Printf(TEXT("[M] 행성 전체 (반 바퀴 %.0fm)"), UE_PI * PlanetRadius / 100.0)
		: FString::Printf(TEXT("[M] 근거리 %.0fm"), LocalRangeCm / 100.0);
	FSlateDrawElement::MakeText(OutDrawElements, Layer, Box(Origin + FVector2f(4.f, D + 21.f), FVector2f(D, 18.f)), RangeText, Font, ESlateDrawEffect::None, RimColor);

	return Layer;
}

int32 UHGPlanetMinimap::PaintRadar(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 Layer,
	const FVector2f& Origin, const FVector2f& Mid, float D, float Inner) const
{
	const AHGPlanetGenerator* P = Planet.Get();
	const FVector ViewerLocation = Viewer->GetActorLocation();
	const FVector Up = GetRadarUp();
	const FVector Forward = GetViewForward();
	const double RangeCm = bWholePlanet ? RadarFarCm : RadarNearCm;
	const double PixelsPerCm = Inner / RangeCm;
	const FPaintGeometry Whole = AllottedGeometry.ToPaintGeometry();
	auto Box = [&AllottedGeometry](const FVector2f& TopLeft, const FVector2f& Size)
	{
		return AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft));
	};
	auto ToWidget = [&Mid, PixelsPerCm](const FVector2D& MapCm)
	{
		return Mid + FVector2f(static_cast<float>(MapCm.X * PixelsPerCm), static_cast<float>(-MapCm.Y * PixelsPerCm));
	};
	auto Project = [&](const FVector& Point, float* OutHeight = nullptr)
	{
		return ProjectToRadar(ViewerLocation, Up, Forward, Point, OutHeight);
	};
	// 사장님보다 3m 넘게 위/아래에 있으면 점 옆에 작은 위/아래 삼각형.
	auto HeightMark = [&](const FVector2f& Pos, float Height, const FLinearColor& Color)
	{
		if (FMath::Abs(Height) < 300.f)
		{
			return;
		}
		const float S = Height > 0.f ? -1.f : 1.f; // 화면 Y는 아래가 +
		const FVector2f Base = Pos + FVector2f(6.f, 0.f);
		TArray<FVector2f> Tri = { Base + FVector2f(0.f, S * 5.f), Base + FVector2f(4.f, -S * 2.f), Base + FVector2f(-4.f, -S * 2.f), Base + FVector2f(0.f, S * 5.f) };
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Whole, Tri, ESlateDrawEffect::None, Color, true, 1.5f);
	};

	// 소행성 지대: 소행성을 크기대로 원으로. 지금 서 있거나 끌리는 소행성은 밝게.
	++Layer;
	if (P->GetPlanet().bAsteroidField)
	{
		const AHGExplorerCharacter* Explorer = Cast<AHGExplorerCharacter>(Viewer.Get());
		const int32 Current = Explorer ? Explorer->GetCurrentBody() : INDEX_NONE;
		const TArray<FHGGravityBody>& Bodies = P->GetGravityBodies();
		for (int32 i = 0; i < Bodies.Num(); ++i)
		{
			const FVector2D MapCm = Project(Bodies[i].Center);
			if (MapCm.Size() - Bodies[i].Radius > RangeCm)
			{
				continue;
			}
			const float RadiusPx = FMath::Max(3.f, static_cast<float>(Bodies[i].Radius * PixelsPerCm));
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, Whole, CirclePoints(ToWidget(MapCm), RadiusPx, 24), ESlateDrawEffect::None,
				i == Current ? RadarCurrentRockColor : RadarRockColor, true, i == Current ? 2.f : 1.f);
		}
	}

	// 폐우주선: 윤곽선, 문(청록 점), 쓰지 않은 설계도 단말(금색 점).
	if (P->GetPlanet().bDerelict)
	{
		TArray<FVector2f> Outline;
		for (const FVector& Corner : P->GetShipOutline())
		{
			Outline.Add(ToWidget(Project(Corner)));
		}
		if (Outline.Num() > 0)
		{
			// 처음 점으로 닫는다. 배열 원소를 같은 배열에 바로 넣으면 늘어나며 주소가 바뀔 수 있어 먼저 복사한다.
			const FVector2f First = Outline[0];
			Outline.Add(First);
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, Whole, Outline, ESlateDrawEffect::None, ShipColor, true, 2.f);
		}
		++Layer;
		for (int32 i = 0; i < P->GetInteractableCount(); ++i)
		{
			const FHGInteractable& Point = P->GetInteractable(i);
			if (Point.Type == EHGInteractType::Blueprint && Point.bUsed)
			{
				continue;
			}
			float Height = 0.f;
			const FVector2D MapCm = Project(Point.Location, &Height);
			if (MapCm.Size() > RangeCm)
			{
				continue;
			}
			const FLinearColor Color = Point.Type == EHGInteractType::Blueprint ? TerminalColor : DoorColor;
			const FVector2f Pos = ToWidget(MapCm);
			constexpr float Size = 8.f;
			FSlateDrawElement::MakeBox(OutDrawElements, Layer, Box(Pos - FVector2f(Size * 0.5f), FVector2f(Size)), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
			HeightMark(Pos, Height, Color);
		}
	}

	// 재료 표식 (캔 것, 다른 시간대 것은 그리지 않는다)
	++Layer;
	constexpr float DotSize = 6.f;
	for (const FHGIngredientMarker& Marker : P->GetIngredientMarkers())
	{
		if (!Marker.IsAvailable())
		{
			continue;
		}
		float Height = 0.f;
		const FVector2D MapCm = Project(Marker.Location, &Height);
		if (MapCm.Size() > RangeCm)
		{
			continue;
		}
		const FVector2f Pos = ToWidget(MapCm);
		const FLinearColor Color = P->GetIngredientColor(Marker.Kind);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, Box(Pos - FVector2f(DotSize * 0.5f), FVector2f(DotSize)), CircleBrush(), ESlateDrawEffect::None, Color);
		HeightMark(Pos, Height, Color);
		DrawPing(OutDrawElements, Layer, Whole, Pos, static_cast<float>(P->GetWorld()->GetTimeSeconds()) - Marker.PingTime, Color);
	}

	// 귀환 발판/포드: 범위 안이면 금색 원, 밖이면 테두리 화살표 (행성 지도와 같다).
	++Layer;
	FVector2D PadMapCm;
	FVector PadLocation;
	const bool bHasPad = GetReturnPadOnMap(PadMapCm) && P->GetReturnPadLocation(PadLocation);
	if (bHasPad)
	{
		if (PadMapCm.Size() <= RangeCm)
		{
			const FVector2f Pos = ToWidget(PadMapCm);
			constexpr float PadSize = 12.f;
			FSlateDrawElement::MakeBox(OutDrawElements, Layer, Box(Pos - FVector2f(PadSize * 0.5f), FVector2f(PadSize)), CircleBrush(), ESlateDrawEffect::None, PadColor);
		}
		else
		{
			const FVector2f Dir = FVector2f(static_cast<float>(PadMapCm.X), static_cast<float>(-PadMapCm.Y)).GetSafeNormal();
			const FVector2f Side(-Dir.Y, Dir.X);
			const FVector2f Tip = Mid + Dir * (D * 0.5f - 3.f);
			const FVector2f Base = Tip - Dir * 12.f;
			TArray<FVector2f> Arrow = { Tip, Base + Side * 7.f, Base - Side * 7.f, Tip };
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, Whole, Arrow, ESlateDrawEffect::None, PadColor, true, 3.f);
		}
	}

	// 사장님 화살표
	++Layer;
	TArray<FVector2f> PlayerArrow = { Mid + FVector2f(0.f, -9.f), Mid + FVector2f(6.f, 7.f), Mid + FVector2f(0.f, 3.f), Mid + FVector2f(-6.f, 7.f), Mid + FVector2f(0.f, -9.f) };
	FSlateDrawElement::MakeLines(OutDrawElements, Layer, Whole, PlayerArrow, ESlateDrawEffect::None, PlayerColor, true, 2.f);

	// 아래 글자: 귀환 거리(직선), 레이더 반경.
	++Layer;
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	const FString PadText = bHasPad
		? FString::Printf(TEXT("%s %.0fm"), P->GetPlanet().bDerelict ? TEXT("귀환 포드") : TEXT("귀환 발판"), FVector::Dist(ViewerLocation, PadLocation) / 100.0)
		: FString(TEXT("귀환 발판 없음"));
	FSlateDrawElement::MakeText(OutDrawElements, Layer, Box(Origin + FVector2f(4.f, D + 2.f), FVector2f(D, 18.f)), PadText, Font, ESlateDrawEffect::None, PadColor);
	const FString RangeText = FString::Printf(TEXT("[M] 레이더 %.0fm (삼각형 = 위/아래)"), RangeCm / 100.0);
	FSlateDrawElement::MakeText(OutDrawElements, Layer, Box(Origin + FVector2f(4.f, D + 21.f), FVector2f(D, 18.f)), RangeText, Font, ESlateDrawEffect::None, RimColor);
	return Layer;
}