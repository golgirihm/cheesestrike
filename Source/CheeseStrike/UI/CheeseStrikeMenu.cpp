// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMenu.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "Rules/CheeseStrikeRules.h"
#include "Sessions/CheeseStrikeSessionSubsystem.h"

namespace CheeseStrikeMenu
{
	EMenuPage PageFor(bool bMatchStarted, bool bMatchEnded, bool bInAMatch, bool bPaused, EMenuPage Chosen)
	{
		if (bMatchEnded)
		{
			return EMenuPage::None;
		}
		if (bMatchStarted)
		{
			return bPaused ? EMenuPage::Pause : EMenuPage::None;
		}
		if (bInAMatch)
		{
			return EMenuPage::Lobby;
		}
		return Chosen == EMenuPage::Host || Chosen == EMenuPage::Find ? Chosen : EMenuPage::Main;
	}
}

namespace
{
	const FLinearColor Ink(0.92f, 0.9f, 0.86f);
	const FLinearColor Dim(0.6f, 0.58f, 0.55f);
	const FLinearColor Cheese(0.96f, 0.72f, 0.2f);
	const FLinearColor PanelColour(0.04f, 0.04f, 0.045f, 0.82f);

	FSlateFontInfo Font(int32 Size, const FName& Style = FName("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}
}

void SCheeseStrikeMenu::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	ChosenKillLimit = CheeseStrikeRules::KillLimit;
	ChosenRespawnDelay = CheeseStrikeRules::RespawnDelay;
	ChildSlot
	[
		SNew(SOverlay)
		// The lobby covers the whole view.
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SAssignNew(Lobby, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(PanelColour.R, PanelColour.G, PanelColour.B, 1.0f))
			.Padding(FMargin(64.0f, 48.0f))
			[
				SAssignNew(LobbyContent, SBox).HAlign(HAlign_Center).VAlign(VAlign_Fill)
			]
		]
		// The game's name over the front end, at the top in the middle.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.0f, 40.0f, 0.0f, 0.0f))
		[
			SAssignNew(Masthead, SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("CHEESESTRIKE"))).Font(Font(56, FName("Bold"))).ColorAndOpacity(Cheese)
				.ShadowOffset(FVector2D(2.0f, 2.0f)).ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("GOUDA OVERLOAD"))).Font(Font(20, FName("Bold"))).ColorAndOpacity(Ink)
				.ShadowOffset(FVector2D(1.0f, 1.0f)).ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
			]
		]
		// The pages' panel on the left.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill)
		[
			SAssignNew(Panel, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(PanelColour)
			.Padding(FMargin(48.0f, 64.0f))
			[
				SAssignNew(Content, SBox).WidthOverride(400.0f).VAlign(VAlign_Center)
			]
		]
	];
}

void SCheeseStrikeMenu::Show(EMenuPage InPage)
{
	Page = InPage;
	Refresh();
}

