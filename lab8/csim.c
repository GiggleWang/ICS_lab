/*
name: Wang Yixiao
id: ics522031910732
*/
#include "cachelab.h"
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
typedef struct
{
    int validBit;
    int tagBit;
    int LRU_counter;
} cache_line;
cache_line line;
int s, S, E, b, t;
int hitCount = 0;
int missCount = 0;
int evictionCount = 0;
char filePath[1000]; // 定义file的路径
bool haveV = false;
cache_line **cache;
void helpInfo()
{
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n\n");
    printf("Examples:\n");
    printf("  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}
void updateCacheOnMiss(int setIndex, int addressOfTag);

void dealWithAddress(uint64_t addr)
{
    int setIndex = (addr >> b) & ((1 << s) - 1); // 使用位掩码计算setIndex
    int addressOfTag = addr >> (s + b);
    for (int i = 0; i < E; i++)
    {
        if (cache[setIndex][i].tagBit == addressOfTag && cache[setIndex][i].validBit)
        {
            // Cache hit
            hitCount++;
            cache[setIndex][i].LRU_counter = 0;
            if (haveV)
                printf(" hit");
            return;
        }
    }
    // Cache miss
    missCount++;
    if (haveV)
        printf(" miss");
    updateCacheOnMiss(setIndex, addressOfTag);
}

void updateCacheOnMiss(int setIndex, int addressOfTag)
{
    for (int i = 0; i < E; i++)
    {
        if (!cache[setIndex][i].validBit)
        { // 查找第一个无效位
            cache[setIndex][i] = (cache_line){1, addressOfTag, 0};
            return;
        }
    }
    // 若所有行都有效，则执行eviction策略
    evictionCount++;
    if (haveV)
        printf(" eviction");
    int leastUsedIndex = 0;
    for (int i = 1; i < E; i++)
    {
        if (cache[setIndex][i].LRU_counter > cache[setIndex][leastUsedIndex].LRU_counter)
        {
            leastUsedIndex = i;
        }
    }
    cache[setIndex][leastUsedIndex] = (cache_line){1, addressOfTag, 0}; // 替换最少使用的行
}
int main(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (opt)
        {
        case 'v':
            haveV = true;
            break;
        case 's':
            s = atoi(optarg);
            S = (int)pow(2, s);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            t = atoi(optarg);
            strcpy(filePath, optarg);
            break;
        case 'h':
        default:
            helpInfo();
            exit(EXIT_FAILURE);
        }
    }
    FILE *file = fopen(filePath, "r");
    assert(file);
    if (s < 0 || E < 0 || b < 0 || file == NULL)
    {
        helpInfo();
        exit(0);
    }

    cache = (cache_line **)malloc(S * sizeof(cache_line));
    assert(cache);

    for (int i = 0; i < S; i++)
    {
        cache[i] = (cache_line *)malloc(E * sizeof(cache_line));
        assert(cache[i]);
        memset(cache[i], 0, sizeof(cache_line) * E); // 初始化值，全置零
    }

    char type;
    uint64_t address;
    int size;
    while (fscanf(file, " %c %lx,%d", &type, &address, &size) > 0)
    {
        if (haveV)
        {
            printf("%c %lx,%d", type, address, size);
        }
        if (type != 'I')
        {
            dealWithAddress(address);
        }

        if (type == 'M')
            dealWithAddress(address);
        if (haveV)
        {
            printf("\n");
        }
        for (int i = 0; i < S; ++i)
        {
            for (int j = 0; j < E; ++j)
            {
                if (cache[i][j].validBit)
                {
                    cache[i][j].LRU_counter++;
                }
            }
        }
    }
    fclose(file);
    for (int i = 0; i < S; ++i)
    {
        free(cache[i]);
    }
    free(cache);
    printSummary(hitCount, missCount, evictionCount);
    return 0;
}
