// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapComponent.h"


UStreetMapComponent::UStreetMapComponent(const FObjectInitializer& ObjectInitializer)
	: URuntimeMeshComponent(ObjectInitializer),
	  StreetMap(nullptr)
{
	// We don't currently need to be ticked.  This can be overridden in a derived class though.
	PrimaryComponentTick.bCanEverTick = false;
	this->bAutoActivate = false;	// NOTE: Components instantiated through C++ are not automatically active, so they'll only tick once and then go to sleep!

	// We don't currently need InitializeComponent() to be called on us.  This can be overridden in a
	// derived class though.
	bWantsInitializeComponent = false;

	// Turn on shadows.  It looks better.
	CastShadow = true;

	// Our mesh is too complicated to be a useful occluder.
	bUseAsOccluder = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMaterialAsset(TEXT("/StreetMap/StreetMapDefaultMaterial"));
	StreetMapDefaultMaterial = DefaultMaterialAsset.Object;
}


void UStreetMapComponent::SetStreetMap(class UStreetMap* NewStreetMap, bool bClearPreviousMeshIfAny /*= false*/, bool bRebuildMesh /*= false */)
{
	if (StreetMap != NewStreetMap)
	{
		StreetMap = NewStreetMap;

		if (bClearPreviousMeshIfAny)
			this->ClearMeshSection(0);

		if (bRebuildMesh)
			BuildMesh();
	}
}


