#include "M1WeaponComponent.h"

#include "M1Audio.h"

#include "M1Projectile.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Sentinel "current ammo" for infinite-mag weapons (Fist): big enough to
	// never empty in a session, so GetAmmo()/the empty path are never tripped.
	static constexpr int32 GM1InfiniteAmmo = 1000000;
}

UM1WeaponComponent::UM1WeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// --- Canonical M1 SFX. The "Monster"/"Ambient"/"Weapon" folders are
	// py-marathon-utils' cosmetic id//1000 buckets, not real categories. Load
	// once via ConstructorHelpers (constructor-only) and share across defs. ---
	static ConstructorHelpers::FObjectFinder<USoundBase> MagnumFire(
		TEXT("/Game/Audio/Monster/A_magnum_firing.A_magnum_firing"));
	static ConstructorHelpers::FObjectFinder<USoundBase> MagnumReload(
		TEXT("/Game/Audio/Monster/A_magnum_reloading.A_magnum_reloading"));
	static ConstructorHelpers::FObjectFinder<USoundBase> EmptyGun(
		TEXT("/Game/Audio/Monster/A_empty_gun.A_empty_gun"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Ricochet(
		TEXT("/Game/Audio/Ambient/A_metallic_ricochet.A_metallic_ricochet"));
	static ConstructorHelpers::FObjectFinder<USoundBase> FusionFire(
		TEXT("/Game/Audio/Weapon/A_fusion_firing.A_fusion_firing"));
	static ConstructorHelpers::FObjectFinder<USoundBase> FusionExplode(
		TEXT("/Game/Audio/Weapon/A_fusion_exploding.A_fusion_exploding"));
	static ConstructorHelpers::FObjectFinder<USoundBase> RifleFire(
		TEXT("/Game/Audio/Monster/A_assault_rifle_firing.A_assault_rifle_firing"));
	static ConstructorHelpers::FObjectFinder<USoundBase> RocketFire(
		TEXT("/Game/Audio/Weapon/A_rocket_firing.A_rocket_firing"));
	static ConstructorHelpers::FObjectFinder<USoundBase> RocketExplode(
		TEXT("/Game/Audio/Monster/A_rocket_exploding.A_rocket_exploding"));
	static ConstructorHelpers::FObjectFinder<USoundBase> FlamethrowerFire(
		TEXT("/Game/Audio/Weapon/A_flamethrower.A_flamethrower"));
	static ConstructorHelpers::FObjectFinder<USoundBase> EnforcerAttack(
		TEXT("/Game/Audio/Weapon/A_enforcer_attack.A_enforcer_attack"));
	static ConstructorHelpers::FObjectFinder<USoundBase> GrenadeFire(
		TEXT("/Game/Audio/Monster/A_grenade_launcher_firing.A_grenade_launcher_firing"));
	static ConstructorHelpers::FObjectFinder<USoundBase> GrenadeExplode(
		TEXT("/Game/Audio/Weapon/A_grenade_exploding.A_grenade_exploding"));
	static ConstructorHelpers::FObjectFinder<USoundBase> FusionCharge(
		TEXT("/Game/Audio/Weapon/A_fusion_charging.A_fusion_charging"));

	USoundBase* const SndMagnumFire   = MagnumFire.Succeeded()      ? MagnumFire.Object      : nullptr;
	USoundBase* const SndMagnumReload = MagnumReload.Succeeded()    ? MagnumReload.Object    : nullptr;
	USoundBase* const SndEmpty        = EmptyGun.Succeeded()        ? EmptyGun.Object        : nullptr;
	USoundBase* const SndRicochet     = Ricochet.Succeeded()        ? Ricochet.Object        : nullptr;
	USoundBase* const SndFusionFire   = FusionFire.Succeeded()      ? FusionFire.Object      : nullptr;
	USoundBase* const SndFusionExpl   = FusionExplode.Succeeded()   ? FusionExplode.Object   : nullptr;
	USoundBase* const SndRifleFire    = RifleFire.Succeeded()       ? RifleFire.Object       : nullptr;
	USoundBase* const SndRocketFire   = RocketFire.Succeeded()      ? RocketFire.Object      : nullptr;
	USoundBase* const SndRocketExpl   = RocketExplode.Succeeded()   ? RocketExplode.Object   : nullptr;
	USoundBase* const SndFlame        = FlamethrowerFire.Succeeded()? FlamethrowerFire.Object: nullptr;
	USoundBase* const SndEnforcer     = EnforcerAttack.Succeeded()  ? EnforcerAttack.Object  : nullptr;
	USoundBase* const SndGrenadeFire  = GrenadeFire.Succeeded()     ? GrenadeFire.Object     : nullptr;
	USoundBase* const SndGrenadeExpl  = GrenadeExplode.Succeeded()  ? GrenadeExplode.Object  : nullptr;
	USoundBase* const SndFusionCharge = FusionCharge.Succeeded()    ? FusionCharge.Object    : nullptr;

	// --- The 7 M1 weapons (canonical stats), indices 0..6. ---
	Weapons.Reserve(7);

	// 0: Fist — infinite-ammo melee, no SFX.
	{
		FM1WeaponDef W;
		W.Name = TEXT("Fist");
		W.FireInterval = 0.333f;
		W.MagSize = -1;
		W.ReloadTime = 0.0f;
		W.Damage = 50.0f;
		W.Range = 90.0f;
		W.bProjectile = false;
		W.ProjectileSpeed = 0.0f;
		W.BlastRadius = 0.0f;
		W.bAutomatic = false;
		W.bMelee = true;
		W.bDepletes = false;
		Weapons.Add(W);
	}

	// 1: Magnum (.44 AM pistol).
	{
		FM1WeaponDef W;
		W.Name = TEXT("Magnum");
		W.FireInterval = 0.233f;
		W.MagSize = 8;
		W.ReloadTime = 1.0f;
		W.Damage = 20.0f;
		W.Range = 8000.0f;
		W.bAutomatic = false;
		W.FireSound = SndMagnumFire;
		W.ReloadSound = SndMagnumReload;
		W.EmptySound = SndEmpty;
		W.ImpactSound = SndRicochet;
		Weapons.Add(W);
	}

	// 2: Fusion Pistol — energy projectile, no blast.
	{
		FM1WeaponDef W;
		W.Name = TEXT("Fusion Pistol");
		W.FireInterval = 0.167f;
		W.MagSize = 20;
		W.ReloadTime = 2.0f;
		W.Damage = 30.0f;
		W.Range = 8000.0f;
		W.bProjectile = true;
		W.ProjectileSpeed = 750.0f;
		W.BlastRadius = 0.0f;
		W.bAutomatic = false;
		W.FireSound = SndFusionFire;
		W.ReloadSound = SndMagnumReload;
		W.EmptySound = SndEmpty;
		W.ImpactSound = SndFusionExpl;
		// Secondary: charged heavy bolt — hold RMB 0.5s (15 ticks), released bolt
		// does 80 dmg at 999 cm/s. Shares the 20-cell magazine.
		W.bHasSecondary = true;
		W.SecDamage = 80.0f;
		W.SecFireInterval = 0.333f;
		W.SecMagSize = -1;
		W.bSecProjectile = true;
		W.SecProjectileSpeed = 999.0f;
		W.SecChargeTime = 0.5f;
		W.SecFireSound = SndFusionFire;
		W.SecChargeSound = SndFusionCharge;
		W.SecImpactSound = SndFusionExpl;
		Weapons.Add(W);
	}

	// 3: Assault Rifle (MA-75) — full-auto hitscan.
	{
		FM1WeaponDef W;
		W.Name = TEXT("Assault Rifle");
		W.FireInterval = 0.0333f;
		W.MagSize = 52;
		W.ReloadTime = 1.5f;
		W.Damage = 8.0f;
		W.Range = 8000.0f;
		W.bAutomatic = true;
		W.FireSound = SndRifleFire;
		W.ReloadSound = SndMagnumReload;
		W.EmptySound = SndEmpty;
		W.ImpactSound = SndRicochet;
		// Secondary: grenade launcher — its own 7-round magazine, 80 dmg with a
		// 75cm blast, 750 cm/s, 0.167s (5 ticks) between grenades.
		W.bHasSecondary = true;
		W.SecDamage = 80.0f;
		W.SecFireInterval = 0.167f;
		W.SecMagSize = 7;
		W.bSecProjectile = true;
		W.SecProjectileSpeed = 750.0f;
		W.SecBlastRadius = 75.0f;
		W.SecFireSound = SndGrenadeFire;
		W.SecImpactSound = SndGrenadeExpl;
		Weapons.Add(W);
	}

	// 4: Rocket Launcher (SPNKR) — explosive projectile.
	{
		FM1WeaponDef W;
		W.Name = TEXT("Rocket Launcher");
		W.FireInterval = 0.5f;
		W.MagSize = 2;
		W.ReloadTime = 3.0f;
		W.Damage = 250.0f;
		W.Range = 8000.0f;
		W.bProjectile = true;
		W.ProjectileSpeed = 750.0f;
		W.BlastRadius = 150.0f;
		W.bAutomatic = false;
		W.FireSound = SndRocketFire;
		W.ReloadSound = SndMagnumReload;
		W.EmptySound = SndEmpty;
		W.ImpactSound = SndRocketExpl;
		Weapons.Add(W);
	}

	// 5: Flamethrower (TOZT) — short-range full-auto hitscan, no impact SFX.
	{
		FM1WeaponDef W;
		W.Name = TEXT("Flamethrower");
		W.FireInterval = 0.0333f;
		W.MagSize = 210;
		W.ReloadTime = 2.0f;
		W.Damage = 8.0f;
		W.Range = 600.0f;
		W.bAutomatic = true;
		W.FireSound = SndFlame;
		W.ReloadSound = SndMagnumReload;
		W.EmptySound = SndEmpty;
		Weapons.Add(W);
	}

	// 6: Alien Weapon — full-auto, depletes (never reloads), no reload SFX.
	{
		FM1WeaponDef W;
		W.Name = TEXT("Alien Weapon");
		W.FireInterval = 0.0333f;
		W.MagSize = 150;
		W.ReloadTime = 0.0f;
		W.Damage = 10.0f;
		W.Range = 2000.0f;
		W.bAutomatic = true;
		W.bDepletes = true;
		W.FireSound = SndEnforcer;
		W.EmptySound = SndEmpty;
		W.ImpactSound = SndRicochet;
		// Secondary: charged 15-bolt burst (burst_count 15, 10-tick charge),
		// spraying from the shared 150-bolt pool.
		W.bHasSecondary = true;
		W.SecDamage = 10.0f;
		W.SecFireInterval = 0.333f;
		W.SecMagSize = -1;
		W.bSecProjectile = false;
		W.SecChargeTime = 0.333f;
		W.SecBurstCount = 15;
		W.SecRange = 2000.0f;
		W.SecFireSound = SndEnforcer;
		W.SecImpactSound = SndRicochet;
		Weapons.Add(W);
	}
}

void UM1WeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// Init per-weapon ammo: full mag, or the infinite sentinel for MagSize < 0.
	Ammo.SetNum(Weapons.Num());
	SecAmmo.SetNum(Weapons.Num());
	for (int32 i = 0; i < Weapons.Num(); ++i)
	{
		Ammo[i] = Weapons[i].MagSize < 0 ? GM1InfiniteAmmo : Weapons[i].MagSize;
		SecAmmo[i] = Weapons[i].SecMagSize < 0 ? 0 : Weapons[i].SecMagSize;
	}

	CurrentIndex = FMath::Clamp(CurrentIndex, 0, Weapons.Num() - 1);
	ShowAmmo();
}

