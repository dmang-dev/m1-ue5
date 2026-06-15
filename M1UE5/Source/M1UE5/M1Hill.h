#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M1Hill.generated.h"

class UStaticMeshComponent;

/**
 * Authored King-of-the-Hill marker: a flat translucent disc players stand on to
 * score. Place one in a level to define the hill; if a KOTH match finds none, the
 * game mode spawns one at the PlayerStarts centroid. The control radius is read
 * from Radius (the visual disc is scaled to match).
 */
UCLASS()
class M1UE5_API AM1Hill : public AActor
{
	GENERATED_BODY()

public:
	AM1Hill();

	/** Horizontal control radius, in UE cm. Players within this of the hill score. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "M1Net")
	float Radius = 500.0f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Disc = nullptr;
};
