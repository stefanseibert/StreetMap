// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "StreetMapActor.h"


AStreetMapActor::AStreetMapActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	_cityGenerator(nullptr)
{
	StreetMapComponent = CreateDefaultSubobject<UStreetMapComponent>(TEXT("StreetMapComp"));
	RootComponent = StreetMapComponent;

	PrimaryActorTick.bCanEverTick = true;
}

void AStreetMapActor::Tick(float delta)
{
	Super::Tick(delta);

	if (_cityGenerator == nullptr)
	{
		AProceduralCityActor* cityGenerator = AProceduralCityActor::GetGeneratorInstance(GetWorld());
		if (cityGenerator)
		{
			_cityGenerator = cityGenerator;
			cityGenerator->RegisterDataSource(this, "RespondToQuadrantActivate");
		}
	}
}

void AStreetMapActor::RespondToQuadrantActivate(const float minLatitude, const float minLongitude, const float quadrantSize, const uint32 quadrantId)
{
	// TODO: Generate data for quadrant 

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5, FColor::Red, "Did receive quadrant activation delegate!");
	}
}