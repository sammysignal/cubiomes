#include "finders.h"
#include "villagePosList.h"
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

// global constants
const int MC = MC_1_16;
const int REGION_LENGTH = 512;

// configurable
//  (discrepancy < SEARCH_WIDTH && villageAngleIsBetween) || (villageDistanceFromSpawn < CLOSE_VILLAGE_MAX_DIST)
const float SEARCH_WIDTH = 100.0;
const float ANGLE_WIDTH = 1;    // 0.1 radians is about 6 degrees.
const float CLOSE_VILLAGE_MAX_DIST = 30.0;
const int SEEDS_TO_CHECK = 100000;
const bool DEBUG = false;

// global counters
int seedsChecked = 0;
int candidateSeeds = 0;
int totalPlainsVillages = 0;
int plainsVillagesOnTheWay = 0;
int oceanStrongholds = 0;
int regionsChecked = 0;


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
 * Quake's fast inverse square root
 * @param x
 * @return
 */
float invSqrt(float x){
    float xhalf = 0.5f * x;
    int i = *(int*)&x;            // store floating-point bits in integer
    i = 0x5f3759df - (i >> 1);    // initial guess for Newton's method
    x = *(float*)&i;              // convert new bits into float
    x = x*(1.5f - xhalf*x*x);     // One round of Newton's method
    return x;
}

/**
 * Shortest distance from a point to a line, as given here:
 *   https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line
 *
 * @param p1_x - Line passes through point (p1_x, p1_y) and (p2_x, p2_y)
 * @param p1_y
 * @param p2_x
 * @param p2_y
 * @param x - (x,y) describes the point to calculate the distance to.
 * @param y
 * @return distance
 */
float pointToLineDistance(float p1_x, float p1_y, float p2_x, float p2_y, float x, float y) {
    float numerator = fabs((p2_x-p1_x) * (p1_y-y) - (p1_x-x) * (p2_y-p1_y));
    float inv_denominator = invSqrt(pow((p2_x-p1_x), 2) + pow((p2_y-p1_y), 2));
    return numerator * inv_denominator;
}

/**
 * Distance between two points
 * @param p1_x
 * @param p1_y
 * @param p2_x
 * @param p2_y
 * @return
 */
float pointToPointDistance(float p1_x, float p1_y, float p2_x, float p2_y) {
    return sqrt(pow((p2_x - p1_x), 2) + pow((p2_y - p1_y), 2));
}


/**
 * Angle of point.
 *
 *      1, 0  => 0
 *      0, 1  => pi/2
 *      -1, 0 => pi (or -pi)
 *      0, -1 => -pi
 * @param x
 * @param y
 * @return
 */
float angleOfPoint(float x, float y) {
    return atan2(y,x);
}

/**
 * Get a plains village in the region and update the p value with the coordinates of that village.
 * Return true if there is a plains village in the given region, otherwise false.
 * @param ssp - Seed, spawn, portal info
 * @param layerAddress - layer address
 * @param x - region x cord to check
 * @param z - region z cord to check
 * @param p - pos to update with found village cords
 * @return true if plains village is in region
 */
bool getPlainsVillageForGivenRegion(SSP ssp, LayerStack* layerAddress, int x, int z, Pos* p) {
    regionsChecked++;
    int villageType = Village;
    Pos villagePos; // block position to be checked
    if (getStructurePos(villageType, MC, ssp.seed, x, z, &villagePos)) {
//        printf("found village at %d, %d in region %d, %d\n", villagePos.x, villagePos.z, x, z);

        if (isViableStructurePos(villageType, MC, layerAddress, ssp.seed, x, z)) {
            Pos villageBiomePos = { villagePos.x + 8, villagePos.z + 8 };
            int villagePosBiomeId = getBiomeAtPos(layerAddress, villagePos);
            int villageBiomePosBiomeId = getBiomeAtPos(layerAddress, villageBiomePos);

            if (DEBUG) {
                printf("villageBiome: %d\n", villagePosBiomeId);
                printf("villageBiomePosBiomeId: %d\n", villageBiomePosBiomeId);
                printf("villagePos: %d, %d\n", villagePos.x, villagePos.z);
            }


            p->x = villagePos.x;
            p->z = villagePos.z;

            /**
             * To estimate the village type, either the block at the village chunk border
             *  or the block at the center of the village block must be plains.
             */
            return (villagePosBiomeId == plains || villagePosBiomeId == sunflower_plains)
                   || (villageBiomePosBiomeId == plains || villageBiomePosBiomeId == sunflower_plains);

        } else {
            if (DEBUG) {
                printf("village is not viable\n");
            }
        }
    } else {
        if (DEBUG) {
            printf("no village\n");
        }
    }

    return false;
}


