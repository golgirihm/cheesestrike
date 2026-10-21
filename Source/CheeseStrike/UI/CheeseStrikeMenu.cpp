// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMenu.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "Rules/CheeseStrikeBots.h"
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
		return Chosen == EMenuPage::Find ? Chosen : EMenuPage::Main;
	}
}

namespace
{
	const FLinearColor Ink(0.92f, 0.9f, 0.86f);
	const FLinearColor Muted(0.6f, 0.58f, 0.55f);
	const FLinearColor Cheese(0.96f, 0.72f, 0.2f);
	const FLinearColor Mozzarella(0.85f, 0.9f, 1.0f);
	const FLinearColor PanelColour(0.04f, 0.04f, 0.045f, 0.82f);

	FSlateFontInfo Font(int32 Size, const FName& Style = FName("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}

	/** The menu's looks, made once: rounded buttons in three kinds, chips, and the pictures from Content/UI. */
	struct FLooks
	{
		FButtonStyle Plain;
		FButtonStyle Primary;
		FButtonStyle Danger;
		FSlateRoundedBoxBrush PlayerChip;
		FSlateRoundedBoxBrush BotChip;
		TUniquePtr<FSlateDynamicImageBrush> PlayerIcon;
		TUniquePtr<FSlateDynamicImageBrush> BotIcon;
		TUniquePtr<FSlateDynamicImageBrush> CrownIcon;
		TUniquePtr<FSlateDynamicImageBrush> LobbyTile;

		FLooks()
			: PlayerChip(FLinearColor(0.16f, 0.16f, 0.17f, 1.0f), 8.0f)
			, BotChip(FLinearColor(0.1f, 0.1f, 0.11f, 1.0f), 8.0f)
		{
			const auto Make = [](FButtonStyle& Style, const FLinearColor& Rest, const FLinearColor& Hover, const FLinearColor& Press)
			{
				Style.SetNormal(FSlateRoundedBoxBrush(Rest, CheeseStrikeMenu::Rounding));
				Style.SetHovered(FSlateRoundedBoxBrush(Hover, CheeseStrikeMenu::Rounding));
				Style.SetPressed(FSlateRoundedBoxBrush(Press, CheeseStrikeMenu::Rounding));
				Style.SetNormalForeground(Ink);
				Style.SetHoveredForeground(FLinearColor::White);
				Style.SetPressedForeground(FLinearColor::White);
				Style.SetNormalPadding(FMargin(0.0f));
				Style.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
			};
			Make(Plain, FLinearColor(0.18f, 0.18f, 0.19f), FLinearColor(0.28f, 0.28f, 0.3f), FLinearColor(0.12f, 0.12f, 0.13f));
			Make(Primary, FLinearColor(0.1f, 0.42f, 0.9f), FLinearColor(0.2f, 0.52f, 1.0f), FLinearColor(0.08f, 0.32f, 0.72f));
			Make(Danger, FLinearColor(0.72f, 0.16f, 0.14f), FLinearColor(0.85f, 0.24f, 0.2f), FLinearColor(0.55f, 0.1f, 0.1f));
			const FString Folder = FPaths::ProjectContentDir() / TEXT("UI");
			PlayerIcon = MakeUnique<FSlateDynamicImageBrush>(FName(*(Folder / TEXT("player.png"))), FVector2D(22.0, 22.0));
			BotIcon = MakeUnique<FSlateDynamicImageBrush>(FName(*(Folder / TEXT("bot.png"))), FVector2D(22.0, 22.0));
			CrownIcon = MakeUnique<FSlateDynamicImageBrush>(FName(*(Folder / TEXT("crown.png"))), FVector2D(22.0, 22.0));
			LobbyTile = MakeUnique<FSlateDynamicImageBrush>(FName(*(Folder / TEXT("lobby_tile.png"))), FVector2D(256.0, 256.0), FLinearColor::White, ESlateBrushTileType::Both);
		}
	};

	FLooks& Looks()
	{
		static FLooks Made;
		return Made;
	}
}

void SCheeseStrikeMenu::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	ChildSlot
	[
		SNew(SOverlay)
		// The lobby covers the whole view, over its tile.
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SAssignNew(Lobby, SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(PanelColour.R, PanelColour.G, PanelColour.B, 1.0f))
			]
			+ SOverlay::Slot()
			[
				SNew(SImage).Image(Looks().LobbyTile.Get()).ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
			]
			+ SOverlay::Slot().Padding(FMargin(56.0f, 40.0f))
			[
				SAssignNew(LobbyContent, SBox).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
			]
		]
		// The game's name over the front end, at the top in the middle.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.0f, 36.0f, 0.0f, 0.0f))
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
	const bool bFrontEnd = Page == EMenuPage::Main || Page == EMenuPage::Find;
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

FReply SCheeseStrikeMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_Special_Right)
	{
		if (Controller.IsValid())
		{
			Controller->MenuPressed();
		}
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

const FButtonStyle& SCheeseStrikeMenu::ButtonStyle(EButtonKind Kind) const
{
	switch (Kind)
	{
	case EButtonKind::Primary: return Looks().Primary;
	case EButtonKind::Danger: return Looks().Danger;
	default: return Looks().Plain;
	}
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakePage()
{
	switch (Page)
	{
	case EMenuPage::Main: return MakeMain();
	case EMenuPage::Find: return MakeFind();
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
	return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(16)).ColorAndOpacity(bDim ? Muted : Ink).Margin(FMargin(0.0f, 4.0f)).AutoWrapText(true);
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeButton(const FString& Text, TFunction<void()> Do, EButtonKind Kind, TSharedPtr<SWidget> Inside)
{
	TSharedRef<SHorizontalBox> Label = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(18, FName("Bold")))
		];
	if (Inside.IsValid())
	{
		Label->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f)) [ Inside.ToSharedRef() ];
	}
	return SNew(SBox).Padding(FMargin(0.0f, 4.0f))
	[
		SNew(SButton)
		.ButtonStyle(&ButtonStyle(Kind))
		.ContentPadding(FMargin(18.0f, 10.0f))
		.HAlign(HAlign_Left)
		.OnClicked_Lambda([Do]() { Do(); return FReply::Handled(); })
		[
			Label
		]
	];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeChoices(const FString& Label, const TArray<FString>& Choices, int32 Chosen, bool bCanPick, TFunction<void(int32)> Pick)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		Row->AddSlot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
		[
			SNew(SButton)
			.ButtonStyle(&ButtonStyle(Index == Chosen ? EButtonKind::Primary : EButtonKind::Plain))
			.IsEnabled(bCanPick)
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
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("HOST A MATCH"), [Player]() { if (Player.IsValid()) Player->HostFromMenu(); }) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("FIND A MATCH"), [Player]() { if (Player.IsValid()) Player->ChooseMenu(EMenuPage::Find); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f)) [ MakeButton(TEXT("QUIT"), [Player]() { if (Player.IsValid()) Player->QuitFromMenu(); }) ];
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

