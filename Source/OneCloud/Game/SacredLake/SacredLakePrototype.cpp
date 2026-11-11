#include "SacredLakePrototype.h"

#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Enemies/OneCloudEnemy.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Player/OneCloudPlayerController.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "TimerManager.h"
#include "UI/Hud/SacredLakeWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Weather/OneCloudRain.h"

namespace
{
	constexpr float NormalWater = 60.f;
	constexpr float LowWater = 15.f;
	constexpr float ExtractionAmount = 4.f;
	constexpr float ExtractionIntervalSeconds = 3.f;
	constexpr float PrototypeExtractionWindupSeconds = 1.f;
	constexpr float ShoreRetryIntervalSeconds = 0.5f;
}

// 关卡生命周期

// 预先创建旧原型的抽水样条。森林里的每个敌人需要独立水流时也以它为模板。
ASacredLakePrototype::ASacredLakePrototype()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Siphon = CreateDefaultSubobject<USplineMeshComponent>(TEXT("Siphon"));
	Siphon->SetupAttachment(GetRootComponent());
	Siphon->SetMobility(EComponentMobility::Movable);
	Siphon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Siphon->SetCanEverAffectNavigation(false);
	Siphon->SetCastShadow(false);
	Siphon->SetVisibility(false);
	Siphon->SetForwardAxis(ESplineMeshAxis::Z, false);
	Siphon->SetSplineUpDir(FVector::RightVector, false);
	Siphon->SetStartScale(FVector2D(0.045, 0.045), false);
	Siphon->SetEndScale(FVector2D(0.020, 0.020), false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Tube(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Siphon->SetStaticMesh(Tube.Object);
}

// 共同入口。绑定本地玩家和 HUD，校验关卡引用并初始化水面，再按地图配置进入各自的规则。
void ASacredLakePrototype::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0))
	{
		BindPrototypeInput(*Player);
		if (HudClass)
		{
			Hud = CreateWidget<USacredLakeWidget>(Player, HudClass);
			if (Hud)
			{
				Hud->AddToViewport();
			}
		}
	}

	Resources.Initialize(InitialWater, bUseForestRules);
	const bool bValidSpawnSetup = bUseForestRules ||
		(SpawnPoints.IsValidIndex(SpawnSide) && IsValid(SpawnPoints[SpawnSide]));
	if (!IsValid(WaterSurface) || !IsValid(Rain) || !EnemyClass ||
		!bValidSpawnSetup)
	{
		UE_LOG(LogTemp, Error, TEXT("%s is missing a water surface, rain, enemy class or spawn point."), *GetName());
		Finish(ESacredLakeResult::Unavailable);
		return;
	}

	WaterOrigin = WaterSurface->GetActorLocation();
	if (SiphonMaterial)
	{
		Siphon->SetMaterial(0, SiphonMaterial);
	}
	UpdateWater();
	if (Resources.LakeWater <= 0.f)
	{
		Finish(ESacredLakeResult::Dry);
		return;
	}

	if (bUseForestRules)
	{
		if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
		{
			// 森林的透明效果关闭额外折射通道，原有视口选项在 EndPlay 恢复。
			ForestViewport = Viewport;
			bForestPreviousRefraction = Viewport->GetEngineShowFlags()->Refraction;
			Viewport->GetEngineShowFlags()->SetRefraction(false);
		}
		BeginForest();
	}
	else
	{
		GetWorldTimerManager().SetTimer(SpawnTimer, this, &ASacredLakePrototype::SpawnExtractor,
			FMath::Max(SpawnDelaySeconds, 0.01f), false);
	}
}

// 离开地图或结束 PIE 时清理计时器、输入和界面。视口可能跨关卡复用，离开森林前要恢复渲染选项。
void ASacredLakePrototype::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	if (UGameViewportClient* Viewport = ForestViewport.Get())
	{
		Viewport->GetEngineShowFlags()->SetRefraction(bForestPreviousRefraction);
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalInput.Get())
	{
		Subsystem->RemoveMappingContext(InputMappingContext);
	}
	if (bUseForestRules)
	{
		EndForest();
	}
	else if (IsValid(Extractor))
	{
		Extractor->OnShoreReached.RemoveAll(this);
		Extractor->OnShoreMoveFailed.RemoveAll(this);
		Extractor->OnDefeated.RemoveAll(this);
	}
	if (Hud)
	{
		Hud->RemoveFromParent();
	}
	Super::EndPlay(EndPlayReason);
}