void UStreetMapComponent::GenerateMesh()
{
	/////////////////////////////////////////////////////////
	// Visual tweakables for generated Street Map mesh
	//
	const float RoadZ = MeshBuildSettings.RoadOffesetZ;
	const bool bWant3DBuildings = MeshBuildSettings.bWant3DBuildings;
	const float BuildingLevelFloorFactor = MeshBuildSettings.BuildingLevelFloorFactor;
	const bool bWantLitBuildings = MeshBuildSettings.bWantLitBuildings;
	const bool bWantBuildingBorderOnGround = !bWant3DBuildings;
	const float StreetThickness = MeshBuildSettings.StreetThickness;
	const FColor StreetColor = MeshBuildSettings.StreetColor.ToFColor( false );
	const float MajorRoadThickness = MeshBuildSettings.MajorRoadThickness;
	const FColor MajorRoadColor = MeshBuildSettings.MajorRoadColor.ToFColor( false );
	const float HighwayThickness = MeshBuildSettings.HighwayThickness;
	const FColor HighwayColor = MeshBuildSettings.HighwayColor.ToFColor( false );
	const float BuildingBorderThickness = MeshBuildSettings.BuildingBorderThickness;
	FLinearColor BuildingBorderLinearColor = MeshBuildSettings.BuildingBorderLinearColor;
	const float BuildingBorderZ = MeshBuildSettings.BuildingBorderZ;
	const FColor BuildingBorderColor( BuildingBorderLinearColor.ToFColor( false ) );
	const FColor BuildingFillColor( FLinearColor( BuildingBorderLinearColor * 0.33f ).CopyWithNewOpacity( 1.0f ).ToFColor( false ) );
	/////////////////////////////////////////////////////////

	if( StreetMap != nullptr )
	{
		FBox MeshBoundingBox;
		MeshBoundingBox.Init();

		const auto& Roads = StreetMap->GetRoads();
		const auto& Nodes = StreetMap->GetNodes();
		const auto& Buildings = StreetMap->GetBuildings();

		// Handling all roads in the street map file
		for( const auto& Road : Roads )
		{
			float RoadThickness = StreetThickness;
			FColor RoadColor = StreetColor;
			switch( Road.RoadType )
			{
				case EStreetMapRoadType::Highway:
					RoadThickness = HighwayThickness;
					RoadColor = HighwayColor;
					break;
					
				case EStreetMapRoadType::MajorRoad:
					RoadThickness = MajorRoadThickness;
					RoadColor = MajorRoadColor;
					break;
					
				case EStreetMapRoadType::Street:
				case EStreetMapRoadType::Other:
					break;
					
				default:
					check( 0 );
					break;
			}
			
			for( int32 PointIndex = 0; PointIndex < Road.RoadPoints.Num() - 1; ++PointIndex )
			{
				AddThick2DLine( 
					Road.RoadPoints[ PointIndex ],
					Road.RoadPoints[ PointIndex + 1 ],
					RoadZ,
					RoadThickness,
					RoadColor,
					RoadColor,
					MeshBoundingBox );
			}
		}
		
		TArray< int32 > TempIndices;
		TArray< int32 > TriangulatedVertexIndices;
		TArray< FVector > TempPoints;
		for( int32 BuildingIndex = 0; BuildingIndex < Buildings.Num(); ++BuildingIndex )
		{
			const auto& Building = Buildings[ BuildingIndex ];

			// Building mesh (or filled area, if the building has no height)

			// Triangulate this building
			// @todo: Performance: Triangulating lots of building polygons is quite slow.  We could easily do this 
			//        as part of the import process and store tessellated geometry instead of doing this at load time.
			bool WindsClockwise;
			if( FPolygonTools::TriangulatePolygon( Building.BuildingPoints, TempIndices, /* Out */ TriangulatedVertexIndices, /* Out */ WindsClockwise ) )
			{
				// @todo: Performance: We could preprocess the building shapes so that the points always wind
				//        in a consistent direction, so we can skip determining the winding above.

				const int32 FirstTopVertexIndex = this->Vertices.Num();

				// calculate fill Z for buildings
				// either use the defined height or extrapolate from building level count
				float BuildingFillZ = 0.0f;
				if (bWant3DBuildings) {
					if (Building.Height > 0) {
						BuildingFillZ = Building.Height;
					}
					else if (Building.BuildingLevels > 0) {
						BuildingFillZ = (float)Building.BuildingLevels * BuildingLevelFloorFactor;
					}
				}		

				// Top of building
				{
					TempPoints.SetNum( Building.BuildingPoints.Num(), false );
					for( int32 PointIndex = 0; PointIndex < Building.BuildingPoints.Num(); ++PointIndex )
					{
						TempPoints[ PointIndex ] = FVector( Building.BuildingPoints[ ( Building.BuildingPoints.Num() - PointIndex ) - 1 ], BuildingFillZ );
					}
					AddTriangles( TempPoints, TriangulatedVertexIndices, FVector::ForwardVector, FVector::UpVector, BuildingFillColor, MeshBoundingBox );
				}

				if( bWant3DBuildings && (Building.Height > KINDA_SMALL_NUMBER || Building.BuildingLevels > 0) )
				{
					// NOTE: Lit buildings can't share vertices beyond quads (all quads have their own face normals), so this uses a lot more geometry!
					if( bWantLitBuildings )
					{
						// Create edges for the walls of the 3D buildings
						for( int32 LeftPointIndex = 0; LeftPointIndex < Building.BuildingPoints.Num(); ++LeftPointIndex )
						{
							const int32 RightPointIndex = ( LeftPointIndex + 1 ) % Building.BuildingPoints.Num();

							TempPoints.SetNum( 4, false );

							const int32 TopLeftVertexIndex = 0;
							TempPoints[ TopLeftVertexIndex ] = FVector( Building.BuildingPoints[ WindsClockwise ? RightPointIndex : LeftPointIndex ], BuildingFillZ );

							const int32 TopRightVertexIndex = 1;
							TempPoints[ TopRightVertexIndex ] = FVector( Building.BuildingPoints[ WindsClockwise ? LeftPointIndex : RightPointIndex ], BuildingFillZ );

							const int32 BottomRightVertexIndex = 2;
							TempPoints[ BottomRightVertexIndex ] = FVector( Building.BuildingPoints[ WindsClockwise ? LeftPointIndex : RightPointIndex ], 0.0f );

							const int32 BottomLeftVertexIndex = 3;
							TempPoints[ BottomLeftVertexIndex ] = FVector( Building.BuildingPoints[ WindsClockwise ? RightPointIndex : LeftPointIndex ], 0.0f );


							TempIndices.SetNum( 6, false );

							TempIndices[ 0 ] = BottomLeftVertexIndex;
							TempIndices[ 1 ] = TopLeftVertexIndex;
							TempIndices[ 2 ] = BottomRightVertexIndex;

							TempIndices[ 3 ] = BottomRightVertexIndex;
							TempIndices[ 4 ] = TopLeftVertexIndex;
							TempIndices[ 5 ] = TopRightVertexIndex;

							const FVector FaceNormal = FVector::CrossProduct( ( TempPoints[ 0 ] - TempPoints[ 2 ] ).GetSafeNormal(), ( TempPoints[ 0 ] - TempPoints[ 1 ] ).GetSafeNormal() );
							const FVector ForwardVector = FVector::UpVector;
							const FVector UpVector = FaceNormal;
							AddTriangles( TempPoints, TempIndices, ForwardVector, UpVector, BuildingFillColor, MeshBoundingBox );
						}
					}
					else
					{
						// Create vertices for the bottom
						const int32 FirstBottomVertexIndex = this->Vertices.Num();
						for( int32 PointIndex = 0; PointIndex < Building.BuildingPoints.Num(); ++PointIndex )
						{
							const FVector2D Point = Building.BuildingPoints[ PointIndex ];

							FStreetMapVertex& NewVertex = *new( this->Vertices )FStreetMapVertex();
							NewVertex.Position = FVector( Point, 0.0f );
							NewVertex.UV0 = FVector2D( 0.0f, 0.0f );
							NewVertex.Tangent = FVector::ForwardVector;
							NewVertex.Normal = FVector::UpVector;
							NewVertex.Color = BuildingFillColor;

							MeshBoundingBox += NewVertex.Position;
						}

						// Create edges for the walls of the 3D buildings
						for( int32 LeftPointIndex = 0; LeftPointIndex < Building.BuildingPoints.Num(); ++LeftPointIndex )
						{
							const int32 RightPointIndex = ( LeftPointIndex + 1 ) % Building.BuildingPoints.Num();

							const int32 BottomLeftVertexIndex = FirstBottomVertexIndex + LeftPointIndex;
							const int32 BottomRightVertexIndex = FirstBottomVertexIndex + RightPointIndex;
							const int32 TopRightVertexIndex = FirstTopVertexIndex + RightPointIndex;
							const int32 TopLeftVertexIndex = FirstTopVertexIndex + LeftPointIndex;

							this->Indices.Add( BottomLeftVertexIndex );
							this->Indices.Add( TopLeftVertexIndex );
							this->Indices.Add( BottomRightVertexIndex );

							this->Indices.Add( BottomRightVertexIndex );
							this->Indices.Add( TopLeftVertexIndex );
							this->Indices.Add( TopRightVertexIndex );
						}
					}
				}
			}
			else
			{
				// @todo: Triangulation failed for some reason, possibly due to degenerate polygons.  We can
				//        probably improve the algorithm to avoid this happening.
			}

			// Building border
			if( bWantBuildingBorderOnGround )
			{
				for( int32 PointIndex = 0; PointIndex < Building.BuildingPoints.Num(); ++PointIndex )
				{
					AddThick2DLine(
						Building.BuildingPoints[ PointIndex ],
						Building.BuildingPoints[ ( PointIndex + 1 ) % Building.BuildingPoints.Num() ],
						BuildingBorderZ,
						BuildingBorderThickness,		// Thickness
						BuildingBorderColor,
						BuildingBorderColor,
						MeshBoundingBox );
				}
			}
		}
	}

	this->CreateMeshSection(0, Vertices, Indices, false, EUpdateFrequency::Average, ESectionUpdateFlags::None);
}