void UM1WeaponComponent::StartFire()
{
	UWorld* World = GetWorld();
	if (!World || bReloading)
	{
		return;
	}
	if (World->GetTimeSeconds() - LastFireTime < CurrentWeapon().FireInterval)
	{
		return;  // fire-rate cooldown
	}
	if (CurrentWeapon().MagSize >= 0 && CurrentAmmo() <= 0)
	{
		if (CurrentWeapon().EmptySound)
		{
			UGameplayStatics::SpawnSound2D(this, CurrentWeapon().EmptySound);
		}
		LastFireTime = World->GetTimeSeconds();
		return;
	}

	FireShot();

	// Automatic weapons keep firing while held, at their fire interval.
	if (CurrentWeapon().bAutomatic)
	{
		World->GetTimerManager().SetTimer(
			AutoFireTimerHandle, this, &UM1WeaponComponent::FireShot,
			CurrentWeapon().FireInterval, true);
	}
}

void UM1WeaponComponent::StopFire()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
	}
}

int32& UM1WeaponComponent::SecondaryAmmoPool()
{
	return CurrentWeapon().SecMagSize >= 0 ? SecAmmo[CurrentIndex] : Ammo[CurrentIndex];
}

void UM1WeaponComponent::StartAltFire()
{
	UWorld* World = GetWorld();
	const FM1WeaponDef& W = CurrentWeapon();
	if (!World || bReloading || !W.bHasSecondary)
	{
		return;
	}

	if (W.SecChargeTime > 0.0f)
	{
		// Hold-to-charge (Fusion heavy bolt, Alien burst): the shot fires on
		// release, from StopAltFire, once the charge time has elapsed.
		if (!bCharging && SecondaryAmmoPool() > 0)
		{
			bCharging = true;
			ChargeStartTime = World->GetTimeSeconds();
			if (W.SecChargeSound)
			{
				UGameplayStatics::SpawnSound2D(this, W.SecChargeSound);
			}
			ShowAmmo();
		}
		return;
	}

	// Instant secondary (AR grenade): semi-auto on its own cooldown.
	if (World->GetTimeSeconds() - LastSecFireTime < W.SecFireInterval)
	{
		return;
	}
	if (SecondaryAmmoPool() <= 0)
	{
		if (W.EmptySound)
		{
			UGameplayStatics::SpawnSound2D(this, W.EmptySound);
		}
		LastSecFireTime = World->GetTimeSeconds();
		return;
	}
	FireSecondaryShot();
}

