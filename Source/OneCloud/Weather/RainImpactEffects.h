#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RainImpactSurface.h"
#include "RainImpactEffects.generated.h"

class UDecalComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class USkeletalMeshComponent;
class UStaticMesh;
struct FHitResult;

/** 从落点查询结果显示近景反馈，按表面节流并复用实例和贴花。 */
UCLASS()
class ONECLOUD_API URainImpactEffects : public UActorComponent
{
	GENERATED_BODY()

public:
	URainImpactEffects();
	// 每批回传先保存相机与时间，供随后多次 PresentImpact 共用。
	void BeginImpactBatch();
	void PresentImpact(ERainImpactSurface Surface, const FHitResult& Hit);

private:
	// 贴花由天气 Actor 持有。槽只保存可复用组件、湿地区域索引和过期时间。
	struct FDecalSlot
	{
		TWeakObjectPtr<UDecalComponent> Component;
		FIntPoint SandCell = FIntPoint::ZeroValue;
		double ExpiresAt = 0.0;
	};

	UInstancedStaticMeshComponent* CreateInstances(UMaterialInterface* Material, int32 CustomFloats);
	void PresentSpray(ERainImpactSurface Surface, FVector Position, FVector Normal);
	void PresentRipple(FVector Position, FVector Normal);
	void PresentWetSand(const FHitResult& Hit);
	void PresentEtch(const FHitResult& Hit, USkeletalMeshComponent& Skin);
	UDecalComponent* PrepareDecal(FDecalSlot& Slot, UMaterialInterface* Material,
		FVector Position, FVector Normal, FVector SizeCm, float LifetimeSeconds);
	void HideExpiredDecals();

	UPROPERTY(EditDefaultsOnly, Category = "Impact Visuals")
	TObjectPtr<UStaticMesh> ImpactPlane;

	UPROPERTY(EditDefaultsOnly, Category = "Impact Visuals")
	TObjectPtr<UMaterialInterface> SprayMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Impact Visuals")
	TObjectPtr<UMaterialInterface> RippleMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Impact Visuals")
	TObjectPtr<UMaterialInterface> WetSandMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Impact Visuals")
	TObjectPtr<UMaterialInterface> EtchMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> SprayInstances;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> RippleInstances;

	// 湿地与腐蚀分别用固定槽数。Next* 是写满后循环覆盖的游标。
	TArray<FDecalSlot> WetSandSlots;
	TArray<FDecalSlot> EtchSlots;
	FTimerHandle DecalCleanup;
	int32 NextSpray = 0;
	int32 NextRipple = 0;
	int32 NextWetSand = 0;
	int32 NextEtch = 0;
	// 按表面记录下次允许绘制的世界时间，不参与雨声节流。
	double NextSurfaceImpact[static_cast<int32>(ERainImpactSurface::Count)] = {};
	double BatchTime = 0.0;
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
	FQuat SprayFacing = FQuat::Identity;
	bool bHasView = false;
};
