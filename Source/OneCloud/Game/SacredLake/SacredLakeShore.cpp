#include "SacredLakePrototype.h"

#include "Enemies/OneCloudEnemy.h"
#include "Engine/World.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

namespace
{
	constexpr float ShoreClearanceCm = 140.f;
	constexpr float ExtractionReachCm = 430.f;
	constexpr float MinimumDryClearanceCm = 3.f;
}

// 取水岸位的入口与各层空间查询

// 从敌人所在方向沿多条射线向湖心寻找水线。候选岸位要有导航投影、未被占用并能全程走干地。
bool ASacredLakePrototype::FindShoreGoal(AOneCloudEnemy& Enemy, const FVector& Start, FVector& Goal) const
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation)
	{
		return false;
	}

	const FVector Outward = (Start - WaterOrigin).GetSafeNormal2D();
	const float Angles[] = {0.f, 12.f, -12.f, 24.f, -24.f, 42.f, -42.f, 60.f, -60.f, 90.f, -90.f};
	for (float Angle : Angles)
	{
		const FVector Direction = Outward.RotateAngleAxis(Angle, FVector::UpVector);
		for (float Distance = ShoreSearchRadius; Distance >= 450.f; Distance -= 100.f)
		{
			FVector Ground;
			if (!TraceGround(WaterOrigin + Direction * Distance, Ground) || !IsWet(Ground))
			{
				continue;
			}

			FVector Shore;
			FNavLocation Projected;
			FVector WaterPoint;
			if (TraceGround(Ground + Direction * ShoreClearanceCm, Shore)
				&& Navigation->ProjectPointToNavigation(Shore, Projected, FVector(90.0, 90.0, 180.0),
					&Enemy.GetNavAgentPropertiesRef())
				&& FindNearbyWater(Projected.Location, WaterPoint)
				&& !IsShoreOccupied(Enemy, Projected.Location))
			{
				const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
					GetWorld(), Start, Projected.Location, &Enemy);
				if (Path && Path->IsValid() && !Path->IsPartial() && IsDryPath(Path->PathPoints))
				{
					Goal = Projected.Location;
					return true;
				}
			}
			break;
		}
	}
	return false;
}

// 从实际站位向湖心步进，找取水半径内的第一个湿点，返回水面高度上的世界坐标。
bool ASacredLakePrototype::FindNearbyWater(const FVector& Position, FVector& WaterPoint) const
{
	FVector Ground;
	if (!TraceGround(Position, Ground) || IsWet(Ground))
	{
		return false;
	}

	const FVector Direction = (WaterOrigin - Position).GetSafeNormal2D();
	for (float Distance = 45.f; Distance <= ExtractionReachCm; Distance += 45.f)
	{
		if (TraceGround(Position + Direction * Distance, Ground) && IsWet(Ground))
		{
			WaterPoint = FVector(Ground.X, Ground.Y, WaterHeightCm);
			return true;
		}
	}
	return false;
}

// 导航路径可能经过被水淹没的地形。沿每段路线约每 120 厘米复核地面和水位。
bool ASacredLakePrototype::IsDryPath(const TArray<FVector>& Points) const
{
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		const FVector& Start = Points[Index - 1];
		const FVector& End = Points[Index];
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(Start, End) / 120.f));
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			FVector Ground;
			if (!TraceGround(FMath::Lerp(Start, End, static_cast<float>(Step) / Steps), Ground) || IsWet(Ground))
			{
				return false;
			}
		}
	}
	return true;
}

// 用 XY 发竖直射线测地形高度。配置专用地形时只查该 Actor，避免把场景装饰当成地面。
bool ASacredLakePrototype::TraceGround(const FVector& XY, FVector& Ground) const
{
	const FVector Start(XY.X, XY.Y, WaterOrigin.Z + 5000.0);
	const FVector End(XY.X, XY.Y, WaterOrigin.Z - 8000.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SacredLakeGround), true, this);
	Params.AddIgnoredActor(WaterSurface);
	FHitResult Hit;
	const bool bHit = IsValid(GroundActor)
		? GroundActor->ActorLineTraceSingle(Hit, Start, End, ECC_Visibility, Params)
		: GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	if (!bHit)
	{
		return false;
	}
	Ground = Hit.ImpactPoint;
	return true;
}

// 先确认坐标位于湖盆的水平范围，再比较世界高度与水位，给干地留出少量净空。
bool ASacredLakePrototype::IsWet(const FVector& Ground) const
{
	const FVector Offset = Ground - WaterOrigin;
	return FMath::Abs(Offset.X) <= WaterFootprint.X && FMath::Abs(Offset.Y) <= WaterFootprint.Y
		&& Ground.Z <= WaterHeightCm + MinimumDryClearanceCm;
}
