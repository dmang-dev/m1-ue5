#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "M1Enemy.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class USoundBase;
struct FM1MonsterRow;
struct FM1SpriteState;

/**
 * A Marathon 1 monster: a billboarded sprite card on an ACharacter capsule,
 * data-driven from the generated GM1MonsterTable (Tools/gen_monster_assets.py).
 *
 * Marathon-style behavior, deliberately WITHOUT AIController/navmesh:
 *   Idle -> (player inside VisualRange + line of sight) -> Chase: steer straight
 *   at the player via AddMovementInput, sidestep briefly when blocked, attack on
 *   a fixed interval inside attack range (melee damage or an AM1Projectile).
 * Damage arrives through the standard UE TakeDamage path (ApplyDamage /
 * ApplyRadialDamage from the player's weapons), so no special casing anywhere.
 */
UCLASS()
class M1UE5_API AM1Enemy : public ACharacter
{
	GENERATED_BODY()

public:
	AM1Enemy();

	/** Look up a generated monster row (nullptr if the type was skipped). */
	static const FM1MonsterRow* FindRow(int32 MonsterType);

	/** Apply a monster type's stats, sprites and sounds. Call right after
	 *  SpawnActor. Returns false (and leaves the enemy inert) for unknown types.
	 *  Missing textures/sounds are tolerated: the enemy still fights, it just
	 *  renders with the default material / stays silent. */
	bool InitFromType(int32 MonsterType);

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

private:
	enum class EM1EnemyState : uint8
	{
		Idle,
		Chase,
		Dead,
	};

	void SetSpriteTexture(UTexture2D* Texture);
	bool HasLineOfSight(const FVector& TargetLocation) const;

	/** Load + cache a shared sprite texture by asset path (TMap so we never
	 *  LoadObject per frame). Returns nullptr (and caches the miss) when the
	 *  texture isn't imported yet -- callers must tolerate that. */
	UTexture2D* GetSharedTexture(const TCHAR* Path);

	/** Pick the (view, frame) shared texture for a state given the camera's
	 *  position relative to the enemy's facing. Marathon mirrors 5 views to 8;
	 *  see the d->(view,mirror) table in the .cpp. Sets bOutFlip. nullptr on a
	 *  missing/empty state or a not-yet-imported texture. */
	UTexture2D* PickViewTexture(const FM1SpriteState& StateData, int32 Frame,
		bool& bOutFlip) const;

	/** Apply a state's sprite for this tick: compute direction, advance frame,
	 *  swap the texture, and set the horizontal flip via the plane's scale Y. */
	void UpdateSpriteForState(const FM1SpriteState& StateData, int32 Frame);

	/** Placement Z is floor-relative: drop a trace and rest the capsule on the
	 *  sector mesh under the spawn point. */
	void SnapToFloor();

	void Activate(float Now);

	/** Land an attack on the player. bMelee = a close-range melee hit; otherwise
	 *  fire an AM1Projectile (the firer's Instigator, so it ignores itself but
	 *  CAN hit other monsters -- Marathon infighting). */
	void PerformAttack(APawn* Player, bool bMelee);
	void Die();

	/** Camera-facing sprite card (engine Plane + M_M1Sprite dynamic instance). */
	UPROPERTY(VisibleAnywhere, Category = "Sprite")
	UStaticMeshComponent* SpritePlane;

	/** Master sprite material (found in ctor; null until sprites are imported). */
	UPROPERTY()
	UMaterialInterface* SpriteMaterial = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* SpriteMID = nullptr;

	UPROPERTY()
	UTexture2D* CurrentTexture = nullptr;

	/** Shared-texture cache keyed by asset path. Values may be null (cached miss
	 *  for a not-yet-imported texture) so we don't re-attempt LoadObject. */
	UPROPERTY()
	TMap<FString, UTexture2D*> TextureCache;

	UPROPERTY()
	USoundBase* ActivationSound = nullptr;

	UPROPERTY()
	USoundBase* ProjFlybySound = nullptr;

	UPROPERTY()
	USoundBase* MeleeSound = nullptr;

	UPROPERTY()
	USoundBase* RandomSound = nullptr;

	UPROPERTY()
	USoundBase* DeathSound = nullptr;

	/** Row in the generated static table (never freed; safe to keep a pointer). */
	const FM1MonsterRow* Row = nullptr;

	EM1EnemyState State = EM1EnemyState::Idle;
	float HitPoints = 1.0f;
	bool bHasLOS = false;

	/** Magnitudes of the sprite plane's relative scale, captured in InitFromType.
	 *  The billboard horizontal flip is applied by negating scale Y (the plane's
	 *  local Y is its horizontal axis after the ctor's roll+90/yaw-90); X/Z keep
	 *  their sign. */
	float FlipScaleX = 1.0f;
	float FlipScaleY = 1.0f;
	float FlipScaleZ = 1.0f;
	int32 FlipSign = 0;  // 0 = unset, so the first apply always writes

	/** The enemy's logical FACING yaw (world deg) for view selection. The actor
	 *  yaw itself is the billboard (always at the camera), so facing is tracked
	 *  separately: toward the player while chasing, else the spawn facing. */
	float FacingYawDeg = 0.0f;

	/** Staggered sense/LOS timer so 300+ enemies never trace on the same frame. */
	float NextSenseTime = 0.0f;
	float NextAttackTime = 0.0f;
	float NextRandomSoundTime = 0.0f;

	/** Marathon-ish unstick: when blocked while chasing, veer 45 deg for 0.5s. */
	float SidestepEndTime = -1.0f;
	float SidestepSign = 1.0f;

	float AnimTime = 0.0f;
	int32 AnimFrame = -1;
};
