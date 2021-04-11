#include "finders.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

/**
 * find issg seeds using dds seeds found on ssg discord.
 *
 * in this file we use 2021-01-12_unique-village-150-shipwreck-300-ocean-portal-1500.csv
 * in order to filter down to plains villages, and then sort by distance to stronghold and pretty print
 * the results so they're easier to check later.
 *
 * We are looking for seeds where the runner can:
 *  - spawn in a plains village
 *  - punch two wood for slabs (crafting table, wood for slabs)
 *  - loot a hotel (plus maybe one other bed)
 *  - fly directly to portal room in under 30 seconds
 *
 */


const int MC = MC_1_16;
int seedsChecked = 0;
int candidateSeeds = 0;

/**
 * This struct captures all the information about a 12-eye seed, as provided by
 */
STRUCT(SSP) {
    int64_t seed;
    int spawn_x;
    int spawn_z;
    int portal_x;
    int portal_z;
    int portal_distance;
    int csv_line;
};

/**
 * Simple CSV line reader.
 *  input:
 *   line: "Sammy, smehra, password123"
 *   num:  2
 *  output: "smehra"
 *
 * @param line - CSV line
 * @param num - the 1-indexed column number of the value you want
 * @return the value
 */
char* getField(char* line, int num) {
    char* lineString = strdup(line);
    char* tok = strtok(lineString, ",");

    for (int i = 0; i < num; i++) {
        tok = strtok(NULL, ",");
        if (tok == NULL) {
            free(lineString);
            return NULL;
        }
    }

    free(lineString);
    return tok;
}

int64_t getSeedFromString(char* seedString) {
    return strtoll(seedString, NULL, 10);
}

void printSSP(SSP ssp) {
    printf("{\n  seed: %lld,\n  spawn_x: %d\n  spawn_z: %d,\n  portal_x: %d\n  portal_z: %d\n  portal_distance: %d\n}\n"
           , ssp.seed, ssp.spawn_x, ssp.spawn_z, ssp.portal_x, ssp.portal_z, ssp.portal_distance);
}

SSP csvLineToSSP(char* line, int csvLine) {
    char* seedString = getField(line, 0);
    int64_t seed = getSeedFromString(seedString);
    int spawn_x = atoi(getField(line, 2));
    int spawn_z = atoi(getField(line, 3));
    int portal_x = atoi(getField(line, 4));
    int portal_z = atoi(getField(line, 5));
    int portal_distance = atoi(getField(line, 6));

    SSP ssp = {
        seed,
        spawn_x,
        spawn_z,
        portal_x,
        portal_z,
        portal_distance,
        csvLine
    };

    return ssp;
}

/**
 * return true if a PLAINS village is less that 16 blocks away from spawn in both the x and y directions.
 * This function assumes that there is a nearby village, but it may not be a plains village.
 * @param ssp - seed, spawn, end portal info
 * @return boolean
 */
bool isPlainsVillageCloseToSpawn(SSP ssp, LayerStack* layerAddress, Pos* villagePosAddress) {
    int village = Village;
    Pos villagePos; // block position to be checked
    if (getStructurePos(village, MC, ssp.seed, 0, 0, &villagePos)) {
        Pos villageBiomePos = { villagePos.x + 8, villagePos.z + 8 };
        int villagePosBiomeId = getBiomeAtPos(layerAddress, villagePos);
        int villageBiomePosBiomeId = getBiomeAtPos(layerAddress, villageBiomePos);
        /**
         * To estimate the village type, either the block at the village chunk border
         *  or the block at the center of the village block must be plains.
         */
        return (villagePosBiomeId == plains || villagePosBiomeId == sunflower_plains)
            || (villageBiomePosBiomeId == plains || villageBiomePosBiomeId == sunflower_plains);
    }

    return false;
}

int sspDistanceCompare(void* ssp_a, void* ssp_b) {
    return ((SSP*) ssp_a)->portal_distance - ((SSP*) ssp_b)->portal_distance;
}

int main(){
    // First initialize the global biome table. This sets up properties such as
    // the category and temperature of each biome.
    initBiomes();

    // Initialize a stack of biome layers that reflects the biome generation of Minecraft 1.16
    LayerStack g;
    setupGenerator(&g, MC);

    FILE* stream = fopen("assets/2021-01-12_unique-village-150-shipwreck-300-ocean-portal-1500.csv", "r");

    // first run this as a big number to avoid overflow
    int results_size = 192;
    SSP results[results_size];

    char line[1024];

    // read the first csv line and do nothing, since it is header line
    fgets(line, 1024, stream);

    while (fgets(line, 1024, stream)) {
        // get the seed, spawn, end portal info for this seed
        SSP ssp = csvLineToSSP(line, candidateSeeds+1);

        applySeed(&g, ssp.seed);

        Pos villagePos;
        if (isPlainsVillageCloseToSpawn(ssp, &g, &villagePos)) {

            Pos endPortalPos = { ssp.portal_x, ssp.portal_z };

            printf("%d - %lld end portal at: /tp @s %d 40 %d\n", candidateSeeds+1, ssp.seed, ssp.portal_x, ssp.portal_z);

            results[candidateSeeds] = ssp;
            candidateSeeds++;

        }

        seedsChecked++;

//        if (seedsChecked >= 10) {
//            break;
//        }

    }

    fclose(stream);

    /////// Printing Results

    printf("//// Results ////\n");

    qsort(results, results_size, sizeof(SSP), sspDistanceCompare);

    for (int j = 0; j < results_size; j++) {
        printf("%d - %lld end portal at: /tp @s %d 40 %d\n", results[j].csv_line, results[j].seed, results[j].portal_x, results[j].portal_z);
//        printf("distance of the above: %d\n", results[j].portal_distance);
    }

    printf("Checked %d seeds\n", seedsChecked);
    if (candidateSeeds > 0) {
        printf("Found %d candidate seeds.\n", candidateSeeds);
    } else {
        printf("No candidates.\n");
    }

    return 0;
}

