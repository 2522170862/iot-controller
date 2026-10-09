#include "DashboardView.h"

static_assert(DashboardView::gaugeSegmentsForPercent(-5.0f) == 0);
static_assert(DashboardView::gaugeSegmentsForPercent(0.0f) == 0);
static_assert(DashboardView::gaugeSegmentsForPercent(50.0f) == 10);
static_assert(DashboardView::gaugeSegmentsForPercent(75.0f) == 15);
static_assert(DashboardView::gaugeSegmentsForPercent(100.0f) == 20);
static_assert(DashboardView::gaugeSegmentsForPercent(120.0f) == 20);
static_assert(DashboardView::overviewContentWidth() == 226);
static_assert(DashboardView::pressureCardWidth() ==
              DashboardView::overviewContentWidth());
static_assert(!DashboardView::showsRs485StatusCard());
static_assert(DashboardView::renderDue(100, 0, false));
static_assert(!DashboardView::renderDue(499, 0, true));
static_assert(DashboardView::renderDue(500, 0, true));
// Wi-Fi 状态变化不能等待下一次 500 ms 的整页刷新。
static_assert(DashboardView::networkBadgeRefreshDue(false, false, false));
static_assert(DashboardView::networkBadgeRefreshDue(true, false, true));
static_assert(!DashboardView::networkBadgeRefreshDue(true, true, true));

int main() { return 0; }
