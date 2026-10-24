// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeRules.h"
#include "Kismet/GameplayStatics.h"
#include "CheeseStrikeBots.h"
#include "CheeseStrikeModes.h"

namespace CheeseStrikeRules
{
	int32 FurthestFrom(TArrayView<const FVector> Spots, TArrayView<const FVector> Enemies)
	{
		int32 Best = Spots.Num() > 0 ? 0 : -1;
		double BestDistance = -1.0;
		for (int32 Index = 0; Index < Spots.Num(); ++Index)
		{
			double Nearest = TNumericLimits<double>::Max();
			for (const FVector& Enemy : Enemies)
			{
				Nearest = FMath::Min(Nearest, FVector::DistSquared(Spots[Index], Enemy));
			}
			if (Enemies.Num() > 0 && Nearest > BestDistance)
			{
				BestDistance = Nearest;
				Best = Index;
			}
		}
		return Best;
	}

	TArray<FStanding> Sorted(TArray<FStanding> Standings)
	{
		Standings.StableSort([](const FStanding& A, const FStanding& B)
		{
			if (A.Kills != B.Kills)
			{
				return A.Kills > B.Kills;
			}
			if (A.Deaths != B.Deaths)
			{
				return A.Deaths < B.Deaths;
			}
			return A.Order < B.Order;
		});
		return Standings;
	}

	bool IsOver(int32 MostKills, int32 Limit)
	{
		return Limit > 0 && MostKills >= Limit;
	}

	FString Leader(const TArray<FStanding>& Standings)
	{
		const TArray<FStanding> InOrder = Sorted(Standings);
		return InOrder.Num() > 0 ? InOrder[0].Name : FString();
	}

	float Option(const FString& Options, const TCHAR* Name, float Otherwise)
	{
		const FString Value = UGameplayStatics::ParseOption(Options, Name);
		return Value.IsNumeric() ? FCString::Atof(*Value) : Otherwise;
	}

	int32 KillLimitFrom(const FString& Options)
	{
		return FMath::Clamp(FMath::RoundToInt(Option(Options, TEXT("KillLimit"), static_cast<float>(KillLimit))), 1, 1000);
	}

	float RespawnDelayFrom(const FString& Options)
	{
		return FMath::Clamp(Option(Options, TEXT("RespawnDelay"), RespawnDelay), 0.5f, 15.0f);
	}

	bool StartsAtOnce(const FString& Options)
	{
		return UGameplayStatics::HasOption(Options, TEXT("StartNow"));
	}

	const FMapChoice Maps[] = {
		{TEXT("The Creamery"), TEXT("/Game/CheeseStrike/Maps/Lvl_Creamery"), TEXT("creamery.png")},
		{TEXT("The Labyrinth"), TEXT("/Game/CheeseStrike/Maps/Lvl_Labyrinth"), TEXT("labyrinth.png")},
		{TEXT("Gruyère Grotto"), TEXT("/Game/CheeseStrike/Maps/Lvl_Grotto"), TEXT("grotto.png")},
		{TEXT("Rind Market"), TEXT("/Game/CheeseStrike/Maps/Lvl_RindMarket"), TEXT("market.png")},
		{TEXT("The Fondue Floor"), TEXT("/Game/CheeseStrike/Maps/Lvl_FondueFloor"), TEXT("fondue.png")},
		{TEXT("Crumb Kitchen"), TEXT("/Game/CheeseStrike/Maps/Lvl_Kitchen"), TEXT("kitchen.png")},
		{TEXT("Proving Ground"), TEXT("/Game/Variant_Shooter/Lvl_Shooter"), TEXT("proving_ground.png")},
	};

	int32 NumMaps()
	{
		return UE_ARRAY_COUNT(Maps);
	}

	int32 DefaultMap()
	{
		return FMath::Max(0, MapIndexFor(TEXT("/Game/CheeseStrike/Maps/Lvl_Grotto")));
	}

	FString HostOptions()
	{
		const CheeseStrikeModes::FModeInfo& Mode = CheeseStrikeModes::Info(CheeseStrikeModes::EMode::Deathmatch);
		return FString::Printf(TEXT("?KillLimit=%d?RespawnDelay=%.1f?Map=%d?Mode=%s?Bots=%d?BotSkill=%d"), Mode.DefaultKillLimit, RespawnDelay, DefaultMap(), Mode.Key, DefaultBots, CheeseStrikeBots::DefaultSkill);
	}

