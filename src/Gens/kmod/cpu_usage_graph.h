#ifndef KMOD_CPU_USAGE_GRAPH_H
#define KMOD_CPU_USAGE_GRAPH_H
#include "frame_graph.h"
#define CPU_USAGE_HISTORY FRAME_GRAPH_HISTORY
static void CPUUsage_DrawGraph(const DRAWITEMSTRUCT *item, const unsigned long *values, unsigned int count)
{
    char text[64];
    if (count) wsprintf(text, "%lu.%lu %%", values[count-1]/10, values[count-1]%10);
    else lstrcpy(text, "--");
    FrameGraph_Draw(item, values, count, 1000, text, "100", "50");
}
#endif
