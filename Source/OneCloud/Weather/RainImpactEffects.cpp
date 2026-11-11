#include "RainImpactEffects.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 SprayCapacity = 96;
	constexpr int32 RippleCapacity = 24;
	constexpr int32 WetSandCapacity = 64;
	constexpr int32 EtchCapacity = 16;
	constexpr double ViewDistanceCm = 3400.0;
	constexpr double WetSandCellCm = 160.0;
	constexpr float WetSandLifetimeSeconds = 6.f;
	constexpr float EtchLifetimeSeconds = 1.4f;
	// 以 ERainImpactSurface 的枚举值索引，控制各种表面的显示频率。
	constexpr double SurfaceIntervalSeconds[] = {0.0, 0.12, 0.068, 0.085, 0.055, 0.095, 0.12};
	static_assert(UE_ARRAY_COUNT(SurfaceIntervalSeconds) == static_cast<int32>(ERainImpactSurface::Count));

	struct FSprayStyle
	{
		int32 DropletCount;
		float SpeedCmPerSecond;
		float SizeScale;
		float MaterialKind;
	};

	FSprayStyle SprayStyleFor(ERainImpactSurface Surface)
	{
		switch (Surface)
		{
		case ERainImpactSurface::Leaf:  return {3, 60.f, 0.75f, 0.f};
		case ERainImpactSurface::Metal: return {5, 190.f, 1.f, 0.f};
		case ERainImpactSurface::Flesh: return {5, 104.f, 1.f, 1.f};
		default:                        return {4, 108.f, 1.f, 0.f}; // Water
		}
	}

	// 网格实例池逐步填满，随后循环覆盖。Custom Data 把动画所需数据传给材质。
	void WriteInstance(UInstancedStaticMeshComponent& Mesh, int32& Cursor, int32 Capacity,
		const FTransform& Transform, TConstArrayView<float> Data, bool bCommit)
	{
		if (Cursor < Mesh.GetInstanceCount())
		{
			Mesh.UpdateInstanceTransform(Cursor, Transform, true, false, true);
		}
		else
		{
			Mesh.AddInstance(Transform, true);
		}
		Mesh.SetCustomData(Cursor, Data, bCommit);
		Cursor = (Cursor + 1) % Capacity;
	}

	FVector LeafSprayOrigin(const FHitResult& Hit)
	{
		const UInstancedSkinnedMeshComponent* Trees = Cast<UInstancedSkinnedMeshComponent>(Hit.GetComponent());
		FTransform TreeTransform;
		if (!Trees || !Trees->GetSkinnedAsset() || Hit.Item < 0 || Hit.Item >= Trees->GetInstanceCount() ||
			!Trees->GetInstanceTransform(Trees->GetInstanceId(Hit.Item), TreeTransform, true))
		{
			return Hit.ImpactPoint - FVector(0.0, 0.0, 24.0);
		}

		// 树冠查询体比稀疏的外叶更宽。把水花向树冠内部移动，避免漂在叶片外。
		const FBoxSphereBounds Crown = Trees->GetSkinnedAsset()->GetBounds().TransformBy(TreeTransform);
		const FVector TowardCenter = Crown.Origin - Hit.ImpactPoint;
		const double CrownHeight = FMath::Min(Hit.ImpactPoint.Z - 24.0, Crown.Origin.Z + Crown.BoxExtent.Z * 0.53);
		return FVector(Hit.ImpactPoint.X + TowardCenter.X * 0.58,
			Hit.ImpactPoint.Y + TowardCenter.Y * 0.58, CrownHeight);
	}

	// 查询可能打到敌人的胶囊。显示前尽量修正到骨骼网格或物理资产表面。
	FHitResult ResolveEnemyVisualHit(const FHitResult& Hit, USkeletalMeshComponent& Mesh)
	{
		FHitResult SkinHit;
		const FVector ProbeStart = Hit.ImpactPoint + Hit.ImpactNormal * 80.0;
		if (Mesh.LineTraceComponent(SkinHit, ProbeStart, Hit.ImpactPoint - Hit.ImpactNormal * 250.0,
			FCollisionQueryParams()))
		{
			return SkinHit;
		}
		FClosestPointOnPhysicsAsset Closest;
		SkinHit = Hit;
		if (Mesh.GetClosestPointOnPhysicsAsset(ProbeStart, Closest, false) && Closest.Distance >= 0.f &&
			Closest.Distance <= 330.f && !Closest.Normal.IsNearlyZero())
		{
			SkinHit.ImpactPoint = Closest.ClosestWorldPosition;
			SkinHit.ImpactNormal = Closest.Normal;
			SkinHit.BoneName = Closest.BoneName;
		}
		else
		{
			const FBoxSphereBounds& Bounds = Mesh.Bounds;
			const FVector ApproximateSurface = Bounds.Origin + Hit.ImpactNormal * Bounds.BoxExtent * 0.65;
			SkinHit.ImpactPoint = Bounds.GetBox().GetClosestPointTo(ApproximateSurface);
		}
		return SkinHit;
	}
}