	int32 MapIndexFor(const FString& LevelPackage)
	{
		FString Package = LevelPackage;
		Package.Split(TEXT("."), &Package, nullptr);
		// A level played in the editor carries the editor's prefix on its name (UEDPIE_0_Lvl_Creamery).
		FString Folder;
		FString Name;
		if (Package.Split(TEXT("/"), &Folder, &Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd) && Name.StartsWith(TEXT("UEDPIE_")))
		{
			Name.Split(TEXT("_"), nullptr, &Name);
			Name.Split(TEXT("_"), nullptr, &Name);
			Package = Folder + TEXT("/") + Name;
		}
		for (int32 Index = 0; Index < NumMaps(); ++Index)
		{
			if (Package.Equals(Maps[Index].Level, ESearchCase::IgnoreCase))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 MapFrom(const FString& Options, int32 Otherwise)
	{
		const int32 Index = FMath::RoundToInt(Option(Options, TEXT("Map"), -1.0f));
		return Index >= 0 && Index < NumMaps() ? Index : Otherwise;
	}

	int32 PlayersFrom(const FString& Options)
	{
		return FMath::Max(0, FMath::RoundToInt(Option(Options, TEXT("Players"), 0.0f)));
	}

	bool ShouldBegin(int32 Expected, int32 Ready, int32 In, float SecondsWaited)
	{
		return In > 0 && (Ready >= Expected || SecondsWaited > WaitForPlayers);
	}

	int32 BotSeedFrom(const FString& Options, int32 Otherwise)
	{
		const FString Value = UGameplayStatics::ParseOption(Options, TEXT("BotSeed"));
		return Value.IsNumeric() ? FCString::Atoi(*Value) : Otherwise;
	}

	const TCHAR* const TeamNames[Teams] = {TEXT("The Gouda Guys"), TEXT("Mozza Radicals")};

	int32 TeamToJoin(const int32 (&Counts)[Teams])
	{
		int32 Fewest = 0;
		for (int32 Team = 1; Team < Teams; ++Team)
		{
			if (Counts[Team] < Counts[Fewest])
			{
				Fewest = Team;
			}
		}
		return Fewest;
	}

	namespace
	{
		const TCHAR* const AllBotNames[] = {
			TEXT("Gorgon Zola"), TEXT("Camem Bert"), TEXT("Mozza Rella"), TEXT("Sir Cheddar"), TEXT("Brie Ann"), TEXT("Edam Adams"),
			TEXT("Stilton Blue"), TEXT("Parmesan Pete"), TEXT("Manchego Max"), TEXT("Swiss Miss"), TEXT("Ricotta Rick"), TEXT("Havarti Hart"),
			TEXT("Provolone Pat"), TEXT("Monterey Jack"), TEXT("Pepper Jack"), TEXT("Limburger Lou"), TEXT("Emmental Emma"), TEXT("Gruyere Greg"),
			TEXT("Raclette Ray"), TEXT("Burrata Bo"), TEXT("Paneer Pam"), TEXT("Halloumi Hal"), TEXT("Count Roquefort"), TEXT("Gouda Vibes"),
		};
	}

	int32 NumBotNames()
	{
		return UE_ARRAY_COUNT(AllBotNames);
	}

	TArray<FString> BotNames(int32 Seed, int32 Count)
	{
		TArray<FString> Names;
		for (const TCHAR* Name : AllBotNames)
		{
			Names.Add(Name);
		}
		FRandomStream Random(Seed);
		for (int32 Index = Names.Num() - 1; Index > 0; --Index)
		{
			Names.Swap(Index, Random.RandRange(0, Index));
		}
		Names.SetNum(FMath::Clamp(Count, 0, Names.Num()));
		return Names;
	}

	int32 BotsFrom(const FString& Options)
	{
		return FMath::Max(-1, FMath::RoundToInt(Option(Options, TEXT("Bots"), static_cast<float>(DefaultBots))));
	}

	int32 BotsIn(int32 Players, int32 BotsWanted)
	{
		const int32 Empty = MostBots(Players);
		return BotsWanted < 0 ? Empty : FMath::Min(BotsWanted, Empty);
	}

	int32 MostBots(int32 Players)
	{
		return FMath::Max(0, SeatCount - Players);
	}

	TArray<FRosterEntry> Seats(const TArray<FRosterEntry>& Players, const TArray<FString>& Bots, int32 BotsWanted, const TArray<int32>& BotSides)
	{
		TArray<FRosterEntry> Taken;
		int32 Sizes[Teams] = {};
		int32 BotsIn = 0;
		for (const FRosterEntry& Player : Players)
		{
			Taken.Add(Player);
			if (Player.Team >= 0 && Player.Team < Teams)
			{
				Sizes[Player.Team] = FMath::Min(Sizes[Player.Team] + 1, TeamSize);
			}
			BotsIn += Player.bBot ? 1 : 0;
		}
		// The bots that will stand in, in the order they were added: each on the side the host put it on, or dealt to the
		// side with fewer, so that the sides stay even; a full side passes it to the other.
		const int32 Wanted = BotsWanted < 0 ? SeatCount : BotsWanted;
		for (int32 Bot = BotsIn; Bot < Wanted && Bots.IsValidIndex(Bot); ++Bot)
		{
			int32 Side = BotSides.IsValidIndex(Bot) && BotSides[Bot] >= 0 && BotSides[Bot] < Teams ? BotSides[Bot] : INDEX_NONE;
			if (Side == INDEX_NONE)
			{
				Side = Sizes[1] < Sizes[0] ? 1 : 0;
			}
			if (Sizes[Side] >= TeamSize)
			{
				Side = 1 - Side;
			}
			if (Sizes[Side] >= TeamSize)
			{
				break;
			}
			++Sizes[Side];
			FRosterEntry Entry;
			Entry.Name = Bots[Bot];
			Entry.Team = Side;
			Entry.bBot = true;
			Entry.Bot = Bot;
			Taken.Add(Entry);
		}
		return Taken;
	}

	TArray<FRosterEntry> TeamColumn(int32 Team, const TArray<FRosterEntry>& Players, const TArray<FString>& Bots, int32 BotsWanted, const TArray<int32>& BotSides)
	{
		TArray<FRosterEntry> Column;
		for (const FRosterEntry& Seat : Seats(Players, Bots, BotsWanted, BotSides))
		{
			if (Seat.Team == Team && Column.Num() < TeamSize)
			{
				Column.Add(Seat);
			}
		}
		return Column;
	}

	int32 Taken(const TArray<FRosterEntry>& Seats, int32 Team)
	{
		int32 Count = 0;
		for (const FRosterEntry& Seat : Seats)
		{
			Count += !Seat.bEmpty && (Team == INDEX_NONE || Seat.Team == Team) ? 1 : 0;
		}
		return Count;
	}

	bool HasRoomForAPlayer(const TArray<FRosterEntry>& Players, int32 Team)
	{
		if (Team < 0 || Team >= Teams)
		{
			return false;
		}
		int32 There = 0;
		for (const FRosterEntry& Player : Players)
		{
			There += !Player.bBot && Player.Team == Team ? 1 : 0;
		}
		return There < TeamSize;
	}

	bool MoveBot(const TArray<FRosterEntry>& Players, const TArray<FString>& Bots, int32 BotsWanted, TArray<int32>& BotSides, int32 Bot)
	{
		const TArray<FRosterEntry> Now = Seats(Players, Bots, BotsWanted, BotSides);
		const FRosterEntry* Moving = Now.FindByPredicate([Bot](const FRosterEntry& Seat) { return Seat.bBot && Seat.Bot == Bot; });
		if (Moving == nullptr || Moving->Team < 0 || Moving->Team >= Teams || Taken(Now, 1 - Moving->Team) >= TeamSize)
		{
			return false;
		}
		int32 Count = 0;
		for (const FRosterEntry& Seat : Now)
		{
			Count = Seat.bBot ? FMath::Max(Count, Seat.Bot + 1) : Count;
		}
		TArray<int32> Pinned;
		Pinned.Init(INDEX_NONE, Count);
		for (const FRosterEntry& Seat : Now)
		{
			if (Seat.bBot && Pinned.IsValidIndex(Seat.Bot))
			{
				Pinned[Seat.Bot] = Seat.Bot == Bot ? 1 - Seat.Team : Seat.Team;
			}
		}
		BotSides = Pinned;
		return true;
	}

	FString BotSidesOption(const TArray<int32>& BotSides)
	{
		FString Digits;
		for (const int32 Side : BotSides)
		{
			Digits.AppendChar(Side >= 0 && Side < Teams ? TCHAR('0' + Side) : TCHAR('x'));
		}
		return Digits;
	}

	TArray<int32> BotSidesFrom(const FString& Options)
	{
		TArray<int32> Sides;
		for (const TCHAR Each : UGameplayStatics::ParseOption(Options, TEXT("BotSides")))
		{
			Sides.Add(Each >= TEXT('0') && Each < TEXT('0') + Teams ? Each - TEXT('0') : INDEX_NONE);
		}
		return Sides;
	}

	FString WhyNotStart(const TArray<FRosterEntry>& Seats, bool bTeams)
	{
		if (Taken(Seats) < 2)
		{
			return TEXT("Add a bot or wait for a player");
		}
		if (bTeams && (Taken(Seats, 0) == 0 || Taken(Seats, 1) == 0))
		{
			return TEXT("Each side needs someone");
		}
		return FString();
	}

	TArray<FRosterEntry> WithEmptySeats(TArray<FRosterEntry> Column, int32 NumSeats, int32 Team)
	{
		if (Column.Num() > NumSeats)
		{
			Column.SetNum(FMath::Max(0, NumSeats));
		}
		while (Column.Num() < NumSeats)
		{
			FRosterEntry Empty;
			Empty.Team = Team;
			Empty.bEmpty = true;
			Column.Add(Empty);
		}
		return Column;
	}
}
