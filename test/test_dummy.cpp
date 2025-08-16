#include <Arduino.h>
#include <unity.h>

void setUp(){}
void tearDown(){}

void test_dummy(){ TEST_ASSERT_TRUE(true); }

void setup(){
  UNITY_BEGIN();
  RUN_TEST(test_dummy);
  UNITY_END();
}

void loop(){}
