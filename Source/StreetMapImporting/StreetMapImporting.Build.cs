// Copyright 2017 Mike Fricker. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class StreetMapImporting : ModuleRules
{
    public StreetMapImporting(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivatePCHHeaderFile = "StreetMapImporting.h";
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "XmlParser",
                "AssetTools",
                "Projects",
                "Slate",
                "EditorStyle",
                "SlateCore",
                "PropertyEditor",
                "RenderCore",
                "RHI",
                "RawMesh",
                "AssetTools",
                "AssetRegistry",
                "StreetMapRuntime"
            }
        );
    }
}