void UM1WeaponComponent::StopAltFire()
{
	UWorld* World = GetWorld();
	if (!World || !bCharging)
	{
		return;
	}
	const FM1WeaponDef& W = CurrentWeapon();
	const bool bCharged =
		World->GetTimeSeconds() - ChargeStartTime >= W.SecChargeTime;
	bCharging = false;

	if (bCharged && !bReloading && SecondaryAmmoPool() > 0)
	{
		FireSecondaryShot();
	}
	else
	{
		ShowAmmo();  // clear the "(charging)" readout on an aborted charge
	}
}

void UM1WeaponComponent::FireSecondaryShot()
{
	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Ctrl = Pawn ? Pawn->GetController() : nullptr;
	if (!World || !Ctrl)
	{
		return;
	}
	const FM1WeaponDef& W = CurrentWeapon();

	LastSecFireTime = World->GetTimeSeconds();
	if (W.SecFireSound)
	{
		UGameplayStatics::SpawnSound2D(this, W.SecFireSound);
	}

	FVector ViewLoc;
	FRotator ViewRot;
	Ctrl->GetPlayerViewPoint(ViewLoc, ViewRot);

	// Burst secondaries (Alien: 15 bolts) spray with a small spread; everything
	// else is a single shot. Each bolt draws from the pool until it runs dry.
	const int32 Shots = FMath::Max(1, W.SecBurstCount);
	for (int32 i = 0; i < Shots && SecondaryAmmoPool() > 0; ++i)
	{
		--SecondaryAmmoPool();

		FRotator ShotRot = ViewRot;
		if (Shots > 1)
		{
			ShotRot.Yaw += FMath::FRandRange(-4.0f, 4.0f);
			ShotRot.Pitch += FMath::FRandRange(-3.0f, 3.0f);
		}
		const FVector AimDir = ShotRot.Vector();

		if (W.bSecProjectile)
		{
			const FVector SpawnLoc = ViewLoc + AimDir * 60.0f;
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = Pawn;
			SpawnParams.Instigator = Pawn;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AM1Projectile* Proj = World->SpawnActor<AM1Projectile>(
				AM1Projectile::StaticClass(), SpawnLoc, AimDir.Rotation(), SpawnParams))
			{
				Proj->Init(W.SecProjectileSpeed, W.SecDamage, W.SecBlastRadius, W.SecImpactSound);
			}
		}
		else
		{
			const FVector End = ViewLoc + AimDir * W.SecRange;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(Pawn);
			FHitResult Hit;
			const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLoc, End, ECC_Visibility, Params);
			const FVector ImpactPoint = bHit ? Hit.ImpactPoint : End;
			if (bHit)
			{
				if (W.SecImpactSound && i == 0)  // one impact sound per burst, not 15
				{
					UGameplayStatics::SpawnSoundAtLocation(World, W.SecImpactSound, ImpactPoint, FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation());
				}
				DrawDebugSphere(World, ImpactPoint, 6.0f, 8, FColor::Cyan, false, 2.0f);
				if (AActor* HitActor = Hit.GetActor())
				{
					UGameplayStatics::ApplyPointDamage(HitActor, W.SecDamage, AimDir, Hit, Ctrl, Pawn, nullptr);
				}
			}
			DrawDebugLine(World, ViewLoc, ImpactPoint, FColor::Cyan, false, 0.25f, 0, 0.5f);
		}
	}

	ShowAmmo();
}