#if WITH_EDITOR
void UStreetMapComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Check to see if the "StreetMap" property changed.
	if (PropertyChangedEvent.Property != nullptr)
	{
		const FName PropertyName(PropertyChangedEvent.Property->GetFName());
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UStreetMapComponent, StreetMap))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.NotifyCustomizationModuleChanged();
		}
	}

	// Call the parent implementation of this function
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif	// WITH_EDITOR


void UStreetMapComponent::BuildMesh()
{
	ClearMesh();
	GenerateMesh();
	MarkRenderStateDirty();
	AssignDefaultMaterialIfNeeded();
	Modify();
}


void UStreetMapComponent::AssignDefaultMaterialIfNeeded()
{
	if (this->GetNumMaterials() == 0 || this->GetMaterial(0) == nullptr)
	{
		if (!HasValidMesh() || GetDefaultMaterial() == nullptr)
			return;

		this->SetMaterial(0, GetDefaultMaterial());
	}
}


void UStreetMapComponent::ClearMesh()
{
	Vertices.Reset();
	Indices.Reset();
}


void UStreetMapComponent::AddThick2DLine( const FVector2D Start, const FVector2D End, const float Z, const float Thickness, const FColor& StartColor, const FColor& EndColor, FBox& MeshBoundingBox )
{
	const float HalfThickness = Thickness * 0.5f;

	const FVector2D LineDirection = ( End - Start ).GetSafeNormal();
	const FVector2D RightVector( -LineDirection.Y, LineDirection.X );

	const int32 BottomLeftVertexIndex = Vertices.Num();
	FStreetMapVertex& BottomLeftVertex = *new( Vertices )FStreetMapVertex();

	BottomLeftVertex.Position = FVector( Start - RightVector * HalfThickness, Z );
	BottomLeftVertex.UV0 = FVector2D( 0.0f, 0.0f );
	BottomLeftVertex.Tangent = FVector( LineDirection, 0.0f );
	BottomLeftVertex.Normal = FVector::UpVector;
	BottomLeftVertex.Color = StartColor;
	MeshBoundingBox += BottomLeftVertex.Position;

	const int32 BottomRightVertexIndex = Vertices.Num();
	FStreetMapVertex& BottomRightVertex = *new( Vertices )FStreetMapVertex();
	BottomRightVertex.Position = FVector( Start + RightVector * HalfThickness, Z );
	BottomRightVertex.UV0 = FVector2D( 1.0f, 0.0f );
	BottomRightVertex.Tangent = FVector( LineDirection, 0.0f );
	BottomRightVertex.Normal = FVector::UpVector;
	BottomRightVertex.Color = StartColor;
	MeshBoundingBox += BottomRightVertex.Position;

	const int32 TopRightVertexIndex = Vertices.Num();
	FStreetMapVertex& TopRightVertex = *new( Vertices )FStreetMapVertex();
	TopRightVertex.Position = FVector( End + RightVector * HalfThickness, Z );
	TopRightVertex.UV0 = FVector2D( 1.0f, 1.0f );
	TopRightVertex.Tangent = FVector( LineDirection, 0.0f );
	TopRightVertex.Normal = FVector::UpVector;
	TopRightVertex.Color = EndColor;
	MeshBoundingBox += TopRightVertex.Position;

	const int32 TopLeftVertexIndex = Vertices.Num();
	FStreetMapVertex& TopLeftVertex = *new( Vertices )FStreetMapVertex();
	TopLeftVertex.Position = FVector( End - RightVector * HalfThickness, Z );
	TopLeftVertex.UV0 = FVector2D( 0.0f, 1.0f );
	TopLeftVertex.Tangent = FVector( LineDirection, 0.0f );
	TopLeftVertex.Normal = FVector::UpVector;
	TopLeftVertex.Color = EndColor;
	MeshBoundingBox += TopLeftVertex.Position;

	Indices.Add( BottomLeftVertexIndex );
	Indices.Add( BottomRightVertexIndex );
	Indices.Add( TopRightVertexIndex );

	Indices.Add( BottomLeftVertexIndex );
	Indices.Add( TopRightVertexIndex );
	Indices.Add( TopLeftVertexIndex );
};


void UStreetMapComponent::AddTriangles( const TArray<FVector>& Points, const TArray<int32>& PointIndices, const FVector& ForwardVector, const FVector& UpVector, const FColor& Color, FBox& MeshBoundingBox )
{
	const int32 FirstVertexIndex = Vertices.Num();

	for( FVector Point : Points )
	{
		FStreetMapVertex& NewVertex = *new( Vertices )FStreetMapVertex();
		NewVertex.Position = Point;
		NewVertex.UV0 = FVector2D( 0.0f, 0.0f );
		NewVertex.Tangent = ForwardVector;
		NewVertex.Normal = UpVector;
		NewVertex.Color = Color;

		MeshBoundingBox += NewVertex.Position;
	}

	for( int32 PointIndex : PointIndices )
	{
		Indices.Add( FirstVertexIndex + PointIndex );
	}
};


FString UStreetMapComponent::GetStreetMapAssetName() const
{
	return StreetMap != nullptr ? StreetMap->GetName() : FString(TEXT("NONE"));
}

