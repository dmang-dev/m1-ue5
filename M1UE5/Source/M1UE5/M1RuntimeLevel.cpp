#include "M1RuntimeLevel.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

AM1RuntimeLevel::AM1RuntimeLevel()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RuntimeMesh"));
	RootComponent = Mesh;
	Mesh->bUseComplexAsSimpleCollision = true;
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Master(
		TEXT("/Game/Runtime/M_M1Runtime.M_M1Runtime"));
	MasterMaterial = Master.Get();
}

FString AM1RuntimeLevel::ResolveDataRoot()
{
	// A root is "ready" only if it holds the converted intermediates (Geometry/).
	auto HasData = [](const FString& Dir)
	{
		return !Dir.IsEmpty() && FPaths::DirectoryExists(FPaths::Combine(Dir, TEXT("Geometry")));
	};

	// 1. Explicit override (testing / a custom install location).
	const FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("M1_DATA_ROOT"));
	if (HasData(Env))
	{
		return Env;
	}
	// 2. The per-user install cache the Asset Setup wizard writes (Phase 2 default):
	//    %LOCALAPPDATA%\M1UE5\Data -- no admin elevation, survives reinstalls.
	const FString AppData = FPaths::Combine(FPlatformProcess::UserSettingsDir(),
		TEXT("M1UE5"), TEXT("Data"));
	if (HasData(AppData))
	{
		return AppData;
	}
	// 3. In-project Reference/ (dev tree / running from the project dir).
	const FString ProjRef = FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"));
	if (HasData(ProjRef))
	{
		return ProjRef;
	}
	// 4. Dev fallback (last resort when launched from a staged build on this box).
	return TEXT("C:/m1-ue5/M1UE5/Reference");
}

bool AM1RuntimeLevel::IsDataAvailable()
{
	const FString GeoDir = FPaths::Combine(ResolveDataRoot(), TEXT("Geometry"));
	TArray<FString> Objs;
	IFileManager::Get().FindFiles(Objs, *(GeoDir / TEXT("*.obj")), true, false);
	return Objs.Num() > 0;
}

UTexture2D* AM1RuntimeLevel::LoadPNG(const FString& AbsPath)
{
	if (UTexture2D** Cached = TextureCache.Find(AbsPath))
	{
		return *Cached;
	}
	UTexture2D* Result = nullptr;
	TArray<uint8> File;
	if (FFileHelper::LoadFileToArray(File, *AbsPath))
	{
		IImageWrapperModule& Mod = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Wrapper = Mod.CreateImageWrapper(EImageFormat::PNG);
		TArray<uint8> Raw;
		if (Wrapper.IsValid() && Wrapper->SetCompressed(File.GetData(), File.Num())
			&& Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
		{
			const int32 W = Wrapper->GetWidth();
			const int32 H = Wrapper->GetHeight();
			Result = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
			if (Result)
			{
				if (FTexturePlatformData* PD = Result->GetPlatformData())
				{
					void* Dst = PD->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
					FMemory::Memcpy(Dst, Raw.GetData(), Raw.Num());
					PD->Mips[0].BulkData.Unlock();
				}
				Result->SRGB = true;                          // color art
				Result->Filter = TextureFilter::TF_Nearest;   // crisp M1 pixels
				Result->UpdateResource();
			}
		}
	}
	TextureCache.Add(AbsPath, Result);  // cache even null: don't re-attempt a bad path
	return Result;
}

TMap<FString, FString> AM1RuntimeLevel::ParseMTL(const FString& MtlPath)
{
	TMap<FString, FString> Out;
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *MtlPath))
	{
		return Out;
	}
	const FString MtlDir = FPaths::GetPath(MtlPath);
	FString Current;
	for (const FString& Raw : Lines)
	{
		const FString Line = Raw.TrimStartAndEnd();
		if (Line.StartsWith(TEXT("newmtl ")))
		{
			Current = Line.Mid(7).TrimStartAndEnd();
		}
		else if (Line.StartsWith(TEXT("map_Kd ")) && !Current.IsEmpty())
		{
			FString Rel = Line.Mid(7).TrimStartAndEnd();
			FString Abs = FPaths::ConvertRelativePathToFull(FPaths::Combine(MtlDir, Rel));
			Out.Add(Current, Abs);
		}
	}
	return Out;
}

namespace
{
	// Parse the leading integer of an OBJ face token "v", "v/vt" or "v/vt/vn".
	// OBJ indices are 1-based; negatives are relative to the end (rare here).
	void ParseFaceVert(const FString& Token, int32& OutPos, int32& OutUV)
	{
		OutPos = 0; OutUV = 0;
		TArray<FString> Parts;
		Token.ParseIntoArray(Parts, TEXT("/"), false);
		if (Parts.Num() >= 1 && !Parts[0].IsEmpty()) { OutPos = FCString::Atoi(*Parts[0]); }
		if (Parts.Num() >= 2 && !Parts[1].IsEmpty()) { OutUV = FCString::Atoi(*Parts[1]); }
	}

