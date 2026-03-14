// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "ExampleIndirectInstancing/ExampleIndirectInstancingActor.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef INDIRECTINSTANCING_ExampleIndirectInstancingActor_generated_h
#error "ExampleIndirectInstancingActor.generated.h already included, missing '#pragma once' in ExampleIndirectInstancingActor.h"
#endif
#define INDIRECTINSTANCING_ExampleIndirectInstancingActor_generated_h

#define FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingActor_h_12_INCLASS \
private: \
	static void StaticRegisterNativesAExampleIndirectInstancing(); \
	friend struct Z_Construct_UClass_AExampleIndirectInstancing_Statics; \
public: \
	DECLARE_CLASS(AExampleIndirectInstancing, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/IndirectInstancing"), INDIRECTINSTANCING_API) \
	DECLARE_SERIALIZER(AExampleIndirectInstancing)


#define FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingActor_h_12_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	INDIRECTINSTANCING_API AExampleIndirectInstancing(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(AExampleIndirectInstancing) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(INDIRECTINSTANCING_API, AExampleIndirectInstancing); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AExampleIndirectInstancing); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AExampleIndirectInstancing(AExampleIndirectInstancing&&); \
	AExampleIndirectInstancing(const AExampleIndirectInstancing&); \
public: \
	INDIRECTINSTANCING_API virtual ~AExampleIndirectInstancing();


#define FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingActor_h_9_PROLOG
#define FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingActor_h_12_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingActor_h_12_INCLASS \
	FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingActor_h_12_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> INDIRECTINSTANCING_API UClass* StaticClass<class AExampleIndirectInstancing>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_JetRacing_Plugins_IndirectInstancing_Plugin_Source_IndirectInstancing_Public_ExampleIndirectInstancing_ExampleIndirectInstancingActor_h


PRAGMA_ENABLE_DEPRECATION_WARNINGS
