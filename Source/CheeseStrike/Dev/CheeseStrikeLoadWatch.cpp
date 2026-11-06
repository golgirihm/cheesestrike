// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeLoadWatch.h"
#include "CheeseStrike.h"
#include "ContentStreaming.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "PipelineStateCache.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
	TAutoConsoleVariable<float> CVarLoadWatch(TEXT("CheeseStrike.LoadWatch"), CheeseStrikeLoadWatch::Seconds,
		TEXT("How many seconds after a level loads the log says, once a second, how its frames go and what the engine is busy with (0: none)."));

	struct FWatch
	{
		TWeakObjectPtr<UWorld> World;
		FString Level;
		double StartedAt = 0.0;
		double LastFrameAt = 0.0;
		double SecondFrom = 0.0;
		TArray<float> Second;
		TArray<float> All;
		int32 SlowSeconds = 0;
		FTSTicker::FDelegateHandle Ticker;
	};

	FWatch Watch;
	/** When the last level loaded (FPlatformTime), for IsWarm; 0 before any. */
	double LoadedAt = 0.0;
	bool bSaidWarm = false;

	/** What the engine is busy with now, for the log: "" if nothing. */
	FString Busy(UWorld* World)
	{
		TArray<FString> Parts;
		if (const int32 Psos = CheeseStrikeLoadWatch::PsosCompiling(); Psos > 0)
		{
			Parts.Add(FString::Printf(TEXT("%d PSOs compiling"), Psos));
		}
#if WITH_EDITOR
		if (GShaderCompilingManager != nullptr && GShaderCompilingManager->GetNumRemainingJobs() > 0)
		{
			Parts.Add(FString::Printf(TEXT("%d shaders compiling"), GShaderCompilingManager->GetNumRemainingJobs()));
		}
#endif
		if (const UNavigationSystemV1* Navigation = World != nullptr ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr)
		{
			if (const int32 Tasks = Navigation->GetNumRemainingBuildTasks() + Navigation->GetNumRunningBuildTasks(); Tasks > 0)
			{
				Parts.Add(FString::Printf(TEXT("navigation building (%d tiles)"), Tasks));
			}
		}
		if (const int32 Textures = IStreamingManager::Get().GetNumWantingResources(); Textures > 0)
		{
			Parts.Add(FString::Printf(TEXT("%d textures streaming"), Textures));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	void Stop()
	{
		if (Watch.Ticker.IsValid())
		{
			FTSTicker::RemoveTicker(Watch.Ticker);
			Watch.Ticker.Reset();
		}
	}

	bool Tick(float)
	{
		UWorld* World = Watch.World.Get();
		const double Now = FPlatformTime::Seconds();
		if (World == nullptr)
		{
			Watch.Ticker.Reset();
			return false;
		}
		Watch.Second.Add(static_cast<float>(Now - Watch.LastFrameAt));
		Watch.All.Add(Watch.Second.Last());
		Watch.LastFrameAt = Now;
		if (Now - Watch.SecondFrom >= 1.0)
		{
			bool bSlow = false;
			const FString Frames = CheeseStrikeLoadWatch::DescribeSecond(Watch.Second, &bSlow);
			Watch.SlowSeconds += bSlow ? 1 : 0;
			const FString Doing = Busy(World);
			UE_LOG(LogCheeseStrike, Display, TEXT("Load watch: %s %.0f s: %s%s%s"), *Watch.Level, Now - Watch.StartedAt, *Frames, Doing.IsEmpty() ? TEXT("") : TEXT("; "), *Doing);
			Watch.Second.Reset();
			Watch.SecondFrom = Now;
		}
		if (Now - Watch.StartedAt >= CVarLoadWatch.GetValueOnGameThread())
		{
			float Worst = 0.0f;
			double Total = 0.0;
			for (const float Each : Watch.All)
			{
				Worst = FMath::Max(Worst, Each);
				Total += Each;
			}
			UE_LOG(LogCheeseStrike, Display, TEXT("Load watch: %s, the first %.0f s: %d frames, %.0f fps on the whole (mean %.1f ms, worst %.0f ms), %d seconds under %.0f fps"),
				*Watch.Level, Now - Watch.StartedAt, Watch.All.Num(), Total > 0.0 ? Watch.All.Num() / Total : 0.0, Watch.All.Num() > 0 ? 1000.0 * Total / Watch.All.Num() : 0.0,
				1000.0f * Worst, Watch.SlowSeconds, CheeseStrikeLoadWatch::SlowFps);
			Watch.Ticker.Reset();
			return false;
		}
		return true;
	}
}

bool CheeseStrikeLoadWatch::IsWarm(int32 Psos, float SinceLoaded)
{
	return Psos <= 0 || SinceLoaded >= MostWarmingSeconds;
}

bool CheeseStrikeLoadWatch::IsWarm()
{
	const int32 Psos = PsosCompiling();
	const float Since = LoadedAt > 0.0 ? static_cast<float>(FPlatformTime::Seconds() - LoadedAt) : MostWarmingSeconds;
	const bool bWarm = IsWarm(Psos, Since);
	if (bWarm && !bSaidWarm && LoadedAt > 0.0)
	{
		bSaidWarm = true;
		UE_LOG(LogCheeseStrike, Display, TEXT("Warm: the level's pipeline states %s %.1f s after it loaded"), Psos <= 0 ? TEXT("compiled") : TEXT("still compiling, waited for long enough,"), Since);
	}
	return bWarm;
}

int32 CheeseStrikeLoadWatch::PsosCompiling()
{
	return static_cast<int32>(PipelineStateCache::NumActivePrecacheRequests());
}

FString CheeseStrikeLoadWatch::DescribeSecond(const TArray<float>& Frames, bool* bSlow)
{
	double Total = 0.0;
	float Worst = 0.0f;
	for (const float Each : Frames)
	{
		Total += Each;
		Worst = FMath::Max(Worst, Each);
	}
	const double Fps = Total > 0.0 ? Frames.Num() / Total : 0.0;
	if (bSlow != nullptr)
	{
		*bSlow = Fps < SlowFps;
	}
	return FString::Printf(TEXT("%.0f fps (mean %.0f ms, worst %.0f ms)"), Fps, Frames.Num() > 0 ? 1000.0 * Total / Frames.Num() : 0.0, 1000.0f * Worst);
}

void CheeseStrikeLoadWatch::Start(UWorld* World)
{
	Stop();
	LoadedAt = FPlatformTime::Seconds();
	bSaidWarm = false;
	if (World == nullptr || CVarLoadWatch.GetValueOnGameThread() <= 0.0f)
	{
		return;
	}
	Watch = FWatch();
	Watch.World = World;
	Watch.Level = World->GetOutermost()->GetName();
	Watch.Level = FPaths::GetBaseFilename(Watch.Level);
	Watch.StartedAt = Watch.LastFrameAt = Watch.SecondFrom = FPlatformTime::Seconds();
	UE_LOG(LogCheeseStrike, Display, TEXT("Load watch: %s loaded; %s"), *Watch.Level, *Busy(World));
	Watch.Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
}
