// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The scoreboard, shown while its key is held and for good once the match is
// over: the mode or the winner, the goal, then every player in order (each
// side apart under its name and score in a team mode). A row is a small
// figure for who plays it (a person or a robot), the name, the status (blank
// alive, DOWN, OUT or DEAD, as CheeseStrikeHUD::StatusWord has it), Gun
// Game's step, or King of the Hill's time on the hill (and in the solo mode
// the points, which it is ordered by), the kills and the deaths. In a mode of
// sides each side's heading has its score beside its name, saying what it
// counts (rounds won, points, kills: CheeseStrikeHUD::TeamScoreLabel). What
// a row says is CheeseStrikeHUD::BoardRow's.

#include "CheeseStrikeHUD.h"
#include "Engine/Canvas.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "CheeseStrikeHUDDraw.h"
#include "UI/CheeseStrikeHUDRules.h"
#include "Rules/CheeseStrikePodiums.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeRules.h"
#include "Weapons/CheeseStrikeArsenal.h"

/** The HUD's drawing, named through an alias rather than used as a whole: the packaged game's unity build would carry a using-directive into the files after this one. */
namespace HudDraw = CheeseStrikeHUDDraw;

namespace
{
	/**
	 * The figure for who plays a row, `Size` tall about `Middle`: a person's
	 * head and shoulders, or a robot's square head with its eyes, ears and
	 * aerial.
	 */
	void BoardFigure(HudDraw::FShapes& Shapes, CheeseStrikeHUD::EPlayerKind Kind, const FVector2D& Middle, float Size, const FLinearColor& Colour)
	{
		if (Kind == CheeseStrikeHUD::EPlayerKind::Person)
		{
			Shapes.Disc(Middle + FVector2D(0.0f, -0.2f * Size), 0.2f * Size, Colour, 16);
			// The shoulders: a dome standing on the bottom.
			TArray<FVector2D, TInlineAllocator<14>> Dome;
			const FVector2D Base = Middle + FVector2D(0.0f, 0.5f * Size);
			const float Radius = 0.4f * Size;
			for (int32 Step = 0; Step <= 12; ++Step)
			{
				const float Angle = PI * Step / 12.0f;
				Dome.Add(Base + FVector2D(Radius * FMath::Cos(Angle), -0.62f * Size * FMath::Sin(Angle)));
			}
			Shapes.Polygon(Dome, Colour);
			return;
		}
		// A robot's bust: a square head with ears, an aerial and eyes, on square shoulders.
		const float Head = 0.7f * Size;
		const float HeadHigh = 0.48f * Size;
		const FVector2D Corner = Middle + FVector2D(-Head / 2.0f, -0.2f * Size);
		Shapes.Rect(Corner.X, Corner.Y, Head, HeadHigh, Colour);
		Shapes.Rect(Corner.X - 0.1f * Size, Corner.Y + 0.14f * Size, 0.1f * Size, 0.2f * Size, Colour);
		Shapes.Rect(Corner.X + Head, Corner.Y + 0.14f * Size, 0.1f * Size, 0.2f * Size, Colour);
		Shapes.Rect(Middle.X - 0.04f * Size, Corner.Y - 0.18f * Size, 0.08f * Size, 0.18f * Size, Colour);
		Shapes.Disc(FVector2D(Middle.X, Corner.Y - 0.22f * Size), 0.09f * Size, Colour, 10);
		Shapes.Quad(Middle + FVector2D(-0.3f * Size, 0.34f * Size), Middle + FVector2D(0.3f * Size, 0.34f * Size), Middle + FVector2D(0.4f * Size, 0.5f * Size), Middle + FVector2D(-0.4f * Size, 0.5f * Size), Colour);
		// The eyes and the mouth, cut out dark.
		const FLinearColor Eye(0.02f, 0.02f, 0.025f, 0.95f);
		Shapes.Rect(Corner.X + 0.12f * Size, Corner.Y + 0.12f * Size, 0.15f * Size, 0.13f * Size, Eye);
		Shapes.Rect(Corner.X + Head - 0.27f * Size, Corner.Y + 0.12f * Size, 0.15f * Size, 0.13f * Size, Eye);
		Shapes.Rect(Corner.X + 0.2f * Size, Corner.Y + 0.33f * Size, Head - 0.4f * Size, 0.05f * Size, Eye);
	}

