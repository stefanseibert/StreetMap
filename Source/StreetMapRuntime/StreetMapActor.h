// Copyright 2017 Mike Fricker. All Rights Reserved.
#pragma once

#include "StreetMapRuntime.h"
#include "StreetMapComponent.h"
#include "ProceduralCityActor.h"
#include "StreetMapActor.generated.h"


/** An actor that renders a street map mesh component */
UCLASS(hidecategories = (Physics)) // Physics category in detail panel is hidden. Our component/Actor is not simulated !
class STREETMAPRUNTIME_API AStreetMapActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/**  Component that represents a section of street map roads and buildings */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StreetMap")
		class UStreetMapComponent* StreetMapComponent;

private:
	AProceduralCityActor* _cityGenerator;

public: 
	FORCEINLINE class UStreetMapComponent* GetStreetMapComponent() { return StreetMapComponent; }

	virtual void Tick(float delta) override;

	UFUNCTION()
	void RespondToQuadrantActivate(const float minLatitude, const float minLongitude, const float quadrantSize, const uint32 quadrantId);
};
