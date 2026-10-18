#pragma once
#include "windows.h"
struct MAGCOLOREFFECT { float transform[5][5]; };
BOOL MagInitialize();
BOOL MagUninitialize();
BOOL MagGetFullscreenColorEffect(MAGCOLOREFFECT*);
BOOL MagSetFullscreenColorEffect(MAGCOLOREFFECT*);