	/** The status column's chip behind its word: down a solid red-orange, out a grey, dead an outline. */
	void BoardChip(HudDraw::FShapes& Shapes, CheeseStrikeHUD::EStatus Status, float X, float Y, float Width, float Height, float U)
	{
		switch (Status)
		{
		case CheeseStrikeHUD::EStatus::Down:
			Shapes.RoundedRect(X, Y, Width, Height, Height / 2.0f, HudDraw::Faded(CheeseStrikeHUD::DownColour, 0.9f));
			break;
		case CheeseStrikeHUD::EStatus::Out:
			Shapes.RoundedRect(X, Y, Width, Height, Height / 2.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.1f));
			break;
		case CheeseStrikeHUD::EStatus::Respawning:
			Shapes.RoundedFrame(X, Y, Width, Height, Height / 2.0f, FMath::Max(1.0f, 1.5f * U), HudDraw::Faded(HudDraw::Soft, 0.6f));
			break;
		default:
			break;
		}
	}
}

void ACheeseStrikeHUD::DrawScoreboard(const ACheeseStrikeGameState* Match, bool bOver)
{
	if (Match == nullptr)
	{
		return;
	}
	const float U = Unit();
	const float Width = Canvas->SizeX;
	const APlayerController* Viewer = GetOwningPlayerController();
	const APlayerState* MyState = Viewer != nullptr ? Viewer->PlayerState.Get() : nullptr;
	const bool bLadder = Match->GetModeInfo().WinBy == CheeseStrikeModes::EWinBy::Ladder;
	const int32 Steps = CheeseStrikeArsenal::GunGameLadder().Num();
	// King of the Hill: each player's time on the hill; in the solo mode, their points first, which it is played to.
	const bool bHill = Match->GetModeInfo().WinBy == CheeseStrikeModes::EWinBy::Hill;
	const bool bSoloHill = bHill && !Match->HasTeams();
	// Execution counts downs beside the kills.
	const bool bExecution = Match->GetMode() == CheeseStrikeModes::EMode::Execution;

	// The players in order: in a team mode, each side apart under its name and score; in a free-for-all, everyone together.
	// Gun Game goes by the step up the ladder, then the kills.
	struct FLine
	{
		CheeseStrikeHUD::FBoardRow Row;
		int32 Step = 0;
		int32 HillSeconds = 0;
		int32 HillPoints = 0;
		int32 Assists = 0;
		int32 Downs = 0;
	};
	struct FSection
	{
		int32 Team = INDEX_NONE;
		TArray<FLine> Lines;
	};
	TArray<FSection> Sections;
	for (int32 Team = 0; Team < (Match->HasTeams() ? CheeseStrikeRules::Teams : 1); ++Team)
	{
		Sections.AddDefaulted_GetRef().Team = Match->HasTeams() ? Team : INDEX_NONE;
	}
	{
		// Each player's standing beside them, to find them again once in order (players' ids aren't told apart for bots).
		TArray<TArray<TPair<CheeseStrikeRules::FStanding, const ACheeseStrikePlayerState*>>> Players;
		Players.SetNum(Sections.Num());
		for (const APlayerState* Player : Match->PlayerArray)
		{
			if (const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Player))
			{
				// Gun Game by the step first, the solo hill by the points first; the podium orders them alike.
				CheeseStrikePodiums::FContender Contender;
				Contender.Standing = Ours->Standing();
				Contender.GunLevel = Ours->GetGunLevel();
				Contender.HillPoints = Ours->GetHillPoints();
				Contender.HillSeconds = Ours->GetHillSeconds();
				const CheeseStrikeRules::FStanding Standing = CheeseStrikePodiums::RankedStanding(Contender, Match->GetModeInfo().WinBy, Match->HasTeams());
				Players[Match->HasTeams() ? FMath::Clamp(Match->TeamOf(Ours), 0, CheeseStrikeRules::Teams - 1) : 0].Emplace(Standing, Ours);
			}
		}
		for (int32 Index = 0; Index < Sections.Num(); ++Index)
		{
			TArray<CheeseStrikeRules::FStanding> Standings;
			for (const TPair<CheeseStrikeRules::FStanding, const ACheeseStrikePlayerState*>& Each : Players[Index])
			{
				Standings.Add(Each.Key);
			}
			for (const CheeseStrikeRules::FStanding& Standing : CheeseStrikeRules::Sorted(Standings))
			{
				const int32 Found = Players[Index].IndexOfByPredicate([&Standing](const TPair<CheeseStrikeRules::FStanding, const ACheeseStrikePlayerState*>& Each)
				{
					return Each.Value != nullptr && Each.Key.Name == Standing.Name && Each.Key.Kills == Standing.Kills && Each.Key.Deaths == Standing.Deaths && Each.Key.Order == Standing.Order;
				});
				if (Found == INDEX_NONE)
				{
					continue;
				}
				const ACheeseStrikePlayerState* Player = Players[Index][Found].Value;
				Players[Index][Found].Value = nullptr;
				FLine& Line = Sections[Index].Lines.AddDefaulted_GetRef();
				Line.Row = CheeseStrikeHUD::BoardRow(Player->Standing(), StatusOf(Player, BodyOf(Player)), Player->IsABot(), Player == MyState);
				Line.Step = Player->GetGunLevel() + 1;
				Line.HillSeconds = Player->GetHillSeconds();
				Line.HillPoints = Player->GetHillPoints();
				Line.Assists = Player->GetAssists();
				Line.Downs = Player->GetDowns();
			}
		}
	}

	// The columns, from the panel's left: the figure, the name, the status; from its right: the deaths, the kills and Gun Game's step.
	const float RowHeight = 30.0f * U;
	const float SectionHeight = Match->HasTeams() ? 40.0f * U : 0.0f;
	const float PanelWidth = (bSoloHill ? 830.0f : (bLadder || bHill || bExecution) ? 740.0f : 660.0f) * U;
	const float Left = (Width - PanelWidth) / 2.0f;
	const float FigureX = Left + 36.0f * U;
	const float NameX = Left + 60.0f * U;
	const float DeathsX = Left + PanelWidth - 20.0f * U;
	const float AssistsX = Left + PanelWidth - 100.0f * U;
	const float KillsX = Left + PanelWidth - 180.0f * U;
	// Gun Game's step, the hill's time, or Execution's downs.
	const float StepX = Left + PanelWidth - 260.0f * U;
	const float PointsX = Left + PanelWidth - 340.0f * U;
	const float ChipWidth = 66.0f * U;
	const float StatusX = (bSoloHill ? PointsX - 110.0f * U : (bLadder || bHill || bExecution) ? StepX - 110.0f * U : KillsX - 110.0f * U) - ChipWidth / 2.0f;
	const float NameWidth = StatusX - ChipWidth / 2.0f - 14.0f * U - NameX;

	float PanelHeight = 128.0f * U;
	for (const FSection& Section : Sections)
	{
		PanelHeight += SectionHeight + RowHeight * Section.Lines.Num();
	}
	const float Top = Canvas->SizeY * 0.2f;
	const FSlateFontInfo Heading = HudDraw::Font(13.0f * U);
	const FSlateFontInfo RowFont = HudDraw::Font(18.0f * U);
	const FSlateFontInfo ChipFont = HudDraw::Font(14.0f * U);
	const FSlateFontInfo Side = HudDraw::Font(20.0f * U);
	const float RowText = HudDraw::Measure(TEXT("Ag"), RowFont).Y;
	const float ChipText = HudDraw::Measure(TEXT("DOWN"), ChipFont).Y;
	const float ChipHeight = ChipText + 2.0f * U;
	const FLinearColor OwnColour = HudDraw::Accent;

	HudDraw::FShapes Shapes;
	// A rounded panel, its top edge lit from side to side, clipped to the panel's corners.
	const float PanelRound = 12.0f * U;
	Shapes.RoundedRect(Left, Top, PanelWidth, PanelHeight, PanelRound, FLinearColor(0.02f, 0.02f, 0.025f, 0.85f));
	{
		const FVector2D PanelMin(Left, Top);
		const FVector2D PanelMax(Left + PanelWidth, Top + PanelHeight);
		Shapes.SmoothPolygon(CheeseStrikeHUD::TopStrip(PanelMin, PanelMax, PanelRound, 3.0f * U), bOver ? HudDraw::Accent : HudDraw::Ink);
	}
	Shapes.Rect(Left + 20.0f * U, Top + 104.0f * U, PanelWidth - 40.0f * U, U, HudDraw::Rule);
	{
		float Y = Top + 114.0f * U;
		for (const FSection& Section : Sections)
		{
			if (Section.Team != INDEX_NONE)
			{
				// A strip of the side's colour down the left of its rows.
				Shapes.RoundedRect(Left + 10.0f * U, Y + 6.0f * U, 3.0f * U, SectionHeight - 10.0f * U + RowHeight * Section.Lines.Num(), 1.5f * U, HudDraw::SideColour(Section.Team));
				Y += SectionHeight;
			}
			for (const FLine& Line : Section.Lines)
			{
				const float Alpha = CheeseStrikeHUD::RowAlpha(Line.Row.Status);
				const float Middle = Y + RowText / 2.0f;
				BoardFigure(Shapes, Line.Row.Kind, FVector2D(FigureX, Middle), 22.0f * U, HudDraw::Faded(Line.Row.bMine ? OwnColour : HudDraw::Soft, Alpha));
				BoardChip(Shapes, Line.Row.Status, StatusX - ChipWidth / 2.0f, Middle - ChipHeight / 2.0f, ChipWidth, ChipHeight, U);
				Y += RowHeight;
			}
		}
	}
	Shapes.Draw(Canvas);

	const FString Title = bOver ? (Match->GetWinnerName().IsEmpty() ? TEXT("MATCH OVER") : FString::Printf(TEXT("%s %s"), *Match->GetWinnerName().ToUpper(), Match->HasTeams() ? TEXT("WIN") : TEXT("WINS"))) : CheeseStrikeModes::Title(Match->GetMode()).ToUpper();
	const FSlateFontInfo TitleFont = HudDraw::Font(30.0f * U);
	// A winner's long name cut to leave the panel's edges clear.
	const FString Fitted = CheeseStrikeHUD::FitNameIn(Title, Match->GetWinnerName().ToUpper(), PanelWidth - 140.0f * U, [&TitleFont](const FString& Text) { return static_cast<float>(HudDraw::Measure(Text, TitleFont).X); });
	HudDraw::Text(Canvas, Fitted, FVector2D(Width / 2.0f, Top + 16.0f * U), TitleFont, bOver ? HudDraw::Accent : HudDraw::Ink, 0.5f);
	const float ToLobby = Match->GetLobbyAt() > 0.0f ? FMath::Max(0.0f, Match->GetLobbyAt() - static_cast<float>(Match->GetServerWorldTimeSeconds())) : -1.0f;
	const FString Under = bOver && ToLobby >= 0.0f ? FString::Printf(TEXT("%s  ·  back to the lobby in %.0f s"), *Match->GoalText(), FMath::CeilToFloat(ToLobby)) : Match->GoalText();
	HudDraw::Text(Canvas, Under.ToUpper(), FVector2D(Width / 2.0f, Top + 58.0f * U), HudDraw::Font(14.0f * U), HudDraw::Soft, 0.5f);
	float Y = Top + 84.0f * U;
	HudDraw::Text(Canvas, TEXT("PLAYER"), FVector2D(NameX, Y), Heading, HudDraw::Soft);
	HudDraw::Text(Canvas, TEXT("STATUS"), FVector2D(StatusX, Y), Heading, HudDraw::Soft, 0.5f);
	if (bLadder)
	{
		HudDraw::Text(Canvas, TEXT("STEP"), FVector2D(StepX, Y), Heading, HudDraw::Soft, 1.0f);
	}
	if (bHill)
	{
		HudDraw::Text(Canvas, TEXT("ON THE HILL"), FVector2D(StepX, Y), Heading, HudDraw::Soft, 1.0f);
	}
	if (bExecution)
	{
		HudDraw::Text(Canvas, TEXT("DOWNS"), FVector2D(StepX, Y), Heading, HudDraw::Soft, 1.0f);
	}
	HudDraw::Text(Canvas, TEXT("ASSISTS"), FVector2D(AssistsX, Y), Heading, HudDraw::Soft, 1.0f);
	if (bSoloHill)
	{
		HudDraw::Text(Canvas, TEXT("POINTS"), FVector2D(PointsX, Y), Heading, HudDraw::Soft, 1.0f);
	}
	HudDraw::Text(Canvas, TEXT("KILLS"), FVector2D(KillsX, Y), Heading, HudDraw::Soft, 1.0f);
	HudDraw::Text(Canvas, TEXT("DEATHS"), FVector2D(DeathsX, Y), Heading, HudDraw::Soft, 1.0f);
	const auto MeasureName = [&RowFont](const FString& Text) { return static_cast<float>(HudDraw::Measure(Text, RowFont).X); };
	Y = Top + 114.0f * U;
	for (const FSection& Section : Sections)
	{
		if (Section.Team != INDEX_NONE)
		{
			// The side's name, and beside it its score with what it counts ("3 ROUNDS WON"), clear of the players' columns.
			const FLinearColor Colour = HudDraw::SideColour(Section.Team);
			const FString SideName = FString(CheeseStrikeRules::TeamNames[Section.Team]).ToUpper();
			HudDraw::Text(Canvas, SideName, FVector2D(Left + 24.0f * U, Y + 6.0f * U), Side, Colour);
			const float ScoreX = Left + 24.0f * U + HudDraw::Measure(SideName, Side).X + 16.0f * U;
			const FString Score = FString::FromInt(Match->GetTeamScore(Section.Team));
			HudDraw::Text(Canvas, Score, FVector2D(ScoreX, Y + 6.0f * U), Side, HudDraw::Ink);
			const float LabelX = ScoreX + HudDraw::Measure(Score, Side).X + 6.0f * U;
			HudDraw::Text(Canvas, CheeseStrikeHUD::TeamScoreLabel(Match->GetModeInfo().WinBy, Match->GetTeamScore(Section.Team)), FVector2D(LabelX, Y + 6.0f * U + HudDraw::Measure(TEXT("0"), Side).Y - HudDraw::Measure(TEXT("0"), Heading).Y - 2.0f * U), Heading, HudDraw::Soft);
			Y += SectionHeight;
		}
		for (const FLine& Line : Section.Lines)
		{
			const CheeseStrikeHUD::FBoardRow& Row = Line.Row;
			const FLinearColor Colour = HudDraw::Faded(Row.bMine ? OwnColour : HudDraw::Ink, CheeseStrikeHUD::RowAlpha(Row.Status));
			HudDraw::Text(Canvas, CheeseStrikeHUD::FitName(Row.Name, NameWidth, MeasureName), FVector2D(NameX, Y), RowFont, Colour);
			const FString Word = CheeseStrikeHUD::StatusWord(Row.Status);
			if (!Word.IsEmpty())
			{
				const FLinearColor WordColour = Row.Status == CheeseStrikeHUD::EStatus::Down ? HudDraw::Ink : HudDraw::Soft;
				HudDraw::Text(Canvas, Word, FVector2D(StatusX, Y + RowText / 2.0f - ChipText / 2.0f), ChipFont, WordColour, 0.5f, Row.Status == CheeseStrikeHUD::EStatus::Down ? false : true);
			}
			if (bLadder)
			{
				HudDraw::Text(Canvas, FString::Printf(TEXT("%d/%d"), Line.Step, Steps), FVector2D(StepX, Y), RowFont, Colour, 1.0f);
			}
			if (bHill)
			{
				HudDraw::Text(Canvas, FString::Printf(TEXT("%d:%02d"), Line.HillSeconds / 60, Line.HillSeconds % 60), FVector2D(StepX, Y), RowFont, Colour, 1.0f);
			}
			if (bSoloHill)
			{
				HudDraw::Text(Canvas, FString::FromInt(Line.HillPoints), FVector2D(PointsX, Y), RowFont, Colour, 1.0f);
			}
			HudDraw::Text(Canvas, FString::FromInt(Row.Kills), FVector2D(KillsX, Y), RowFont, Colour, 1.0f);
			HudDraw::Text(Canvas, FString::FromInt(Line.Assists), FVector2D(AssistsX, Y), RowFont, Colour, 1.0f);
			if (bExecution)
			{
				HudDraw::Text(Canvas, FString::FromInt(Line.Downs), FVector2D(StepX, Y), RowFont, Colour, 1.0f);
			}
			HudDraw::Text(Canvas, FString::FromInt(Row.Deaths), FVector2D(DeathsX, Y), RowFont, Colour, 1.0f);
			Y += RowHeight;
		}
	}
}
