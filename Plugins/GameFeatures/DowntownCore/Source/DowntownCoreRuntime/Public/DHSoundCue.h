#pragma once

#include "GameplayCueNotify_Static.h"

#include "DHSoundCue.generated.h"

/**
 * Sounds of Downtown Hunting's gameplay cues (GameplayCue.DH.*), played at the cue's location (or the target's) on
 * every machine: the loadout abilities and the sniper's heavy report. One Blueprint child, /Game/GameplayCues/DH/
 * GCN_DH_Sounds with the tag GameplayCue.DH, makes the cue manager find it; it receives all GameplayCue.DH.* tags
 * (cues go to the closest parent tag that has a notify) and picks the sounds by the original tag.
 * Server: DHSoundCue::Play.
 */
UCLASS(Blueprintable)
class UDHSoundCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	/** Server: plays the GameplayCue.DH.<Name> sounds at the pawn for everyone. */
	static void Play(APawn* Pawn, const TCHAR* Name);

	/** Every machine: a big explosion (bomb, airstrike), Lyra's grenade blast with its outdoor distance layers. */
	static void PlayExplosion(const UObject* Context, const FVector& Location, float Volume);

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
