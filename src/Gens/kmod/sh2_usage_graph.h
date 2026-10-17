#ifndef KMOD_SH2_USAGE_GRAPH_H
#define KMOD_SH2_USAGE_GRAPH_H
#include "../SH2.h"
#include "cpu_usage_graph.h"
static void SH2Usage_DrawGraph(const DRAWITEMSTRUCT *item, SH2_CONTEXT *cpu, int active)
{
    UINT32 values[SH2_USAGE_HISTORY];
    unsigned int count = active ? SH2_Usage_GetHistory(cpu, values) : 0;
    CPUUsage_DrawGraph(item, values, count);
}
#endif
