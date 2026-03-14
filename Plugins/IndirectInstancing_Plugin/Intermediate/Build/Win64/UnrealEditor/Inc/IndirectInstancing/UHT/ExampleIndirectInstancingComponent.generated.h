// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef INDIRECTINSTANCING_ExampleIndirectInstancingComponent_generated_h
#error "ExampleIndirectInstancingComponent.generated.h already included, missing '#pragma once' in ExampleIndirectInstancingComponent.h"
#endif
#define INDIRECTINSTANCING_ExampleIndirectInstancingComponent_generated_h

#define FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_15_INCLASS \
private: \
	static void StaticRegisterNativesUExampleIndirectInstancingComponent(); \
	friend struct Z_Construct_UClass_UExampleIndirectInstancingComponent_Statics; \
public: \
	DECLARE_CLASS(UExampleIndirectInstancingComponent, UPrimitiveComponent, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/IndirectInstancing"), NO_API) \
	DECLARE_SERIALIZER(UExampleIndirectInstancingComponent)


#define FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_15_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UExampleIndirectInstancingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UExampleIndirectInstancingComponent) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UExampleIndirectInstancingComponent); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UExampleIndirectInstancingComponent); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	UExampleIndirectInstancingComponent(UExampleIndirectInstancingComponent&&); \
	UExampleIndirectInstancingComponent(const UExampleIndirectInstancingComponent&); \
public: \
	NO_API virtual ~UExampleIndirectInstancingComponent();


#define FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_12_PROLOG
#define FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_15_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_15_INCLASS \
	FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h_15_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> INDIRECTINSTANCING_API UClass* StaticClass<class UExampleIndirectInstancingComponent>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingComponent_h


PRAGMA_ENABLE_DEPRECATION_WARNINGS