void UM1WeaponComponent::FireShot()
{
	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Ctrl = Pawn ? Pawn->GetController() : nullptr;
	if (!World || !Ctrl)
	{
		return;
	}

	// Auto-fire timer ticks can outrun the magazine / a reload — guard here too.
	if (bReloading)
	{
		StopFire();
		return;
	}
	const FM1WeaponDef& W = CurrentWeapon();
	if (W.MagSize >= 0 && CurrentAmmo() <= 0)
	{
		if (W.EmptySound)
		{
			UGameplayStatics::SpawnSound2D(this, W.EmptySound);
		}
		StopFire();  // stop the auto loop; player must release/reload
		ShowAmmo();
		return;
	}

	LastFireTime = World->GetTimeSeconds();
	if (W.MagSize >= 0)
	{
		--CurrentAmmo();
	}
	if (W.FireSound)
	{
		UGameplayStatics::SpawnSound2D(this, W.FireSound);
	}

	FVector ViewLoc;
	FRotator ViewRot;
	Ctrl->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector AimDir = ViewRot.Vector();

	if (W.bProjectile)
	{
		// Spawn the bolt/rocket ~60cm ahead of the eye so it clears the camera.
		const FVector SpawnLoc = ViewLoc + AimDir * 60.0f;
		const FRotator SpawnRot = AimDir.Rotation();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Pawn;
		SpawnParams.Instigator = Pawn;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (AM1Projectile* Proj = World->SpawnActor<AM1Projectile>(
			AM1Projectile::StaticClass(), SpawnLoc, SpawnRot, SpawnParams))
		{
			Proj->Init(W.ProjectileSpeed, W.Damage, W.BlastRadius, W.ImpactSound);
		}
	}
	else
	{
		// Hitscan (incl. melee Fist): trace from the eye along Range.
		const FVector End = ViewLoc + AimDir * W.Range;

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Pawn);
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLoc, End, ECC_Visibility, Params);
		const FVector ImpactPoint = bHit ? Hit.ImpactPoint : End;

		if (bHit)
		{
			if (W.ImpactSound)
			{
				UGameplayStatics::SpawnSoundAtLocation(World, W.ImpactSound, ImpactPoint, FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation());
			}
			DrawDebugSphere(World, ImpactPoint, 6.0f, 8, FColor::Yellow, false, 2.0f);
			// Standard point-damage path: AM1Enemy (and anything else with a
			// TakeDamage override) handles the rest.
			if (AActor* HitActor = Hit.GetActor())
			{
				UGameplayStatics::ApplyPointDamage(HitActor, W.Damage, AimDir, Hit, Ctrl, Pawn, nullptr);
			}
		}
		DrawDebugLine(World, ViewLoc, ImpactPoint, FColor::Red, false, 0.25f, 0, 0.5f);
	}

	ShowAmmo();
}