void SCheeseStrikeMenu::Refresh()
{
	const bool bLobby = Page == EMenuPage::Lobby;
	const bool bFrontEnd = Page == EMenuPage::Main || Page == EMenuPage::Host || Page == EMenuPage::Find;
	if (Lobby.IsValid())
	{
		Lobby->SetVisibility(bLobby ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (Masthead.IsValid())
	{
		Masthead->SetVisibility(bFrontEnd ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
	if (Panel.IsValid())
	{
		Panel->SetVisibility(bLobby || Page == EMenuPage::None ? EVisibility::Collapsed : EVisibility::Visible);
	}
	if (bLobby && LobbyContent.IsValid())
	{
		LobbyContent->SetContent(MakeLobby());
	}
	else if (Content.IsValid())
	{
		Content->SetContent(MakePage());
	}
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakePage()
{
	switch (Page)
	{
	case EMenuPage::Main: return MakeMain();
	case EMenuPage::Host: return MakeHost();
	case EMenuPage::Find: return MakeFind();
	case EMenuPage::Lobby: return MakeLobby();
	case EMenuPage::Pause: return MakePause();
	default: return SNullWidget::NullWidget;
	}
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeTitle(const FString& Text)
{
	return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(34, FName("Bold"))).ColorAndOpacity(Cheese).Margin(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeLine(const FString& Text, bool bDim)
{
	return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(16)).ColorAndOpacity(bDim ? Dim : Ink).Margin(FMargin(0.0f, 4.0f)).AutoWrapText(true);
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeButton(const FString& Text, TFunction<void()> Do, bool bChosen)
{
	return SNew(SBox).Padding(FMargin(0.0f, 4.0f))
	[
		SNew(SButton)
		.ButtonStyle(FCoreStyle::Get(), bChosen ? "PrimaryButton" : "Button")
		.ContentPadding(FMargin(18.0f, 10.0f))
		.HAlign(HAlign_Left)
		.OnClicked_Lambda([Do]() { Do(); return FReply::Handled(); })
		[
			SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(18, FName("Bold")))
		]
	];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeChoices(const FString& Label, const TArray<FString>& Choices, int32 Chosen, TFunction<void(int32)> Pick)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		Row->AddSlot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), Index == Chosen ? "PrimaryButton" : "Button")
			.ContentPadding(FMargin(12.0f, 6.0f))
			.OnClicked_Lambda([Pick, Index]() { Pick(Index); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(FText::FromString(Choices[Index])).Font(Font(15, FName("Bold")))
			]
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 4.0f)) [ MakeLine(Label, true) ]
		+ SVerticalBox::Slot().AutoHeight() [ Row ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeMain()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	const UCheeseStrikeSessionSubsystem* Sessions = Player.IsValid() && Player->GetGameInstance() != nullptr ? Player->GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>() : nullptr;
	const FString Through = Sessions != nullptr && Sessions->ServiceName() == TEXT("STEAM") ? TEXT("Matches go through Steam.") : TEXT("Without Steam, matches are found on the local network only.");
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("PLAY ALONE"), [Player]() { if (Player.IsValid()) Player->PlayAlone(); }) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("HOST A MATCH"), [Player]() { if (Player.IsValid()) Player->ChooseMenu(EMenuPage::Host); }) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("FIND A MATCH"), [Player]() { if (Player.IsValid()) Player->ChooseMenu(EMenuPage::Find); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f)) [ MakeButton(TEXT("QUIT"), [Player]() { if (Player.IsValid()) Player->QuitFromMenu(); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 24.0f, 0.0f, 0.0f)) [ MakeLine(Through, true) ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeHost()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	TArray<FString> Limits;
	int32 LimitChosen = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeRules::KillLimitChoices); ++Index)
	{
		Limits.Add(FString::FromInt(CheeseStrikeRules::KillLimitChoices[Index]));
		if (CheeseStrikeRules::KillLimitChoices[Index] == ChosenKillLimit)
		{
			LimitChosen = Index;
		}
	}
	TArray<FString> Delays;
	int32 DelayChosen = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeRules::RespawnDelayChoices); ++Index)
	{
		Delays.Add(FString::Printf(TEXT("%.0f s"), CheeseStrikeRules::RespawnDelayChoices[Index]));
		if (CheeseStrikeRules::RespawnDelayChoices[Index] == ChosenRespawnDelay)
		{
			DelayChosen = Index;
		}
	}
	const int32 KillLimit = ChosenKillLimit;
	const float RespawnDelay = ChosenRespawnDelay;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("HOST A MATCH")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeLine(TEXT("Free-for-all on the shooter level."), true) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeChoices(TEXT("First to how many kills"), Limits, LimitChosen, [this](int32 Index) { ChosenKillLimit = CheeseStrikeRules::KillLimitChoices[Index]; Refresh(); }) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeChoices(TEXT("Back how long after a death"), Delays, DelayChosen, [this](int32 Index) { ChosenRespawnDelay = CheeseStrikeRules::RespawnDelayChoices[Index]; Refresh(); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 24.0f, 0.0f, 0.0f)) [ MakeButton(TEXT("HOST"), [Player, KillLimit, RespawnDelay]() { if (Player.IsValid()) Player->HostFromMenu(KillLimit, RespawnDelay); }, true) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("BACK"), [Player]() { if (Player.IsValid()) Player->ChooseMenu(EMenuPage::Main); }) ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeFind()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	UCheeseStrikeSessionSubsystem* Sessions = Player.IsValid() && Player->GetGameInstance() != nullptr ? Player->GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>() : nullptr;
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	if (Sessions != nullptr)
	{
		const TArray<FString>& Found = Sessions->GetFound();
		for (int32 Index = 0; Index < Found.Num(); ++Index)
		{
			List->AddSlot().AutoHeight() [ MakeButton(Found[Index], [Sessions, Index]() { Sessions->Join(Index); }) ];
		}
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("FIND A MATCH")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeLine(Sessions != nullptr ? Sessions->GetStatus() : TEXT("No online service"), true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f)) [ SNew(SScrollBox) + SScrollBox::Slot() [ List ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f)) [ MakeButton(TEXT("SEARCH AGAIN"), [Player]() { if (Player.IsValid()) Player->FindFromMenu(); }) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("BACK"), [Player]() { if (Player.IsValid()) Player->ChooseMenu(EMenuPage::Main); }) ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeTeamColumn(int32 Team)
{
	const ACheeseStrikeGameState* Match = Controller.IsValid() && Controller->GetWorld() != nullptr ? Controller->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const FLinearColor TeamColour = Team == 0 ? Cheese : FLinearColor(0.85f, 0.9f, 1.0f);
	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
		[
			SNew(STextBlock).Text(FText::FromString(FString(CheeseStrikeRules::TeamNames[Team]).ToUpper())).Font(Font(24, FName("Bold"))).ColorAndOpacity(TeamColour)
		];
	if (Match != nullptr)
	{
		for (const CheeseStrikeRules::FRosterEntry& Entry : Match->TeamColumn(Team))
		{
			Column->AddSlot().AutoHeight().Padding(FMargin(0.0f, 3.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(Entry.Name)).Font(Font(17, Entry.bBot ? FName("Regular") : FName("Bold"))).ColorAndOpacity(Entry.bBot ? Dim : Ink)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock).Text(FText::FromString(Entry.bBot ? TEXT("bot") : TEXT(""))).Font(Font(12)).ColorAndOpacity(Dim)
				]
			];
		}
	}
	return SNew(SBox).WidthOverride(340.0f) [ Column ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeLobby()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	const ACheeseStrikeGameState* Match = Player.IsValid() && Player->GetWorld() != nullptr ? Player->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const bool bHost = Player.IsValid() && Player->HasAuthority();
	FString Note;
	if (Match != nullptr && Match->IsCountingDown())
	{
		Note = FString::Printf(TEXT("Starting in %.0f"), FMath::CeilToFloat(Match->SecondsToStart()));
	}
	else
	{
		Note = bHost ? TEXT("Start when everyone is here.") : TEXT("Waiting for the host to start.");
	}
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (bHost && !(Match != nullptr && Match->IsCountingDown()))
	{
		Buttons->AddSlot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f)) [ MakeButton(TEXT("START THE MATCH"), [Player]() { if (Player.IsValid()) Player->StartMatchFromMenu(); }, true) ];
	}
	Buttons->AddSlot().AutoWidth() [ MakeButton(TEXT("LEAVE"), [Player]() { if (Player.IsValid()) Player->LeaveFromMenu(); }) ];
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center) [ MakeTitle(TEXT("LOBBY")) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center) [ MakeLine(Match != nullptr ? FString::Printf(TEXT("First to %d kills, back %.0f s after a death."), Match->GetKillLimit(), Match->GetRespawnDelay()) : FString(), true) ]
		+ SVerticalBox::Slot().FillHeight(1.0f).HAlign(HAlign_Center).Padding(FMargin(0.0f, 32.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 48.0f, 0.0f)) [ MakeTeamColumn(0) ]
			+ SHorizontalBox::Slot().AutoWidth() [ MakeTeamColumn(1) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f)) [ MakeLine(Note) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center) [ Buttons ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakePause()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("PAUSED")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeLine(TEXT("The match goes on without you."), true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f)) [ MakeButton(TEXT("RESUME"), [Player]() { if (Player.IsValid()) Player->TogglePause(); }, true) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("LEAVE THE MATCH"), [Player]() { if (Player.IsValid()) Player->LeaveFromMenu(); }) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("QUIT"), [Player]() { if (Player.IsValid()) Player->QuitFromMenu(); }) ];
}
