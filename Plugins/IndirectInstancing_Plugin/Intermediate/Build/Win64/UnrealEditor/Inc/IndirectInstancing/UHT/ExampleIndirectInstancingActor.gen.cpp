// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "IndirectInstancing/Public/ExampleIndirectInstancing/ExampleIndirectInstancingActor.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeExampleIndirectInstancingActor() {}

// Begin Cross Module References
ENGINE_API UClass* Z_Construct_UClass_AActor();
INDIRECTINSTANCING_API UClass* Z_Construct_UClass_AExampleIndirectInstancing();
INDIRECTINSTANCING_API UClass* Z_Construct_UClass_AExampleIndirectInstancing_NoRegister();
INDIRECTINSTANCING_API UClass* Z_Construct_UClass_UExampleIndirectInstancingComponent_NoRegister();
UPackage* Z_Construct_UPackage__Script_IndirectInstancing();
// End Cross Module References

// Begin Class AExampleIndirectInstancing
void AExampleIndirectInstancing::StaticRegisterNativesAExampleIndirectInstancing()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(AExampleIndirectInstancing);
UClass* Z_Construct_UClass_AExampleIndirectInstancing_NoRegister()
{
	return AExampleIndirectInstancing::StaticClass();
}
struct Z_Construct_UClass_AExampleIndirectInstancing_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "HideCategories", "Cooking Input LOD" },
		{ "IncludePath", "ExampleIndirectInstancing/ExampleIndirectInstancingActor.h" },
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingActor.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ExampleIndirectInstancingComponent_MetaData[] = {
		{ "AllowPrivateAccess", "true" },
		{ "Category", "ExampleIndirectInstancing" },
		{ "EditInline", "true" },
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingActor.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FObjectPropertyParams NewProp_ExampleIndirectInstancingComponent;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AExampleIndirectInstancing>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_AExampleIndirectInstancing_Statics::NewProp_ExampleIndirectInstancingComponent = { "ExampleIndirectInstancingComponent", nullptr, (EPropertyFlags)0x00400000000a001d, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AExampleIndirectInstancing, ExampleIndirectInstancingComponent), Z_Construct_UClass_UExampleIndirectInstancingComponent_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ExampleIndirectInstancingComponent_MetaData), NewProp_ExampleIndirectInstancingComponent_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AExampleIndirectInstancing_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AExampleIndirectInstancing_Statics::NewProp_ExampleIndirectInstancingComponent,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AExampleIndirectInstancing_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_AExampleIndirectInstancing_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_IndirectInstancing,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AExampleIndirectInstancing_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AExampleIndirectInstancing_Statics::ClassParams = {
	&AExampleIndirectInstancing::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AExampleIndirectInstancing_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AExampleIndirectInstancing_Statics::PropPointers),
	0,
	0x008800A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AExampleIndirectInstancing_Statics::Class_MetaDataParams), Z_Construct_UClass_AExampleIndirectInstancing_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_AExampleIndirectInstancing()
{
	if (!Z_Registration_Info_UClass_AExampleIndirectInstancing.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AExampleIndirectInstancing.OuterSingleton, Z_Construct_UClass_AExampleIndirectInstancing_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AExampleIndirectInstancing.OuterSingleton;
}
template<> INDIRECTINSTANCING_API UClass* StaticClass<AExampleIndirectInstancing>()
{
	return AExampleIndirectInstancing::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(AExampleIndirectInstancing);
AExampleIndirectInstancing::~AExampleIndirectInstancing() {}
// End Class AExampleIndirectInstancing

// Begin Registration
struct Z_CompiledInDeferFile_FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingActor_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_AExampleIndirectInstancing, AExampleIndirectInstancing::StaticClass, TEXT("AExampleIndirectInstancing"), &Z_Registration_Info_UClass_AExampleIndirectInstancing, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AExampleIndirectInstancing), 2665356695U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingActor_h_3533951866(TEXT("/Script/IndirectInstancing"),
	Z_CompiledInDeferFile_FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingActor_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingActor_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