// 本地输入与胜负

// 关卡 Actor 负责降水、重试和守湖技能，精灵仍通过自己的输入组件负责移动和相机。
void ASacredLakePrototype::BindPrototypeInput(APlayerController& Player)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player.GetLocalPlayer());
	if (!Subsystem || !InputMappingContext || !RainAction || !RetryAction)
	{
		UE_LOG(LogTemp, Error, TEXT("%s is missing its local input mapping or actions."), *GetName());
		return;
	}

	CreateInputComponent(UEnhancedInputComponent::StaticClass());
	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(InputComponent);
	Input->BindAction(RainAction, ETriggerEvent::Started, this, &ASacredLakePrototype::ToggleRain);
	// 松开重试键后才换图，避免同一次长按影响新加载的关卡。
	Input->BindAction(RetryAction, ETriggerEvent::Completed, this, &ASacredLakePrototype::RequestRetryFromInput);
	if (bUseForestRules)
	{
		BindForestInput(*Input);
		if (PrecipitationAction)
		{
			Input->BindAction(PrecipitationAction, ETriggerEvent::Started, this, &ASacredLakePrototype::CycleForestPrecipitation);
		}
	}
	EnableInput(&Player);
	Subsystem->AddMappingContext(InputMappingContext, 0);
	LocalInput = Subsystem;
}

// 防守中交给菜单确认，避免误按丢失进度。旧地图使用普通控制器时仍保留原重试方式。
void ASacredLakePrototype::RequestRetryFromInput()
{
	if (AOneCloudPlayerController* Player = Cast<AOneCloudPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		Player->RequestRetry();
	}
	else
	{
		Retry();
	}
}

// 森林开雨前先结算已过去的时间，再用最新云水判断能否打开。旧原型沿用独立的开关。
void ASacredLakePrototype::ToggleRain()
{
	// 右键在种植预览中只取消落点选择；Q 仍可独立收雨。
	if (bUseForestRules && bForestPlantingPreview)
	{
		const APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0);
		if (Player && Player->IsInputKeyDown(EKeys::RightMouseButton))
		{
			CancelForestPlanting();
			return;
		}
	}
	if (Result != ESacredLakeResult::Defending || !IsValid(Rain)) return;
	if (bUseForestRules)
	{
		if (!CanActInForest()) return;
		SetForestRainRequested(!IsForestRainRequested());
		ShowForestFeedback(IsForestRainRequested()
			? NSLOCTEXT("ForestFeedback", "RainRequested", "已请求降水 · 云水不足时会积蓄后恢复")
			: NSLOCTEXT("ForestFeedback", "RainStopped", "已收雨 · 空中的雨滴仍会落下"), false);
	}
	else Rain->SetRaining(!Rain->IsRaining());
	UpdateHud();
}

// Tab 只选择局部天气，基础雨继续守护其余岸线。
void ASacredLakePrototype::CycleForestPrecipitation()
{
	if (!CanActInForest() || !IsValid(Rain)) return;
	Rain->CyclePrecipitation();
	ShowForestFeedback(NSLOCTEXT("ForestFeedback", "LocalWeather", "局部天气已选择 · 左键指定落区，基础雨继续覆盖"), false);
	UpdateHud();
}
// 胜负或配置缺失只结算一次。结束前结清云水与湖水，停止游戏计时器和天气，再刷新结果。
void ASacredLakePrototype::Finish(ESacredLakeResult NewResult)
{
	if (Result != ESacredLakeResult::Defending)
	{
		return;
	}
	if (GetWorldTimerManager().IsTimerActive(ForestTimer))
	{
		SettleForestResources(GetWorld()->GetTimeSeconds());
	}
	Result = NewResult;
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	GetWorldTimerManager().ClearTimer(ShoreRetryTimer);
	if (bUseForestRules)
	{
		EndForest();
		if (Result == ESacredLakeResult::Won || Result == ESacredLakeResult::Dry)
		{
			StartForestMusic(Result == ESacredLakeResult::Won ? 40.f : 8.f);
		}
	}
	else
	{
		StopExtraction();
		if (IsValid(Extractor) && Extractor->IsAlive())
		{
			Extractor->CancelShoreMove();
		}
	}
	if (IsValid(Rain))
	{
		Rain->SetRaining(false);
	}
	UpdateHud();
	OnResultChanged.Broadcast(Result);
}

