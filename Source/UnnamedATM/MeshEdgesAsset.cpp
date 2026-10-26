#include "MeshEdgesAsset.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

#if WITH_EDITOR
namespace
{
	struct FWorkSegment
	{
		FVector Start;
		FVector End;
		float Angle;   // angle (degrés) porté par ce segment
	};

	// Douglas-Peucker : ajoute à Out les points gardés entre First et Last, sauf Last
	void SimplifyRange(const TArray<FVector>& Points, int32 First, int32 Last, float Tolerance, TArray<FVector>& Out)
	{
		float MaxDist = -1.f;
		int32 MaxIndex = First;
		for (int32 i = First + 1; i < Last; ++i)
		{
			const float Dist = FMath::PointDistToSegment(Points[i], Points[First], Points[Last]);
			if (Dist > MaxDist)
			{
				MaxDist = Dist;
				MaxIndex = i;
			}
		}

		if (MaxDist > Tolerance)
		{
			SimplifyRange(Points, First, MaxIndex, Tolerance, Out);
			SimplifyRange(Points, MaxIndex, Last, Tolerance, Out);
		}
		else
		{
			Out.Add(Points[First]);
		}
	}

	TArray<FVector> Simplify(const TArray<FVector>& Points, bool bClosed, float Tolerance)
	{
		TArray<FVector> Out;
		if (bClosed)
		{
			int32 Far = 0;
			float FarDist = -1.f;
			for (int32 i = 1; i < Points.Num() - 1; ++i)
			{
				const float Dist = FVector::DistSquared(Points[0], Points[i]);
				if (Dist > FarDist)
				{
					FarDist = Dist;
					Far = i;
				}
			}
			SimplifyRange(Points, 0, Far, Tolerance, Out);
			SimplifyRange(Points, Far, Points.Num() - 1, Tolerance, Out);
		}
		else
		{
			SimplifyRange(Points, 0, Points.Num() - 1, Tolerance, Out);
		}
		Out.Add(Points.Last());
		return Out;
	}

	// Tous les points de Candidate sont-ils à moins de Distance du segment Rep ?
	bool IsCoveredBy(const FWorkSegment& Candidate, const FWorkSegment& Rep, float Distance)
	{
		const float Step = FMath::Max(Distance, 0.1f);
		const int32 Samples = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Candidate.Start, Candidate.End) / Step));
		for (int32 i = 0; i <= Samples; ++i)
		{
			const FVector P = FMath::Lerp(Candidate.Start, Candidate.End, static_cast<float>(i) / Samples);
			if (FMath::PointDistToSegment(P, Rep.Start, Rep.End) > Distance)
			{
				return false;
			}
		}
		return true;
	}
}