	// Per-surface emissive brightness from the material name's `_b<pct>` suffix
	// (scen_to_obj bakes the LITE level in). coll 8 = computer/terminal screens ->
	// always full-bright. Matches Tools/ue5_headless_build.py _make_material.
	float EmissiveBucket(const FString& MatName)
	{
		if (MatName.StartsWith(TEXT("tex_8_"))) { return 1.0f; }
		int32 BIdx;
		if (MatName.FindLastChar('b', BIdx) && BIdx > 0 && MatName[BIdx - 1] == '_')
		{
			const FString Num = MatName.Mid(BIdx + 1);
			if (Num.IsNumeric()) { return FMath::Clamp(FCString::Atoi(*Num) / 100.0f, 0.05f, 1.0f); }
		}
		return 1.0f;
	}
}

FString AM1RuntimeLevel::ResolveGeoBasename(const FString& Stem)
{
	const FString GeoDir = FPaths::Combine(ResolveDataRoot(), TEXT("Geometry"));
	if (FPaths::FileExists(FPaths::Combine(GeoDir, Stem + TEXT(".obj"))))
	{
		return Stem;  // exact match (e.g. "00_Arrival")
	}
	// Fall back to the leading "NN_" index prefix, which both naming schemes share.
	int32 Underscore = INDEX_NONE;
	if (Stem.FindChar(TEXT('_'), Underscore) && Underscore > 0)
	{
		const FString Prefix = Stem.Left(Underscore + 1);  // "20_"
		TArray<FString> Found;
		IFileManager::Get().FindFiles(Found, *(GeoDir / (Prefix + TEXT("*.obj"))), true, false);
		if (Found.Num() > 0)
		{
			return FPaths::GetBaseFilename(Found[0]);  // actual OBJ base name
		}
	}
	return Stem;
}

