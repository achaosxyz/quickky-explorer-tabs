#include "app/workspace_refresh.h"

#include <cstdlib>
#include <iostream>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " << #expression << '\n'; \
    return EXIT_FAILURE; } } while (false)

int main()
{
    using quickky::WorkspaceRefreshSchedule;
    {
        WorkspaceRefreshSchedule schedule;
        CHECK(!schedule.Consume());
        CHECK(schedule.AfterRefresh(0, false) == 0);
        CHECK(schedule.Request(0) == 25);
        // Duplicate events extend readiness, never rearm/postpone the first pass.
        CHECK(schedule.Request(10) == 0);
        CHECK(schedule.Consume());
        CHECK(!schedule.Consume());
        CHECK(schedule.AfterRefresh(25, false) == 100);
        CHECK(schedule.Consume());
        CHECK(schedule.AfterRefresh(1009, false) == 100);
        CHECK(schedule.Consume());
        CHECK(schedule.AfterRefresh(1010, false) == 0);
        CHECK(!schedule.Consume());
    }
    {
        WorkspaceRefreshSchedule schedule;
        CHECK(schedule.Request(0) == 25);
        CHECK(schedule.Consume());
        // Discovery can pump while a pass is inside Shell COM. Keep its timer.
        CHECK(schedule.Request(30) == 25);
        CHECK(schedule.AfterRefresh(40, true) == 0);
        CHECK(schedule.AfterRefresh(50, false) == 0);
        CHECK(schedule.Consume());
        CHECK(schedule.AfterRefresh(60, false) == 100);
        CHECK(schedule.Consume());
        CHECK(schedule.AfterRefresh(1030, false) == 0);
    }
    {
        WorkspaceRefreshSchedule schedule;
        // Successful native transfers continue after readiness expires, but
        // failed/unconfirmed moves never start a new continuation chain.
        for (std::uint64_t now = 2000; now < 5000; now += 100) {
            CHECK(schedule.AfterRefresh(now, true) == 100);
            CHECK(schedule.Consume());
        }
        CHECK(schedule.AfterRefresh(5000, false) == 0);
        CHECK(!schedule.Consume());
    }
    {
        WorkspaceRefreshSchedule schedule;
        CHECK(schedule.Request(0) == 25);
        schedule.Reset();
        CHECK(!schedule.Consume());
        CHECK(schedule.AfterRefresh(100, false) == 0);
        // Re-enable starts with fresh state, not a stale readiness deadline.
        CHECK(schedule.Request(2000) == 25);
        CHECK(schedule.Consume());
        CHECK(schedule.AfterRefresh(3000, false) == 0);
    }
    {
        WorkspaceRefreshSchedule schedule;
        const std::uint64_t now = (std::uint64_t{1} << 32) + 123;
        CHECK(schedule.Request(now) == 25);
        CHECK(schedule.Consume());
        CHECK(schedule.AfterRefresh(now + 999, false) == 100);
        CHECK(schedule.Consume());
        CHECK(schedule.AfterRefresh(now + 1000, false) == 0);
    }
    std::cout << "Workspace refresh scheduling checks passed.\n";
    return EXIT_SUCCESS;
}
