// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeIndirectInstancing_init() {}
	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_IndirectInstancing;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_IndirectInstancing()
	{
		if (!Z_Registration_Info_UPackage__Script_IndirectInstancing.OuterSingleton)
		{
			static const UECodeGen_Private::FPackageParams PackageParams = {
				"/Script/IndirectInstancing",
				nullptr,
				0,
				PKG_CompiledIn | 0x00000000,
				0x4D2A2494,
				0x9B709EF0,
				METADATA_PARAMS(0, nullptr)
			};
			UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_IndirectInstancing.OuterSingleton, PackageParams);
		}
		return Z_Registration_Info_UPackage__Script_IndirectInstancing.OuterSingleton;
	}
	static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_IndirectInstancing(Z_Construct_UPackage__Script_IndirectInstancing, TEXT("/Script/IndirectInstancing"), Z_Registration_Info_UPackage__Script_IndirectInstancing, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0x4D2A2494, 0x9B709EF0));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
