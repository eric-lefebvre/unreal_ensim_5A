#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SoundListener.generated.h"

UINTERFACE(Blueprintable)
class UNNAMEDATM_API USoundListener : public UInterface
{
	GENERATED_BODY()
};

// À implémenter par le pawn joueur (Class Settings > Interfaces)
class UNNAMEDATM_API ISoundListener
{
	GENERATED_BODY()

public:
	// Appelé quand le front d'une onde audible atteint l'écouteur.
	// SourceLocation : origine de l'onde.
	// Range : portée de l'onde.
	// bThroughWalls : le trajet source -> écouteur traverse un obstacle.
	UFUNCTION(BlueprintImplementableEvent, Category = "Wave")
	void OnSoundHeard(FVector SourceLocation, float Range, bool bThroughWalls);
};