int32 AM1RuntimeLevel::BuildFromStem(const FString& Stem)
{
	const FString Root = ResolveDataRoot();
	const FString Base = ResolveGeoBasename(Stem);
	const FString ObjPath = FPaths::Combine(Root, TEXT("Geometry"), Base + TEXT(".obj"));
	const FString MtlPath = FPaths::Combine(Root, TEXT("Geometry"), Base + TEXT(".mtl"));

	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *ObjPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[M1Runtime] OBJ not found: %s"), *ObjPath);
		return 0;
	}
	const TMap<FString, FString> MatTex = ParseMTL(MtlPath);

	// Shared OBJ vertex pools (1-based in the file).
	TArray<FVector> Positions;
	TArray<FVector2D> UVs;
	Positions.Reserve(Lines.Num());

	// Per-material section accumulators.
	struct FSection
	{
		TArray<FVector> Verts;
		TArray<FVector2D> UV0;
		TArray<int32> Tris;
		TMap<uint64, int32> Dedup;   // (posIdx<<32 | uvIdx) -> local vertex index
	};
	TMap<FString, FSection> Sections;
	TArray<FString> SectionOrder;
	FString CurMat = TEXT("default");

	auto LocalVert = [&](FSection& S, int32 PosIdx, int32 UVIdx) -> int32
	{
		const uint64 Key = (static_cast<uint64>(static_cast<uint32>(PosIdx)) << 32)
			| static_cast<uint32>(UVIdx);
		if (int32* Found = S.Dedup.Find(Key)) { return *Found; }
		// OBJ is 1-based; negate Y to match the editor importer (the OBJ is authored
		// Z-up with Y pre-flipped by scen_to_obj; UE negates Y again on import).
		FVector P = Positions.IsValidIndex(PosIdx - 1) ? Positions[PosIdx - 1] : FVector::ZeroVector;
		P.Y = -P.Y;
		FVector2D UV = UVs.IsValidIndex(UVIdx - 1) ? UVs[UVIdx - 1] : FVector2D::ZeroVector;
		UV.Y = 1.0f - UV.Y;  // OBJ V is bottom-up; UE UV V is top-down
		const int32 Idx = S.Verts.Add(P);
		S.UV0.Add(UV);
		S.Dedup.Add(Key, Idx);
		return Idx;
	};

	for (const FString& Raw : Lines)
	{
		const FString Line = Raw.TrimStartAndEnd();
		if (Line.StartsWith(TEXT("v ")))
		{
			TArray<FString> T; Line.ParseIntoArray(T, TEXT(" "), true);
			if (T.Num() >= 4)
			{
				Positions.Add(FVector(FCString::Atod(*T[1]), FCString::Atod(*T[2]), FCString::Atod(*T[3])));
			}
		}
		else if (Line.StartsWith(TEXT("vt ")))
		{
			TArray<FString> T; Line.ParseIntoArray(T, TEXT(" "), true);
			if (T.Num() >= 3)
			{
				UVs.Add(FVector2D(FCString::Atod(*T[1]), FCString::Atod(*T[2])));
			}
		}
		else if (Line.StartsWith(TEXT("usemtl ")))
		{
			CurMat = Line.Mid(7).TrimStartAndEnd();
			if (!Sections.Contains(CurMat)) { Sections.Add(CurMat); SectionOrder.Add(CurMat); }
		}
		else if (Line.StartsWith(TEXT("f ")))
		{
			TArray<FString> T; Line.ParseIntoArray(T, TEXT(" "), true);
			if (T.Num() < 4) { continue; }  // need >= 3 verts (T[0] = "f")
			if (!Sections.Contains(CurMat)) { Sections.Add(CurMat); SectionOrder.Add(CurMat); }
			FSection& S = Sections[CurMat];
			// Fan-triangulate the polygon; reverse winding (i0,i2,i1) because the
			// Y-negation flips handedness, keeping faces consistently oriented.
			int32 P0, U0; ParseFaceVert(T[1], P0, U0);
			const int32 V0 = LocalVert(S, P0, U0);
			for (int32 i = 2; i + 1 <= T.Num() - 1; ++i)
			{
				int32 Pa, Ua, Pb, Ub;
				ParseFaceVert(T[i], Pa, Ua);
				ParseFaceVert(T[i + 1], Pb, Ub);
				const int32 Va = LocalVert(S, Pa, Ua);
				const int32 Vb = LocalVert(S, Pb, Ub);
				S.Tris.Add(V0); S.Tris.Add(Vb); S.Tris.Add(Va);  // reversed winding
			}
		}
	}

	// Emit a mesh section per material, with computed normals/tangents + collision.
	NumSections = 0; TotalVerts = 0; TotalTris = 0;
	int32 SectionIndex = 0;
	for (const FString& MatName : SectionOrder)
	{
		FSection& S = Sections[MatName];
		if (S.Verts.Num() == 0 || S.Tris.Num() == 0) { continue; }

		TArray<FVector> Normals;
		TArray<FProcMeshTangent> Tangents;
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
			S.Verts, S.Tris, S.UV0, Normals, Tangents);

		const TArray<FColor> NoColors;
		Mesh->CreateMeshSection(SectionIndex, S.Verts, S.Tris, Normals, S.UV0,
			NoColors, Tangents, /*bCreateCollision*/ true);

		if (MasterMaterial)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MasterMaterial, this);
			if (MID)
			{
				if (const FString* TexPath = MatTex.Find(MatName))
				{
					if (UTexture2D* Tex = LoadPNG(*TexPath))
					{
						MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
					}
				}
				MID->SetScalarParameterValue(TEXT("Emissive"), EmissiveBucket(MatName));
				Mesh->SetMaterial(SectionIndex, MID);
			}
		}

		TotalVerts += S.Verts.Num();
		TotalTris += S.Tris.Num() / 3;
		++SectionIndex;
		++NumSections;
	}

	BoundsExtent = Mesh->Bounds.BoxExtent;
	UE_LOG(LogTemp, Warning, TEXT("[M1Runtime] built '%s' from %s: %d sections, %d verts, %d tris, extent=(%.0f,%.0f,%.0f)"),
		*Stem, *Root, NumSections, TotalVerts, TotalTris, BoundsExtent.X, BoundsExtent.Y, BoundsExtent.Z);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(40, 12.0f, NumSections > 0 ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("M1Runtime %s: %d sections, %d tris"), *Stem, NumSections, TotalTris));
	}
	return NumSections;
}

bool AM1RuntimeLevel::LoadPlayerStart(const FString& Stem, FVector& OutLoc, float& OutYaw)
{
	const FString Path = FPaths::Combine(ResolveDataRoot(), TEXT("Geometry"),
		ResolveGeoBasename(Stem) + TEXT("_placements.json"));
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* PS = nullptr;
	if (!Root->TryGetObjectField(TEXT("player_start"), PS) || !PS || !PS->IsValid())
	{
		return false;
	}
	// Placements are already in the final UE space that matches the imported (and
	// thus the runtime-built) geometry -- no Y negation here.
	OutLoc = FVector((*PS)->GetNumberField(TEXT("x")),
	                 (*PS)->GetNumberField(TEXT("y")),
	                 (*PS)->GetNumberField(TEXT("z")));
	OutYaw = (*PS)->HasField(TEXT("facing_deg")) ? (*PS)->GetNumberField(TEXT("facing_deg")) : 0.0f;
	return true;
}
