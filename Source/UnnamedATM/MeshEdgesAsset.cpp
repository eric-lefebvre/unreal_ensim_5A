#include "MeshEdgesAsset.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

#if WITH_EDITOR
void UMeshEdgesAsset::Generate()
{
	check(SourceMesh);
	const FStaticMeshRenderData* RenderData = SourceMesh->GetRenderData();
	check(RenderData && RenderData->LODResources.Num() > 0);

	const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
	const FPositionVertexBuffer& Positions = LOD.VertexBuffers.PositionVertexBuffer;
	const FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();

	// 1. Fusionner les sommets à la même position (dupliqués par les coutures d'UV et les arêtes dures)
	TMap<FVector, int32> PositionToId;
	TArray<FVector> Welded;
	TArray<int32> Remap;
	Remap.SetNum(Positions.GetNumVertices());

	for (uint32 i = 0; i < Positions.GetNumVertices(); ++i)
	{
		const FVector P = FVector(Positions.VertexPosition(i));
		const FVector Key = P.GridSnap(0.01);
		if (const int32* Found = PositionToId.Find(Key))
		{
			Remap[i] = *Found;
		}
		else
		{
			const int32 Id = Welded.Add(P);
			PositionToId.Add(Key, Id);
			Remap[i] = Id;
		}
	}

	// 2. Pour chaque arête, la normale des triangles qui la partagent
	TMap<TPair<int32, int32>, TArray<FVector>> EdgeNormals;

	for (int32 t = 0; t + 2 < Indices.Num(); t += 3)
	{
		const int32 Tri[3] = { Remap[Indices[t]], Remap[Indices[t + 1]], Remap[Indices[t + 2]] };
		if (Tri[0] == Tri[1] || Tri[1] == Tri[2] || Tri[0] == Tri[2])
		{
			continue; // triangle dégénéré
		}

		const FVector Normal = FVector::CrossProduct(
			Welded[Tri[1]] - Welded[Tri[0]],
			Welded[Tri[2]] - Welded[Tri[0]]).GetSafeNormal();

		for (int32 e = 0; e < 3; ++e)
		{
			const int32 A = Tri[e];
			const int32 B = Tri[(e + 1) % 3];
			EdgeNormals.FindOrAdd(TPair<int32, int32>(FMath::Min(A, B), FMath::Max(A, B))).Add(Normal);
		}
	}

	// 3. Garder les arêtes vives (angle > seuil) et les bords ouverts (un seul triangle)
	const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(AngleThresholdDeg));
	Edges.Reset();

	for (const TPair<TPair<int32, int32>, TArray<FVector>>& Entry : EdgeNormals)
	{
		const TArray<FVector>& Normals = Entry.Value;
		const bool bSharp = Normals.Num() != 2
			|| FVector::DotProduct(Normals[0], Normals[1]) < CosThreshold;

		if (bSharp)
		{
			FEdgeSegment Segment;
			Segment.Start = Welded[Entry.Key.Key];
			Segment.End = Welded[Entry.Key.Value];
			Edges.Add(Segment);
		}
	}

	MarkPackageDirty();
}
#endif