#include "BuildingContainer.h"
#include "FortPickup.h"
#include "FortLootPackage.h"
#include "FortGameModeAthena.h"
#include "gui.h"

bool ABuildingContainer::SpawnLoot(AFortPawn* Pawn)
{
	auto GameMode = Cast<AFortGameModeAthena>(GetWorld()->GetGameMode());
	FVector LocationToSpawnLoot = this->GetActorLocation() + this->GetActorRightVector() * 70.f + this->GetActorUpVector() * 50.f;

	static auto SearchLootTierGroupOffset = this->GetOffset("SearchLootTierGroup");
	auto RedirectedLootTier = GameMode->RedirectLootTier(this->Get<FName>(SearchLootTierGroupOffset));

	// LOG_INFO(LogInteraction, "RedirectedLootTier: {}", RedirectedLootTier.ToString());

	auto LootDrops = PickLootDrops(RedirectedLootTier, -1, bDebugPrintLooting);

	// LOG_INFO(LogInteraction, "LootDrops.size(): {}", LootDrops.size());

	for (int i = 0; i < LootDrops.size(); i++)
	{
		auto& lootDrop = LootDrops.at(i);

		PickupCreateData CreateData;
		CreateData.bToss = true;
		// CreateData.PawnOwner = Pawn;
		CreateData.ItemEntry = FFortItemEntry::MakeItemEntry(lootDrop->GetItemDefinition(), lootDrop->GetCount(), lootDrop->GetLoadedAmmo());
		CreateData.SpawnLocation = LocationToSpawnLoot;
		CreateData.SourceType = EFortPickupSourceTypeFlag::GetContainerValue();
		CreateData.bRandomRotation = true;
		CreateData.bShouldFreeItemEntryWhenDeconstructed = true;

		auto NewPickup = AFortPickup::SpawnPickup(CreateData);
	}

	return true;
}