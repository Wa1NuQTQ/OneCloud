#include "SacredLakePrototype.h"

#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionShape.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Weather/OneCloudRain.h"

namespace
{
	constexpr float PlantingGrowthSeconds = 12.f;
	constexpr double MatureSaplingHeightCm = 430.0;
	constexpr double SeedlingScale = 0.35;
	constexpr double TreeSelectionRadiusCm = 220.0;
	constexpr double TreeSpacingCm = 450.0;
	constexpr double HighestWaterClearanceCm = 30.0;
	constexpr float PlantingObstacleRadiusCm = 80.f;
	constexpr float PlantingObstacleHalfHeightCm = 160.f;
}

// 种植、成长和种子奖励

// 保留旧蓝图入口。新的 E 键预览和鼠标确认也使用相同的位置校验，不再寻找预设树位。
bool ASacredLakePrototype::TryPlantAtClosestPlot()
{
	FVector Target;
	if (!ResolvePlantingAim(Target))
	{
		ShowForestFeedback(NSLOCTEXT("ForestFeedback", "AimPlantGround", "瞄准岛上的地面或已种下的树根"));
		return false;
	}
	return TryPlantAtLocation(Target);
}

// 确认点击后才结算和扣费；生成失败、位置不合适或成长中的树都保留原来的水量和种子。
bool ASacredLakePrototype::TryPlantAtLocation(FVector GroundTarget)
{
	if (!CanActInForest()) { return false; }
	SettleForestResources(GetWorld()->GetTimeSeconds());
	int32 PlotIndex;
	FVector PlantGround;
	FText BlockReason;
	if (!ResolveForestPlant(GroundTarget, PlotIndex, PlantGround, BlockReason))
	{
		ShowForestFeedback(BlockReason);
		return false;
	}
	FForestPlot& Plot = ForestPlots[PlotIndex];
	AActor* Tree = Plot.Tree.Get();
	if (!Plot.bPlanted)
	{
		const FTransform Transform(FRotator::ZeroRotator, PlantGround);
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (PlantedTreeClass)
		{
			Tree = GetWorld()->SpawnActor<AActor>(PlantedTreeClass, Transform, Params);
		}
		else if (PlantedTreeMesh)
		{
			AStaticMeshActor* Proxy = GetWorld()->SpawnActor<AStaticMeshActor>(
				Transform.GetLocation(), Transform.Rotator(), Params);
			if (Proxy)
			{
				UStaticMeshComponent* Mesh = Proxy->GetStaticMeshComponent();
				Mesh->SetMobility(EComponentMobility::Movable);
				Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Mesh->SetCanEverAffectNavigation(false);
				Mesh->SetStaticMesh(PlantedTreeMesh);
				const FBox Bounds = PlantedTreeMesh->GetBoundingBox();
				Proxy->SetActorScale3D(FVector(MatureSaplingHeightCm / FMath::Max(1.0, Bounds.GetSize().Z)));
				Tree = Proxy;
			}
		}
		if (!IsValid(Tree))
		{
			ShowForestFeedback(NSLOCTEXT("ForestFeedback", "PlantSpawnFailed", "树苗未能生成 · 本次未消耗种子和湖水"));
			return false;
		}
		Plot.Tree = Tree;
		Plot.Ground = PlantGround;
		Plot.MatureScale = Tree->GetActorScale3D();
		// 专用种植模型的原点就在树根。只改变缩放，避免偏移枢轴或整棵 Actor 包围盒让树根滑动。
		Tree->SetActorScale3D(Plot.MatureScale * SeedlingScale);
	}
	else
	{
		// 二、三级扩大现有树，不生成叠在一起的新 Actor；缩放始终围绕树根。
		const double PreviousSize = 1.0 + 0.45 * (Plot.Level - 1);
		const double NextSize = 1.0 + 0.45 * Plot.Level;
		const double ScaleRatio = NextSize / PreviousSize;
		Plot.MatureScale = Tree->GetActorScale3D() * ScaleRatio;
	}

	Resources.Plant();
	Plot.GrowthStartScale = Tree->GetActorScale3D();
	Plot.GrowthStartedSeconds = GetWorld()->GetTimeSeconds();
	Plot.MatureAtSeconds = Plot.GrowthStartedSeconds + PlantingGrowthSeconds;
	Plot.bPlanted = true;
	Plot.bMature = false;
	ShowForestFeedback(NSLOCTEXT("ForestFeedback", "TreePlanted", "开始扎根 · 12 秒后提升回水、云容量和雨量"), false);
	UpdateWater();
	UpdateHud();
	return true;
}

