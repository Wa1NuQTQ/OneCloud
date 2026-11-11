#include "SacredLakeWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "ForestLakeHudInk.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "UnrealClient.h"

// 将规则层给出的世界锚点转为屏幕图形，交互条件仍由规则计算。
namespace
{
	using namespace OneCloud::ForestHud;
	const FLinearColor ReticleIdle(0.66f, 0.75f, 0.73f, 0.45f);
	const FLinearColor RainPreview(0.41f, 0.84f, 0.88f, 0.52f);
	const FLinearColor InvaderColor(0.98f, 0.70f, 0.48f);
	const FLinearColor ExtractionColor(0.79f, 0.51f, 1.0f);
	const FLinearColor PlantValidColor(0.42f, 0.94f, 0.63f);
	const FLinearColor PlantInvalidColor(0.97f, 0.35f, 0.25f);
	constexpr double RefillMarkerHeightCm = 95.0;

	// 在一次绘制内复用相机矩阵、有效画面矩形和 UI 坐标换算。
	struct FHudProjection
	{
		FMatrix ViewProjection;
		FIntRect ViewRect;
		FVector2D LocalOrigin;
		FVector2D LocalPerPixel;
		FVector2f Min;
		FVector2f Max;

		FVector2f Center() const { return (Min + Max) * 0.5f; }

		bool Project(const FVector& Position, FVector2f& LocalPosition) const
		{
			FVector2D Pixel;
			if (!FSceneView::ProjectWorldToScreen(Position, ViewRect, ViewProjection, Pixel) || Pixel.ContainsNaN())
			{
				return false;
			}
			LocalPosition = FVector2f(LocalOrigin + Pixel * LocalPerPixel);
			return true;
		}

		bool Contains(FVector2f Position, float Inset = 0.f) const
		{
			return Position.X >= Min.X + Inset && Position.X <= Max.X - Inset &&
				Position.Y >= Min.Y + Inset && Position.Y <= Max.Y - Inset;
		}
	};

	TOptional<FHudProjection> MakeHudProjection(const APlayerController& Player, const FGeometry& Geometry)
	{
		const ULocalPlayer* LocalPlayer = Player.GetLocalPlayer();
		FViewport* Viewport = LocalPlayer && LocalPlayer->ViewportClient ? LocalPlayer->ViewportClient->Viewport : nullptr;
		FSceneViewProjectionData Projection;
		if (!Viewport || !LocalPlayer->GetProjectionData(Viewport, Projection))
		{
			return {};
		}
		const FIntPoint ViewportSize = Viewport->GetSizeXY();
		// 使用受相机宽高比约束的画面范围，避开可能存在的黑边。
		const FIntRect& ViewRect = Projection.GetConstrainedViewRect();
		if (ViewportSize.X <= 0 || ViewportSize.Y <= 0 || ViewRect.Width() <= 0 || ViewRect.Height() <= 0)
		{
			return {};
		}

		// 投影得到视口像素，Slate 使用控件局部坐标。在一帧内只求一次换算比例。
		FVector2D LocalOrigin;
		FVector2D LocalCorner;
		const FVector2D PixelSize(ViewportSize);
		USlateBlueprintLibrary::ScreenToWidgetLocal(&Player, Geometry, FVector2D::ZeroVector, LocalOrigin);
		USlateBlueprintLibrary::ScreenToWidgetLocal(&Player, Geometry, PixelSize, LocalCorner);
		const FVector2D LocalPerPixel = (LocalCorner - LocalOrigin) / PixelSize;
		if (LocalPerPixel.X <= 0.0 || LocalPerPixel.Y <= 0.0)
		{
			return {};
		}
		return FHudProjection{
			Projection.ComputeViewProjectionMatrix(), ViewRect, LocalOrigin, LocalPerPixel,
			FVector2f(LocalOrigin + FVector2D(ViewRect.Min) * LocalPerPixel),
			FVector2f(LocalOrigin + FVector2D(ViewRect.Max) * LocalPerPixel)
		};
	}

