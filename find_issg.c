#include "finders.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>


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
    int spawn_x = atoi(getField(line, 1));
    int spawn_z = atoi(getField(line, 2));
    int portal_x = atoi(getField(line, 3));
    int portal_z = atoi(getField(line, 4));
    int portal_distance = atoi(getField(line, 5));

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
 * return true if a village is less that 16 blocks away from spawn in both the x and y directions.
 * @param ssp - seed, spawn, end portal info
 * @return boolean
 */
bool isVillageCloseToSpawn(SSP ssp, LayerStack* layerAddress, Pos* villagePosAddress) {
    int villageType = Village;
    Pos villagePos; // block position to be checked
    if (getStructurePos(villageType, MC, ssp.seed, 0, 0, &villagePos)) {
        // Look for a seed with the structure near spawn
        if (abs(ssp.spawn_x - villagePos.x) <= 16 && abs(ssp.spawn_z - villagePos.z) <= 16) {
            if (isViableStructurePos(villageType, MC, layerAddress, ssp.seed, villagePos.x, villagePos.z)) {
//                printf("Seed %lld has a Village at (%d, %d).\n", seed, villagePos.x, villagePos.z);
//                printf("And spawn is at: (%d, %d).\n", spawn_x, spawn_z);
//                printf("Checked %d seeds.\n", seedsChecked);

                villagePosAddress->x = villagePos.x;
                villagePosAddress->z = villagePos.z;
                return true;
            }
        }
    }

    return false;
}

/**
 * True if location is in an ocean
 * @param layerAddress - Address of the layer stack
 * @param pos - position to check
 * @return boolean
 */
bool isOcean(LayerStack* layerAddress, Pos pos) {
    return (getBiomeAtPos(layerAddress, pos) == Ocean);
}

/**
 * Return true if blacksmith is in the village at position villagePos
 * @param seed - world seed
 * @param villagePos - village position
 * @return boolean
 */
bool isBlacksmithInVillage(int64_t seed, Pos villagePos) {
    return true;

    int housesOut[9];
    getHouseList(seed, villagePos.x, villagePos.z, housesOut);
    printf("housesOut: %d, %d, %d, %d, %d, %d, %d, %d, %d\n", housesOut[0], housesOut[1], housesOut[2], housesOut[3], housesOut[4], housesOut[5], housesOut[6], housesOut[7], housesOut[8]);
//    return (housesOut[Blacksmith] > 0);
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

    FILE* stream = fopen("assets/all_12_eyes_sorted_ring_1_first.txt", "r");

    SSP results[131];

    char line[1024];

    // read the first csv line and do nothing, since it is header line
    fgets(line, 1024, stream);

    while (fgets(line, 1024, stream)) {
        // get the seed, spawn, end portal info for this seed
        SSP ssp = csvLineToSSP(line, candidateSeeds+1);

        // First, let's filter down by village-close-to-spawn
        Pos villagePos;
        if (isVillageCloseToSpawn(ssp, &g, &villagePos)) {

            // Now, let's check if the 12-eye stronghold might be ocean-exposed
            Pos endPortalPos = { ssp.portal_x, ssp.portal_z };
            if (isOcean(&g, endPortalPos)) {

                // finally, lets check for blacksmiths in the nearby village:
                if (isBlacksmithInVillage(ssp.seed, villagePos)) {
//                    printf("Seed %lld has a Village near spawn and 12-eye stronghold is oceanic.\n", ssp.seed);
//                    printf("12-eye portal is at: (%d, %d).\n", ssp.portal_x, ssp.portal_z);
//                    printSSP(ssp);
//
                    printf("%d - %lld end portal at: /tp @s %d 40 %d\n", candidateSeeds+1, ssp.seed, ssp.portal_x, ssp.portal_z);

                    results[candidateSeeds] = ssp;
                    candidateSeeds++;
                }
            }
        }

        seedsChecked++;

//        if (seedsChecked >= 300000) {
//            break;
//        }

    }

    fclose(stream);

    qsort(results, 131, sizeof(SSP), sspDistanceCompare);

    for (int j = 0; j < 131; j++) {
        printf("%d - %lld end portal at: /tp @s %d 40 %d\n", results[j].csv_line, results[j].seed, results[j].portal_x, results[j].portal_z);
        printf("distance of the above: %d\n", results[j].portal_distance);
    }

    printf("Checked %d seeds\n", seedsChecked);
    if (candidateSeeds > 0) {
        printf("Found %d candidate seeds.\n", candidateSeeds);
    } else {
        printf("No candidates.\n");
    }

    return 0;
}