// 重新打开当前地图，靠关卡初始化恢复敌人、资源和计时器。SkipIntro 由漫画组件读取。
void ASacredLakePrototype::Retry()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true, TEXT("SkipIntro"));
}

// 森林和旧原型共用的水面、界面和生成

// 根据 0、15、60、130 点实际水量插值得到世界高度（厘米）。同步水面、雨滴碰撞和湿岸材质。
void ASacredLakePrototype::UpdateWater()
{
	// 树能增加湖容量，但相同的实际储量仍对应相同的水位。
	const float LakeWater = Resources.LakeWater;
	float Offset;
	if (LakeWater < LowWater)
	{
		Offset = FMath::Lerp(MinimumWaterZOffset - 60.f, MinimumWaterZOffset, LakeWater / LowWater);
	}
	else if (LakeWater <= NormalWater)
	{
		Offset = FMath::Lerp(MinimumWaterZOffset, 0.f, (LakeWater - LowWater) / (NormalWater - LowWater));
	}
	else
	{
		Offset = FMath::Lerp(0.f, MaximumWaterZOffset,
			(LakeWater - NormalWater) / (FSacredLakeResources::PhysicalCapacity - NormalWater));
	}
	WaterHeightCm = WaterOrigin.Z + Offset;
	WaterSurface->SetActorLocation(FVector(WaterOrigin.X, WaterOrigin.Y, WaterHeightCm));
	WaterSurface->SetActorHiddenInGame(LakeWater <= 0.f);
	WaterSurface->SetActorEnableCollision(LakeWater > 0.f);
	if (SurfaceParameters)
	{
		UMaterialParameterCollectionInstance* Parameters = GetWorld()->GetParameterCollectionInstance(SurfaceParameters);
		Parameters->SetScalarParameterValue(TEXT("WaterSurfaceZ"), WaterHeightCm);
		Parameters->SetScalarParameterValue(TEXT("LakeWater"), LakeWater);
	}
	if (!bUseForestRules)
	{
		UpdateHud();
	}
}

// 把当前结果作为显示数据发给 HUD。资源值和胜负仍由这个 Actor 管理。
void ASacredLakePrototype::UpdateHud()
{
	if (Hud)
	{
		Hud->Refresh(Resources.LakeWater, Resources.LakeCapacity, Result, bExtracting, IsValid(Rain) && Rain->IsRaining());
		if (bUseForestRules)
		{
			Hud->RefreshForest(Resources.CloudWater, Resources.CloudCapacity, Resources.Seeds,
				GetForestPressure01(), IsForestBossActive());
			UpdateForestSkillHud();
		}
	}
}

