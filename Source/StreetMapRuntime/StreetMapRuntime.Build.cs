// Copyright 2017 Mike Fricker. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class StreetMapRuntime : ModuleRules
{
    public StreetMapRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivatePCHHeaderFile = "StreetMapRuntime.h";
		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "Core",
				"CoreUObject",
				"Engine",
				"RHI",
				"RenderCore",
                "PropertyEditor"
            }
		);
	}
}