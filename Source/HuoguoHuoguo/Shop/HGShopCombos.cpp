#include "Shop/HGShopCombos.h"

#include "HuoguoHuoguo.h"

FString FHGComboResult::Describe(const UDataTable* Table) const
{
	TArray<FString> Parts;
	for (int32 i = 0; i < Ids.Num(); ++i)
	{
		const FHGComboRow* Row = Table ? Table->FindRow<FHGComboRow>(Ids[i], TEXT("ComboDescribe"), false) : nullptr;
		Parts.Add(FString::Printf(TEXT("%s +%d%%"), *Names[i], Row ? Row->BonusPercent : 0));
	}
	return FString::Join(Parts, TEXT(", "));
}

TMap<FName, int32> HGCombo::CountAround(const FIntRect& TableRect, const TArray<FHGGridItem>& Items)
{
	TMap<FName, int32> Counts;
	for (const FHGGridItem& Item : Items)
	{
		// Touches는 겹치는 것(식탁 자신)을 빼고 둘레에 걸친 것만 참이다.
		// ID와 꼬리표를 같은 표에 센다: 콤보 Needs에 "Bamboo"(그 가구)나 "Plant"(화분류 아무거나)를 쓸 수 있다.
		if (FHGShopGrid::Touches(TableRect, Item.Rect))
		{
			Counts.FindOrAdd(Item.Id) += 1;
			for (const FName& Tag : Item.Tags)
			{
				Counts.FindOrAdd(Tag) += 1;
			}
		}
	}
	return Counts;
}

namespace
{
	// "Planter+Lantern*2" -> {Planter: 1, Lantern: 2}. 형식이 틀린 조각은 로그를 남기고 건너뛴다.
	TMap<FName, int32> ParseNeeds(const FString& Needs, FName ComboId)
	{
		TMap<FName, int32> Result;
		TArray<FString> Tokens;
		Needs.ParseIntoArray(Tokens, TEXT("+"), true);
		for (FString Token : Tokens)
		{
			Token.TrimStartAndEndInline();
			FString IdText = Token;
			int32 Count = 1;
			FString Left;
			FString Right;
			if (Token.Split(TEXT("*"), &Left, &Right))
			{
				IdText = Left.TrimStartAndEnd();
				Count = FCString::Atoi(*Right.TrimStartAndEnd());
			}
			if (IdText.IsEmpty() || Count <= 0)
			{
				UE_LOG(LogHuoguo, Warning, TEXT("[Combo] %s의 Needs 조각 '%s'를 읽지 못했다"), *ComboId.ToString(), *Token);
				continue;
			}
			Result.FindOrAdd(FName(*IdText)) += Count;
		}
		return Result;
	}
}

FHGComboResult HGCombo::Evaluate(const UDataTable* Table, const TMap<FName, int32>& Counts)
{
	FHGComboResult Result;
	if (!Table)
	{
		return Result;
	}
	for (const FName& RowName : Table->GetRowNames())
	{
		const FHGComboRow* Row = Table->FindRow<FHGComboRow>(RowName, TEXT("ComboEvaluate"), false);
		if (!Row)
		{
			continue;
		}
		const TMap<FName, int32> Needs = ParseNeeds(Row->Needs, RowName);
		if (Needs.IsEmpty())
		{
			continue;
		}
		bool bAll = true;
		for (const TPair<FName, int32>& Need : Needs)
		{
			const int32* Have = Counts.Find(Need.Key);
			if (!Have || *Have < Need.Value)
			{
				bAll = false;
				break;
			}
		}
		if (bAll)
		{
			Result.Ids.Add(RowName);
			Result.Names.Add(Row->DisplayName.ToString());
			Result.BonusPercent += Row->BonusPercent;
		}
	}
	return Result;
}

FHGComboResult HGCombo::EvaluateTable(const UDataTable* Table, const FIntRect& TableRect, const TArray<FHGGridItem>& Items)
{
	return Evaluate(Table, CountAround(TableRect, Items));
}