// 先认准玩家点中的现有树，再检查新位置。失败时不会偷偷改成另一棵树或附近空位。
bool ASacredLakePrototype::ResolveForestPlant(FVector GroundTarget, int32& PlotIndex,
	FVector& PlantGround, FText& BlockReason) const
{
	PlotIndex = INDEX_NONE;
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!CanActInForest() || !IsValid(Pawn) || GroundTarget.ContainsNaN() || !TraceGround(GroundTarget, PlantGround))
	{
		BlockReason = NSLOCTEXT("ForestFeedback", "AimPlantGround", "瞄准岛上的地面或已种下的树根");
		return false;
	}
	double ClosestDistanceSquared = FMath::Square(TreeSelectionRadiusCm);
	for (int32 Index = 0; Index < ForestPlots.Num(); ++Index)
	{
		const FForestPlot& Plot = ForestPlots[Index];
		const double DistanceSquared = FVector::DistSquared2D(PlantGround, Plot.Ground);
		if (Plot.bPlanted && DistanceSquared <= ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			PlotIndex = Index;
		}
	}
	if (PlotIndex != INDEX_NONE)
	{
		const FForestPlot& Plot = ForestPlots[PlotIndex];
		PlantGround = Plot.Ground;
		if (!Plot.Tree.IsValid() || !Plot.bMature)
		{
			BlockReason = NSLOCTEXT("ForestFeedback", "TreeGrowing", "这棵树正在扎根 · 成长结束后再培育");
			return false;
		}
		if (Plot.Level >= FSacredLakeResources::MaxTreeLevel)
		{
			BlockReason = NSLOCTEXT("ForestFeedback", "TreeFullyGrown", "这棵树已经长成 · 请照料其他树木");
			return false;
		}
	}
	if (FVector::DistSquared(Pawn->GetActorLocation(), PlantGround) > FMath::Square(PlantingRangeCm))
	{
		BlockReason = NSLOCTEXT("ForestFeedback", "PlantTooFar", "种植位置太远 · 请靠近湖心岛");
		return false;
	}
	if (!Resources.CanPlant())
	{
		BlockReason = Resources.Seeds <= 0
			? NSLOCTEXT("ForestFeedback", "NoSeeds", "种子不足 · 击败六只普通怪或一只精英可获得种子")
			: NSLOCTEXT("ForestFeedback", "PlantLakeLow", "湖水不足 · 种树需要 5 点，另保留 10 点圣湖水");
		return false;
	}
	if (PlotIndex != INDEX_NONE) { return true; }

	const FVector2D IslandOffset(PlantGround.X - PlantingIslandCenterXY.X, PlantGround.Y - PlantingIslandCenterXY.Y);
	if (PlantingIslandHalfExtentCm.X <= 0.0 || PlantingIslandHalfExtentCm.Y <= 0.0
		|| FMath::Square(IslandOffset.X / PlantingIslandHalfExtentCm.X)
			+ FMath::Square(IslandOffset.Y / PlantingIslandHalfExtentCm.Y) > 1.0
		|| PlantGround.Z < WaterOrigin.Z + MaximumWaterZOffset + HighestWaterClearanceCm)
	{
		BlockReason = NSLOCTEXT("ForestFeedback", "PlantOnIsland", "选择湖心岛上较高的地面 · 涨水时树根也需保持干燥");
		return false;
	}
	for (const FForestPlot& Plot : ForestPlots)
	{
		if (Plot.bPlanted && FVector::DistSquared2D(PlantGround, Plot.Ground) < FMath::Square(TreeSpacingCm))
		{
			BlockReason = NSLOCTEXT("ForestFeedback", "PlantTooClose", "树木之间留出空间 · 想培育现有树，请瞄准它的树根");
			return false;
		}
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestPlantingSpace), false, this);
	Params.AddIgnoredActor(Pawn);
	Params.AddIgnoredActor(GroundActor);
	Params.AddIgnoredActor(WaterSurface);
	const FVector ClearanceCenter = PlantGround + FVector::UpVector * (PlantingObstacleHalfHeightCm + 10.f);
	if (GetWorld()->OverlapBlockingTestByChannel(ClearanceCenter, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(PlantingObstacleRadiusCm, PlantingObstacleHalfHeightCm), Params))
	{
		BlockReason = NSLOCTEXT("ForestFeedback", "PlantBlocked", "这里有树干或岩石 · 选择旁边空出的地面");
		return false;
	}
	PlotIndex = ForestPlots.IndexOfByPredicate([](const FForestPlot& Plot) { return !Plot.bPlanted; });
	if (PlotIndex == INDEX_NONE)
	{
		BlockReason = NSLOCTEXT("ForestFeedback", "PlantLimit", "六棵树已经种齐 · 瞄准已种树根继续培育");
		return false;
	}
	if (!PlantedTreeClass && !PlantedTreeMesh)
	{
		BlockReason = NSLOCTEXT("ForestFeedback", "PlantAssetMissing", "本关缺少种植树苗资源 · 本次未消耗种子和湖水");
		return false;
	}
	return true;
}