	// 世界坐标采样圆周以保留透视，遇到不可见点断开线段，防止跨屏幕补线。
	void DrawFocusRing(const FHudInk& Ink, const FHudProjection& Projection, const FVector& Center, float RadiusCm,
		const FLinearColor& Color, bool bDashed)
	{
		constexpr int32 Sides = 32;
		TArray<FVector2f> Points;
		Points.Reserve(Sides + 1);
		for (int32 Side = 0; Side <= Sides; ++Side)
		{
			const double Angle = Side * (2.0 * UE_PI / Sides);
			const FVector Position = Center + FVector(FMath::Cos(Angle) * RadiusCm, FMath::Sin(Angle) * RadiusCm, 12.0);
			FVector2f Pixel;
			if ((!bDashed || Side % 4 < 3) && Projection.Project(Position, Pixel) && Projection.Contains(Pixel, 1.f))
			{
				Points.Add(Pixel);
			}
			else if (!Points.IsEmpty())
			{
				if (Points.Num() > 1)
				{
					Ink.Stroke(FVector2f::ZeroVector, MoveTemp(Points), Color, 1.35f);
				}
				Points.Reset();
			}
		}
		if (Points.Num() > 1)
		{
			Ink.Stroke(FVector2f::ZeroVector, MoveTemp(Points), Color, 1.35f);
		}
	}

	void DrawSeedling(const FHudInk& Ink, FVector2f Center, const FLinearColor& Color)
	{
		Ink.Stroke(Center, {{0.f, 7.f}, {0.f, 0.f}, {-6.f, -5.f}, {-8.f, -5.f}, {-6.f, 0.f}, {0.f, 2.f},
			{6.f, -4.f}, {8.f, -4.f}, {6.f, 1.f}, {0.f, 3.f}}, Color);
	}

	void DrawPlot(const FHudInk& Ink, const FHudProjection& Projection, const FVector& Plot, bool bValid)
	{
		const FLinearColor& Color = bValid ? PlantValidColor : PlantInvalidColor;
		DrawFocusRing(Ink, Projection, Plot, 100.f, Color, !bValid);
		FVector2f Position;
		if (!Projection.Project(Plot + FVector(0.0, 0.0, 55.0), Position) || !Projection.Contains(Position, 16.f))
		{
			return;
		}
		Ink.Box(Position - FVector2f(12.f, 12.f), {24.f, 24.f}, PanelColor);
		DrawSeedling(Ink, Position, Color);
		if (!bValid) { Ink.Stroke(Position, {{-9.f, 9.f}, {9.f, -9.f}}, Color); }
	}

	FVector2f DrawEdgeArrow(const FHudInk& Ink, const FHudProjection& Projection, const APlayerController& Player,
		const FVector& WorldPosition, const FVector2f* ProjectedPosition, const FLinearColor& Color)
	{
		const FVector2f ScreenCenter = Projection.Center();
		FVector2f Direction;
		if (ProjectedPosition)
		{
			Direction = (*ProjectedPosition - ScreenCenter).GetSafeNormal();
		}
		else
		{
			// 目标在镜头后时没有可用的投影，改用相机右方向估计屏幕左右。
			FVector ViewPosition;
			FRotator ViewRotation;
			Player.GetPlayerViewPoint(ViewPosition, ViewRotation);
			const FVector Offset = (WorldPosition - ViewPosition).GetSafeNormal();
			const FVector ViewRight = FRotationMatrix(ViewRotation).GetScaledAxis(EAxis::Y);
			Direction = FVector2f(static_cast<float>(FVector::DotProduct(ViewRight, Offset)), 0.65f).GetSafeNormal();
		}

		// 求从画面中心沿该方向与安全矩形相交的位置，给边缘留出余量。
		const FVector2f Room = (Projection.Max - Projection.Min) * 0.5f - FVector2f(32.f, 45.f);
		const float EdgeDistance = FMath::Min(Room.X / FMath::Max(FMath::Abs(Direction.X), 0.001f),
			Room.Y / FMath::Max(FMath::Abs(Direction.Y), 0.001f));
		const FVector2f Edge = ScreenCenter + Direction * FMath::Max(0.f, EdgeDistance);
		const FVector2f Side(-Direction.Y, Direction.X);
		Ink.Stroke(Edge, {Side * -6.f - Direction * 5.f, Direction * 5.f, Side * 6.f - Direction * 5.f}, Color, 2.f);
		return Edge;
	}

