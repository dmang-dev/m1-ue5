#include "M1Character.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"

// All 37 cooked Marathon levels (sorted), for in-game level cycling.
namespace
{
	static const TCHAR* const GM1Levels[] = {
		TEXT("/Game/Levels/M1/L_00_Arrival"),
		TEXT("/Game/Levels/M1/L_01_Bigger_Guns_Nearby"),
		TEXT("/Game/Levels/M1/L_02_Never_Burn_Money"),
		TEXT("/Game/Levels/M1/L_03_Defend_THIS"),
		TEXT("/Game/Levels/M1/L_04_Couch_Fishing"),
		TEXT("/Game/Levels/M1/L_05_The_Rose"),
		TEXT("/Game/Levels/M1/L_06_Smells_Like_Napalm_Tastes_Like_Chicken"),
		TEXT("/Game/Levels/M1/L_07_Cool_Fusion"),
		TEXT("/Game/Levels/M1/L_08_G4_Sunbathing"),
		TEXT("/Game/Levels/M1/L_09_Blaspheme_Quarantine"),
		TEXT("/Game/Levels/M1/L_10_Bob_B_Q"),
		TEXT("/Game/Levels/M1/L_11_Shake_Before_Using"),
		TEXT("/Game/Levels/M1/L_12_Fire_Fire_Fire_Fire_Fire"),
		TEXT("/Game/Levels/M1/L_13_Colony_Ship_For_Sale_Cheap"),
		TEXT("/Game/Levels/M1/L_14_Habe_Quiddam"),
		TEXT("/Game/Levels/M1/L_15_Neither_High_nor_Low"),
		TEXT("/Game/Levels/M1/L_16_Pfhor_Your_Eyes_Only"),
		TEXT("/Game/Levels/M1/L_17_No_Artificial_Colors"),
		TEXT("/Game/Levels/M1/L_18_Unpfhorgiven"),
		TEXT("/Game/Levels/M1/L_19_Two_Times_Two_Equals"),
		TEXT("/Game/Levels/M1/L_20_Beware_of_Low_Flying_Defense_Drones"),
		TEXT("/Game/Levels/M1/L_21_Eupfhoria"),
		TEXT("/Game/Levels/M1/L_22_Pfhoraphobia"),
		TEXT("/Game/Levels/M1/L_23_Ain_t_Got_Time_Pfhor_This"),
		TEXT("/Game/Levels/M1/L_24_Welcome_to_the_Revolution"),
		TEXT("/Game/Levels/M1/L_25_Try_again"),
		TEXT("/Game/Levels/M1/L_26_Ingue_Ferroque"),
		TEXT("/Game/Levels/M1/L_27_Mars_Needs_Women"),
		TEXT("/Game/Levels/M1/L_28_Carnage_Palace_Deeee_Luxe"),
		TEXT("/Game/Levels/M1/L_29_5_D_space"),
		TEXT("/Game/Levels/M1/L_30_Arena"),
		TEXT("/Game/Levels/M1/L_31_E_equals_MC_WHAT"),
		TEXT("/Game/Levels/M1/L_32_Showered_With_Grenades"),
		TEXT("/Game/Levels/M1/L_33_Spiral_Insanity"),
		TEXT("/Game/Levels/M1/L_34_Waldo_World_Arena"),
		TEXT("/Game/Levels/M1/L_35_What_Goes_Up_Must_Come_Down"),
		TEXT("/Game/Levels/M1/L_36_You_don_t_need_to_see_my_I_D"),
	};
	static constexpr int32 GM1LevelCount = UE_ARRAY_COUNT(GM1Levels);
}

AM1Character::AM1Character()
{
	PrimaryActorTick.bCanEverTick = false;

	// Mouse yaw turns the whole pawn; pitch is camera-only (typical FPS).
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f)); // eye height
	FirstPersonCamera->bUsePawnControlRotation = true;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->JumpZVelocity = 420.0f;
		Move->AirControl = 0.5f;
		Move->MaxWalkSpeed = 600.0f;
	}
}

void AM1Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AM1Character::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AM1Character::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("NextLevel", IE_Pressed, this, &AM1Character::NextLevel);
	PlayerInputComponent->BindAction("PrevLevel", IE_Pressed, this, &AM1Character::PrevLevel);
}

void AM1Character::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine && GetWorld())
	{
		const FString MapName = GetWorld()->GetMapName();
		GEngine->AddOnScreenDebugMessage(1, 8.0f, FColor::Green,
			FString::Printf(TEXT("Marathon UE5  -  %s"), *MapName));
		GEngine->AddOnScreenDebugMessage(2, 8.0f, FColor::Silver,
			TEXT("WASD move | mouse look | Space jump | [ and ] cycle levels"));
	}
}

void AM1Character::NextLevel()
{
	CycleLevel(1);
}

void AM1Character::PrevLevel()
{
	CycleLevel(-1);
}

void AM1Character::CycleLevel(int32 Direction)
{
	if (!GetWorld())
	{
		return;
	}

	// GetMapName() is the short package name (possibly PIE-prefixed); match it
	// against each level's clean filename.
	const FString Current = GetWorld()->GetMapName();
	int32 Index = 0;
	for (int32 i = 0; i < GM1LevelCount; ++i)
	{
		if (Current.EndsWith(FPaths::GetCleanFilename(GM1Levels[i])))
		{
			Index = i;
			break;
		}
	}

	const int32 Next = ((Index + Direction) % GM1LevelCount + GM1LevelCount) % GM1LevelCount;
	UGameplayStatics::OpenLevel(this, FName(GM1Levels[Next]));
}

void AM1Character::MoveForward(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
	}
}

void AM1Character::MoveRight(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
	}
}