URainImpactEffects::URainImpactEffects()
{
	bAutoActivate = true;
}

// 回传批次内共用相机信息。水花面朝镜头，水纹仍沿实际表面。
void URainImpactEffects::BeginImpactBatch()
{
	const APlayerCameraManager* Camera = IsActive() ? UGameplayStatics::GetPlayerCameraManager(this, 0) : nullptr;
	bHasView = Camera != nullptr;
	if (Camera)
	{
		BatchTime = GetWorld()->GetTimeSeconds();
		ViewLocation = Camera->GetCameraLocation();
		const FRotationMatrix ViewAxes(Camera->GetCameraRotation());
		ViewForward = ViewAxes.GetUnitAxis(EAxis::X);
		SprayFacing = FRotationMatrix::MakeFromZY(-ViewForward, ViewAxes.GetUnitAxis(EAxis::Z)).ToQuat();
	}
}

// 仅给近处、镜头前方的落点发视觉反馈。跳过这里仍可以播放落点声音。
void URainImpactEffects::PresentImpact(ERainImpactSurface Surface, const FHitResult& Hit)
{
	if (!bHasView || Surface == ERainImpactSurface::Unknown)
	{
		return;
	}
	const int32 Index = static_cast<int32>(Surface);
	if (BatchTime < NextSurfaceImpact[Index])
	{
		return;
	}
	const FVector ToHit = Hit.ImpactPoint - ViewLocation;
	if (ToHit.SizeSquared() > FMath::Square(ViewDistanceCm) || FVector::DotProduct(ToHit, ViewForward) < -60.0)
	{
		return;
	}
	NextSurfaceImpact[Index] = BatchTime + SurfaceIntervalSeconds[Index];

	if (Surface == ERainImpactSurface::Sand || Surface == ERainImpactSurface::Soil)
	{
		PresentWetSand(Hit);
		return;
	}
	if (Surface == ERainImpactSurface::Water)
	{
		PresentRipple(Hit.ImpactPoint, Hit.ImpactNormal);
		PresentSpray(Surface, Hit.ImpactPoint, Hit.ImpactNormal);
		return;
	}
	if (Surface == ERainImpactSurface::Leaf)
	{
		PresentSpray(Surface, LeafSprayOrigin(Hit), Hit.ImpactNormal);
		return;
	}
	AActor* Target = Hit.GetActor();
	USkeletalMeshComponent* Skin = Target ? Target->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	const FHitResult VisualHit = Skin ? ResolveEnemyVisualHit(Hit, *Skin) : Hit;
	if (Surface == ERainImpactSurface::Flesh && Skin)
	{
		PresentEtch(VisualHit, *Skin);
	}
	PresentSpray(Surface, VisualHit.ImpactPoint, VisualHit.ImpactNormal);
}

