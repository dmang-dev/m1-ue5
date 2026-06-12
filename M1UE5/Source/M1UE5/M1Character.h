#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "M1Character.generated.h"

class UCameraComponent;
class USoundBase;
class UM1WeaponComponent;

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

	/** Monster melee/projectile damage lands here (ApplyDamage and friends). */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	/** Single source of truth for the build stamp (this TU's compile time), so the
	 *  menu and HUD always show the SAME value instead of per-file compile times. */
	static FString GetBuildStamp();

	/** The HUD viewmodel reads weapon state through this (M1HUD.cpp). */
	UM1WeaponComponent* GetWeaponComponent() const { return Weapon; }

	/** Current shields, for the HUD shield bar (M1HUD.cpp). */
	float GetHealth() const { return Health; }

	/** Full shields = the canonical starting pool (M1Tuning::GPlayerShields).
	 *  Used as the shield bar's 100% width. NOTE: canisters can push real M1
	 *  shields to 2x/3x; until that pickup system exists this is the 1x cap. */
	float GetMaxHealth() const;

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void MoveForward(float Value);
	void MoveRight(float Value);

	/** Roam the campaign: jump to the next/previous Marathon level ([ and ]). */
	void NextLevel();
	void PrevLevel();
	void CycleLevel(int32 Direction);

	/** Fire (LMB press/release) / reload (R) the current weapon. */
	void OnFire();
	void OnFireStop();
	void OnAltFire();
	void OnAltFireStop();
	void OnReload();

	/** Weapon selection: number keys 1-7 pick a slot; the scroll wheel cycles. */
	void OnSelectWeapon1();
	void OnSelectWeapon2();
	void OnSelectWeapon3();
	void OnSelectWeapon4();
	void OnSelectWeapon5();
	void OnSelectWeapon6();
	void OnSelectWeapon7();
	void OnNextWeapon();
	void OnPrevWeapon();

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	UCameraComponent* FirstPersonCamera;

	/** Marathon teleport SFX, played on level arrival (BeginPlay) so cycling
	 *  levels feels like Marathon's inter-level teleport. */
	UPROPERTY()
	USoundBase* TeleportSound = nullptr;

	/** First-person weapon (hitscan firing, ammo, reload). */
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	UM1WeaponComponent* Weapon;

	/** Marathon "shields". No regeneration -- recharge canisters/chargers are a
	 *  later system; v1 is a flat pool with restart-on-death. */
	UPROPERTY(VisibleAnywhere, Category = "Health")
	float Health = 150.0f;  // set from M1Tuning::GPlayerShields in BeginPlay

	/** HUD key 11: "Shields  NNN" readout (updated on BeginPlay + every hit). */
	void ShowShields() const;
};
