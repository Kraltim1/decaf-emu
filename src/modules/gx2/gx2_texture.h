#pragma once
#include "modules/gx2/gx2_enum.h"
#include "modules/gx2/gx2_surface.h"
#include "types.h"
#include "utils/be_val.h"
#include "utils/structsize.h"

struct GX2Sampler;

#pragma pack(push, 1)

struct GX2Texture
{
   GX2Surface surface;
   be_val<uint32_t> viewFirstMip;
   be_val<uint32_t> viewNumMips;
   be_val<uint32_t> viewFirstSlice;
   be_val<uint32_t> viewNumSlices;
   UNKNOWN(24);
};
CHECK_OFFSET(GX2Texture, 0x0, surface);
CHECK_OFFSET(GX2Texture, 0x74, viewFirstMip);
CHECK_OFFSET(GX2Texture, 0x78, viewNumMips);
CHECK_OFFSET(GX2Texture, 0x7c, viewFirstSlice);
CHECK_OFFSET(GX2Texture, 0x80, viewNumSlices);
CHECK_SIZE(GX2Texture, 0x9c);

#pragma pack(pop)

void
GX2InitTextureRegs(GX2Texture *texture);

void
GX2SetPixelTexture(GX2Texture *texture, uint32_t unit);
