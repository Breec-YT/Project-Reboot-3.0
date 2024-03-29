#pragma once

#include "reboot.h"
#include "Stack.h"
#include "Actor.h"
#include "hooking.h"
#include "SoftObjectPtr.h"
#include "FortGameModeAthena.h"
#include "GameplayStatics.h"

// Vehicle class name changes multiple times across versions, so I made it it's own file.

static inline void ServerVehicleUpdate(UObject* Context, FFrame& Stack, void* Ret)
{
	auto Params = Stack.Locals;

	auto Vehicle = Cast<AActor>(Context);
	static auto RootComponentOffset = Vehicle->GetOffset("RootComponent");
	auto Mesh = /* Cast<UPrimitiveComponent> */(Vehicle->Get(RootComponentOffset));

	FTransform Transform{};

	static std::string StateStructName = FindObject("/Script/FortniteGame.ReplicatedPhysicsPawnState") ? "/Script/FortniteGame.ReplicatedPhysicsPawnState" : "/Script/FortniteGame.ReplicatedAthenaVehiclePhysicsState";

	if (StateStructName.empty())
		return;

	auto StateStruct = FindObject(StateStructName);

	if (!StateStruct)
		return;

	auto State = (void*)(__int64(Params) + 0);

	static auto RotationOffset = FindOffsetStruct(StateStructName, "Rotation");
	static auto TranslationOffset = FindOffsetStruct(StateStructName, "Translation");

	if (Engine_Version >= 420) // S4-S12
	{
		float v50 = -2.0;
		float v49 = 2.5;

		auto Rotation = (FQuat*)(__int64(State) + RotationOffset);

		Rotation->X -= v49;
		Rotation->Y /= 0.3;
		Rotation->Z -= v50;
		Rotation->W /= -1.2;

		Transform.Rotation = *Rotation;
	}
	else
	{
		auto Rotation = (FQuat*)(__int64(State) + RotationOffset);

		Transform.Rotation = *Rotation;
	}

	Transform.Translation = *(FVector*)(__int64(State) + TranslationOffset);
	Transform.Scale3D = FVector{ 1, 1, 1 };

	bool bTeleport = true; // this maybe be false??
	bool bSweep = false;

	static auto K2_SetWorldTransformFn = FindObject<UFunction>(L"/Script/Engine.SceneComponent.K2_SetWorldTransform");
	static auto SetPhysicsLinearVelocityFn = FindObject<UFunction>(L"/Script/Engine.PrimitiveComponent.SetPhysicsLinearVelocity");
	static auto SetPhysicsAngularVelocityFn = FindObject<UFunction>(L"/Script/Engine.PrimitiveComponent.SetPhysicsAngularVelocity");
	static auto LinearVelocityOffset = FindOffsetStruct(StateStructName, "LinearVelocity");
	static auto AngularVelocityOffset = FindOffsetStruct(StateStructName, "AngularVelocity");
	static auto K2_SetWorldTransformParamSize = K2_SetWorldTransformFn->GetPropertiesSize();

	auto K2_SetWorldTransformParams = Alloc(K2_SetWorldTransformParamSize);

	{
		static auto NewTransformOffset = FindOffsetStruct("/Script/Engine.SceneComponent.K2_SetWorldTransform", "NewTransform");
		static auto bSweepOffset = FindOffsetStruct("/Script/Engine.SceneComponent.K2_SetWorldTransform", "bSweep");
		static auto bTeleportOffset = FindOffsetStruct("/Script/Engine.SceneComponent.K2_SetWorldTransform", "bTeleport");

		*(FTransform*)(__int64(K2_SetWorldTransformParams) + NewTransformOffset) = Transform;
		*(bool*)(__int64(K2_SetWorldTransformParams) + bSweepOffset) = bSweep;
		*(bool*)(__int64(K2_SetWorldTransformParams) + bTeleportOffset) = bTeleport;
	}

	Mesh->ProcessEvent(K2_SetWorldTransformFn, K2_SetWorldTransformParams);
	// Mesh->bComponentToWorldUpdated = true;

	struct { FVector NewVel; bool bAddToCurrent; FName BoneName; } 
	UPrimitiveComponent_SetPhysicsLinearVelocity_Params{
		*(FVector*)(__int64(State) + LinearVelocityOffset),
		0,
		FName()
	};

	struct { FVector NewAngVel; bool bAddToCurrent; FName BoneName; }
	UPrimitiveComponent_SetPhysicsAngularVelocity_Params{
		*(FVector*)(__int64(State) + AngularVelocityOffset),
		0,
		FName()
	};

	Mesh->ProcessEvent(SetPhysicsLinearVelocityFn, &UPrimitiveComponent_SetPhysicsLinearVelocity_Params);
	Mesh->ProcessEvent(SetPhysicsAngularVelocityFn, &UPrimitiveComponent_SetPhysicsAngularVelocity_Params);
}

