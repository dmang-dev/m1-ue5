#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/Package.h"

/**
 * Shared distance attenuation for world-located SFX (enemy cries, ricochets,
 * explosions). The raw imported SoundWaves carry no attenuation asset, and
 * UGameplayStatics::SpawnSoundAtLocation without one plays UNATTENUATED and
 * UNSPATIALIZED -- a distant monster sounds like it's in your ear. Built once
 * at runtime (rooted against GC) instead of authoring a .uasset so the audio
 * pipeline stays fully scripted.
 */
inline USoundAttenuation* M1DefaultAttenuation()
{
	static USoundAttenuation* Attenuation = nullptr;
	if (!Attenuation)
	{
		Attenuation = NewObject<USoundAttenuation>(
			GetTransientPackage(), TEXT("M1DefaultAttenuation"));
		Attenuation->AddToRoot();
		FSoundAttenuationSettings& S = Attenuation->Attenuation;
		S.bAttenuate = true;
		S.bSpatialize = true;
		// Full volume within ~3m, natural falloff to silence by ~40m -- roughly
		// Marathon's audible range at 100cm/WU.
		S.AttenuationShapeExtents = FVector(300.0f, 0.0f, 0.0f);
		S.FalloffDistance = 4000.0f;
	}
	return Attenuation;
}
