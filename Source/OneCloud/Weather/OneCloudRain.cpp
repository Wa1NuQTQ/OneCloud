#include "OneCloudRain.h"
#include "RainImpactEffects.h"
#include "Enemies/OneCloudEnemy.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 物理材质给地面和树冠分类，敌人的护甲配置还可覆盖胶囊体材质。
	ERainImpactSurface ClassifyImpact(const FHitResult& Hit, const AOneCloudEnemy* Enemy)
	{
		UPhysicalMaterial* Material = Hit.PhysMaterial.Get();
		if (!Material || Material->SurfaceType == SurfaceType_Default)
		{
			if (UInstancedSkinnedMeshComponent* Tree = Cast<UInstancedSkinnedMeshComponent>(Hit.GetComponent()))
			{
				if (UBodySetup* Body = Tree->GetBodySetup())
				{
					Material = Body->GetPhysMaterial();
				}
			}
		}

		// SurfaceType1 至 5 的含义见 DefaultEngine.ini。
		const EPhysicalSurface Surface = UPhysicalMaterial::DetermineSurfaceType(Material);
		if (Enemy)
		{
			return Surface == SurfaceType4 || Enemy->IsArmoredRainSurface() ?
				ERainImpactSurface::Metal : ERainImpactSurface::Flesh;
		}
		switch (Surface)
		{
		case SurfaceType1: return ERainImpactSurface::Sand;
		case SurfaceType2: return ERainImpactSurface::Water;
		case SurfaceType3: return ERainImpactSurface::Leaf;
		case SurfaceType4: return ERainImpactSurface::Metal;
		case SurfaceType5: return ERainImpactSurface::Soil;
		default: return ERainImpactSurface::Unknown;
		}
	}
}