// 材质用开始时间、寿命、初速度和类型计算水花的位移与淡出。
void URainImpactEffects::PresentSpray(ERainImpactSurface Surface, FVector Position, FVector Normal)
{
	if (!SprayInstances)
	{
		SprayInstances = CreateInstances(SprayMaterial, 6);
	}
	if (!SprayInstances)
	{
		return;
	}
	const FSprayStyle Style = SprayStyleFor(Surface);
	for (int32 Index = 0; Index < Style.DropletCount; ++Index)
	{
		const FVector Direction = FMath::VRandCone(Normal, 1.10f);
		const FVector Velocity = Direction * FMath::FRandRange(Style.SpeedCmPerSecond * 0.6f, Style.SpeedCmPerSecond);
		const float Width = FMath::FRandRange(0.024f, 0.042f) * Style.SizeScale;
		// 一组水花写完才提交渲染更新，避免每个实例都请求一次刷新。
		const float Data[] = {static_cast<float>(BatchTime), FMath::FRandRange(0.28f, 0.45f),
			static_cast<float>(Velocity.X), static_cast<float>(Velocity.Y), static_cast<float>(Velocity.Z), Style.MaterialKind};
		WriteInstance(*SprayInstances, NextSpray, SprayCapacity,
			FTransform(SprayFacing, Position + Normal * 5.0 + Direction * 4.0, FVector(Width, Width * 1.65f, 1.0)), Data,
			Index == Style.DropletCount - 1);
	}
}

// 涟漪沿命中法线铺平，用材质时间扩散。少量抬高以避免和水面重叠闪烁。
void URainImpactEffects::PresentRipple(FVector Position, FVector Normal)
{
	if (!RippleInstances)
	{
		RippleInstances = CreateInstances(RippleMaterial, 2);
	}
	if (RippleInstances)
	{
		const float Data[] = {static_cast<float>(BatchTime), FMath::FRandRange(0.75f, 1.1f)};
		const float Diameter = FMath::FRandRange(0.73f, 1.19f);
		WriteInstance(*RippleInstances, NextRipple, RippleCapacity,
			FTransform(FRotationMatrix::MakeFromZ(Normal).ToQuat(), Position + Normal * 2.0,
				FVector(Diameter, Diameter, 1.0)), Data, true);
	}
}

// 把泥土和沙地划成固定大小的格，同一区域连续滴落会延续湿印。
void URainImpactEffects::PresentWetSand(const FHitResult& Hit)
{
	if (!WetSandMaterial)
	{
		return;
	}
	if (WetSandSlots.IsEmpty())
	{
		WetSandSlots.SetNum(WetSandCapacity);
	}
	const FIntPoint Cell(FMath::FloorToInt(Hit.ImpactPoint.X / WetSandCellCm),
		FMath::FloorToInt(Hit.ImpactPoint.Y / WetSandCellCm));
	for (FDecalSlot& Slot : WetSandSlots)
	{
		if (Slot.SandCell == Cell && Slot.ExpiresAt > BatchTime && Slot.Component.IsValid())
		{
			// 重置淡出起点，沿用现有贴花的位置和大小。
			if (UMaterialInstanceDynamic* Instance = Cast<UMaterialInstanceDynamic>(Slot.Component->GetDecalMaterial()))
			{
				Instance->SetScalarParameterValue(TEXT("ImpactStartSeconds"), BatchTime);
			}
			Slot.ExpiresAt = BatchTime + WetSandLifetimeSeconds;
			return;
		}
	}
	FDecalSlot& Slot = WetSandSlots[NextWetSand];
	NextWetSand = (NextWetSand + 1) % WetSandCapacity;
	Slot.SandCell = Cell;
	const float Width = FMath::FRandRange(48.f, 86.f);
	PrepareDecal(Slot, WetSandMaterial, Hit.ImpactPoint, Hit.ImpactNormal, FVector(12, Width, Width),
		WetSandLifetimeSeconds);
}

void URainImpactEffects::PresentEtch(const FHitResult& Hit, USkeletalMeshComponent& Skin)
{
	if (!EtchMaterial)
	{
		return;
	}
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const FVector Position = Hit.ImpactPoint;
	constexpr double RadiusCm = 27.0;
	double DepthCm = 60.0;
	if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Skin.GetOwner()->GetRootComponent()))
	{
		// 物理体可能比可见表面略宽，需要一定投影深度。
		// 接近地面时缩短投影或放弃绘制，避免腐蚀痕出现在沙地上。
		const double ClearanceCm = Position.Z - Capsule->Bounds.GetBox().Min.Z - 18.0;
		const double LateralDropCm = RadiusCm * FMath::Sqrt(2.0 * FMath::Max(0.0, 1.0 - FMath::Square(Normal.Z)));
		DepthCm = FMath::Min(DepthCm, (ClearanceCm - LateralDropCm) / FMath::Max(0.15, FMath::Abs(Normal.Z)));
		if (DepthCm < 8.0)
		{
			return;
		}
	}
	if (EtchSlots.IsEmpty())
	{
		EtchSlots.SetNum(EtchCapacity);
	}
	FDecalSlot& Slot = EtchSlots[NextEtch];
	NextEtch = (NextEtch + 1) % EtchCapacity;
	UDecalComponent* Decal = PrepareDecal(Slot, EtchMaterial, Position, Normal, FVector(DepthCm, RadiusCm, RadiusCm),
		EtchLifetimeSeconds);
	// 贴花附在命中的骨骼上，使腐蚀痕跟随怪物动作。
	const FName Bone = Hit.BoneName.IsNone() ? Skin.FindClosestBone(Position) : Hit.BoneName;
	Decal->AttachToComponent(&Skin, FAttachmentTransformRules::KeepWorldTransform, Bone);
}