void UMeshEdgesAsset::Generate()
{
	check(SourceMesh);
	const FStaticMeshRenderData* RenderData = SourceMesh->GetRenderData();
	check(RenderData && RenderData->LODResources.Num() > 0);

	const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
	const FPositionVertexBuffer& Positions = LOD.VertexBuffers.PositionVertexBuffer;
	const FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();

	// 1. Fusionner les sommets à la même position
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

	// 2. Normales des triangles adjacents à chaque arête
	TMap<TPair<int32, int32>, TArray<FVector>> EdgeNormals;

	for (int32 t = 0; t + 2 < Indices.Num(); t += 3)
	{
		const int32 Tri[3] = { Remap[Indices[t]], Remap[Indices[t + 1]], Remap[Indices[t + 2]] };
		if (Tri[0] == Tri[1] || Tri[1] == Tri[2] || Tri[0] == Tri[2])
		{
			continue;
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

	// 3. Arêtes candidates, avec leur angle (180° pour un bord ouvert)
	TArray<TPair<int32, int32>> Selected;
	TArray<float> SelectedAngle;

	for (const TPair<TPair<int32, int32>, TArray<FVector>>& Entry : EdgeNormals)
	{
		const TArray<FVector>& Normals = Entry.Value;
		const float Angle = (Normals.Num() != 2)
			? 180.f
			: FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Normals[0], Normals[1]), -1.0, 1.0)));

		if (Angle >= MinStepAngleDeg)
		{
			Selected.Add(Entry.Key);
			SelectedAngle.Add(Angle);
		}
	}

	// 4. Chaînage
	TMap<int32, TArray<int32>> VertexEdges;
	for (int32 e = 0; e < Selected.Num(); ++e)
	{
		VertexEdges.FindOrAdd(Selected[e].Key).Add(e);
		VertexEdges.FindOrAdd(Selected[e].Value).Add(e);
	}

	TArray<bool> Used;
	Used.Init(false, Selected.Num());
	TArray<FWorkSegment> Work;

	// Parcourt une chaîne, la simplifie, et ajoute ses segments avec l'angle moyen de la chaîne
	auto WalkAndAdd = [&](int32 StartVertex, int32 FirstEdge, bool bClosed)
		{
			TArray<FVector> Points = { Welded[StartVertex] };
			float AngleSum = 0.f;
			int32 EdgeCount = 0;
			int32 Current = StartVertex;
			int32 Edge = FirstEdge;

			while (true)
			{
				Used[Edge] = true;
				AngleSum += SelectedAngle[Edge];
				++EdgeCount;

				const int32 Next = (Selected[Edge].Key == Current) ? Selected[Edge].Value : Selected[Edge].Key;
				Points.Add(Welded[Next]);

				const TArray<int32>& Around = VertexEdges[Next];
				if (Next == StartVertex || Around.Num() != 2)
				{
					break;
				}
				const int32 Other = (Around[0] == Edge) ? Around[1] : Around[0];
				if (Used[Other])
				{
					break;
				}
				Current = Next;
				Edge = Other;
			}

			const float Angle = AngleSum / EdgeCount;
			const TArray<FVector> Simplified = Simplify(Points, bClosed, SimplifyTolerance);
			for (int32 i = 0; i + 1 < Simplified.Num(); ++i)
			{
				Work.Add({ Simplified[i], Simplified[i + 1], Angle });
			}
		};

	for (const TPair<int32, TArray<int32>>& Entry : VertexEdges)
	{
		if (Entry.Value.Num() == 2)
		{
			continue;
		}
		for (const int32 Edge : Entry.Value)
		{
			if (!Used[Edge])
			{
				WalkAndAdd(Entry.Key, Edge, false);
			}
		}
	}
	for (int32 e = 0; e < Selected.Num(); ++e)
	{
		if (!Used[e])
		{
			WalkAndAdd(Selected[e].Key, e, true);
		}
	}

	// 5. Regroupement : les segments collés au même représentant additionnent leurs angles
	Work.Sort([](const FWorkSegment& A, const FWorkSegment& B)
		{
			return FVector::DistSquared(A.Start, A.End) > FVector::DistSquared(B.Start, B.End);
		});

	TArray<FWorkSegment> Reps;
	TArray<float> RepTotalAngle;

	for (const FWorkSegment& Candidate : Work)
	{
		int32 Found = INDEX_NONE;
		for (int32 r = 0; r < Reps.Num(); ++r)
		{
			if (IsCoveredBy(Candidate, Reps[r], MergeDistance))
			{
				Found = r;
				break;
			}
		}

		if (Found == INDEX_NONE)
		{
			Reps.Add(Candidate);
			RepTotalAngle.Add(Candidate.Angle);
		}
		else
		{
			RepTotalAngle[Found] += Candidate.Angle;
		}
	}

	// 6. Garder les groupes dont l'angle total atteint le seuil
	Edges.Reset();
	for (int32 r = 0; r < Reps.Num(); ++r)
	{
		if (RepTotalAngle[r] >= AngleThresholdDeg)
		{
			Edges.Add({ Reps[r].Start, Reps[r].End });
		}
	}

	UE_LOG(LogTemp, Display, TEXT("Generate %s : %d candidates, %d segments, %d groupes, %d gardes"),
		*SourceMesh->GetName(), Selected.Num(), Work.Num(), Reps.Num(), Edges.Num());

	MarkPackageDirty();
}
#endif