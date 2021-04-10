#pragma once
#include "LogicalClock.hpp"
namespace Kuai
{
GlobalClock GlobalClock::clock;
thread_local ThreadClock ThreadClock::tls_clock;
} // namespace Kuai