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
