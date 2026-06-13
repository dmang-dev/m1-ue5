#pragma once

#include "CoreMinimal.h"

/**
 * One-stop combat tuning. These are the knobs to turn after a playtest -- the
 * rest of the combat numbers (per-monster vitality/speed/damage, per-weapon
 * fire rates) are canonical Physics.phys values in the generated tables and
 * should NOT be hand-tuned here.
 *
 * Context for the defaults: there is no shield recharge yet (Marathon's
 * rechargers/pickups are a later system), so the player cannot heal within a
 * level and death restarts it. Until then the survivability levers are
 * GPlayerShields and GIncomingDamageScale -- raise shields or lower the scale
 * if the authentic full Pfhor population proves too punishing.
 */
namespace M1Tuning
{
	// --- Player ---
	// Marathon's starting shields are 150 (1x; canisters take it to 2x/3x).
	constexpr float GPlayerShields = 150.0f;

	// Suit oxygen pool (HUD's left gauge). Marathon drains oxygen ONLY on VACUUM
	// levels (Minf env bit 0 -- in M1 that's just L08 "G4 Sunbathing"). 100 = full
	// = the gauge max (a clean unit, not a canonical M1 value).
	constexpr float GPlayerOxygen = 100.0f;
	// Vacuum drain: full empties in ~150s. Out of air -> suffocation damage to
	// shields per second until the player refills (oxygen recharger) -- there is
	// no leaving vacuum mid-level since the whole level is vacuum.
	constexpr float GOxygenDrainPerSec = 0.67f;
	constexpr float GSuffocationDmgPerSec = 8.0f;

	// --- Player movement / camera feel (consolidated here for playtest iteration;
	//     these were scattered magic numbers in AM1Character). No behaviour change
	//     vs. the prior inline values -- relocated so a feel pass has one place to
	//     turn. ---
	// First-person FOV. Marathon rendered narrower than UE's 90 default; 75 reads
	// closer to the original and keeps off-axis wall terminals readable. The bezel
	// SceneCapture uses the same value so the framed view matches the camera.
	constexpr float GPlayerFOV = 75.0f;
	// Ground move speed. NOTE: Marathon's canonical max-forward maps to ~430 cm/s
	// at 100cm/WU; 600 is a touch arcadey -- a candidate to lower in a feel pass.
	constexpr float GWalkSpeed = 600.0f;
	constexpr float GJumpZVelocity = 420.0f;
	constexpr float GAirControl = 0.5f;
	// Near clip: tight Marathon rooms (~1m) need a small near plane so the camera
	// doesn't see through a wall when the player is pressed against it.
	constexpr float GNearClipCm = 5.0f;

	// Global multiplier on damage the player TAKES. 1.0 = canonical. Drop toward
	// ~0.5 for a gentler v1 while there is no way to recover shields.
	constexpr float GIncomingDamageScale = 1.0f;

	// --- Enemy engagement (cross-cutting; per-type ranges are in the table) ---
	// Melee strikes when the player is within (capsule radius + this).
	constexpr float GMeleeReachCm = 60.0f;
	// Cap on ranged engagement distance, so a sniper across a huge room doesn't
	// plink you from off-screen. Per-type canonical range is used when smaller.
	constexpr float GRangedRangeCapCm = 1500.0f;
	// Beat between an enemy waking and its first attack (reaction telegraph).
	constexpr float GFirstAttackDelay = 0.5f;
}