TSharedRef<SWidget> SCheeseStrikeMenu::MakeNameChip(const FString& Name, bool bBot, bool bHost, int32 Team)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
		[
			SNew(SImage).Image(bBot ? Looks().BotIcon.Get() : Looks().PlayerIcon.Get()).ColorAndOpacity(bBot ? Muted : (Team == 0 ? Cheese : Mozzarella))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(Name)).Font(Font(16, bBot ? FName("Regular") : FName("Bold"))).ColorAndOpacity(bBot ? Muted : Ink)
		];
	if (bHost)
	{
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f)) [ SNew(SImage).Image(Looks().CrownIcon.Get()) ];
	}
	else if (bBot)
	{
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f)) [ SNew(STextBlock).Text(FText::FromString(TEXT("bot"))).Font(Font(11)).ColorAndOpacity(Muted) ];
	}
	return SNew(SBorder)
		.BorderImage(bBot ? &Looks().BotChip : &Looks().PlayerChip)
		.Padding(FMargin(12.0f, 8.0f))
		[
			Row
		];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeTeamColumn(int32 Team)
{
	const ACheeseStrikeGameState* Match = Controller.IsValid() && Controller->GetWorld() != nullptr ? Controller->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(4.0f, 0.0f, 0.0f, 12.0f))
		[
			SNew(STextBlock).Text(FText::FromString(FString(CheeseStrikeRules::TeamNames[Team]).ToUpper())).Font(Font(22, FName("Bold"))).ColorAndOpacity(Team == 0 ? Cheese : Mozzarella)
		];
	if (Match != nullptr)
	{
		for (const CheeseStrikeRules::FRosterEntry& Entry : Match->TeamColumn(Team))
		{
			Column->AddSlot().AutoHeight().Padding(FMargin(0.0f, 3.0f))
			[
				MakeNameChip(Entry.Name, Entry.bBot, !Entry.bBot && Entry.Name == Match->GetHostName(), Team)
			];
		}
	}
	return SNew(SBox).WidthOverride(300.0f) [ Column ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeLobbyRules()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	const ACheeseStrikeGameState* Match = Player.IsValid() && Player->GetWorld() != nullptr ? Player->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const bool bHost = Player.IsValid() && Player->HasAuthority();
	const int32 KillLimit = Match != nullptr ? Match->GetKillLimit() : CheeseStrikeRules::KillLimit;
	const float RespawnDelay = Match != nullptr ? Match->GetRespawnDelay() : CheeseStrikeRules::RespawnDelay;
	TArray<FString> Limits;
	int32 LimitChosen = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeRules::KillLimitChoices); ++Index)
	{
		Limits.Add(FString::FromInt(CheeseStrikeRules::KillLimitChoices[Index]));
		if (CheeseStrikeRules::KillLimitChoices[Index] == KillLimit)
		{
			LimitChosen = Index;
		}
	}
	TArray<FString> Delays;
	int32 DelayChosen = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeRules::RespawnDelayChoices); ++Index)
	{
		Delays.Add(FString::Printf(TEXT("%.0f s"), CheeseStrikeRules::RespawnDelayChoices[Index]));
		if (FMath::IsNearlyEqual(CheeseStrikeRules::RespawnDelayChoices[Index], RespawnDelay))
		{
			DelayChosen = Index;
		}
	}
	TArray<FString> MapNames;
	for (int32 Index = 0; Index < CheeseStrikeRules::NumMaps(); ++Index)
	{
		MapNames.Add(FString(CheeseStrikeRules::Maps[Index].Name).ToUpper());
	}
	const int32 MapChosen = Match != nullptr ? Match->GetMapIndex() : INDEX_NONE;
	TArray<FString> Skills;
	for (const CheeseStrikeBots::FSkill& Skill : CheeseStrikeBots::Skills)
	{
		Skills.Add(FString(Skill.Name).ToUpper());
	}
	const int32 SkillChosen = Match != nullptr ? Match->GetBotSkill() : CheeseStrikeBots::DefaultSkill;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("LOBBY")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeLine(TEXT("Free-for-all."), true) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeChoices(TEXT("Map"), MapNames, MapChosen, bHost, [Player, KillLimit, RespawnDelay](int32 Index) { if (Player.IsValid()) Player->SetRulesFromMenu(KillLimit, RespawnDelay, Index); }) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeChoices(TEXT("First to how many kills"), Limits, LimitChosen, bHost, [Player, RespawnDelay](int32 Index) { if (Player.IsValid()) Player->SetRulesFromMenu(CheeseStrikeRules::KillLimitChoices[Index], RespawnDelay); }) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeChoices(TEXT("Back how long after a death"), Delays, DelayChosen, bHost, [Player, KillLimit](int32 Index) { if (Player.IsValid()) Player->SetRulesFromMenu(KillLimit, CheeseStrikeRules::RespawnDelayChoices[Index]); }) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeChoices(TEXT("Bots"), Skills, SkillChosen, bHost, [Player](int32 Index) { if (Player.IsValid()) Player->SetBotSkillFromMenu(Index); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f)) [ MakeLine(bHost ? TEXT("Everyone here sees the rules as you set them.") : TEXT("The host sets the rules."), true) ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeLobby()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	const ACheeseStrikeGameState* Match = Player.IsValid() && Player->GetWorld() != nullptr ? Player->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const bool bHost = Player.IsValid() && Player->HasAuthority();
	const bool bCounting = Match != nullptr && Match->IsCountingDown();
	const FString Seconds = bCounting ? FString::Printf(TEXT("%.0f"), FMath::CeilToFloat(Match->SecondsToStart())) : FString();
	TSharedRef<SWidget> Spinner = SNew(SCircularThrobber).Radius(10.0f).NumPieces(8).Period(0.8f).PieceImage(FCoreStyle::Get().GetBrush("Throbber.CircleChunk")).ColorAndOpacity(FLinearColor::White);

	TSharedRef<SHorizontalBox> Bottom = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeButton(TEXT("LEAVE"), [Player]() { if (Player.IsValid()) Player->LeaveFromMenu(); }) ]
		+ SHorizontalBox::Slot().FillWidth(1.0f) [ SNew(SSpacer) ];
	if (bHost && bCounting)
	{
		TSharedRef<SHorizontalBox> Inside = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Spinner ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f)) [ SNew(STextBlock).Text(FText::FromString(Seconds)).Font(Font(18, FName("Bold"))) ];
		Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center) [ MakeButton(TEXT("CANCEL"), [Player]() { if (Player.IsValid()) Player->CancelStartFromMenu(); }, EButtonKind::Danger, Inside) ];
	}
	else if (bHost)
	{
		Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center) [ MakeButton(TEXT("START THE MATCH"), [Player]() { if (Player.IsValid()) Player->StartMatchFromMenu(); }, EButtonKind::Primary) ];
	}
	else if (bCounting)
	{
		Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Spinner ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f)) [ MakeLine(FString::Printf(TEXT("Starting in %s"), *Seconds)) ]
		];
	}
	else
	{
		Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center) [ MakeLine(TEXT("Waiting for the host to start."), true) ];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(0.0f, 0.0f, 64.0f, 0.0f))
			[
				SNew(SBox).WidthOverride(400.0f) [ MakeLobbyRules() ]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right).VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 24.0f, 0.0f)) [ MakeTeamColumn(0) ]
				+ SHorizontalBox::Slot().AutoWidth() [ MakeTeamColumn(1) ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 24.0f, 0.0f, 0.0f)) [ Bottom ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakePause()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("PAUSED")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeLine(TEXT("The match goes on without you."), true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f)) [ MakeButton(TEXT("RESUME"), [Player]() { if (Player.IsValid()) Player->TogglePause(); }, EButtonKind::Primary) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("LEAVE THE MATCH"), [Player]() { if (Player.IsValid()) Player->LeaveFromMenu(); }) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("QUIT"), [Player]() { if (Player.IsValid()) Player->QuitFromMenu(); }) ];
}