AOneCloudRain::AOneCloudRain()
{
	PrimaryActorTick.bCanEverTick = false;

	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Rain"));
	SetRootComponent(NiagaraComponent);
	NiagaraComponent->SetAutoActivate(false);
	ImpactEffects = CreateDefaultSubobject<URainImpactEffects>(TEXT("ImpactEffects"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerSphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	WeatherMarkerMesh = MarkerSphere.Object;
}

// 先接好 Niagara 导出和天气组件，再按关卡初始配置开始发射。
void AOneCloudRain::BeginPlay()
{
	Super::BeginPlay();
	CacheSandSounds();
	NiagaraComponent->SetVariableObject(TEXT("User.ImpactHandler"), this);
	if (bUseWeatherRainDamage)
	{
		LocalRainComponent = CreatePrecipitationComponent(NiagaraComponent->GetAsset());
		SnowPrecipitationComponent = CreatePrecipitationComponent(SnowPrecipitationSystem);
		HailPrecipitationComponent = CreatePrecipitationComponent(HailPrecipitationSystem);
		UpdateNiagaraCoverage();
	}
	HideLightning();
	SetRaining(bStartRaining);
}

void AOneCloudRain::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 按生命周期回收技能、降水结算和闪电的计时回调。
	CancelWeatherSkills();
	ClearPrecipitationSnow();
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.ClearTimer(WeatherRainTimer);
	Timers.ClearTimer(LightningTimer);
	Timers.ClearTimer(LightningEndTimer);
	Timers.ClearTimer(ThunderTimer);
	HideLightning();
	Super::EndPlay(EndPlayReason);
}

void AOneCloudRain::SetRaining(bool bEnabled)
{
	if (bIsRaining == bEnabled)
	{
		return;
	}

	bIsRaining = bEnabled;
	// 地面结算会延后生效，Niagara 发射组件则立即响应。
	QueueRainPhase();
	UpdatePrecipitationEmission();
	if (bIsRaining)
	{
		ScheduleLightning();
	}
	else
	{
		// 资产设为停止发射后继续模拟存量粒子，因此尾雨仍能落地。
		GetWorldTimerManager().ClearTimer(LightningTimer);
	}
	OnRainEmissionChanged.Broadcast(bIsRaining);
}

// 这条链路处理 GPU 导出的逐粒反馈。森林地图的战斗判定见 ResolveWeatherRain。
void AOneCloudRain::ReceiveParticleData_Implementation(const TArray<FBasicParticleData>& Data,
	UNiagaraSystem* NiagaraSystem, const FVector& SimulationPositionOffset)
{
	UWorld* World = GetWorld();
	if (!World || !NiagaraSystem || Data.IsEmpty())
	{
		return;
	}
	// 切换天气后旧组件仍可能返回最后几批数据，所以接受三个已配置的来源。
	if (NiagaraSystem != NiagaraComponent->GetAsset() &&
		(!SnowPrecipitationComponent || NiagaraSystem != SnowPrecipitationComponent->GetAsset()) &&
		(!HailPrecipitationComponent || NiagaraSystem != HailPrecipitationComponent->GetAsset()))
	{
		return;
	}

	// GPU 候选点只提供落点线索。CPU 用 Visibility 射线确认首个阻挡对象，并取回物理材质。
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RainSurface), false, this);
	QueryParams.bReturnPhysicalMaterial = true;
	constexpr double SurfaceProbePaddingCm = 50.0;
	const double EmissionHeightCm = NiagaraComponent->GetComponentLocation().Z + SurfaceProbePaddingCm;
	ImpactEffects->BeginImpactBatch();
	// 整批落点共用世界、音频设备和天气音量，逐粒只查询和播放自己的结果。
	const bool bCanPlayAudio = GEngine && GEngine->UseSound() && World->bAllowAudioPlayback &&
		!World->IsNetMode(NM_DedicatedServer);
	FAudioDeviceHandle Audio = bCanPlayAudio ? World->GetAudioDevice() : FAudioDeviceHandle();
	const bool bSnowImpact = SnowPrecipitationComponent && NiagaraSystem == SnowPrecipitationComponent->GetAsset();
	const bool bHailImpact = HailPrecipitationComponent && NiagaraSystem == HailPrecipitationComponent->GetAsset();
	const float WeatherImpactVolume = ImpactVolume * (bSnowImpact ? 0.65f : 1.f);

	for (const FBasicParticleData& Particle : Data)
	{
		// 导出位置相对于 Niagara 的模拟坐标块，叠加偏移后才可用于世界查询。
		const FVector Position = FVector(Particle.Position) + SimulationPositionOffset;
		if (Position.ContainsNaN())
		{
			continue;
		}

		// 从发射高度反推轨迹，使斜风里的雨也会先打到沿途的树冠和遮挡。
		const double TraceHeightCm = FMath::Max(EmissionHeightCm, Position.Z + SurfaceProbePaddingCm);
		FVector TraceStart(Position.X, Position.Y, TraceHeightCm);
		const FVector Velocity(Particle.Velocity);
		if (!Velocity.ContainsNaN() && Velocity.Z < -1.0)
		{
			const double SecondsBackToEmission = (TraceHeightCm - Position.Z) / -Velocity.Z;
			TraceStart = Position - Velocity * SecondsBackToEmission;
		}
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(
			Hit, TraceStart, Position - FVector(0.0, 0.0, SurfaceProbePaddingCm), ECC_Visibility, QueryParams);
		if (!bHit)
		{
			continue;
		}
		AOneCloudEnemy* Enemy = Cast<AOneCloudEnemy>(Hit.GetActor());
		const ERainImpactSurface Surface = ClassifyImpact(Hit, Enemy);
		ImpactEffects->PresentImpact(Surface, Hit);
		// 旧图沿用逐滴伤害。森林图只用这个落点播声音和画特效。
		if (Enemy && !bUseWeatherRainDamage)
		{
			if (Enemy->IsAlive() && FMath::IsFinite(DamagePerDrop) && DamagePerDrop > 0.f)
			{
				UGameplayStatics::ApplyPointDamage(Enemy, DamagePerDrop,
					(Position - TraceStart).GetSafeNormal(), Hit, GetInstigatorController(), this, nullptr);
			}
		}
		if (!Audio)
		{
			continue;
		}
		if (USoundBase* ImpactSound = SelectWeatherImpactSound(Surface, bSnowImpact, bHailImpact))
		{
			Audio->PlaySoundAtLocation(ImpactSound, World, WeatherImpactVolume, 1.f, 0.f,
				Hit.ImpactPoint, FRotator::ZeroRotator, ImpactAttenuation.Get());
		}
	}
}
