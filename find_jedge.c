// check the biome at a block position
#include "finders.h"
#include <stdio.h>

int main()
{
    // First initialize the global biome table. This sets up properties such as
    // the category and temperature of each biome.
    initBiomes();

    // Initialize a stack of biome layers that reflects the biome generation of
    // Minecraft 1.16
    LayerStack g;
    setupGenerator(&g, MC_1_16);

    int64_t seed;
    Pos pos = {0,0}; // block position to be checked

    for (seed = 0; ; seed++)
    {
        // Go through the layers in the layer stack and initialize the seed
        // dependent aspects of the generator.
        applySeed(&g, seed);

        // To get the biome at single block position we can use getBiomeAtPos().
        int biomeID = getBiomeAtPos(&g, pos);
        if (biomeID == jungle_edge)
            break;
    }

    printf("Seed %" PRId64 " has a Junge Edge biome at block position "
        "(%d, %d).\n", seed, pos.x, pos.z);

    return 0;
}

