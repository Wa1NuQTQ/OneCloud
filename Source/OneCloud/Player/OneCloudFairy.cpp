#include "OneCloudFairy.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "TimerManager.h"

AOneCloudFairy::AOneCloudFairy()
{
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->InitCapsuleSize(32.f, 88.f);
	// 飞行由 Character Movement 更新胶囊、加速度与制动，镜头可独立绕角色旋转。
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->DefaultLandMovementMode = MOVE_Flying;
	Movement->DefaultWaterMovementMode = MOVE_Flying;
	Movement->MaxFlySpeed = 600.f;
	Movement->MaxAcceleration = 1200.f;
	Movement->BrakingDecelerationFlying = 1000.f;
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 360.f, 0.f);

	// Spring Arm 用控制旋转摆放第三人称镜头，也负责相机遇到障碍时的缩臂。
	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(RootComponent);
	CameraArm->TargetArmLength = 450.f;
	CameraArm->SocketOffset = FVector(0.f, 40.f, 60.f);
	CameraArm->bUsePawnControlRotation = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);

	WingAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("WingAudio"));
	WingAudio->SetupAttachment(GetMesh());
	WingAudio->SetAutoActivate(false);
}

void AOneCloudFairy::BeginPlay()
{
	Super::BeginPlay();
	StartWingAudio();
}

void AOneCloudFairy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 重试会卸载关卡。先清除侧掠和施法回调，再释放角色跟随音频。
	GetWorldTimerManager().ClearTimer(SidestepTimer);
	CancelSkillCast();
	WingAudio->Stop();
	Super::EndPlay(EndPlayReason);
}

// 本地输入注册和动作处理。
void AOneCloudFairy::PawnClientRestart()
{
	Super::PawnClientRestart();
	// Pawn 在本地控制权建立或恢复后运行此回调，输入映射装到对应的 Local Player。
	StartWingAudio();
	APlayerController* Player = Cast<APlayerController>(GetController());
	if (!Player || !MappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Input =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player->GetLocalPlayer()))
	{
		// 只刷新本角色的映射，不移除其他系统的输入。
		Input->RemoveMappingContext(MappingContext);
		Input->AddMappingContext(MappingContext, 0);
	}
}

void AOneCloudFairy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOneCloudFairy::Move);
		Input->BindAction(FlyAction, ETriggerEvent::Triggered, this, &AOneCloudFairy::Fly);
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AOneCloudFairy::Look);
	}
}

void AOneCloudFairy::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	// 平移只跟随相机朝向的水平分量，抬头不会改变飞行高度。
	const FRotationMatrix CameraYaw(FRotator(0.f, GetControlRotation().Yaw, 0.f));
	AddMovementInput(CameraYaw.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(CameraYaw.GetUnitAxis(EAxis::Y), Axis.X);
}

void AOneCloudFairy::Fly(const FInputActionValue& Value)
{
	// 轴值带符号，统一用世界 Z 轴，升降方向独立于相机俯仰。
	AddMovementInput(FVector::UpVector, Value.Get<float>());
}

void AOneCloudFairy::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

// 翼声只为本地精灵播放，飞行姿态变化时平滑调节同一段循环声。
void AOneCloudFairy::StartWingAudio()
{
	if (WingLoopSound && IsLocallyControlled() && !WingAudio->IsPlaying())
	{
		WingAudio->SetAttenuationSettings(WingAttenuation);
		WingAudio->SetSound(WingLoopSound);
		WingAudio->FadeIn(0.25f, 0.18f);
	}
}

void AOneCloudFairy::SetWingMoving(bool bMoving)
{
	if (WingAudio->IsPlaying())
	{
		WingAudio->AdjustVolume(0.18f, bMoving ? 0.42f : 0.18f);
	}
}
