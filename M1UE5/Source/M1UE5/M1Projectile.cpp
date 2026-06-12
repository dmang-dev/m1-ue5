#include "M1Projectile.h"

#include "M1Audio.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "DrawDebugHelpers.h"

AM1Projectile::AM1Projectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Sphere root: small (~8cm) so it reads as a bolt/rocket, blocks the world,
	// and reports Hit events. The instigator pawn is ignored at spawn time in
	// Init so the shooter isn't hit point-blank.
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(8.0f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	// Pawns must block too, or rockets/bolts fly straight through enemies (and
	// enemy bolts through the player). The instigator alone is ignored in Init.
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionSphere->SetNotifyRigidBodyCollision(true);  // generate Hit events
	RootComponent = CollisionSphere;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;  // straight flight in v1

	// Speed is finalized in Init; clean up after ~5s so strays don't linger.
	InitialLifeSpan = 5.0f;
}

void AM1Projectile::Init(float Speed, float InDamage, float InBlastRadius, USoundBase* InExplosionSound)
{
	Damage = InDamage;
	BlastRadius = InBlastRadius;
	ExplosionSound = InExplosionSound;

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;
		// If movement already activated (deferred spawn finished), push the new
		// speed along the current facing so the bolt actually flies.
		ProjectileMovement->Velocity = GetActorForwardVector() * Speed;
	}

	// Ignore the shooter so the projectile doesn't collide point-blank at spawn.
	if (CollisionSphere)
	{
		if (APawn* InstigatorPawn = GetInstigator())
		{
			CollisionSphere->IgnoreActorWhenMoving(InstigatorPawn, true);
		}
		// Bind here (not in ctor) so a re-init never double-binds.
		CollisionSphere->OnComponentHit.AddDynamic(this, &AM1Projectile::OnHit);
	}
}

void AM1Projectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	UWorld* World = GetWorld();
	const FVector ImpactPoint = Hit.ImpactPoint.IsZero() ? GetActorLocation() : FVector(Hit.ImpactPoint);

	if (ExplosionSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(World, ExplosionSound, ImpactPoint, FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, M1DefaultAttenuation());
	}

	if (World)
	{
		const float BurstRadius = BlastRadius > 0.0f ? BlastRadius : 12.0f;
		DrawDebugSphere(World, ImpactPoint, BurstRadius, 12, FColor::Orange, false, 2.0f);
	}

	if (World && BlastRadius > 0.0f)
	{
		// Empty ignore list on purpose: Marathon rockets hurt their shooter too
		// (splash damage/rocket jumps), so the instigator is NOT exempt.
		UGameplayStatics::ApplyRadialDamage(this, Damage, ImpactPoint, BlastRadius,
			nullptr, TArray<AActor*>(), this, GetInstigatorController(), false,
			ECC_Visibility);
	}
	else if (OtherActor && OtherActor != this)
	{
		UGameplayStatics::ApplyPointDamage(OtherActor, Damage,
			GetVelocity().GetSafeNormal(), Hit, GetInstigatorController(), this, nullptr);
	}

	Destroy();
}
