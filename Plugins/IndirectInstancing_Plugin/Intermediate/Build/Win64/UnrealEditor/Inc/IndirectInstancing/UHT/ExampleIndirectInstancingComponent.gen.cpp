// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "IndirectInstancing/Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeExampleIndirectInstancingComponent() {}

// Begin Cross Module References
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FTransform();
ENGINE_API UClass* Z_Construct_UClass_UMaterialInterface_NoRegister();
ENGINE_API UClass* Z_Construct_UClass_UPrimitiveComponent();
ENGINE_API UClass* Z_Construct_UClass_UStaticMesh_NoRegister();
INDIRECTINSTANCING_API UClass* Z_Construct_UClass_UExampleIndirectInstancingComponent();
INDIRECTINSTANCING_API UClass* Z_Construct_UClass_UExampleIndirectInstancingComponent_NoRegister();
UPackage* Z_Construct_UPackage__Script_IndirectInstancing();
// End Cross Module References

// Begin Class UExampleIndirectInstancingComponent
void UExampleIndirectInstancingComponent::StaticRegisterNativesUExampleIndirectInstancingComponent()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UExampleIndirectInstancingComponent);
UClass* Z_Construct_UClass_UExampleIndirectInstancingComponent_NoRegister()
{
	return UExampleIndirectInstancingComponent::StaticClass();
}
struct Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ClassGroupNames", "Rendering" },
		{ "HideCategories", "Activation Collision Cooking HLOD Navigation Object Physics VirtualTexture Mobility VirtualTexture Trigger" },
		{ "IncludePath", "ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
		{ "IsBlueprintBase", "true" },
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Material_MetaData[] = {
		{ "Category", "Rendering" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Material applied to each instance. */" },
#endif
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Material applied to each instance." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Mesh_MetaData[] = {
		{ "Category", "Rendering" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Static mesh to render via indirect instancing. */" },
#endif
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Static mesh to render via indirect instancing." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_InstanceTransforms_MetaData[] = {
		{ "Category", "Instancing" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n\x09 * World-space transforms for each instance.\n\x09 * Leave empty to render a single instance at the component's own transform.\n\x09 */" },
#endif
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "World-space transforms for each instance.\nLeave empty to render a single instance at the component's own transform." },
#endif
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FObjectPropertyParams NewProp_Material;
	static const UECodeGen_Private::FObjectPropertyParams NewProp_Mesh;
	static const UECodeGen_Private::FStructPropertyParams NewProp_InstanceTransforms_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_InstanceTransforms;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UExampleIndirectInstancingComponent>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_Material = { "Material", nullptr, (EPropertyFlags)0x0020080000000001, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UExampleIndirectInstancingComponent, Material), Z_Construct_UClass_UMaterialInterface_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Material_MetaData), NewProp_Material_MetaData) };
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_Mesh = { "Mesh", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UExampleIndirectInstancingComponent, Mesh), Z_Construct_UClass_UStaticMesh_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Mesh_MetaData), NewProp_Mesh_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_InstanceTransforms_Inner = { "InstanceTransforms", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FTransform, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_InstanceTransforms = { "InstanceTransforms", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UExampleIndirectInstancingComponent, InstanceTransforms), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_InstanceTransforms_MetaData), NewProp_InstanceTransforms_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_Material,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_Mesh,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_InstanceTransforms_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_InstanceTransforms,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UPrimitiveComponent,
	(UObject* (*)())Z_Construct_UPackage__Script_IndirectInstancing,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::ClassParams = {
	&UExampleIndirectInstancingComponent::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::PropPointers),
	0,
	0x00B000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::Class_MetaDataParams), Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UExampleIndirectInstancingComponent()
{
	if (!Z_Registration_Info_UClass_UExampleIndirectInstancingComponent.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UExampleIndirectInstancingComponent.OuterSingleton, Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UExampleIndirectInstancingComponent.OuterSingleton;
}
template<> INDIRECTINSTANCING_API UClass* StaticClass<UExampleIndirectInstancingComponent>()
{
	return UExampleIndirectInstancingComponent::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(UExampleIndirectInstancingComponent);
UExampleIndirectInstancingComponent::~UExampleIndirectInstancingComponent() {}
// End Class UExampleIndirectInstancingComponent

// Begin Registration
struct Z_CompiledInDeferFile_FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UExampleIndirectInstancingComponent, UExampleIndirectInstancingComponent::StaticClass, TEXT("UExampleIndirectInstancingComponent"), &Z_Registration_Info_UClass_UExampleIndirectInstancingComponent, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UExampleIndirectInstancingComponent), 1079833050U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_559376913(TEXT("/Script/IndirectInstancing"),
	Z_CompiledInDeferFile_FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