bool ASacredLakePrototype::CanPlantAtLocation(FVector GroundTarget, FVector& PlantGround) const
{
	int32 PlotIndex;
	FText BlockReason;
	return ResolveForestPlant(GroundTarget, PlotIndex, PlantGround, BlockReason);
}

void ASacredLakePrototype::UpdateForestPlots(double Now)
{
	bool bGrowthChanged = false;
	for (FForestPlot& Plot : ForestPlots)
	{
		if (!Plot.bPlanted || Plot.bMature) { continue; }
		if (AActor* Tree = Plot.Tree.Get())
		{
			const double Alpha = FMath::Clamp((Now - Plot.GrowthStartedSeconds) / PlantingGrowthSeconds, 0.0, 1.0);
			const double SmoothAlpha = Alpha * Alpha * (3.0 - 2.0 * Alpha);
			Tree->SetActorScale3D(FMath::Lerp(Plot.GrowthStartScale, Plot.MatureScale, SmoothAlpha));
			if (Now >= Plot.MatureAtSeconds)
			{
				++Plot.Level;
				Plot.bMature = true;
				bGrowthChanged = true;
			}
		}
	}
	if (bGrowthChanged)
	{
		RefreshForestGrowth();
		ShowForestFeedback(NSLOCTEXT("ForestFeedback", "TreeMature", "树木已扎根 · 圣湖恢复、储量与雨量提升"), false);
	}
}

int32 ASacredLakePrototype::GetForestPlotLevel(int32 PlotIndex) const
{
	return ForestPlots.IsValidIndex(PlotIndex) ? ForestPlots[PlotIndex].Level : 0;
}

bool ASacredLakePrototype::GetForestPlotIsGrowing(int32 PlotIndex) const
{
	return ForestPlots.IsValidIndex(PlotIndex) && ForestPlots[PlotIndex].bPlanted && !ForestPlots[PlotIndex].bMature;
}

bool ASacredLakePrototype::GetForestTreeGround(int32 TreeIndex, FVector& Ground) const
{
	if (!ForestPlots.IsValidIndex(TreeIndex) || !ForestPlots[TreeIndex].bPlanted) { return false; }
	Ground = ForestPlots[TreeIndex].Ground;
	return true;
}

