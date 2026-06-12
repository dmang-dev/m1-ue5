#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M1Projectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class USoundBase;

/**
 * A simple ballistic projectile for the energy/explosive Marathon weapons
 * (Fusion bolt, rocket). Spawned by UM1WeaponComponent for bProjectile weapons;
 * flies straight (no gravity in v1), and on contact plays an explosion SFX +
 * draws a debug burst, then destroys itself. Actual area damage is a later pass
 * (no enemies yet).
 */
UCLASS()
class M1UE5_API AM1Projectile : public AActor
{
	GENERATED_BODY()

public:
	AM1Projectile();

	/** Configure flight + payload. Call right after spawning, before the
	 *  projectile starts moving (e.g. between SpawnActorDeferred/FinishSpawning,
	 *  or immediately after a plain SpawnActor). */
	void Init(float Speed, float InDamage, float InBlastRadius, USoundBase* InExplosionSound);

protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

private:
	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	UProjectileMovementComponent* ProjectileMovement;

	/** Small unlit emissive tracer so the bolt/rocket reads in flight (no
	 *  collision; collision stays on the sphere root). */
	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	UStaticMeshComponent* TracerMesh;

	/** Base material the runtime emissive MID is built over (engine basic
	 *  material; may be null if not found -- then the tracer shows untinted). */
	UPROPERTY()
	UMaterialInterface* TracerBaseMaterial = nullptr;

	UPROPERTY()
	USoundBase* ExplosionSound = nullptr;

	float Damage = 0.0f;
	float BlastRadius = 0.0f;
};
