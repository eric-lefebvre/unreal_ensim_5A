#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EdgeRenderer.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class UNNAMEDATM_API AEdgeRenderer : public AActor
{
	GENERATED_BODY()

public:
	AEdgeRenderer();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Edges")
	TObjectPtr<UInstancedStaticMeshComponent> Lines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Edges")
	float Thickness = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Edges")
	float HoldTime = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Edges")
	float FadeTime = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Edges")
	float Intensity = 1.f;

	// Crée ou met à jour l'instance d'un segment. Renvoie son index.
	// N'envoie rien au rendu : appeler Flush() après une série de SetSegment.
	int32 SetSegment(int32 InstanceIndex, const FVector& A, const FVector& B, float RevealTime, float Brightness);

	// Envoie au rendu toutes les modifications faites depuis le dernier Flush.
	void Flush();

protected:
	virtual void BeginPlay() override;
};
