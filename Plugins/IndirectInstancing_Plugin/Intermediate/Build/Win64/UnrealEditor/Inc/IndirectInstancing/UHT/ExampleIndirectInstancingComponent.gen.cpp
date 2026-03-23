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

// Begin Class UExampleIndirectInstancingComponent Function RegenerateRandomInstances
struct Z_Construct_UFunction_UExampleIndirectInstancingComponent_RegenerateRandomInstances_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Instancing|AutoSpawn" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Refill InstanceTransforms with new random instances and recreate the render state. */" },
#endif
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Refill InstanceTransforms with new random instances and recreate the render state." },
#endif
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UExampleIndirectInstancingComponent_RegenerateRandomInstances_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UExampleIndirectInstancingComponent, nullptr, "RegenerateRandomInstances", nullptr, nullptr, nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UExampleIndirectInstancingComponent_RegenerateRandomInstances_Statics::Function_MetaDataParams), Z_Construct_UFunction_UExampleIndirectInstancingComponent_RegenerateRandomInstances_Statics::Function_MetaDataParams) };
UFunction* Z_Construct_UFunction_UExampleIndirectInstancingComponent_RegenerateRandomInstances()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UExampleIndirectInstancingComponent_RegenerateRandomInstances_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UExampleIndirectInstancingComponent::execRegenerateRandomInstances)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->RegenerateRandomInstances();
	P_NATIVE_END;
}
// End Class UExampleIndirectInstancingComponent Function RegenerateRandomInstances

// Begin Class UExampleIndirectInstancingComponent
void UExampleIndirectInstancingComponent::StaticRegisterNativesUExampleIndirectInstancingComponent()
{
	UClass* Class = UExampleIndirectInstancingComponent::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "RegenerateRandomInstances", &UExampleIndirectInstancingComponent::execRegenerateRandomInstances },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
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
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Mesh_MetaData[] = {
		{ "Category", "Rendering" },
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_InstanceTransforms_MetaData[] = {
		{ "Category", "Instancing" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n\x09 * Per-instance transforms in component LOCAL SPACE.\n\x09 * The vertex shader applies the component's LocalToWorld on top, so (0,0,0)\n\x09 * means \"at the component's own location in the world\".\n\x09 * Leave empty with bAutoSpawnInstances=true to auto-generate random instances.\n\x09 */" },
#endif
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Per-instance transforms in component LOCAL SPACE.\nThe vertex shader applies the component's LocalToWorld on top, so (0,0,0)\nmeans \"at the component's own location in the world\".\nLeave empty with bAutoSpawnInstances=true to auto-generate random instances." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bAutoSpawnInstances_MetaData[] = {
		{ "Category", "Instancing|AutoSpawn" },
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RandomInstanceCount_MetaData[] = {
		{ "Category", "Instancing|AutoSpawn" },
		{ "ClampMax", "100000" },
		{ "ClampMin", "1" },
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SpawnRadius_MetaData[] = {
		{ "Category", "Instancing|AutoSpawn" },
		{ "ClampMin", "1.0" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Half-extent (cm) in local XY around the component origin. */" },
#endif
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Half-extent (cm) in local XY around the component origin." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ScaleMin_MetaData[] = {
		{ "Category", "Instancing|AutoSpawn" },
		{ "ClampMin", "0.01" },
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ScaleMax_MetaData[] = {
		{ "Category", "Instancing|AutoSpawn" },
		{ "ClampMin", "0.01" },
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_RandomSeed_MetaData[] = {
		{ "Category", "Instancing|AutoSpawn" },
		{ "ModuleRelativePath", "Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FObjectPropertyParams NewProp_Material;
	static const UECodeGen_Private::FObjectPropertyParams NewProp_Mesh;
	static const UECodeGen_Private::FStructPropertyParams NewProp_InstanceTransforms_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_InstanceTransforms;
	static void NewProp_bAutoSpawnInstances_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bAutoSpawnInstances;
	static const UECodeGen_Private::FIntPropertyParams NewProp_RandomInstanceCount;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_SpawnRadius;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ScaleMin;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_ScaleMax;
	static const UECodeGen_Private::FIntPropertyParams NewProp_RandomSeed;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UExampleIndirectInstancingComponent_RegenerateRandomInstances, "RegenerateRandomInstances" }, // 3736674088
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UExampleIndirectInstancingComponent>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_Material = { "Material", nullptr, (EPropertyFlags)0x0020080000000001, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UExampleIndirectInstancingComponent, Material), Z_Construct_UClass_UMaterialInterface_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Material_MetaData), NewProp_Material_MetaData) };
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_Mesh = { "Mesh", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UExampleIndirectInstancingComponent, Mesh), Z_Construct_UClass_UStaticMesh_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Mesh_MetaData), NewProp_Mesh_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_InstanceTransforms_Inner = { "InstanceTransforms", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FTransform, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_InstanceTransforms = { "InstanceTransforms", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UExampleIndirectInstancingComponent, InstanceTransforms), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_InstanceTransforms_MetaData), NewProp_InstanceTransforms_MetaData) };
void Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_bAutoSpawnInstances_SetBit(void* Obj)
{
	((UExampleIndirectInstancingComponent*)Obj)->bAutoSpawnInstances = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_bAutoSpawnInstances = { "bAutoSpawnInstances", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UExampleIndirectInstancingComponent), &Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_bAutoSpawnInstances_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bAutoSpawnInstances_MetaData), NewProp_bAutoSpawnInstances_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_RandomInstanceCount = { "RandomInstanceCount", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UExampleIndirectInstancingComponent, RandomInstanceCount), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RandomInstanceCount_MetaData), NewProp_RandomInstanceCount_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_SpawnRadius = { "SpawnRadius", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UExampleIndirectInstancingComponent, SpawnRadius), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SpawnRadius_MetaData), NewProp_SpawnRadius_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_ScaleMin = { "ScaleMin", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UExampleIndirectInstancingComponent, ScaleMin), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ScaleMin_MetaData), NewProp_ScaleMin_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_ScaleMax = { "ScaleMax", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UExampleIndirectInstancingComponent, ScaleMax), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ScaleMax_MetaData), NewProp_ScaleMax_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_RandomSeed = { "RandomSeed", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UExampleIndirectInstancingComponent, RandomSeed), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_RandomSeed_MetaData), NewProp_RandomSeed_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_Material,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_Mesh,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_InstanceTransforms_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_InstanceTransforms,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_bAutoSpawnInstances,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_RandomInstanceCount,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_SpawnRadius,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_ScaleMin,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_ScaleMax,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::NewProp_RandomSeed,
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
	FuncInfo,
	Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
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
		{ Z_Construct_UClass_UExampleIndirectInstancingComponent, UExampleIndirectInstancingComponent::StaticClass, TEXT("UExampleIndirectInstancingComponent"), &Z_Registration_Info_UClass_UExampleIndirectInstancingComponent, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UExampleIndirectInstancingComponent), 2162587664U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_1365839486(TEXT("/Script/IndirectInstancing"),
	Z_CompiledInDeferFile_FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