	void DrawSpider(const FHudInk& Ink, const FHudProjection& Projection, const APlayerController& Player,
		const FSacredLakeGuidance::FThreat& Threat)
	{
		const FVector& Spider = Threat.Position;
		const bool bSiphoning = Threat.bSiphoning;
		const FLinearColor& Color = bSiphoning ? ExtractionColor : InvaderColor;
		FVector2f Position;
		const bool bProjected = Projection.Project(Spider, Position);
		if (bProjected && Projection.Contains(Position, 25.f))
		{
			Ink.Diamond(Position, bSiphoning ? 7.f : 5.f, Color);
			Ink.Stroke(Position, {{-4.f, 10.f}, {0.f, 14.f}, {4.f, 10.f}}, Color);
			if (Threat.bMajor) { Ink.Diamond(Position, 11.f, Color); }
			if (Threat.Count > 1)
			{
				Ink.Label(Position + FVector2f(12.f, -8.f), FText::AsNumber(Threat.Count), Color, 12);
			}
			return;
		}
		const FVector2f Edge = DrawEdgeArrow(Ink, Projection, Player, Spider, bProjected ? &Position : nullptr, Color);
		const FVector2f Inward = (Projection.Center() - Edge).GetSafeNormal();
		if (Threat.bMajor) { Ink.Diamond(Edge + Inward * 17.f, 6.f, Color); }
		if (Threat.Count > 1)
		{
			const float Inset = Threat.bMajor ? 36.f : 21.f;
			Ink.Label(Edge + Inward * Inset - FVector2f(4.f, 8.f), FText::AsNumber(Threat.Count), Color, 12);
		}
	}

	void DrawRefillSymbol(const FHudInk& Ink, FVector2f Position, const FLinearColor& Color)
	{
		Ink.Stroke(Position, {{-4.f, -2.f}, {0.f, -7.f}, {4.f, -2.f}}, Color);
		Ink.Diamond(Position + FVector2f(0.f, 4.f), 3.5f, Color);
	}

	void DrawRefillWaypoint(const FHudInk& Ink, const FHudProjection& Projection, const APlayerController& Player,
		const FVector& Anchor)
	{
		const FVector Marker = Anchor + FVector::UpVector * RefillMarkerHeightCm;
		FVector2f Position;
		const bool bProjected = Projection.Project(Marker, Position);
		if (bProjected && Projection.Contains(Position, 16.f))
		{
			Ink.Box(Position - FVector2f(12.f, 12.f), {24.f, 24.f}, PanelColor);
			DrawRefillSymbol(Ink, Position, CloudColor);
			return;
		}
		DrawEdgeArrow(Ink, Projection, Player, Marker, bProjected ? &Position : nullptr, CloudColor);
	}

	void DrawPrompt(const FHudInk& Ink, FVector2f Position, ESacredLakePrompt Prompt)
	{
		const FLinearColor& Color = Prompt == ESacredLakePrompt::Plant ? SeedColor : CloudColor;
		if (Prompt == ESacredLakePrompt::Refill)
		{
			Ink.Box(Position - FVector2f(28.f, 14.f), {56.f, 28.f}, PanelColor);
			Ink.Label(Position + FVector2f(-20.f, -10.f), FText::FromString(TEXT("F")), Color, 14);
			DrawRefillSymbol(Ink, Position + FVector2f(13.f, 0.f), Color);
			return;
		}
		Ink.Box(Position - FVector2f(28.f, 14.f), {56.f, 28.f}, PanelColor);
		const FVector2f Key = Position - FVector2f(12.f, 0.f);
		const FVector2f Action = Position + FVector2f(13.f, 0.f);
		if (Prompt == ESacredLakePrompt::Focus || Prompt == ESacredLakePrompt::Plant)
		{
			Ink.Stroke(Key, {{-6.f, 5.f}, {-6.f, -5.f}, {-3.f, -8.f}, {3.f, -8.f}, {6.f, -5.f},
				{6.f, 5.f}, {3.f, 8.f}, {-3.f, 8.f}, {-6.f, 5.f}}, Color);
			Ink.Box(Key - FVector2f(4.f, 5.f), {3.f, 5.f}, Color);
			if (Prompt == ESacredLakePrompt::Plant) { DrawSeedling(Ink, Action, Color); }
			else
			{
				Ink.Diamond(Action, 6.f, Color);
				Ink.Box(Action - FVector2f(1.f, 1.f), {2.f, 2.f}, Color);
			}
			return;
		}

		Ink.Stroke(Key, {{-8.f, -8.f}, {8.f, -8.f}, {8.f, 8.f}, {-8.f, 8.f}, {-8.f, -8.f}}, Color);
		switch (Prompt)
		{
		case ESacredLakePrompt::Rain:
			Ink.Stroke(Key, {{-3.f, -3.f}, {3.f, -3.f}, {3.f, 3.f}, {-3.f, 3.f}, {-3.f, -3.f}}, Color, 1.2f);
			Ink.Stroke(Key, {{0.f, 1.f}, {4.f, 5.f}}, Color, 1.2f);
			Ink.Stroke(Action, {{-8.f, 0.f}, {-6.f, -4.f}, {-2.f, -4.f}, {0.f, -7.f}, {4.f, -7.f},
				{7.f, -3.f}, {8.f, 0.f}, {-8.f, 0.f}}, Color);
			Ink.Stroke(Action, {{-3.f, 3.f}, {-5.f, 7.f}}, Color);
			Ink.Stroke(Action, {{3.f, 3.f}, {1.f, 7.f}}, Color);
			break;
		default:
			break;
		}
	}
}

