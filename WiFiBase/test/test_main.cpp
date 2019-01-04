#include <Arduino.h>
#include <unity.h>

#include "../WiFiBase.h"

void test_create_wifibase(void) {
  WiFiBase *wfb = new WiFiBase();
  TEST_ASSERT_NOT_NULL(wfb);
  free(wfb);
}

void test_add_networks() {
  WiFiBase *wfb = new WiFiBase(false);
  TEST_ASSERT_NOT_NULL(wfb);
  TEST_ASSERT_EQUAL(wfb->numKnownNetworks(), 0);

  TEST_ASSERT_TRUE(wfb->addKnownNetwork("test_ssid", "test_passwd"));
  TEST_ASSERT_EQUAL(wfb->numKnownNetworks(), 1);

  TEST_ASSERT_TRUE(wfb->hasKnownNetwork("test_ssid"));
  TEST_ASSERT_FALSE(wfb->hasKnownNetwork("unknown"));

  const int NUM_NETWORKS = wfb->MAX_KNOWN_NETWORKS;
  char ssid[32];
  for (int i = 1; i < NUM_NETWORKS; i++) {
    snprintf(ssid, sizeof(ssid), "test_net_%d", i);
    TEST_ASSERT_TRUE(wfb->addKnownNetwork(ssid, ssid));
    TEST_ASSERT_TRUE(wfb->hasKnownNetwork(ssid));
  }
  TEST_ASSERT_EQUAL(wfb->numKnownNetworks(), NUM_NETWORKS);

  /* Lookup the networks again to assure that the realloations worked */
  for (int i = 1; i < NUM_NETWORKS; i++) {
    snprintf(ssid, sizeof(ssid), "test_net_%d", i);
    TEST_ASSERT_TRUE(wfb->hasKnownNetwork(ssid));
  }

  free(wfb);
}

void setup() {
  UNITY_BEGIN();

  RUN_TEST(test_create_wifibase);
  RUN_TEST(test_add_networks);
}

void loop() {
  UNITY_END(); // stop unit testing
}