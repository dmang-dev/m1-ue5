#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "M1WeaponComponent.generated.h"

class USoundBase;

/**
 * Static definition of one Marathon weapon (stats + behavior flags + SFX).
 * Later these may be populated from Physics.phys; for now the full M1 arsenal
 * is built in C++ in the component constructor.
 */
USTRUCT()
struct FM1WeaponDef
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name = TEXT("Magnum");

	UPROPERTY()
	float Damage = 20.0f;          // per shot (applied once enemies exist)

	UPROPERTY()
	float FireInterval = 0.5f;     // seconds between shots

	UPROPERTY()
	int32 MagSize = 8;             // -1 = infinite (never decrements, never reloads)

	UPROPERTY()
	float Range = 8000.0f;         // cm (hitscan reach; also projectile trace fallback)

	UPROPERTY()
	float ReloadTime = 1.2f;       // seconds

	UPROPERTY()
	bool bAutomatic = false;       // holds-to-fire at FireInterval

	UPROPERTY()
	bool bMelee = false;           // very short-range hitscan, no SFX (Fist)

	UPROPERTY()
	bool bProjectile = false;      // spawns AM1Projectile instead of hitscan

	UPROPERTY()
	float ProjectileSpeed = 0.0f;  // cm/s (projectile weapons only)

	UPROPERTY()
	float BlastRadius = 0.0f;      // cm (explosive projectiles)

	UPROPERTY()
	bool bDepletes = false;        // can't reload; ammo just runs out (Alien Weapon)

	UPROPERTY()
	USoundBase* FireSound = nullptr;

	UPROPERTY()
	USoundBase* ReloadSound = nullptr;

	UPROPERTY()
	USoundBase* EmptySound = nullptr;

	UPROPERTY()
	USoundBase* ImpactSound = nullptr;

	// --- Secondary trigger (RMB), canonical M1 trigger[1] data. ---

	UPROPERTY()
	bool bHasSecondary = false;

	UPROPERTY()
	float SecDamage = 0.0f;

	UPROPERTY()
	float SecFireInterval = 0.5f;

	UPROPERTY()
	int32 SecMagSize = -1;         // -1 = shares the primary magazine

	UPROPERTY()
	bool bSecProjectile = false;

	UPROPERTY()
	float SecProjectileSpeed = 0.0f;

	UPROPERTY()
	float SecBlastRadius = 0.0f;

	UPROPERTY()
	float SecChargeTime = 0.0f;    // >0 = hold RMB to charge, fires on release

	UPROPERTY()
	int32 SecBurstCount = 1;       // bolts per (charged) trigger event (Alien = 15)

	UPROPERTY()
	float SecRange = 8000.0f;      // cm, for hitscan secondaries

	UPROPERTY()
	USoundBase* SecFireSound = nullptr;

	UPROPERTY()
	USoundBase* SecChargeSound = nullptr;

	UPROPERTY()
	USoundBase* SecImpactSound = nullptr;
};

/**
 * First-person weapon handling for AM1Character: hitscan + projectile firing,
 * per-weapon ammo, reload, and switching across the full M1 arsenal. Data-driven
 * via FM1WeaponDef so new weapons slot in by adding a definition. Visuals
 * (viewmodel sprite, muzzle flash, decals) are a later pass; for now firing
 * draws a debug tracer / projectile burst and plays the M1 SFX.
 */
UCLASS(ClassGroup = (M1), meta = (BlueprintSpawnableComponent))
class M1UE5_API UM1WeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UM1WeaponComponent();

	/** Pull the trigger. Fires one shot immediately (gated by FireInterval +
	 *  ammo); if the current weapon is automatic, also starts a repeating timer. */
	void StartFire();

	/** Release the trigger: stop any automatic fire. */
	void StopFire();

	/** Pull the secondary trigger (RMB). Charging weapons (Fusion, Alien) begin
	 *  charging; instant secondaries (AR grenade) fire immediately. */
	void StartAltFire();

	/** Release the secondary trigger: a fully-charged weapon fires on release. */
	void StopAltFire();

	/** Begin a reload (no-op if full, reloading, infinite, or a depleting weapon). */
	void Reload();

	/** Cycle / pick the active weapon (stops auto-fire, resets cooldown). */
	void NextWeapon();
	void PrevWeapon();
	void SelectWeapon(int32 Index);

	int32 GetAmmo() const { return Ammo.IsValidIndex(CurrentIndex) ? Ammo[CurrentIndex] : 0; }
	const FM1WeaponDef& GetWeapon() const { return Weapons[CurrentIndex]; }

	// State the HUD viewmodel reads each frame (M1HUD.cpp).
	int32 GetCurrentIndex() const { return CurrentIndex; }
	bool IsReloading() const { return bReloading; }
	float GetLastFireTime() const { return LastFireTime; }

protected:
	virtual void BeginPlay() override;

private:
	void FireShot();
	void FireSecondaryShot();
	void FinishReload();
	void ShowAmmo() const;

	/** Secondary's ammo pool: its own mag (SecMagSize >= 0) or the primary's. */
	int32& SecondaryAmmoPool();

	/** Convenience accessors for the active weapon's def / ammo. */
	const FM1WeaponDef& CurrentWeapon() const { return Weapons[CurrentIndex]; }
	int32& CurrentAmmo() { return Ammo[CurrentIndex]; }

	UPROPERTY()
	TArray<FM1WeaponDef> Weapons;

	/** Per-weapon current ammo (parallel to Weapons). Infinite mags store a
	 *  large sentinel so GetAmmo() stays meaningful but never empties. */
	TArray<int32> Ammo;

	/** Per-weapon secondary ammo (parallel; only meaningful when SecMagSize >= 0,
	 *  e.g. the AR's 7-grenade magazine). */
	TArray<int32> SecAmmo;

	int32 CurrentIndex = 1;  // start on the Magnum
	float LastFireTime = -1000.0f;
	float LastSecFireTime = -1000.0f;
	bool bReloading = false;
	bool bCharging = false;
	float ChargeStartTime = 0.0f;
	FTimerHandle ReloadTimerHandle;
	FTimerHandle AutoFireTimerHandle;
};
