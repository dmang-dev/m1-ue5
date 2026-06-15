#include "M1WeaponComponent.h"

#include "M1Audio.h"
#include "M1WeaponTable.h"  // GENERATED canonical weapon stats (gen_weapon_stats.py)
#include "Net/UnrealNetwork.h"  // DOREPLIFETIME (co-op ammo replication)

#include "M1Projectile.h"
#include "M1Impact.h"
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

	// Co-op: replicate ammo + current weapon so a client's HUD shows correct ammo
	// and the right viewmodel (fire/select are server-authoritative on the pawn).
	SetIsReplicatedByDefault(true);

	// --- Build the arsenal STATS from the generated table (M1WeaponTable.h, from
	// Tools/gen_weapon_stats.py) -- a single regenerable source instead of hardcoded
	// constants. SFX assets are loaded in BeginPlay (LoadObject at runtime, not in
	// the CDO constructor, where ConstructorHelpers would be the only safe option). ---
	Weapons.Reserve(GM1WeaponTableCount);
	for (int32 i = 0; i < GM1WeaponTableCount; ++i)
	{
		const FM1WeaponRow& R = GM1WeaponTable[i];
		FM1WeaponDef W;
		W.Name = R.Name;
		W.Damage = R.Damage;
		W.FireInterval = R.FireInterval;
		W.MagSize = R.MagSize;
		W.Range = R.Range;
		W.ReloadTime = R.ReloadTime;
		W.bAutomatic = R.bAutomatic;
		W.bMelee = R.bMelee;
		W.bProjectile = R.bProjectile;
		W.ProjectileSpeed = R.ProjectileSpeed;
		W.BlastRadius = R.BlastRadius;
		W.bDepletes = R.bDepletes;
		W.bHasSecondary = R.bHasSecondary;
		W.SecDamage = R.SecDamage;
		W.SecFireInterval = R.SecFireInterval;
		W.SecMagSize = R.SecMagSize;
		W.bSecProjectile = R.bSecProjectile;
		W.SecProjectileSpeed = R.SecProjectileSpeed;
		W.SecBlastRadius = R.SecBlastRadius;
		W.SecChargeTime = R.SecChargeTime;
		W.SecBurstCount = R.SecBurstCount;
		W.SecRange = R.SecRange;
		Weapons.Add(W);
	}
}

void UM1WeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UM1WeaponComponent, Ammo);
	DOREPLIFETIME(UM1WeaponComponent, SecAmmo);
	DOREPLIFETIME(UM1WeaponComponent, CurrentIndex);
}

void UM1WeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// Load each weapon's SFX by path from the table (runtime LoadObject, not in the
	// CDO constructor). Empty path = silent. Runs on server + clients.
	auto LoadSnd = [](const TCHAR* Path) -> USoundBase*
	{
		return (Path && Path[0]) ? LoadObject<USoundBase>(nullptr, Path) : nullptr;
	};
	for (int32 i = 0; i < Weapons.Num() && i < GM1WeaponTableCount; ++i)
	{
		const FM1WeaponRow& R = GM1WeaponTable[i];
		Weapons[i].FireSound    = LoadSnd(R.FireSound);
		Weapons[i].ReloadSound  = LoadSnd(R.ReloadSound);
		Weapons[i].EmptySound   = LoadSnd(R.EmptySound);
		Weapons[i].ImpactSound  = LoadSnd(R.ImpactSound);
		Weapons[i].SecFireSound   = LoadSnd(R.SecFireSound);
		Weapons[i].SecChargeSound = LoadSnd(R.SecChargeSound);
		Weapons[i].SecImpactSound = LoadSnd(R.SecImpactSound);
	}

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
#if WITH_EDITOR
				DrawDebugSphere(World, ImpactPoint, 6.0f, 8, FColor::Cyan, false, 2.0f);
#endif
				AM1Impact::Spawn(World, ImpactPoint, 8.0f, FLinearColor(1.6f, 3.2f, 4.0f, 1.0f));
				if (AActor* HitActor = Hit.GetActor())
				{
					UGameplayStatics::ApplyPointDamage(HitActor, W.SecDamage, AimDir, Hit, Ctrl, Pawn, nullptr);
				}
			}
#if WITH_EDITOR
			DrawDebugLine(World, ViewLoc, ImpactPoint, FColor::Cyan, false, 0.25f, 0, 0.5f);
#endif
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
#if WITH_EDITOR
			DrawDebugSphere(World, ImpactPoint, 6.0f, 8, FColor::Yellow, false, 2.0f);
#endif
			AM1Impact::Spawn(World, ImpactPoint, 9.0f, FLinearColor(4.0f, 3.0f, 1.2f, 1.0f));
			// Standard point-damage path: AM1Enemy (and anything else with a
			// TakeDamage override) handles the rest.
			if (AActor* HitActor = Hit.GetActor())
			{
				UGameplayStatics::ApplyPointDamage(HitActor, W.Damage, AimDir, Hit, Ctrl, Pawn, nullptr);
			}
		}
#if WITH_EDITOR
		DrawDebugLine(World, ViewLoc, ImpactPoint, FColor::Red, false, 0.25f, 0, 0.5f);
#endif
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
	// No gameplay readout on the main menu (this pawn can spawn there if the menu
	// level falls back to the gameplay game mode).
	if (UWorld* W = GetWorld())
	{
		if (W->GetMapName().Contains(TEXT("MainMenu")))
		{
			return;
		}
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