int32 USacredLakeWidget::PaintForestGuidance(const FGeometry& Geometry, FSlateWindowElementList& Draw, int32 Layer) const
{
	const APlayerController* Player = GetOwningPlayer();
	if (!Player || (!Guidance.bReticle && Guidance.Prompt == ESacredLakePrompt::None &&
		!Guidance.HasWorldAnchor()))
	{
		return Layer;
	}
	const TOptional<FHudProjection> Projection = MakeHudProjection(*Player, Geometry);
	if (!Projection.IsSet())
	{
		return Layer;
	}
	// 只在有效视图里绘制，并使用高于常驻面板的图层。
	const FHudInk Ink{Geometry, Draw, Layer + 1};
	if (Guidance.Prompt != ESacredLakePrompt::Plant && Guidance.FocusCenter.IsSet() && Guidance.FocusRadiusCm > 0.f)
	{
		DrawFocusRing(Ink, *Projection, Guidance.FocusCenter.GetValue(), Guidance.FocusRadiusCm,
			RainPreview.CopyWithNewOpacity(0.3f), true);
	}
	if (Guidance.QueuedFocusCenter.IsSet() && Guidance.QueuedFocusRadiusCm > 0.f)
	{
		DrawFocusRing(Ink, *Projection, *Guidance.QueuedFocusCenter, Guidance.QueuedFocusRadiusCm, SeedColor, true);
	}
	if (Guidance.ArrivedFocusCenter.IsSet() && Guidance.ArrivedFocusRadiusCm > 0.f)
	{
		DrawFocusRing(Ink, *Projection, *Guidance.ArrivedFocusCenter, Guidance.ArrivedFocusRadiusCm,
			RainPreview.CopyWithNewOpacity(0.85f), false);
	}
	if (Guidance.Plot.IsSet())
	{
		DrawPlot(Ink, *Projection, Guidance.Plot.GetValue(), Guidance.bPlantingValid);
	}
	for (const FSacredLakeGuidance::FThreat& Threat : Guidance.Threats)
	{
		DrawSpider(Ink, *Projection, *Player, Threat);
	}
	if (Guidance.RefillAnchor.IsSet() && Guidance.Prompt != ESacredLakePrompt::Refill)
	{
		DrawRefillWaypoint(Ink, *Projection, *Player, *Guidance.RefillAnchor);
	}
	if (Guidance.bReticle)
	{
		const FVector2f Center = Projection->Center();
		const FLinearColor& Color = Guidance.bPlanting ? (Guidance.bPlantingValid ? PlantValidColor : PlantInvalidColor)
			: (Guidance.FocusCenter.IsSet() ? RainPreview : ReticleIdle);
		Ink.Stroke(Center, {{-6.f, 0.f}, {-3.f, 0.f}}, Color, 1.25f);
		Ink.Stroke(Center, {{3.f, 0.f}, {6.f, 0.f}}, Color, 1.25f);
		Ink.Box(Center - FVector2f(1.f, 1.f), {2.f, 2.f}, Color);
	}
	if (Guidance.Prompt != ESacredLakePrompt::None)
	{
		const FVector2f ViewSize(Geometry.GetLocalSize());
		FVector2f Position(Projection->Center().X, FMath::Min(Projection->Max.Y - 22.f, ViewSize.Y - 128.f));
		FVector2f AnchorOnScreen;
		// 补云点进入安全画面范围后，把文字提示移到仪式石附近。
		if (Guidance.Prompt == ESacredLakePrompt::Refill && Guidance.RefillAnchor.IsSet() &&
			Projection->Project(*Guidance.RefillAnchor + FVector::UpVector * RefillMarkerHeightCm, AnchorOnScreen)
			&& Projection->Contains(AnchorOnScreen, 112.f))
		{
			Position = AnchorOnScreen;
		}
		DrawPrompt(Ink, Position, Guidance.Prompt);
	}
	return Ink.Layer;
}