void getPlainsVillagesForGivenRegions(SSP ssp, LayerStack* layerAddress, int dimOfXsToCheck, int dimOfZsToCheck, int regionCount, PosList* output) {
    for (int i = 0; i < abs(dimOfXsToCheck)+1; i++) {
        for (int j = 0; j < abs(dimOfZsToCheck) + 1; j++) {
            int x = 0;
            if (dimOfXsToCheck < 0) {
                x = -i;
            } else {
                x = i;
            }
            int z = 0;
            if (dimOfZsToCheck < 0) {
                z = -j;
            } else {
                z = j;
            }

//            printf("checking region %d, %d\n", x, z);
            Pos plainsVillagePos = { 0, 0 };
            if (getPlainsVillageForGivenRegion(ssp, layerAddress, x, z, &plainsVillagePos)) {
                totalPlainsVillages++;
                Pos villageLocation = { plainsVillagePos.x, plainsVillagePos.z };
                plAppend(output, &villageLocation);
            }

        }
    }


    ////

//    int villagesFound = 0;
//    printf("regionCount: %d\n", regionCount);
//    for (int i = 0; i < regionCount; i++) {
//        int x = xs[i];
//        int z = zs[i];
//        printf("checking region %d, %d\n", x, z);
//        Pos plainsVillagePos = { 0, 0 };
//        if (getPlainsVillageForGivenRegion(ssp, layerAddress, x, z, &plainsVillagePos)) {
//            Pos villageLocation = { plainsVillagePos.x, plainsVillagePos.z };
//            plAppend(output, &villageLocation);
//            villagesFound++;
//        }
//    }
}

/**
 * computes floor(dividend/divisor) but without any weird off by 1 things.
 *
 * Assumes divisor is always positive!
 *
 * ex:
 *    4, 2 -> 2
 *    5, 2 -> 2
 *    -4, 2 -> -2
 *    -5, 2 -> -3
 *
 * @param dividend
 * @param divisor
 * @return
 */
int floorDivide(int a, int b) {
    if (a % b == 0) {
        return (a / b);
    }

    if (a >= 0) {
        return ceil(((float) a) / b) - 1;
    }

    // else if (a < 0)
    return floor(((float) a) / b);

}

