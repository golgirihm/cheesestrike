// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "Rules/CheeseStrikeShadows.h"
#include "Misc/ConfigCacheIni.h"

bool CheeseStrikeShadows::ShouldTurnOn(const int32 FramesDrawn, const float SecondsSince)
{
	return FramesDrawn >= Frames && SecondsSince >= Seconds;
}

bool CheeseStrikeShadows::WantedFromConfig()
{
	static const TCHAR* Name = TEXT("r.Shadow.Virtual.Enable");
	bool bWanted = true;
	if (GConfig != nullptr)
	{
		GConfig->GetBool(TEXT("/Script/Engine.RendererSettings"), Name, bWanted, GEngineIni);
		GConfig->GetBool(TEXT("SystemSettings"), Name, bWanted, GEngineIni);
	}
	return bWanted;
}
