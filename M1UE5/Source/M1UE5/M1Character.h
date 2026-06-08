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

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void MoveForward(float Value);
	void MoveRight(float Value);

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	UCameraComponent* FirstPersonCamera;
};