bool printRoutableVillageCords(SSP ssp, PosList foundVillages) {
    Node* posNode = foundVillages.list;
    bool foundGoodVillage = false;
    while (posNode != NULL) {
        float discrepancy = pointToLineDistance(
                (float) ssp.spawn_x,
                (float) ssp.spawn_z,
                (float) ssp.portal_x,
                (float) ssp.portal_z,
                (float) posNode->p.x,
                (float) posNode->p.z
        );


        float spawnDistanceFromOrigin = pointToPointDistance(0, 0, ssp.spawn_x, ssp.spawn_z);
        float portalDistanceFromOrigin = pointToPointDistance(0, 0, ssp.portal_x, ssp.portal_z);
        float villageDistanceFromOrigin = pointToPointDistance(0, 0, posNode->p.x, posNode->p.z);

        float villageDistanceFromSpawn = pointToPointDistance(ssp.spawn_x, ssp.spawn_z, posNode->p.x, posNode->p.z);

        float angleOfSpawn = angleOfPoint(ssp.spawn_x, ssp.spawn_z);
        float angleOfPortal = angleOfPoint(ssp.portal_x, ssp.portal_z);
        float angleOfVillage = angleOfPoint(posNode->p.x, posNode->p.z);

        if (DEBUG) {
            printf("discrepancy: %f\n", discrepancy);
            printf("spawnDistanceFromOrigin: %f\n", spawnDistanceFromOrigin);
            printf("portalDistanceFromOrigin: %f\n", portalDistanceFromOrigin);
            printf("villageDistanceFromOrigin: %f\n", villageDistanceFromOrigin);

            printf("villageDistanceFromSpawn: %f\n", villageDistanceFromSpawn);

            printf("angleOfSpawn: %f\n", angleOfSpawn);
            printf("angleOfPortal: %f\n", angleOfPortal);
            printf("angleOfVillage: %f\n", angleOfVillage);

        }

        bool villageBetweenSpawnAndPortal =
          (spawnDistanceFromOrigin < villageDistanceFromOrigin) && (villageDistanceFromOrigin < portalDistanceFromOrigin);

        bool villageAngleIsBetween = false;
        if (angleOfSpawn < angleOfPortal) {
            villageAngleIsBetween = (angleOfSpawn < angleOfVillage + ANGLE_WIDTH) && (angleOfVillage < angleOfPortal + ANGLE_WIDTH);
        } else {
            villageAngleIsBetween = (angleOfPortal < angleOfVillage + ANGLE_WIDTH) && (angleOfVillage < angleOfSpawn + ANGLE_WIDTH);
        }

//        if (discrepancy < SEARCH_WIDTH && villageAngleIsBetween) {
//            printf("tru\n");
//        }
//        if ((villageDistanceFromSpawn < CLOSE_VILLAGE_MAX_DIST)) {
//            printf("its close\n");
//        }

        // Make sure the village isnt too far off the line between the spawn and the end portal.
        if ((discrepancy < SEARCH_WIDTH && villageBetweenSpawnAndPortal && villageAngleIsBetween) || (villageDistanceFromSpawn < CLOSE_VILLAGE_MAX_DIST)){
            printf("Found good village at: (%d, %d)\n", posNode->p.x, posNode->p.z);
            foundGoodVillage = true;
            plainsVillagesOnTheWay++;
        }

        posNode = posNode->next;

    }

    return foundGoodVillage;
}

/**
 * return true if a village is on the way to the portal
 * @param ssp - seed, spawn, end portal info
 * @return boolean
 */
bool isPlainsVillageEnRouteToPortal(SSP ssp, LayerStack* layerAddress, Pos* villagePosAddress) {
    // lets assume that the spawn is well into the same region that the stronghold is in.
    int dimOfXsToCheck = floorDivide(ssp.portal_x, REGION_LENGTH);
    int dimOfZsToCheck = floorDivide(ssp.portal_z, REGION_LENGTH);

    int totalRegionsToCheck = (abs(dimOfXsToCheck) + 1) * (abs(dimOfZsToCheck) + 1);

    if (DEBUG) {
        printf("dimOfXsToCheck: %d\n", dimOfXsToCheck);
        printf("dimOfZsToCheck: %d\n", dimOfZsToCheck);
        printf("totalRegionsToCheck: %d\n", totalRegionsToCheck);
    }

//    int xsToCheck[totalRegionsToCheck];
//    int zsToCheck[totalRegionsToCheck];

    /*
     * kinda dumb, but here we build x,z pairs for each region to look in.
     * e.g.
     *   0, -1 should result in [0], and [-1]
     *   0, -2 should result in [0, 0], and [-1, -2]
     */
//    for (int i = 0; i < abs(dimOfXsToCheck)+1; i++) {
//        for (int j = 0; j < abs(dimOfZsToCheck)+1; j++) {
//            if (dimOfXsToCheck < 0) {
//                xsToCheck[i] = -i;
//            } else {
//                xsToCheck[i] = i;
//            }
//
//            if (dimOfZsToCheck < 0) {
//                zsToCheck[j] = -j;
//            } else {
//                zsToCheck[j] = j;
//            }
//
//            printf("zsToCheck[j]: %d\n", zsToCheck[j]);
//
//            if (DEBUG) {
//                printf("check: [%i, %i]\n", xsToCheck[i], zsToCheck[j]);
//            }
//
//        }
//    }

//    printf("zsToCheck[1]: %i\n", zsToCheck[1]+2);

    PosList foundVillages = { NULL, NULL };

    getPlainsVillagesForGivenRegions(ssp, layerAddress, dimOfXsToCheck, dimOfZsToCheck, totalRegionsToCheck, &foundVillages);

    if (DEBUG) {
        printPosList(foundVillages);
    }

    // now print any pair of village cords that are on the way
    bool routableVillageFound = printRoutableVillageCords(ssp, foundVillages);

    // cleanup
    plFree(&foundVillages);

    return routableVillageFound;
}

