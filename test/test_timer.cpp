#include <unity.h>
#include "TimerStatic.h"

// Test variables
volatile bool callbackCalled = false;
volatile int callbackCount = 0;
#if TIMER_STATIC_ENABLE_OBSERVER
const Timer* observedTimer = nullptr;
#endif

// Test callback functions
void simpleCallback() {
    callbackCalled = true;
    callbackCount++;
}

void paramCallback(void* obj) {
    callbackCalled = true;
    callbackCount++;
}

// Mock time function for testing
unsigned long mockTime = 0;
unsigned long mockMillis() {
    return mockTime;
}

#if TIMER_STATIC_ENABLE_OBSERVER
void observeTimer(const Timer* timer) {
    observedTimer = timer;
}
#endif

void setUp(void) {
    callbackCalled = false;
    callbackCount = 0;
    mockTime = 0;
#if TIMER_STATIC_ENABLE_OBSERVER
    observedTimer = nullptr;
    Timer::setTickObserver(nullptr);
#endif
}

void tearDown(void) {
#if TIMER_STATIC_ENABLE_OBSERVER
    Timer::setTickObserver(nullptr);
#endif
    // Clean up any timers
    Timer::tick();
}

void test_timer_creation() {
    Timer timer(1000, mockMillis, simpleCallback);
    TEST_ASSERT_TRUE(timer.isRun());
    TEST_ASSERT_EQUAL(1000, timer.getPeriod());
}

void test_timer_callback() {
    Timer timer(1000, mockMillis, simpleCallback);
    
    // Advance time to trigger callback
    mockTime = 1000;
    timer.check();
    
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_EQUAL(1, callbackCount);
}

void test_timer_period() {
    Timer timer(500, mockMillis, simpleCallback);
    
    // First trigger
    mockTime = 500;
    timer.check();
    TEST_ASSERT_TRUE(callbackCalled);
    
    // Reset for second test
    callbackCalled = false;
    
    // Second trigger
    mockTime = 1000;
    timer.check();
    TEST_ASSERT_TRUE(callbackCalled);
}

void test_timer_on_off() {
    Timer timer(1000, mockMillis, simpleCallback);
    
    timer.OFF();
    TEST_ASSERT_FALSE(timer.isRun());
    
    timer.ON();
    TEST_ASSERT_TRUE(timer.isRun());
}

void test_delay_timer() {
    Timer timer;
    
    timer.delay(1000, mockMillis, simpleCallback);
    
    // Should not trigger before delay
    mockTime = 500;
    timer.check();
    TEST_ASSERT_FALSE(callbackCalled);
    
    // Should trigger after delay
    mockTime = 1000;
    timer.check();
    TEST_ASSERT_TRUE(callbackCalled);
}

void test_for_count_timer() {
    Timer timer;
    
    timer.forCount(1000, mockMillis, simpleCallback, 3);
    
    // First trigger
    mockTime = 1000;
    timer.check();
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_EQUAL(1, callbackCount);
    
    // Reset for second test
    callbackCalled = false;
    
    // Second trigger
    mockTime = 2000;
    timer.check();
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_EQUAL(2, callbackCount);
    
    // Reset for third test
    callbackCalled = false;
    
    // Third trigger
    mockTime = 3000;
    timer.check();
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_EQUAL(3, callbackCount);
    
    // Should stop after count limit
    callbackCalled = false;
    mockTime = 4000;
    timer.check();
    TEST_ASSERT_FALSE(callbackCalled);
    TEST_ASSERT_FALSE(timer.isRun());
}

void test_unconfigured_timer_is_stopped() {
    Timer timer;
    TEST_ASSERT_FALSE(timer.isRun());

    mockTime = 1000;
    timer.check();
    TEST_ASSERT_FALSE(callbackCalled);
}

void test_tick_observer_sees_timer_before_callback() {
#if TIMER_STATIC_ENABLE_OBSERVER
    Timer timer(1000, mockMillis, simpleCallback);
    Timer::setTickObserver(observeTimer);

    mockTime = 1000;
    Timer::tick();

    TEST_ASSERT_EQUAL_PTR(&timer, observedTimer);
    TEST_ASSERT_TRUE(callbackCalled);
#endif
}

int main() {
    UNITY_BEGIN();
    
    RUN_TEST(test_timer_creation);
    RUN_TEST(test_timer_callback);
    RUN_TEST(test_timer_period);
    RUN_TEST(test_timer_on_off);
    RUN_TEST(test_delay_timer);
    RUN_TEST(test_for_count_timer);
    RUN_TEST(test_unconfigured_timer_is_stopped);
    RUN_TEST(test_tick_observer_sees_timer_before_callback);
    
    return UNITY_END();
}