// 用角色类默认对象的胶囊半高把出生点抬到地面上方。若引擎无法避开碰撞则让调用者改点或重试。
AOneCloudEnemy* ASacredLakePrototype::SpawnEnemy(const AActor& Marker, TSubclassOf<AOneCloudEnemy> Class)
{
	FVector Ground;
	if (!Class || !TraceGround(Marker.GetActorLocation(), Ground) || IsWet(Ground))
	{
		return nullptr;
	}

	const float HalfHeight = Class.GetDefaultObject()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Position = Ground + FVector::UpVector * (HalfHeight + 4.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	return GetWorld()->SpawnActor<AOneCloudEnemy>(Class, Position, Marker.GetActorRotation(), Params);
}

// Spline Mesh 的端点和切线使用组件局部坐标，世界空间的水点和敌人位置要先变换过去。
void ASacredLakePrototype::PositionSiphon(USplineMeshComponent& Stream, const AOneCloudEnemy& Enemy, const FVector& WaterPoint)
{
	const FTransform Transform = Stream.GetComponentTransform();
	const FVector Start = Transform.InverseTransformPosition(WaterPoint + FVector::UpVector * 3.f);
	const FVector End = Transform.InverseTransformPosition(Enemy.GetActorLocation() + FVector::UpVector * 25.f);
	const FVector Travel = End - Start;
	Stream.SetStartAndEnd(Start, FVector::UpVector * Travel.Size() * 0.72, End, Travel * 0.85);
	Stream.SetVisibility(true);
}

// 月夜原型的单怪流程

// 月夜原型只生成一个取水敌人。绑定寻路和击败事件后，开始寻找水线。
void ASacredLakePrototype::SpawnExtractor()
{
	if (SpawnPoints.IsValidIndex(SpawnSide) && IsValid(SpawnPoints[SpawnSide]))
	{
		Extractor = SpawnEnemy(*SpawnPoints[SpawnSide], EnemyClass);
	}
	if (!IsValid(Extractor))
	{
		UE_LOG(LogTemp, Error, TEXT("%s could not spawn the extractor at marker %d."), *GetName(), SpawnSide);
		Finish(ESacredLakeResult::Unavailable);
		return;
	}

	Extractor->OnShoreReached.AddUObject(this, &ASacredLakePrototype::BeginExtraction);
	Extractor->OnShoreMoveFailed.AddUObject(this, &ASacredLakePrototype::ScheduleShoreRetry);
	Extractor->OnDefeated.AddUObject(this, &ASacredLakePrototype::OnExtractorDefeated);
	SeekShore();
}

// 换岸前停止本轮抽水。寻路提交失败时由单次计时器重新采样水线。
void ASacredLakePrototype::SeekShore()
{
	if (Result != ESacredLakeResult::Defending || !IsValid(Extractor) || !Extractor->IsAlive())
	{
		return;
	}
	StopExtraction();
	FVector Goal;
	if (!FindShoreGoal(*Extractor, Extractor->GetNavAgentLocation(), Goal) || !Extractor->MoveToShore(Goal))
	{
		ScheduleShoreRetry();
	}
}

void ASacredLakePrototype::ScheduleShoreRetry()
{
	GetWorldTimerManager().SetTimer(ShoreRetryTimer, this, &ASacredLakePrototype::SeekShore,
		ShoreRetryIntervalSeconds, false);
}

// 寻路成功时再查一次水线。攻击动画和水流先开始，首次扣水等待起手时间。
void ASacredLakePrototype::BeginExtraction()
{
	FVector WaterPoint;
	if (!FindNearbyWater(Extractor->GetActorLocation(), WaterPoint))
	{
		ScheduleShoreRetry();
		return;
	}
	bExtracting = true;
	UpdateSiphon(WaterPoint);
	UpdateHud();
	Extractor->PlayAttack();
	GetWorldTimerManager().SetTimer(ExtractionTimer, this, &ASacredLakePrototype::ExtractWater,
		ExtractionIntervalSeconds, true, PrototypeExtractionWindupSeconds);
}

// 扣水前确认仍够得到水，扣水后的水面降低也可能让同一个站位失效。
void ASacredLakePrototype::ExtractWater()
{
	FVector WaterPoint;
	if (!FindNearbyWater(Extractor->GetActorLocation(), WaterPoint))
	{
		SeekShore();
		return;
	}
	Resources.Extract(ExtractionAmount);
	UpdateWater();
	if (Resources.LakeWater <= 0.f)
	{
		Finish(ESacredLakeResult::Dry);
		return;
	}

	if (!FindNearbyWater(Extractor->GetActorLocation(), WaterPoint))
	{
		SeekShore();
		return;
	}
	UpdateSiphon(WaterPoint);
	Extractor->PlayAttack();
}

void ASacredLakePrototype::StopExtraction()
{
	GetWorldTimerManager().ClearTimer(ExtractionTimer);
	bExtracting = false;
	Siphon->SetVisibility(false);
	UpdateHud();
}

void ASacredLakePrototype::UpdateSiphon(const FVector& WaterPoint)
{
	if (SiphonMaterial)
	{
		PositionSiphon(*Siphon, *Extractor, WaterPoint);
	}
}

// 旧原型里只有一只敌人，击败时仍有湖水即获胜。
void ASacredLakePrototype::OnExtractorDefeated()
{
	Finish(Resources.LakeWater > 0.f ? ESacredLakeResult::Won : ESacredLakeResult::Dry);
}
