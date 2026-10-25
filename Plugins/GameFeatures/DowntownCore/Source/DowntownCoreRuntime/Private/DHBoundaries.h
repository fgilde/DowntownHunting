#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * The invisible walls at the edge of the play area (the maps' blocking volumes) made visible on this client: each gets
 * a box with M_DH_Boundary, orange warning stripes that only fade in within about twelve metres of the camera, so the
 * edge shows up when you walk towards it and stays out of sight otherwise. The minimap marks the same walls
 * (UDHMinimap). Called by UDHGameRulesComponent on every client.
 */
namespace DHBoundaries
{
	void Show(UWorld* World);
}