// 库存不超过尚未支付的成长次数；成长中的本级已经支付，不能再为它发种子。
void ASacredLakePrototype::AwardForestSeed()
{
	int32 RemainingGrowth = 0;
	for (const FForestPlot& Plot : ForestPlots)
	{
		RemainingGrowth += FSacredLakeResources::MaxTreeLevel - Plot.Level - (Plot.bPlanted && !Plot.bMature ? 1 : 0);
	}
	if (Resources.Seeds < RemainingGrowth)
	{
		++Resources.Seeds;
		ShowForestFeedback(NSLOCTEXT("ForestFeedback", "SeedEarned", "获得种子 · 可以种树或提升现有树的等级"), false);
	}
}

// 仪式点补水与技能支出

// 区分 F 正在按住和玩家已进入补水范围。按住时监听角色根组件位移，松开时解除监听。
void ASacredLakePrototype::SetRefillingCloud(bool bEnabled)
{
	if (bEnabled == bCloudRefillHeld || (bEnabled && !CanActInForest()))
	{
		return;
	}
	SettleForestResources(GetWorld()->GetTimeSeconds());
	if (bEnabled)
	{
		bCloudRefillHeld = true;
		bCloudRefillInRange = IsNearCloudRefillAnchor();
		if (!bCloudRefillInRange)
		{
			ShowForestFeedback(NSLOCTEXT("ForestFeedback", "RefillTooFar", "靠近岛西仪式石，再按住 F · 高度也会影响距离"));
		}
		else if (!Resources.CanRefillCloud())
		{
			ShowForestFeedback(Resources.CloudWater >= Resources.CloudCapacity
				? NSLOCTEXT("ForestFeedback", "CloudFull", "云已储满水")
				: NSLOCTEXT("ForestFeedback", "LakeReserve", "圣湖只剩保留水量 · 等待树木恢复"));
		}
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			if (USceneComponent* Root = Pawn->GetRootComponent())
			{
				CloudRefillPawnRoot = Root;
				Root->TransformUpdated.AddWeakLambda(this,
					[this](USceneComponent*, EUpdateTransformFlags, ETeleportType) { UpdateCloudRefillProximity(); });
			}
		}
	}
	else
	{
		StopCloudRefillHold();
	}
	RefreshForestRainBudget();
	UpdateHud();
}

// 补水范围使用三维球形距离（厘米），玩家飞得过高也会离开范围。
bool ASacredLakePrototype::IsNearCloudRefillAnchor() const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	return IsValid(CloudRefillAnchor) && IsValid(Pawn) &&
		FVector::DistSquared(Pawn->GetActorLocation(), CloudRefillAnchor->GetActorLocation()) <=
		FMath::Square(CloudRefillRadiusCm);
}

// 进出范围时先用旧的范围状态结算到此刻，再切换补水状态。
void ASacredLakePrototype::UpdateCloudRefillProximity()
{
	const bool bInRange = bCloudRefillHeld && IsNearCloudRefillAnchor();
	if (bInRange != bCloudRefillInRange)
	{
		SettleForestResources(GetWorld()->GetTimeSeconds());
		bCloudRefillInRange = bInRange;
	}
}

void ASacredLakePrototype::StopCloudRefillHold()
{
	if (USceneComponent* Root = CloudRefillPawnRoot.Get())
	{
		Root->TransformUpdated.RemoveAll(this);
	}
	CloudRefillPawnRoot.Reset();
	bCloudRefillHeld = false;
	bCloudRefillInRange = false;
}

// 主动技能扣费入口。先结清持续收支，扣费后重新计算什么时候会因云水耗尽停雨。
bool ASacredLakePrototype::TrySpendCloudWater(float Amount)
{
	if (!CanActInForest())
	{
		return false;
	}
	SettleForestResources(GetWorld()->GetTimeSeconds());
	const bool bSpent = Resources.SpendCloud(Amount);
	RefreshForestRainBudget();
	return bSpent;
}
