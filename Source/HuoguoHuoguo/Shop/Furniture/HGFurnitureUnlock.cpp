#include "Engine/DataTable.h"
#include "Game/HGGameInstance.h"
#include "HuoguoHuoguo.h"
#include "Shop/Furniture/HGFurnitureData.h"

// 가구 해금 규칙. 선언과 설명은 HGFurnitureData.h의 HGFurnitureUnlock.

bool HGFurnitureUnlock::IsUnlocked(const UHGGameInstance* GI, FName FurnitureId, const FHGFurnitureRow& Row)
{
	return Row.Unlock == EHGFurnitureUnlock::Start || (GI && GI->IsFurnitureUnlocked(FurnitureId));
}

FString HGFurnitureUnlock::DescribeLock(UHGGameInstance* GI, FName FurnitureId, const FHGFurnitureRow& Row)
{
	if (IsUnlocked(GI, FurnitureId, Row))
	{
		return FString();
	}
	switch (Row.Unlock)
	{
	case EHGFurnitureUnlock::Money:
		return FString::Printf(TEXT("잠김: U로 열기, 돈 %d (가진 돈 %d)"), Row.UnlockAmount, GI ? GI->GetMoney() : 0);
	case EHGFurnitureUnlock::Customers:
		return FString::Printf(TEXT("잠김: 손님 %d명이 오면 열림 (지금 %d명)"), Row.UnlockAmount, GI ? GI->GetServedCustomers() : 0);
	case EHGFurnitureUnlock::Ingredient:
		return FString::Printf(TEXT("잠김: U로 열기, 재료 %s %d개 (가방에 %d개)"),
			GI ? *GI->GetIngredientName(Row.UnlockIngredient) : *Row.UnlockIngredient.ToString(), Row.UnlockAmount,
			GI ? GI->GetBagCount(Row.UnlockIngredient) : 0);
	default:
		return FString();
	}
}

bool HGFurnitureUnlock::TryUnlock(UHGGameInstance* GI, FName FurnitureId, const FHGFurnitureRow& Row, FString& OutMessage)
{
	const FString Name = Row.DisplayName.ToString();
	if (!GI)
	{
		OutMessage = TEXT("GameInstance 없음");
		return false;
	}
	if (IsUnlocked(GI, FurnitureId, Row))
	{
		OutMessage = FString::Printf(TEXT("이미 열려 있음: %s"), *Name);
		return false;
	}
	switch (Row.Unlock)
	{
	case EHGFurnitureUnlock::Money:
		if (GI->GetMoney() < Row.UnlockAmount)
		{
			OutMessage = FString::Printf(TEXT("열 수 없음: %s, 돈이 모자람 (필요 %d, 가진 돈 %d)"), *Name, Row.UnlockAmount, GI->GetMoney());
			return false;
		}
		GI->AddMoney(-Row.UnlockAmount);
		break;
	case EHGFurnitureUnlock::Ingredient:
		if (!GI->RemoveFromBag(Row.UnlockIngredient, Row.UnlockAmount))
		{
			OutMessage = FString::Printf(TEXT("열 수 없음: %s 재료 %s 모자람 (필요 %d, 가방에 %d)"), *Name, *GI->GetIngredientName(Row.UnlockIngredient), Row.UnlockAmount, GI->GetBagCount(Row.UnlockIngredient));
			return false;
		}
		break;
	default:
		// 손님 수 조건은 손으로 열 수 없다 (손님을 받으면 저절로 열린다).
		OutMessage = DescribeLock(GI, FurnitureId, Row);
		return false;
	}
	GI->UnlockFurniture(FurnitureId);
	OutMessage = FString::Printf(TEXT("새 가구를 열었다: %s! (놓을 때 값 %d)"), *Name, Row.Price);
	UE_LOG(LogHuoguo, Log, TEXT("[Unlock] %s 열림 (%s)"), *FurnitureId.ToString(),
		Row.Unlock == EHGFurnitureUnlock::Money ? TEXT("돈") : TEXT("재료"));
	return true;
}

TArray<FString> HGFurnitureUnlock::UnlockByCustomers(UHGGameInstance* GI)
{
	TArray<FString> Opened;
	const UDataTable* Table = HGFurnitureData::LoadTable();
	if (!GI || !Table)
	{
		return Opened;
	}
	for (const FName& Id : Table->GetRowNames())
	{
		const FHGFurnitureRow* Row = Table->FindRow<FHGFurnitureRow>(Id, TEXT("UnlockByCustomers"), false);
		if (Row && Row->Unlock == EHGFurnitureUnlock::Customers && GI->GetServedCustomers() >= Row->UnlockAmount && GI->UnlockFurniture(Id))
		{
			Opened.Add(Row->DisplayName.ToString());
			UE_LOG(LogHuoguo, Log, TEXT("[Unlock] %s 열림 (손님 %d명)"), *Id.ToString(), GI->GetServedCustomers());
		}
	}
	return Opened;
}

FIntPoint HGFurnitureUnlock::CountUnlocked(const UHGGameInstance* GI)
{
	FIntPoint Count(0, 0);
	if (const UDataTable* Table = HGFurnitureData::LoadTable())
	{
		for (const FName& Id : Table->GetRowNames())
		{
			if (const FHGFurnitureRow* Row = Table->FindRow<FHGFurnitureRow>(Id, TEXT("CountUnlocked"), false))
			{
				++Count.Y;
				Count.X += IsUnlocked(GI, Id, *Row) ? 1 : 0;
			}
		}
	}
	return Count;
}
