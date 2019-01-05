/**
 * Unit testing of WiFiBase class's connection handling
 *
 * To run tests with platformio:
 *   PLATFORMIO_BUILD_FLAGS='-DUSE_SSID=\"network\" -DUSE_PASSWD=\"password\"' platformio test
 */

#include <Arduino.h>
#include <unity.h>
#include <WiFi.h>

#include "../WiFiBase.h"

/* ssid/password for a good network, should be passed in via compiler flags */
#ifndef USE_SSID
  #define USE_SSID "Unknown"
#endif
#ifndef USE_PASSWD
  #define USE_PASSWD "Unknown"
#endif

/* Basic test of allocation and free */
void test_create_wifibase(void) {
  WiFiBase *wfb = new WiFiBase(false);
  TEST_ASSERT_NOT_NULL(wfb);
  delete wfb;
}

/* Test adding networks to wifibase */
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

  delete wfb;
}

/* Attempt to connect to several non-existent networks */
void test_no_connection() {
  WiFiBase *wfb = new WiFiBase(false);
  TEST_ASSERT_NOT_NULL(wfb);
  wfb->setConnectTimeoutMs(500);

  const int NUM_NETWORKS = 4;
  char ssid[32];
  for (int i = 0; i < NUM_NETWORKS; i++) {
    snprintf(ssid, sizeof(ssid), "test_fail_%d", i);
    TEST_ASSERT_TRUE(wfb->addKnownNetwork(ssid, ssid));
  }

  TEST_ASSERT_FALSE(wfb->startup());
  TEST_ASSERT_FALSE(wfb->connected());

  delete wfb;
}

/*
 * Verify that connection works with a ssid/password provided by compiler flag
 * */
void test_should_connect() {
  WiFiBase *wfb = new WiFiBase(false);
  TEST_ASSERT_NOT_NULL(wfb);

  TEST_ASSERT_TRUE(wfb->setConnectTimeoutMs(20*1000));

  TEST_ASSERT_TRUE(wfb->addKnownNetwork(USE_SSID, USE_PASSWD));

  Serial.println(millis());
  TEST_ASSERT_TRUE(wfb->startup());
  Serial.println(millis());
  TEST_ASSERT_TRUE(wfb->connected());
  TEST_ASSERT_EQUAL_STRING(WiFi.SSID().c_str(), USE_SSID);

  delete wfb;
}

void setup() {
  UNITY_BEGIN();

  RUN_TEST(test_create_wifibase);
  RUN_TEST(test_add_networks);
  RUN_TEST(test_no_connection);
  RUN_TEST(test_should_connect);
  UNITY_END();
}

void loop() {
  UNITY_END(); // stop unit testing
}