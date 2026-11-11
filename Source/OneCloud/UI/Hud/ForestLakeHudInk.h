#pragma once

#include "Brushes/SlateColorBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Clipping.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace OneCloud::ForestHud
{
	inline const FLinearColor PanelColor(0.018f, 0.044f, 0.044f, 0.87f);
	inline const FLinearColor RailColor(0.14f, 0.25f, 0.24f, 0.90f);
	inline const FLinearColor CloudColor(0.53f, 0.73f, 0.91f);
	inline const FLinearColor SeedColor(0.92f, 0.71f, 0.35f);
	inline const FLinearColor MistColor(0.53f, 0.68f, 0.65f);

	// 将局部坐标的形状写入 Slate 绘制列表。调用方决定顺序和图层。
	struct FHudInk
	{
		const FGeometry& Geometry;
		FSlateWindowElementList& Draw;
		int32 Layer;

		void Box(FVector2f Position, FVector2f Size, const FLinearColor& Color) const
		{
			if (Size.X > 0.f && Size.Y > 0.f)
			{
				// 绘制列表之后才消费画刷，使用静态对象保留生命周期，也避免依赖全局样式。
				static const FSlateColorBrush SolidBrush(FLinearColor::White);
				FSlateDrawElement::MakeBox(Draw, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
					&SolidBrush, ESlateDrawEffect::None, Color);
			}
		}

		// 图标由相对原点的点组成，同一形状可在不同局部坐标重复绘制。
		void Stroke(FVector2f Origin, TArray<FVector2f> Points, const FLinearColor& Color, float Width = 1.5f) const
		{
			for (FVector2f& Point : Points)
			{
				Point += Origin;
			}
			FSlateDrawElement::MakeLines(Draw, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points),
				ESlateDrawEffect::None, Color, true, Width);
		}

		void Diamond(FVector2f Center, float Radius, const FLinearColor& Color) const
		{
			Stroke(Center, {{0.f, -Radius}, {Radius, 0.f}, {0.f, Radius}, {-Radius, 0.f}, {0.f, -Radius}}, Color);
		}

		void Label(FVector2f Position, const FText& Text, const FLinearColor& Color, int32 FontSize = 14) const
		{
			const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FontSize);
			FSlateDrawElement::MakeText(Draw, Layer, Geometry.ToPaintGeometry(FVector2f(225.f, 24.f),
				FSlateLayoutTransform(Position)), Text, Font, ESlateDrawEffect::None, Color);
		}

		// 教学和失败提示长度不同。先按真实字体宽度缩至可读的小字号，再用面板边界裁切。
		// 不按字符数估算，避免中文、数字和按键名称混排时超出背景。
		void FittedLabel(FVector2f Position, FVector2f Size, const FText& Text, const FLinearColor& Color) const
		{
			if (Text.IsEmpty() || Size.X <= 0.f || Size.Y <= 0.f)
			{
				return;
			}
			FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14);
			FVector2D Measured(0.0, 18.0);
			if (FSlateApplication::IsInitialized())
			{
				const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				Measured = Measure->Measure(Text, Font);
				if (Measured.X > Size.X)
				{
					Font.Size = FMath::Clamp(FMath::FloorToInt(14.f * Size.X / Measured.X), 11, 14);
					Measured = Measure->Measure(Text, Font);
				}
			}
			Draw.PushClip(FSlateClippingZone(Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position))));
			const FVector2f TextPosition = Position + FVector2f(0.f, FMath::Max(0.f, (Size.Y - static_cast<float>(Measured.Y)) * 0.5f));
			FSlateDrawElement::MakeText(Draw, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(TextPosition)),
				Text, Font, ESlateDrawEffect::None, Color);
			Draw.PopClip();
		}
	};
}