void UM1WeaponComponent::Reload()
{
	const FM1WeaponDef& W = CurrentWeapon();
	if (bReloading || W.bDepletes || W.MagSize < 0 || CurrentAmmo() >= W.MagSize || !GetWorld())
	{
		return;
	}
	StopFire();  // a reload interrupts any held auto-fire
	bReloading = true;
	if (W.ReloadSound)
	{
		UGameplayStatics::SpawnSound2D(this, W.ReloadSound);
	}
	ShowAmmo();
	GetWorld()->GetTimerManager().SetTimer(
		ReloadTimerHandle, this, &UM1WeaponComponent::FinishReload, W.ReloadTime, false);
}

void UM1WeaponComponent::FinishReload()
{
	CurrentAmmo() = CurrentWeapon().MagSize;
	if (CurrentWeapon().SecMagSize >= 0)
	{
		SecAmmo[CurrentIndex] = CurrentWeapon().SecMagSize;  // AR grenades reload too
	}
	bReloading = false;
	ShowAmmo();
}

void UM1WeaponComponent::NextWeapon()
{
	SelectWeapon(CurrentIndex + 1);
}

void UM1WeaponComponent::PrevWeapon()
{
	SelectWeapon(CurrentIndex - 1);
}

void UM1WeaponComponent::SelectWeapon(int32 Index)
{
	if (Weapons.Num() == 0)
	{
		return;
	}

	// Wrap into 0..N-1 so number keys and the scroll wheel both stay valid.
	const int32 N = Weapons.Num();
	const int32 Wrapped = ((Index % N) + N) % N;

	// Switching always cancels in-flight fire/reload/charge and resets the
	// cooldowns so the new weapon can fire immediately.
	StopFire();
	bCharging = false;
	if (bReloading)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ReloadTimerHandle);
		}
		bReloading = false;
	}
	CurrentIndex = Wrapped;
	LastFireTime = -1000.0f;
	LastSecFireTime = -1000.0f;
	ShowAmmo();
}

void UM1WeaponComponent::ShowAmmo() const
{
	if (!GEngine || !Weapons.IsValidIndex(CurrentIndex))
	{
		return;
	}

	const FM1WeaponDef& W = Weapons[CurrentIndex];
	const int32 Cur = Ammo.IsValidIndex(CurrentIndex) ? Ammo[CurrentIndex] : 0;

	FString AmmoText;
	if (W.MagSize < 0)
	{
		AmmoText = TEXT("INF");
	}
	else
	{
		AmmoText = FString::Printf(TEXT("%d / %d"), Cur, W.MagSize);
	}
	if (W.bHasSecondary && W.SecMagSize >= 0)
	{
		const int32 SecCur = SecAmmo.IsValidIndex(CurrentIndex) ? SecAmmo[CurrentIndex] : 0;
		AmmoText += FString::Printf(TEXT("  |  GR %d / %d"), SecCur, W.SecMagSize);
	}

	GEngine->AddOnScreenDebugMessage(
		10, 3600.0f, FColor::Orange,
		FString::Printf(TEXT("%s   %s%s%s"),
			*W.Name, *AmmoText,
			bReloading ? TEXT("  (reloading)") : TEXT(""),
			bCharging ? TEXT("  (charging)") : TEXT("")));
}
