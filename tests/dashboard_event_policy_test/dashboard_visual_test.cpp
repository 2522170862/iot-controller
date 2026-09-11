#include "DashboardView.h"

static_assert(DashboardView::gaugeSegmentsForPercent(-5.0f) == 0);
static_assert(DashboardView::gaugeSegmentsForPercent(0.0f) == 0);
static_assert(DashboardView::gaugeSegmentsForPercent(50.0f) == 10);
static_assert(DashboardView::gaugeSegmentsForPercent(75.0f) == 15);
static_assert(DashboardView::gaugeSegmentsForPercent(100.0f) == 20);
static_assert(DashboardView::gaugeSegmentsForPercent(120.0f) == 20);

int main() { return 0; }