/**
 * True if location is in an ocean
 * @param layerAddress - Address of the layer stack
 * @param pos - position to check
 * @return boolean
 */
bool isOcean(LayerStack* layerAddress, Pos pos) {
    int x = pos.x;
    int z = pos.z;

    Pos pn = {x+8, z};
    Pos ps = {x-8, z};
    Pos pe = {x, z+8};
    Pos pw = {x, z-8};
    int localCords[] = {
            getBiomeAtPos(layerAddress, pos),
            getBiomeAtPos(layerAddress, pn),
            getBiomeAtPos(layerAddress, ps),
            getBiomeAtPos(layerAddress, pe),
            getBiomeAtPos(layerAddress, pw),
    };

    for (int i = 0; i < 5; i++) {
        if (
            localCords[i] == ocean ||
            localCords[i] == frozen_ocean ||
            localCords[i] == deep_ocean ||
            localCords[i] == warm_ocean ||
            localCords[i] == lukewarm_ocean ||
            localCords[i] == cold_ocean ||
            localCords[i] == deep_warm_ocean ||
            localCords[i] == deep_lukewarm_ocean ||
            localCords[i] == deep_cold_ocean ||
            localCords[i] == deep_frozen_ocean) {
                return true;
        }
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

    FILE* stream = fopen("assets/all_12_eyes_sorted_ring_1_first.txt", "r");

    SSP results[SEEDS_TO_CHECK];

    char line[1024];

    // read the first csv line and do nothing, since it is header line
    fgets(line, 1024, stream);

    while (fgets(line, 1024, stream)) {
        // get the seed, spawn, end portal info for this seed
//        char l[] = "-8418116630185684261,-244,-124,-1194,-579,1055";
//        SSP ssp = csvLineToSSP(l, 1);

        SSP ssp = csvLineToSSP(line, candidateSeeds+1);

        // Apply seed to layer stack
        applySeed(&g, ssp.seed);

        // First, let's filter down by ocean stronghold
        Pos villagePos;
        Pos endPortalPos = { ssp.portal_x, ssp.portal_z };

        if (isOcean(&g, endPortalPos)) {
            // Now, let's see if theres a plains village on the way
            Pos endPortalPos = { ssp.portal_x, ssp.portal_z };
            if (isPlainsVillageEnRouteToPortal(ssp, &g, &villagePos)) {
                printf("%lld - distance to portal: %d, end portal at: /tp @s %d 40 %d [%d]\n", ssp.seed, ssp.portal_distance, ssp.portal_x, ssp.portal_z, candidateSeeds+1);

                results[candidateSeeds] = ssp;
                candidateSeeds++;

            }

            oceanStrongholds++;
        }

        seedsChecked++;

        if (seedsChecked >= SEEDS_TO_CHECK) {
            break;
        }

    }

    fclose(stream);


    /////// Printing Results
    printf("//// Results ////\n");


//    for (int j = 0; j < candidateSeeds; j++) {
//        printf("%d - %lld end portal at: /tp @s %d 40 %d\n", results[j].csv_line, results[j].seed, results[j].portal_x, results[j].portal_z);
//        printf("distance of the above: %d\n", results[j].portal_distance);
//    }

    printf("Checked %d seeds\n", seedsChecked);
    if (candidateSeeds > 0) {
        printf("Found %d candidate seeds.\n", candidateSeeds);
    } else {
        printf("No candidates.\n");
    }

//    int totalPlainsVillages = 0;
//    int plainsVillagesOnTheWay = 0;
//    int oceanStrongholds = 0;
    printf("oceanStrongholds: %d (%.1f%%)\n", oceanStrongholds, (oceanStrongholds*100.0/seedsChecked));
    printf("plainsVillagesOnTheWay: %d\n", plainsVillagesOnTheWay);
    printf("regionsChecked: %d\n", regionsChecked);
    printf("totalPlainsVillages: %d\n", totalPlainsVillages);


    return 0;
}