static inline void AddVehicleHook()
{
	static auto FortAthenaVehicleDefault = FindObject("/Script/FortniteGame.Default__FortAthenaVehicle");
	static auto FortPhysicsPawnDefault = FindObject("/Script/FortniteGame.Default__FortPhysicsPawn");

	if (FortPhysicsPawnDefault)
	{
		Hooking::MinHook::Hook(FortPhysicsPawnDefault, FindObject<UFunction>("/Script/FortniteGame.FortPhysicsPawn.ServerMove") ?
			FindObject<UFunction>("/Script/FortniteGame.FortPhysicsPawn.ServerMove") : FindObject<UFunction>("/Script/FortniteGame.FortPhysicsPawn.ServerUpdatePhysicsParams"),
			ServerVehicleUpdate, nullptr, false, true);
	}
	else
	{
		Hooking::MinHook::Hook(FortAthenaVehicleDefault, FindObject<UFunction>("/Script/FortniteGame.FortAthenaVehicle.ServerUpdatePhysicsParams"),
			ServerVehicleUpdate, nullptr, false, true);
	}
}

static inline AActor* SpawnVehicleFromSpawner(AActor* VehicleSpawner)
{
	auto GameMode = Cast<AFortGameModeAthena>(GetWorld()->GetGameMode());

	FTransform SpawnTransform{};
	SpawnTransform.Translation = VehicleSpawner->GetActorLocation();
	SpawnTransform.Rotation = VehicleSpawner->GetActorRotation().Quaternion();
	SpawnTransform.Scale3D = { 1, 1, 1 };

	FActorSpawnParameters SpawnParameters{};
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	static auto VehicleClassOffset = VehicleSpawner->GetOffset("VehicleClass", false);
	static auto BGAClass = FindObject<UClass>("/Script/Engine.BlueprintGeneratedClass");

	if (VehicleClassOffset != -1) // 10.40 and below?
	{
		auto& SoftVehicleClass = VehicleSpawner->Get<TSoftObjectPtr<UClass>>(VehicleClassOffset);
		auto StrongVehicleClass = SoftVehicleClass.Get(BGAClass, true);

		if (!StrongVehicleClass)
		{
			std::string VehicleClassObjectName = SoftVehicleClass.SoftObjectPtr.ObjectID.AssetPathName.ComparisonIndex.Value == 0 ? "InvalidName" : SoftVehicleClass.SoftObjectPtr.ObjectID.AssetPathName.ToString();
			LOG_WARN(LogVehicles, "Failed to load vehicle class: {}", VehicleClassObjectName);
			return nullptr;
		}

		return GetWorld()->SpawnActor<AActor>(StrongVehicleClass, SpawnTransform, SpawnParameters);
	}

	static auto FortVehicleItemDefOffset = VehicleSpawner->GetOffset("FortVehicleItemDef");

	if (FortVehicleItemDefOffset == -1)
		return nullptr;

	auto& SoftFortVehicleItemDef = VehicleSpawner->Get<TSoftObjectPtr<UFortItemDefinition>>(FortVehicleItemDefOffset);
	auto StrongFortVehicleItemDef = SoftFortVehicleItemDef.Get(nullptr, true);

	if (!StrongFortVehicleItemDef)
	{
		std::string FortVehicleItemDefObjectName = SoftFortVehicleItemDef.SoftObjectPtr.ObjectID.AssetPathName.ComparisonIndex.Value == 0 ? "InvalidName" : SoftFortVehicleItemDef.SoftObjectPtr.ObjectID.AssetPathName.ToString();
		LOG_WARN(LogVehicles, "Failed to load vehicle item definition: {}", FortVehicleItemDefObjectName);
		return nullptr;
	}

	static auto VehicleActorClassOffset = StrongFortVehicleItemDef->GetOffset("VehicleActorClass");
	auto& SoftVehicleActorClass = StrongFortVehicleItemDef->Get<TSoftObjectPtr<UClass>>(VehicleActorClassOffset);
	auto StrongVehicleActorClass = SoftVehicleActorClass.Get(BGAClass, true);

	if (!StrongVehicleActorClass)
	{
		std::string VehicleActorClassObjectName = SoftVehicleActorClass.SoftObjectPtr.ObjectID.AssetPathName.ComparisonIndex.Value == 0 ? "InvalidName" : SoftVehicleActorClass.SoftObjectPtr.ObjectID.AssetPathName.ToString();
		LOG_WARN(LogVehicles, "Failed to load vehicle actor class: {}", VehicleActorClassObjectName);
		return nullptr;
	}

	return GetWorld()->SpawnActor<AActor>(StrongVehicleActorClass, SpawnTransform, SpawnParameters);
}

static inline void SpawnVehicles2()
{
	static auto FortAthenaVehicleSpawnerClass = FindObject<UClass>("/Script/FortniteGame.FortAthenaVehicleSpawner");
	TArray<AActor*> AllVehicleSpawners = UGameplayStatics::GetAllActorsOfClass(GetWorld(), FortAthenaVehicleSpawnerClass);

	int AmountOfVehiclesSpawned = 0;

	for (int i = 0; i < AllVehicleSpawners.Num(); i++)
	{
		auto VehicleSpawner = AllVehicleSpawners.at(i);
		auto Vehicle = SpawnVehicleFromSpawner(VehicleSpawner);

		if (Vehicle)
		{
			AmountOfVehiclesSpawned++;
		}
	}

	auto AllVehicleSpawnersNum = AllVehicleSpawners.Num();

	AllVehicleSpawners.Free();

	LOG_INFO(LogGame, "Spawned {}/{} vehicles.", AmountOfVehiclesSpawned, AllVehicleSpawnersNum);
}