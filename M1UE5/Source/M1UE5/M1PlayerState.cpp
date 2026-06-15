#include "M1PlayerState.h"
#include "Net/UnrealNetwork.h"

void AM1PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AM1PlayerState, TeamNum);
}

FLinearColor AM1PlayerState::GetPlayerColor() const
{
	static const FLinearColor TeamColors[] = {
		FLinearColor(0.90f, 0.10f, 0.10f),   // 0 red
		FLinearColor(0.12f, 0.30f, 0.95f),   // 1 blue
		FLinearColor(0.12f, 0.80f, 0.20f),   // 2 green
		FLinearColor(0.95f, 0.80f, 0.10f),   // 3 gold
	};
	if (TeamNum >= 0)
	{
		return TeamColors[TeamNum % 4];
	}
	// Free-for-all: a stable per-player hue from the player id.
	const uint8 Hue = static_cast<uint8>((GetPlayerId() * 61) & 0xFF);
	return FLinearColor::MakeFromHSV8(Hue, 200, 235);
}
