#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "M1Character.generated.h"

class UCameraComponent;

/**
 * First-person walking pawn for the Marathon levels: WASD to walk, mouse to
 * look, Space to jump. Gravity and collision come from ACharacter's capsule +
 * CharacterMovementComponent against the procedural sector meshes (which carry
 * complex-as-simple collision from the headless build).
 */
UCLASS()
class M1UE5_API AM1Character : public ACharacter
{
	GENERATED_BODY()

public:
	AM1Character();

	virtual void BeginPlay() override;

	/** Single source of truth for the build stamp (this TU's compile time), so the
	 *  menu and HUD always show the SAME value instead of per-file compile times. */
	static FString GetBuildStamp();

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void MoveForward(float Value);
	void MoveRight(float Value);

	/** Roam the campaign: jump to the next/previous Marathon level ([ and ]). */
	void NextLevel();
	void PrevLevel();
	void CycleLevel(int32 Direction);

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	UCameraComponent* FirstPersonCamera;
};
