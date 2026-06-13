#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "M1Character.generated.h"

class UCameraComponent;
class USoundBase;
class UM1WeaponComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

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

	/** Drives the vacuum oxygen drain on vacuum levels (no-op elsewhere). */
	virtual void Tick(float DeltaSeconds) override;

	/** Monster melee/projectile damage lands here (ApplyDamage and friends). */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	/** Single source of truth for the build stamp (this TU's compile time), so the
	 *  menu and HUD always show the SAME value instead of per-file compile times. */
	static FString GetBuildStamp();

	/** --- Campaign level order + checkpoint, shared with the menu controller. --- */
	static int32 M1LevelCount();
	static const TCHAR* M1LevelPath(int32 Index);             // clamped package path
	static int32 M1LevelIndexForMap(const FString& MapName);  // -1 if not a campaign map
	static void M1WriteCheckpoint(int32 LevelIndex, float Shields);
	static bool M1ReadCheckpoint(int32& OutLevelIndex, float& OutShields);
	static bool M1HasCheckpoint();

	/** The HUD viewmodel reads weapon state through this (M1HUD.cpp). */
	UM1WeaponComponent* GetWeaponComponent() const { return Weapon; }

	/** The world rendered for the bezel's 3D-viewport hole. Null until BeginPlay
	 *  sets it up (the HUD falls back to a fullscreen view when null). */
	UTextureRenderTarget2D* GetSceneRT() const { return SceneRT; }

	/** Current shields, for the HUD shield bar (M1HUD.cpp). */
	float GetHealth() const { return Health; }

	/** Top up shields toward Cap (1x/2x/3x charger); never reduces. */
	void RefillShields(float Cap);

	/** Top up suit oxygen toward Cap (oxygen recharger); never reduces. */
	void RefillOxygen(float Cap);

	/** --- Terminal (lore) viewer, read by the HUD to draw the active page. --- */
	bool IsTerminalOpen() const { return bTerminalOpen; }
	int32 GetTerminalTextureId() const { return ActiveTermId; }
	int32 GetTerminalPage() const { return ActiveTermPage; }
	int32 GetTerminalPageCount() const { return ActiveTermPages; }

	/** Full shields = the canonical starting pool (M1Tuning::GPlayerShields).
	 *  Used as the shield bar's 100% width. NOTE: canisters can push real M1
	 *  shields to 2x/3x; until that pickup system exists this is the 1x cap. */
	float GetMaxHealth() const;

	/** Current suit oxygen + its full pool, for the HUD oxygen gauge (M1HUD.cpp).
	 *  Drains only in vacuum (no vacuum levels yet, so it reads full for now). */
	float GetOxygen() const { return Oxygen; }
	float GetMaxOxygen() const;

	/** Debug: set suit oxygen from the console ("M1SetOxygen 50") to exercise the
	 *  HUD gauge before the vacuum drain system exists. */
	UFUNCTION(Exec)
	void M1SetOxygen(float NewOxygen);

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void MoveForward(float Value);
	void MoveRight(float Value);

	/** Roam the campaign: jump to the next/previous Marathon level ([ and ]).
	 *  Kept as a dev/fallback nav alongside the story exit-terminal progression
	 *  so a level can never soft-lock if its exit terminal isn't recognised. */
	void NextLevel();
	void PrevLevel();
	void CycleLevel(int32 Direction);

	/** Story progression: checkpoint the NEXT campaign level (carrying shields)
	 *  and open it. Triggered by finishing the level's exit terminal; no-op past
	 *  the last level (epilogue is a later system). */
	void AdvanceLevel();

	/** Use (E): page through the nearest terminal, or open/close it. */
	void OnUse();

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

	/** Renders the world for the authentic bezel's 4:3 viewport hole; the HUD
	 *  composites the result into the metal frame. */
	UPROPERTY(VisibleAnywhere, Category = "HUD")
	USceneCaptureComponent2D* SceneCapture = nullptr;

	UPROPERTY()
	UTextureRenderTarget2D* SceneRT = nullptr;

	/** Marathon "shields". No regeneration -- recharge canisters/chargers are a
	 *  later system; v1 is a flat pool with restart-on-death. */
	UPROPERTY(VisibleAnywhere, Category = "Health")
	float Health = 150.0f;  // set from M1Tuning::GPlayerShields in BeginPlay

	/** Suit oxygen pool (HUD left gauge). Drains in vacuum (see bIsVacuumLevel). */
	UPROPERTY(VisibleAnywhere, Category = "Health")
	float Oxygen = 100.0f;  // set from M1Tuning::GPlayerOxygen in BeginPlay

	/** Cached on BeginPlay: is this a vacuum level (Minf env bit 0)? Only then does
	 *  oxygen drain. In M1 that's just L08 "G4 Sunbathing". */
	bool bIsVacuumLevel = false;

	/** HUD key 11: "Shields  NNN" readout (updated on BeginPlay + every hit). */
	void ShowShields() const;

	/** --- Terminal viewer state. While open the player is frozen (movement
	 *  disabled, weapon fire suppressed) and the HUD draws the page art. --- */
	bool bTerminalOpen = false;
	int32 ActiveTermId = -1;
	int32 ActiveTermPage = 0;
	int32 ActiveTermPages = 0;

	/** Search for a usable terminal in front of the player and open it. */
	void OpenNearestTerminal();
	/** Activate the nearest charger / pattern buffer in front. Returns true if one fired. */
	bool UseNearestPanel();
	/** Restore movement when the terminal closes. */
	void CloseTerminal();

	/** True if TermId is the current level's exit terminal. M1 stores no teleport
	 *  directive in the data, so the generated table flags terminals whose text
	 *  shows a teleport action (bExit); the most-paged flagged terminal is the
	 *  exit, falling back to the most-paged terminal overall on levels with none
	 *  flagged. Reading it to the end calls AdvanceLevel(); [ / ] stays a fallback. */
	bool IsLevelExitTerminal(int32 TermId) const;
};
