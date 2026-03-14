using UnrealBuildTool; 
using System.IO;

public class IndirectInstancing: ModuleRules 

{ 

	public IndirectInstancing(ReadOnlyTargetRules Target) : base(Target) 

	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// Add Renderer Private/Internal include paths (UE5.6 workaround)
		string EnginePath = Path.GetFullPath(Target.RelativeEnginePath);
		string RendererPath = Path.Combine(EnginePath, "Source/Runtime/Renderer");

		PrivateIncludePaths .AddRange(new string[]
		{
			Path.Combine(RendererPath, "Private"),
			Path.Combine(RendererPath, "Internal")
			
		});
		PrivateIncludePaths.AddRange(new string[] 
		{
			//"Runtime/Renderer/Private",
			"IndirectInstancing/Private"
		});
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("TargetPlatform");
		}
		PublicDependencyModuleNames.Add("Core");
		PublicDependencyModuleNames.Add("Engine");
		PublicDependencyModuleNames.Add("MaterialShaderQualitySettings");
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Renderer",
			"RenderCore",
			"RHI",
			"Projects"
		});
		
		if (Target.bBuildEditor == true)
		{

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"UnrealEd",
					"MaterialUtilities",
					"SlateCore",
					"Slate"
				}
			);

			CircularlyReferencedDependentModules.AddRange(
				new string[] {
					"UnrealEd",
					"MaterialUtilities",
				}
			);
		}
	} 

}