// 复用底层组件

// 水花与涟漪只负责画面。关闭碰撞、导航和阴影，减少附带的场景工作。
UInstancedStaticMeshComponent* URainImpactEffects::CreateInstances(UMaterialInterface* Material, int32 CustomFloats)
{
	if (!ImpactPlane || !Material)
	{
		return nullptr;
	}
	UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(GetOwner());
	GetOwner()->AddInstanceComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(ImpactPlane);
	Mesh->SetMaterial(0, Material);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);
	Mesh->SetVisibleInRayTracing(false);
	Mesh->SetBoundsScale(1.4f);
	Mesh->SetNumCustomDataFloats(CustomFloats);
	Mesh->RegisterComponent();
	return Mesh;
}

// 复用贴花前解除上次的骨骼绑定，材质时间和回收时间一起重置。
UDecalComponent* URainImpactEffects::PrepareDecal(FDecalSlot& Slot, UMaterialInterface* Material,
	FVector Position, FVector Normal, FVector SizeCm, float LifetimeSeconds)
{
	UDecalComponent* Decal = Slot.Component.Get();
	if (!Decal)
	{
		Decal = NewObject<UDecalComponent>(GetOwner());
		GetOwner()->AddInstanceComponent(Decal);
		Decal->SetMobility(EComponentMobility::Movable);
		Decal->DecalSize = FVector::OneVector;
		Decal->SetDecalMaterial(Material);
		Decal->SetFadeScreenSize(0.0007f);
		Decal->RegisterComponent();
		Slot.Component = Decal;
	}
	Decal->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Decal->SetWorldTransform(FTransform(FRotationMatrix::MakeFromX(-Normal).ToQuat(), Position, SizeCm));
	Decal->SetVisibility(true);
	UMaterialInstanceDynamic* Instance = Cast<UMaterialInstanceDynamic>(Decal->GetDecalMaterial());
	if (!Instance)
	{
		Instance = Decal->CreateDynamicMaterialInstance();
	}
	if (Instance)
	{
		Instance->SetScalarParameterValue(TEXT("ImpactStartSeconds"), BatchTime);
		Instance->SetScalarParameterValue(TEXT("ImpactLifetimeSeconds"), LifetimeSeconds);
	}
	Slot.ExpiresAt = BatchTime + LifetimeSeconds;
	if (!GetWorld()->GetTimerManager().IsTimerActive(DecalCleanup))
	{
		GetWorld()->GetTimerManager().SetTimer(DecalCleanup, this, &URainImpactEffects::HideExpiredDecals, 0.25f, true);
	}
	return Decal;
}

// 材质负责渐变。到期后隐藏组件，留给下一批落点复用。
void URainImpactEffects::HideExpiredDecals()
{
	const double Now = GetWorld()->GetTimeSeconds();
	bool bAnyVisible = false;
	for (TArray<FDecalSlot>* Pool : {&WetSandSlots, &EtchSlots})
	{
		for (FDecalSlot& Slot : *Pool)
		{
			if (UDecalComponent* Decal = Slot.Component.Get())
			{
				if (Slot.ExpiresAt <= Now)
				{
					Decal->SetVisibility(false);
				}
				else
				{
					bAnyVisible = true;
				}
			}
		}
	}
	if (!bAnyVisible)
	{
		GetWorld()->GetTimerManager().ClearTimer(DecalCleanup);
	}
}
