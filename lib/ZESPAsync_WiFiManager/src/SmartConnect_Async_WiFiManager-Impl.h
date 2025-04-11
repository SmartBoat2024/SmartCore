/****************************************************************************************************************************
  ESPAsync_WiFiManager-Impl.h
  For ESP8266 / ESP32 boards

  ESPAsync_WiFiManager is a library for the ESP8266/Arduino platform, using (ESP)AsyncWebServer to enable easy
  configuration and reconfiguration of WiFi credentials using a Captive Portal.

  Modified from
  1. Tzapu               (https://github.com/tzapu/WiFiManager)
  2. Ken Taylor          (https://github.com/kentaylor)
  3. Alan Steremberg     (https://github.com/alanswx/ESPAsyncWiFiManager)
  4. Khoi Hoang          (https://github.com/khoih-prog/ESP_WiFiManager)

  Built by Khoi Hoang https://github.com/khoih-prog/ESPAsync_WiFiManager
  Licensed under MIT license

  Version: 1.15.1

  Version Modified By  Date      Comments
  ------- -----------  ---------- -----------
  1.0.11  K Hoang      21/08/2020 Initial coding to use (ESP)AsyncWebServer instead of (ESP8266)WebServer. Bump up to v1.0.11
                                  to sync with ESP_WiFiManager v1.0.11
  ...
  1.10.0  K Hoang      29/12/2021 Fix `multiple-definitions` linker error and weird bug related to src_cpp
  1.11.0  K Hoang      17/01/2022 Enable compatibility with old code to include only ESP_WiFiManager.h
  1.12.0  K Hoang      10/02/2022 Add support to new ESP32-S3
  1.12.1  K Hoang      11/02/2022 Add LittleFS support to ESP32-C3. Use core LittleFS instead of Lorol's LITTLEFS for v2.0.0+
  1.12.2  K Hoang      13/03/2022 Optimize code by using passing by `reference` instead of by `value`
  1.13.0  K Hoang      18/08/2022 Using AsynsDNSServer instead of DNSServer
  1.14.0  K Hoang      09/09/2022 Fix ESP32 chipID and add ESP_getChipOUI()
  1.14.1  K Hoang      15/09/2022 Remove dependency on ESP_AsyncWebServer, ESPAsyncTCP and AsyncTCP in `library.properties`
  1.15.0  K Hoang      07/10/2022 Optional display Credentials (SSIDs, PWDs) in Config Portal
  1.15.1  K Hoang      25/10/2022 Using random channel for softAP without password. Add astyle using allman style
 *****************************************************************************************************************************/

#pragma once

#ifndef SmartConnect_Async_WiFiManager_Impl_h
#define SmartConenct_Async_WiFiManager_Impl_h

#include "globals.h"
#include "eeprom_funcs.h"
#include <Ticker.h>
#include "config.h"

Ticker restartTicker;

//////////////////////////////////////////

ESPAsync_WMParameter::ESPAsync_WMParameter(const char *custom)
{
  _WMParam_data._id = NULL;
  _WMParam_data._placeholder = NULL;
  _WMParam_data._length = 0;
  _WMParam_data._value = NULL;
  _WMParam_data._labelPlacement = WFM_LABEL_BEFORE;

  _customHTML = custom;
}

//////////////////////////////////////////

ESPAsync_WMParameter::ESPAsync_WMParameter(const char *id, const char *placeholder, const char *defaultValue,
                                           const int& length, const char *custom, const int& labelPlacement)
{
  init(id, placeholder, defaultValue, length, custom, labelPlacement);
}

//////////////////////////////////////////

// KH, using struct
ESPAsync_WMParameter::ESPAsync_WMParameter(const WMParam_Data& WMParam_data)
{
  init(WMParam_data._id, WMParam_data._placeholder, WMParam_data._value,
       WMParam_data._length, "", WMParam_data._labelPlacement);
}

//////////////////////////////////////////

void ESPAsync_WMParameter::init(const char *id, const char *placeholder, const char *defaultValue,
                                const int& length, const char *custom, const int& labelPlacement)
{
  _WMParam_data._id = id;
  _WMParam_data._placeholder = placeholder;
  _WMParam_data._length = length;
  _WMParam_data._labelPlacement = labelPlacement;

  _WMParam_data._value = new char[_WMParam_data._length + 1];

  if (_WMParam_data._value != NULL)
  {
    memset(_WMParam_data._value, 0, _WMParam_data._length + 1);

    if (defaultValue != NULL)
    {
      strncpy(_WMParam_data._value, defaultValue, _WMParam_data._length);
    }
  }

  _customHTML = custom;
}

//////////////////////////////////////////

ESPAsync_WMParameter::~ESPAsync_WMParameter()
{
  if (_WMParam_data._value != NULL)
  {
    delete[] _WMParam_data._value;
  }
}

//////////////////////////////////////////

// Using Struct to get/set whole data at once
void ESPAsync_WMParameter::setWMParam_Data(const WMParam_Data& WMParam_data)
{
  LOGINFO(F("setWMParam_Data"));

  memcpy(&_WMParam_data, &WMParam_data, sizeof(_WMParam_data));
}

//////////////////////////////////////////

void ESPAsync_WMParameter::getWMParam_Data(WMParam_Data& WMParam_data)
{
  LOGINFO(F("getWMParam_Data"));

  memcpy(&WMParam_data, &_WMParam_data, sizeof(WMParam_data));
}

//////////////////////////////////////////

const char* ESPAsync_WMParameter::getValue()
{
  return _WMParam_data._value;
}

//////////////////////////////////////////

const char* ESPAsync_WMParameter::getID()
{
  return _WMParam_data._id;
}

//////////////////////////////////////////

const char* ESPAsync_WMParameter::getPlaceholder()
{
  return _WMParam_data._placeholder;
}

//////////////////////////////////////////

int ESPAsync_WMParameter::getValueLength()
{
  return _WMParam_data._length;
}

//////////////////////////////////////////

int ESPAsync_WMParameter::getLabelPlacement()
{
  return _WMParam_data._labelPlacement;
}

//////////////////////////////////////////

const char* ESPAsync_WMParameter::getCustomHTML()
{
  return _customHTML;
}

//////////////////////////////////////////

/**
   [getParameters description]
   @access public
*/
ESPAsync_WMParameter** ESPAsync_WiFiManager::getParameters()
{
  return _params;
}

//////////////////////////////////////////
//////////////////////////////////////////

/**
   [getParametersCount description]
   @access public
*/
int ESPAsync_WiFiManager::getParametersCount()
{
  return _paramsCount;
}

//////////////////////////////////////////

char* ESPAsync_WiFiManager::getRFC952_hostname(const char* iHostname)
{
  memset(RFC952_hostname, 0, sizeof(RFC952_hostname));

  size_t len = (RFC952_HOSTNAME_MAXLEN < strlen(iHostname)) ? RFC952_HOSTNAME_MAXLEN : strlen(iHostname);

  size_t j = 0;

  for (size_t i = 0; i < len - 1; i++)
  {
    if (isalnum(iHostname[i]) || iHostname[i] == '-')
    {
      RFC952_hostname[j] = iHostname[i];
      j++;
    }
  }

  // no '-' as last char
  if (isalnum(iHostname[len - 1]) || (iHostname[len - 1] != '-'))
    RFC952_hostname[j] = iHostname[len - 1];

  return RFC952_hostname;
}

//////////////////////////////////////////

ESPAsync_WiFiManager::ESPAsync_WiFiManager(AsyncWebServer * webserver, AsyncDNSServer *dnsserver, const char *iHostname)
{

  server    = webserver;
  dnsServer = dnsserver;

  wifiSSIDs     = NULL;

  // KH
  wifiSSIDscan  = true;
  //wifiSSIDscan  = false;
  //////

  _modeless     = false;
  shouldscan    = true;

#if USE_DYNAMIC_PARAMS
  _max_params = WIFI_MANAGER_MAX_PARAMS;
  _params = (ESPAsync_WMParameter**)malloc(_max_params * sizeof(ESPAsync_WMParameter*));
#endif

  //WiFi not yet started here, must call WiFi.mode(WIFI_STA) and modify function WiFiGenericClass::mode(wifi_mode_t m) !!!
  WiFi.mode(WIFI_STA);

  if (iHostname[0] == 0)
  {
#ifdef ESP8266
    String _hostname = "ESP8266-" + String(ESP.getChipId(), HEX);
#else    //ESP32
    String _hostname = "ESP32-" + String(ESP_getChipId(), HEX);
#endif

    _hostname.toUpperCase();

    getRFC952_hostname(_hostname.c_str());

  }
  else
  {
    // Prepare and store the hostname only not NULL
    getRFC952_hostname(iHostname);
  }

  LOGWARN1(F("RFC925 Hostname ="), RFC952_hostname);

  setHostname();

  networkIndices = NULL;
}

//////////////////////////////////////////

ESPAsync_WiFiManager::~ESPAsync_WiFiManager()
{
#if USE_DYNAMIC_PARAMS

  if (_params != NULL)
  {
    LOGINFO(F("freeing allocated params!"));

    free(_params);
  }

#endif

  if (networkIndices)
  {
    free(networkIndices); //indices array no longer required so free memory
  }
}

//////////////////////////////////////////

#if USE_DYNAMIC_PARAMS
  bool ESPAsync_WiFiManager::addParameter(ESPAsync_WMParameter *p)
#else
  void ESPAsync_WiFiManager::addParameter(ESPAsync_WMParameter *p)
#endif
{
#if USE_DYNAMIC_PARAMS

  if (_paramsCount == _max_params)
  {
    // rezise the params array
    _max_params += WIFI_MANAGER_MAX_PARAMS;

    LOGINFO1(F("Increasing _max_params to:"), _max_params);

    ESPAsync_WMParameter** new_params = (ESPAsync_WMParameter**)realloc(_params,
                                                                        _max_params * sizeof(ESPAsync_WMParameter*));

    if (new_params != NULL)
    {
      _params = new_params;
    }
    else
    {
      LOGINFO(F("ERROR: failed to realloc params, size not increased!"));

      return false;
    }
  }

  _params[_paramsCount] = p;
  _paramsCount++;

  LOGINFO1(F("Adding parameter"), p->getID());

  return true;

#else

  // Danger here. Better to use Tzapu way here
  if (_paramsCount < (WIFI_MANAGER_MAX_PARAMS))
  {
    _params[_paramsCount] = p;
    _paramsCount++;

    LOGINFO1(F("Adding parameter"), p->getID());
  }
  else
  {
    LOGINFO("Can't add parameter. Full");
  }

#endif
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::setupConfigPortal()
{
  stopConfigPortal = false; //Signal not to close config portal

  /*This library assumes autoconnect is set to 1. It usually is
    but just in case check the setting and turn on autoconnect if it is off.
    Some useful discussion at https://github.com/esp8266/Arduino/issues/1615*/
  if (WiFi.getAutoConnect() == 0)
    WiFi.setAutoConnect(1);

#if !( USING_ESP32_S2 || USING_ESP32_C3 )
#ifdef ESP8266
  // KH, mod for Async
  server->reset();
#else   //ESP32
  server->reset();
#endif

  if (!dnsServer)
    dnsServer = new AsyncDNSServer;

#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )

  // optional soft ip config
  // Must be put here before dns server start to take care of the non-default ConfigPortal AP IP.
  // Check (https://github.com/khoih-prog/ESP_WiFiManager/issues/58)
  if (_WiFi_AP_IPconfig._ap_static_ip)
  {
    LOGWARN3(F("Custom AP IP/GW/Subnet = "), _WiFi_AP_IPconfig._ap_static_ip, _WiFi_AP_IPconfig._ap_static_gw,
             _WiFi_AP_IPconfig._ap_static_sn);

    WiFi.softAPConfig(_WiFi_AP_IPconfig._ap_static_ip, _WiFi_AP_IPconfig._ap_static_gw, _WiFi_AP_IPconfig._ap_static_sn);
  }

  /* Setup the DNS server redirecting all the domains to the apIP */
  if (dnsServer)
  {
    dnsServer->setErrorReplyCode(AsyncDNSReplyCode::NoError);

    // AsyncDNSServer started with "*" domain name, all DNS requests will be passsed to WiFi.softAPIP()
    if (! dnsServer->start(DNS_PORT, "*", WiFi.softAPIP()))
    {
      // No socket available
      LOGERROR(F("Can't start DNS Server. No available socket"));
    }
  }

  _configPortalStart = millis();

  LOGWARN1(F("\nConfiguring AP SSID ="), _apName);

  if (_apPassword != NULL)
  {
    if (strlen(_apPassword) < 8 || strlen(_apPassword) > 63)
    {
      // fail passphrase to short or long!
      LOGERROR(F("Invalid AccessPoint password. Ignoring"));

      _apPassword = NULL;
    }

    LOGWARN1(F("AP PWD ="), _apPassword);
  }

  // KH, To enable dynamic/random channel
  static int channel;

  // Use random channel if  _WiFiAPChannel == 0
  if (_WiFiAPChannel == 0)
    channel = (_configPortalStart % MAX_WIFI_CHANNEL) + 1;
  else
    channel = _WiFiAPChannel;

  LOGWARN1(F("AP Channel ="), channel);

  WiFi.softAP(_apName, _apPassword, channel);

  delay(500); // Without delay I've seen the IP address blank

  LOGWARN1(F("AP IP address ="), WiFi.softAPIP());

  /* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */

  server->on("/",         std::bind(&ESPAsync_WiFiManager::handleRoot,        this,
                                    std::placeholders::_1)).setFilter(ON_AP_FILTER);
  server->on("/wifi",     std::bind(&ESPAsync_WiFiManager::handleWifi,        this,
                                    std::placeholders::_1)).setFilter(ON_AP_FILTER);
  server->on("/wifisave", std::bind(&ESPAsync_WiFiManager::handleWifiSave,    this,
                                    std::placeholders::_1)).setFilter(ON_AP_FILTER);
  server->on("/close",    std::bind(&ESPAsync_WiFiManager::handleServerClose, this,
                                    std::placeholders::_1)).setFilter(ON_AP_FILTER);
  server->on("/i",        std::bind(&ESPAsync_WiFiManager::handleInfo,        this,
                                    std::placeholders::_1)).setFilter(ON_AP_FILTER);
  server->on("/r",        std::bind(&ESPAsync_WiFiManager::handleReset,       this,
                                    std::placeholders::_1)).setFilter(ON_AP_FILTER);
  server->on("/state",    std::bind(&ESPAsync_WiFiManager::handleState,       this,
                                    std::placeholders::_1)).setFilter(ON_AP_FILTER);
  server->on("/scan",     std::bind(&ESPAsync_WiFiManager::handleScan,        this,
                                    std::placeholders::_1)).setFilter(ON_AP_FILTER);
  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  server->on("/fwlink",   std::bind(&ESPAsync_WiFiManager::handleRoot,        this,
                                    std::placeholders::_1)).setFilter(ON_AP_FILTER);
  server->onNotFound (std::bind(&ESPAsync_WiFiManager::handleNotFound,        this, std::placeholders::_1));
  server->on("/setStandaloneAP", HTTP_POST, std::bind(&ESPAsync_WiFiManager::handleSetStandaloneAP, this, std::placeholders::_1));
  server->on("/setSerialNumber", HTTP_POST, std::bind(&ESPAsync_WiFiManager::handleSetSerialNumber, this, std::placeholders::_1));
  server->on("/smartconnect", std::bind(&ESPAsync_WiFiManager::handleSmartConnect, this, std::placeholders::_1)).setFilter(ON_AP_FILTER);
  server->on("/standalone",   std::bind(&ESPAsync_WiFiManager::handleStandalone,   this, std::placeholders::_1)).setFilter(ON_AP_FILTER);
  server->on("/provision", HTTP_POST, std::bind(&ESPAsync_WiFiManager::handleAutoSmartConnect, this, std::placeholders::_1)).setFilter(ON_AP_FILTER);



  server->begin(); // Web server start

  LOGWARN(F("HTTP server started"));
}

//////////////////////////////////////////

bool ESPAsync_WiFiManager::autoConnect()
{
#ifdef ESP8266
  String ssid = "ESP_" + String(ESP.getChipId());
#else   //ESP32
  String ssid = "ESP_" + String(ESP_getChipId());
#endif

  return autoConnect(ssid.c_str(), NULL);
}

//////////////////////////////////////////

/* This is not very useful as there has been an assumption that device has to be
  told to connect but Wifi already does it's best to connect in background. Calling this
  method will block until WiFi connects. Sketch can avoid
  blocking call then use (WiFi.status()==WL_CONNECTED) test to see if connected yet.
  See some discussion at https://github.com/tzapu/WiFiManager/issues/68
*/

// To permit autoConnect() to use STA static IP or DHCP IP.
#ifndef AUTOCONNECT_NO_INVALIDATE
  #define AUTOCONNECT_NO_INVALIDATE true
#endif

//////////////////////////////////////////

bool ESPAsync_WiFiManager::autoConnect(char const *apName, char const *apPassword)
{
#if AUTOCONNECT_NO_INVALIDATE
  LOGINFO(F("\nAutoConnect using previously saved SSID/PW, but keep previous settings"));
  // Connect to previously saved SSID/PW, but keep previous settings
  connectWifi();
#else
  LOGINFO(F("\nAutoConnect using previously saved SSID/PW, but invalidate previous settings"));
  // Connect to previously saved SSID/PW, but invalidate previous settings
  connectWifi(WiFi_SSID(), WiFi_Pass());
#endif

  unsigned long startedAt = millis();

  while (millis() - startedAt < 10000)
  {
    //delay(100);
    delay(200);

    if (WiFi.status() == WL_CONNECTED)
    {
      float waited = (millis() - startedAt);

      LOGWARN1(F("Connected after waiting (s) :"), waited / 1000);
      LOGWARN1(F("Local ip ="), WiFi.localIP());

      return true;
    }
  }

  return startConfigPortal(apName, apPassword);
}

//////////////////////////////////////////

String ESPAsync_WiFiManager::networkListAsString()
{
  String pager ;

  //display networks in page
  for (int i = 0; i < wifiSSIDCount; i++)
  {
    if (wifiSSIDs[i].duplicate == true)
      continue; // skip dups

    int quality = getRSSIasQuality(wifiSSIDs[i].RSSI);

    if (_minimumQuality == -1 || _minimumQuality < quality)
    {
      String item = FPSTR(WM_HTTP_ITEM);
      String rssiQ = String(quality);

      item.replace("{v}", wifiSSIDs[i].SSID);
      item.replace("{r}", rssiQ);

#if defined(ESP8266)

      if (wifiSSIDs[i].encryptionType != ENC_TYPE_NONE)
#else
      if (wifiSSIDs[i].encryptionType != WIFI_AUTH_OPEN)
#endif
      {
        item.replace("{i}", "l");
      }
      else
      {
        item.replace("{i}", "");
      }

      pager += item;
    }
    else
    {
      LOGDEBUG(F("Skipping due to quality"));
    }
  }

  return pager;
}

//////////////////////////////////////////

String ESPAsync_WiFiManager::scanModal()
{
  shouldscan = true;
  scan();

  String pager = networkListAsString();

  return pager;
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::scan()
{
  if (!shouldscan)
    return;

  LOGDEBUG(F("About to scan"));

  if (wifiSSIDscan)
  {
    delay(100);
  }

  if (wifiSSIDscan)
  {
    LOGDEBUG(F("Start scan"));

    wifi_ssid_count_t n = WiFi.scanNetworks(false, true);

    LOGDEBUG(F("Scan done"));

    if (n == WIFI_SCAN_FAILED)
    {
      LOGDEBUG(F("WIFI_SCAN_FAILED!"));
    }
    else if (n == WIFI_SCAN_RUNNING)
    {
      LOGDEBUG(F("WIFI_SCAN_RUNNING!"));
    }
    else if (n < 0)
    {
      LOGDEBUG(F("Failed, unknown error code!"));
    }
    else if (n == 0)
    {
      LOGDEBUG(F("No network found"));
      // page += F("No networks found. Refresh to scan again.");
    }
    else
    {
      if (wifiSSIDscan)
      {
        /* WE SHOULD MOVE THIS IN PLACE ATOMICALLY */
        if (wifiSSIDs)
          delete [] wifiSSIDs;

        wifiSSIDs     = new WiFiResult[n];
        wifiSSIDCount = n;

        if (n > 0)
          shouldscan = false;

        for (wifi_ssid_count_t i = 0; i < n; i++)
        {
          wifiSSIDs[i].duplicate = false;

#if defined(ESP8266)
          WiFi.getNetworkInfo(i, wifiSSIDs[i].SSID, wifiSSIDs[i].encryptionType, wifiSSIDs[i].RSSI, wifiSSIDs[i].BSSID,
                              wifiSSIDs[i].channel, wifiSSIDs[i].isHidden);
#else
          WiFi.getNetworkInfo(i, wifiSSIDs[i].SSID, wifiSSIDs[i].encryptionType, wifiSSIDs[i].RSSI, wifiSSIDs[i].BSSID,
                              wifiSSIDs[i].channel);
#endif
        }

        // RSSI SORT
        // old sort
        for (int i = 0; i < n; i++)
        {
          for (int j = i + 1; j < n; j++)
          {
            if (wifiSSIDs[j].RSSI > wifiSSIDs[i].RSSI)
            {
              std::swap(wifiSSIDs[i], wifiSSIDs[j]);
            }
          }
        }

        // remove duplicates ( must be RSSI sorted )
        if (_removeDuplicateAPs)
        {
          String cssid;

          for (int i = 0; i < n; i++)
          {
            if (wifiSSIDs[i].duplicate == true)
              continue;

            cssid = wifiSSIDs[i].SSID;

            for (int j = i + 1; j < n; j++)
            {
              if (cssid == wifiSSIDs[j].SSID)
              {
                LOGDEBUG("DUP AP: " + wifiSSIDs[j].SSID);
                // set dup aps to NULL
                wifiSSIDs[j].duplicate = true;
              }
            }
          }
        }
      }
    }
  }
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::startConfigPortalModeless(char const *apName, char const *apPassword, bool shouldConnectWiFi)
{
  _modeless     = true;
  _apName       = apName;
  _apPassword   = apPassword;

  WiFi.mode(WIFI_AP_STA);

  LOGDEBUG("SET AP STA");

  // try to connect
  if (shouldConnectWiFi && connectWifi("", "") == WL_CONNECTED)
  {
    LOGDEBUG1(F("IP Address:"), WiFi.localIP());

    if ( _savecallback != NULL)
    {
      //todo: check if any custom parameters actually exist, and check if they really changed maybe
      _savecallback();
    }
  }

  if ( _apcallback != NULL)
  {
    _apcallback(this);
  }

  connect = false;
  setupConfigPortal();
  scannow = -1 ;
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::loop()
{
  safeLoop();
  criticalLoop();
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::setInfo()
{
  if (needInfo)
  {
    pager       = infoAsString();
    wifiStatus  = WiFi.status();
    needInfo    = false;
  }
}

//////////////////////////////////////////

// Anything that accesses WiFi, ESP or EEPROM goes here

void ESPAsync_WiFiManager::criticalLoop()
{
  LOGDEBUG(F("criticalLoop: Enter"));

  if (_modeless)
  {
    if (scannow == -1 || ( millis() > scannow + TIME_BETWEEN_MODELESS_SCANS) )
    {
      LOGDEBUG(F("criticalLoop: modeless scan"));

      scan();
      scannow = millis();
    }

    if (connect)
    {
      connect = false;

      LOGDEBUG(F("criticalLoop: Connecting to new AP"));

      // using user-provided  _ssid, _pass in place of system-stored ssid and pass
      if (connectWifi(_ssid, _pass) != WL_CONNECTED)
      {
        LOGDEBUG(F("criticalLoop: Failed to connect."));
      }
      else
      {
        //connected
        // alanswx - should we have a config to decide if we should shut down AP?
        // WiFi.mode(WIFI_STA);
        //notify that configuration has changed and any optional parameters should be saved
        if ( _savecallback != NULL)
        {
          //todo: check if any custom parameters actually exist, and check if they really changed maybe
          _savecallback();
        }

        return;
      }

      if (_shouldBreakAfterConfig)
      {
        //flag set to exit after config after trying to connect
        //notify that configuration has changed and any optional parameters should be saved
        if ( _savecallback != NULL)
        {
          //todo: check if any custom parameters actually exist, and check if they really changed maybe
          _savecallback();
        }
      }
    }
  }
}

//////////////////////////////////////////

// Anything that doesn't access WiFi, ESP or EEPROM can go here

void ESPAsync_WiFiManager::safeLoop()
{
}

///////////////////////////////////////////////////////////

bool ESPAsync_WiFiManager::startConfigPortal()
{
#ifdef ESP8266
  String ssid = "ESP_" + String(ESP.getChipId());
#else   //ESP32
  String ssid = "ESP_" + String(ESP_getChipId());
#endif

  ssid.toUpperCase();

  return startConfigPortal(ssid.c_str(), NULL);
}

//////////////////////////////////////////

bool ESPAsync_WiFiManager::startConfigPortal(char const *apName, char const *apPassword)
{
  WiFi.mode(WIFI_AP_STA);

  _apName = apName;
  _apPassword = apPassword;

  //notify we entered AP mode
  if (_apcallback != NULL)
  {
    LOGINFO("_apcallback");

    _apcallback(this);
  }

  connect = false;

  setupConfigPortal();

  bool TimedOut = true;

  LOGINFO("startConfigPortal : Enter loop");

  scannow = -1 ;

  while (_configPortalTimeout == 0 || ( millis() < _configPortalStart + _configPortalTimeout) )
  {
#if ( USING_ESP32_S2 || USING_ESP32_C3 )
    // Fix ESP32-S2 issue with WebServer (https://github.com/espressif/arduino-esp32/issues/4348)
    delay(1);
#else

    //
    //  we should do a scan every so often here and
    //  try to reconnect to AP while we are at it
    //
    if ( scannow == -1 || ( millis() > scannow + TIME_BETWEEN_MODAL_SCANS) )
    {
      LOGDEBUG(F("startConfigPortal: About to modal scan"));

      // since we are modal, we can scan every time
      shouldscan = true;

#if defined(ESP8266)
      // we might still be connecting, so that has to stop for scanning
      ETS_UART_INTR_DISABLE ();
      wifi_station_disconnect ();
      ETS_UART_INTR_ENABLE ();
#else
      WiFi.disconnect (false);
#endif

      scan();

      //if (_tryConnectDuringConfigPortal)
      //  WiFi.begin(); // try to reconnect to AP

      scannow = millis() ;
    }

#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )

    // yield before processing our flags "connect" and/or "stopConfigPortal"
    yield();

    if (connect)
    {
      TimedOut = false;
      delay(2000);

      LOGERROR(F("Connecting to new AP"));

      // using user-provided  _ssid, _pass in place of system-stored ssid and pass
      if (connectWifi(_ssid, _pass) != WL_CONNECTED)
      {
        LOGERROR(F("Failed to connect"));

        WiFi.mode(WIFI_AP); // Dual mode becomes flaky if not connected to a WiFi network.
      }
      else
      {
        //notify that configuration has changed and any optional parameters should be saved
        if (_savecallback != NULL)
        {
          //todo: check if any custom parameters actually exist, and check if they really changed maybe
          _savecallback();
        }

        break;
      }

      if (_shouldBreakAfterConfig)
      {
        //flag set to exit after config after trying to connect
        //notify that configuration has changed and any optional parameters should be saved
        if (_savecallback != NULL)
        {
          //todo: check if any custom parameters actually exist, and check if they really changed maybe
          _savecallback();
        }

        break;
      }
    }

    if (stopConfigPortal)
    {
      TimedOut = false;

      LOGERROR("Stop ConfigPortal");

      stopConfigPortal = false;
      break;
    }

#if ( defined(TIME_BETWEEN_CONFIG_PORTAL_LOOP) && (TIME_BETWEEN_CONFIG_PORTAL_LOOP > 0) )
#if (_ESPASYNC_WIFIMGR_LOGLEVEL_ > 3)
#warning Using delay in startConfigPortal loop
#endif

    delay(TIME_BETWEEN_CONFIG_PORTAL_LOOP);
#endif
  }

  WiFi.mode(WIFI_STA);

  if (TimedOut)
  {
    setHostname();

    // New v1.0.8 to fix static IP when CP not entered or timed-out
    setWifiStaticIP();

    WiFi.begin();
    int connRes = waitForConnectResult();

    LOGERROR1("Timed out connection result:", getStatus(connRes));
  }

#if !( USING_ESP32_S2 || USING_ESP32_C3 )
  server->reset();
  dnsServer->stop();
#endif

  return  (WiFi.status() == WL_CONNECTED);
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::setWifiStaticIP()
{
#if USE_CONFIGURABLE_DNS

  if (_WiFi_STA_IPconfig._sta_static_ip)
  {
    LOGWARN(F("Custom STA IP/GW/Subnet"));

    //***** Added section for DNS config option *****
    if (_WiFi_STA_IPconfig._sta_static_dns1 && _WiFi_STA_IPconfig._sta_static_dns2)
    {
      LOGWARN(F("DNS1 and DNS2 set"));

      WiFi.config(_WiFi_STA_IPconfig._sta_static_ip, _WiFi_STA_IPconfig._sta_static_gw, _WiFi_STA_IPconfig._sta_static_sn,
                  _WiFi_STA_IPconfig._sta_static_dns1, _WiFi_STA_IPconfig._sta_static_dns2);
    }
    else if (_WiFi_STA_IPconfig._sta_static_dns1)
    {
      LOGWARN(F("Only DNS1 set"));

      WiFi.config(_WiFi_STA_IPconfig._sta_static_ip, _WiFi_STA_IPconfig._sta_static_gw, _WiFi_STA_IPconfig._sta_static_sn,
                  _WiFi_STA_IPconfig._sta_static_dns1);
    }
    else
    {
      LOGWARN(F("No DNS server set"));

      WiFi.config(_WiFi_STA_IPconfig._sta_static_ip, _WiFi_STA_IPconfig._sta_static_gw, _WiFi_STA_IPconfig._sta_static_sn);
    }

    //***** End added section for DNS config option *****

    LOGINFO1(F("setWifiStaticIP IP ="), WiFi.localIP());
  }
  else
  {
    LOGWARN(F("Can't use Custom STA IP/GW/Subnet"));
  }

#else

  // check if we've got static_ip settings, if we do, use those.
  if (_WiFi_STA_IPconfig._sta_static_ip)
  {
    WiFi.config(_WiFi_STA_IPconfig._sta_static_ip, _WiFi_STA_IPconfig._sta_static_gw, _WiFi_STA_IPconfig._sta_static_sn);

    LOGWARN1(F("Custom STA IP/GW/Subnet : "), WiFi.localIP());
  }

#endif
}

//////////////////////////////////////////

int ESPAsync_WiFiManager::reconnectWifi()
{
  int connectResult;

  // using user-provided  _ssid, _pass in place of system-stored ssid and pass
  if ( ( connectResult = connectWifi(_ssid, _pass) ) != WL_CONNECTED)
  {
    LOGERROR1(F("Failed to connect to"), _ssid);

    if ( ( connectResult = connectWifi(_ssid1, _pass1) ) != WL_CONNECTED)
    {
      LOGERROR1(F("Failed to connect to"), _ssid1);
    }
    else
    {
      LOGERROR1(F("Connected to"), _ssid1);
    }
  }
  else
  {
    LOGERROR1(F("Connected to"), _ssid);
  }

  return connectResult;
}

//////////////////////////////////////////

int ESPAsync_WiFiManager::connectWifi(const String& ssid, const String& pass)
{
  // Add option if didn't input/update SSID/PW => Use the previous saved Credentials.
  // But update the Static/DHCP options if changed.
  if ( (ssid != "") || ( (ssid == "") && (WiFi_SSID() != "") ) )
  {
    //fix for auto connect racing issue to avoid resetSettings()
    if (WiFi.status() == WL_CONNECTED)
    {
      LOGWARN(F("Already connected. Bailing out."));

      return WL_CONNECTED;
    }

    if (ssid != "")
      resetSettings();

#ifdef ESP8266
    setWifiStaticIP();
#endif

    WiFi.mode(WIFI_AP_STA); //It will start in station mode if it was previously in AP mode.

    setHostname();

    // KH, Fix ESP32 staticIP after exiting CP
#ifdef ESP32
    setWifiStaticIP();
#endif

    if (ssid != "")
    {
      // Start Wifi with new values.
      LOGWARN(F("Connect to new WiFi using new IP parameters"));

      WiFi.begin(ssid.c_str(), pass.c_str());
    }
    else
    {
      // Start Wifi with old values.
      LOGWARN(F("Connect to previous WiFi using new IP parameters"));

      WiFi.begin();
    }
  }
  else if (WiFi_SSID() == "")
  {
    LOGWARN(F("No saved credentials"));
  }

  int connRes = waitForConnectResult();

  LOGWARN1("Connection result: ", getStatus(connRes));

  //not connected, WPS enabled, no pass - first attempt
  if (_tryWPS && connRes != WL_CONNECTED && pass == "")
  {
    startWPS();
    //should be connected at the end of WPS
    connRes = waitForConnectResult();
  }

  return connRes;
}

//////////////////////////////////////////

wl_status_t ESPAsync_WiFiManager::waitForConnectResult()
{
  if (_connectTimeout == 0)
  {
    unsigned long startedAt = millis();

    // In ESP8266, WiFi.waitForConnectResult() @return wl_status_t (0-255) or -1 on timeout !!!
    // In ESP32, WiFi.waitForConnectResult() @return wl_status_t (0-255)
    // So, using int for connRes to be safe
    //int connRes = WiFi.waitForConnectResult();
    WiFi.waitForConnectResult();

    float waited = (millis() - startedAt);

    LOGWARN1(F("Connected after waiting (s) :"), waited / 1000);
    LOGWARN1(F("Local ip ="), WiFi.localIP());

    // Fix bug, connRes is sometimes not correct.
    //return connRes;
    return WiFi.status();
  }
  else
  {
    LOGERROR(F("Waiting WiFi connection with time out"));

    unsigned long start = millis();
    bool keepConnecting = true;
    wl_status_t status;

    while (keepConnecting)
    {
      status = WiFi.status();

      if (millis() > start + _connectTimeout)
      {
        keepConnecting = false;

        LOGERROR(F("Connection timed out"));
      }

#if ( ESP8266 && (USING_ESP8266_CORE_VERSION >= 30000) )

      if (status == WL_CONNECTED || status == WL_CONNECT_FAILED || status == WL_WRONG_PASSWORD)
#else
      if (status == WL_CONNECTED || status == WL_CONNECT_FAILED)
#endif
      {
        keepConnecting = false;
      }

      delay(100);
    }

    return status;
  }
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::startWPS()
{
#ifdef ESP8266
  LOGINFO("START WPS");
  WiFi.beginWPSConfig();
  LOGINFO("END WPS");
#else   //ESP32
  // TODO
  LOGINFO("ESP32 WPS TODO");
#endif
}

//////////////////////////////////////////

//Convenient for debugging but wasteful of program space.
//Remove if short of space
const char* ESPAsync_WiFiManager::getStatus(const int& status)
{
  switch (status)
  {
    case WL_IDLE_STATUS:
      return "WL_IDLE_STATUS";

    case WL_NO_SSID_AVAIL:
      return "WL_NO_SSID_AVAIL";

    case WL_CONNECTED:
      return "WL_CONNECTED";

    case WL_CONNECT_FAILED:
      return "WL_CONNECT_FAILED";

#if ( ESP8266 && (USING_ESP8266_CORE_VERSION >= 30000) )

    case WL_WRONG_PASSWORD:
      return "WL_WRONG_PASSWORD";
#endif

    case WL_DISCONNECTED:
      return "WL_DISCONNECTED";

    default:
      return "UNKNOWN";
  }
}

//////////////////////////////////////////

String ESPAsync_WiFiManager::getConfigPortalSSID()
{
  return _apName;
}

//////////////////////////////////////////

String ESPAsync_WiFiManager::getConfigPortalPW()
{
  return _apPassword;
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::resetSettings()
{
  LOGINFO(F("Previous settings invalidated"));

#ifdef ESP8266
  WiFi.disconnect(true);
#else
  WiFi.disconnect(true, true);

  // Temporary fix for issue of not clearing WiFi SSID/PW from flash of ESP32
  // See https://github.com/khoih-prog/ESPAsync_WiFiManager/issues/25 and https://github.com/espressif/arduino-esp32/issues/400
  WiFi.begin("0", "0");
#endif

  delay(200);

  return;
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::setTimeout(const unsigned long& seconds)
{
  setConfigPortalTimeout(seconds);
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::setConfigPortalTimeout(const unsigned long& seconds)
{
  _configPortalTimeout = seconds * 1000;
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::setConnectTimeout(const unsigned long& seconds)
{
  _connectTimeout = seconds * 1000;
}

void ESPAsync_WiFiManager::setDebugOutput(bool debug)
{
  _debug = debug;
}

//////////////////////////////////////////

// KH, To enable dynamic/random channel
int ESPAsync_WiFiManager::setConfigPortalChannel(const int& channel)
{
  // If channel < MIN_WIFI_CHANNEL - 1 or channel > MAX_WIFI_CHANNEL => channel = 1
  // If channel == 0 => will use random channel from MIN_WIFI_CHANNEL to MAX_WIFI_CHANNEL
  // If (MIN_WIFI_CHANNEL <= channel <= MAX_WIFI_CHANNEL) => use it
  if ( (channel < MIN_WIFI_CHANNEL - 1) || (channel > MAX_WIFI_CHANNEL) )
    _WiFiAPChannel = 1;
  else if ( (channel >= MIN_WIFI_CHANNEL - 1) && (channel <= MAX_WIFI_CHANNEL) )
    _WiFiAPChannel = channel;

  return _WiFiAPChannel;
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::setAPStaticIPConfig(const IPAddress& ip, const IPAddress& gw, const IPAddress& sn)
{
  LOGINFO(F("setAPStaticIPConfig"));

  _WiFi_AP_IPconfig._ap_static_ip = ip;
  _WiFi_AP_IPconfig._ap_static_gw = gw;
  _WiFi_AP_IPconfig._ap_static_sn = sn;
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::setAPStaticIPConfig(const WiFi_AP_IPConfig& WM_AP_IPconfig)
{
  LOGINFO(F("setAPStaticIPConfig"));

  memcpy((void *) &_WiFi_AP_IPconfig, &WM_AP_IPconfig, sizeof(_WiFi_AP_IPconfig));
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::getAPStaticIPConfig(WiFi_AP_IPConfig  &WM_AP_IPconfig)
{
  LOGINFO(F("getAPStaticIPConfig"));

  memcpy((void *) &WM_AP_IPconfig, &_WiFi_AP_IPconfig, sizeof(WM_AP_IPconfig));
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::setSTAStaticIPConfig(const IPAddress& ip, const IPAddress& gw, const IPAddress& sn)
{
  LOGINFO(F("setSTAStaticIPConfig"));

  _WiFi_STA_IPconfig._sta_static_ip = ip;
  _WiFi_STA_IPconfig._sta_static_gw = gw;
  _WiFi_STA_IPconfig._sta_static_sn = sn;
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::setSTAStaticIPConfig(const WiFi_STA_IPConfig& WM_STA_IPconfig)
{
  LOGINFO(F("setSTAStaticIPConfig"));

  memcpy((void *) &_WiFi_STA_IPconfig, &WM_STA_IPconfig, sizeof(_WiFi_STA_IPconfig));
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::getSTAStaticIPConfig(WiFi_STA_IPConfig& WM_STA_IPconfig)
{
  LOGINFO(F("getSTAStaticIPConfig"));

  memcpy((void *) &WM_STA_IPconfig, &_WiFi_STA_IPconfig, sizeof(WM_STA_IPconfig));
}


//////////////////////////////////////////

#if USE_CONFIGURABLE_DNS
void ESPAsync_WiFiManager::setSTAStaticIPConfig(const IPAddress& ip, const IPAddress& gw, const IPAddress& sn,
                                                const IPAddress& dns_address_1, const IPAddress& dns_address_2)
{
  LOGINFO(F("setSTAStaticIPConfig for USE_CONFIGURABLE_DNS"));

  _WiFi_STA_IPconfig._sta_static_ip = ip;
  _WiFi_STA_IPconfig._sta_static_gw = gw;
  _WiFi_STA_IPconfig._sta_static_sn = sn;
  _WiFi_STA_IPconfig._sta_static_dns1 = dns_address_1; //***** Added argument *****
  _WiFi_STA_IPconfig._sta_static_dns2 = dns_address_2; //***** Added argument *****
}
#endif

//////////////////////////////////////////

void ESPAsync_WiFiManager::setMinimumSignalQuality(const int& quality)
{
  _minimumQuality = quality;
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::setBreakAfterConfig(bool shouldBreak)
{
  _shouldBreakAfterConfig = shouldBreak;
}

//////////////////////////////////////////

void ESPAsync_WiFiManager::reportStatus(String& page)
{
  page += FPSTR(WM_HTTP_SCRIPT_NTP_MSG);

  /*if (WiFi_SSID() != "")
  {
    page += F("Configured to connect to AP <b>");
    page += WiFi_SSID();

    if (WiFi.status() == WL_CONNECTED)
    {
      page += F(" and connected</b> on IP <a href=\"http://");
      page += WiFi.localIP().toString();
      page += F("/\">");
      page += WiFi.localIP().toString();
      page += F("</a>");
    }
    else
    {
      page += F(" but not connected.</b>");
    }
  }
  else
  {
    page += F("No network configured.");
  }*/
}

//////////////////////////////////////////

// Handle root or redirect to captive portal
void ESPAsync_WiFiManager::handleRoot(AsyncWebServerRequest *request)
{
  LOGDEBUG(F("Handle root"));

  // Disable _configPortalTimeout when someone accessing Portal to give some time to config
  _configPortalTimeout = 0;

  if (captivePortal(request))
  {
    // If captive portal redirect instead of displaying the error page.
    return;
  }

  String page = FPSTR(WM_HTTP_HEAD_START);
  page.replace("{v}", "Options");

  page += FPSTR(WM_HTTP_SCRIPT);
  page += FPSTR(WM_HTTP_SCRIPT_NTP);
  page += FPSTR(WM_HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(WM_HTTP_HEAD_END);
  // 🔽 Add this logo block right after WM_HTTP_HEAD_END
  page += F("<div style='text-align:center; margin-top:10px; margin-bottom:10px;'>"
    "<img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAMgAAAA+CAYAAABuk1SaAAAACXBIWXMAAJnKAACZygHjkaQiAAALNmlUWHRYTUw6Y29tLmFkb2JlLnhtcAAAAAAAPD94cGFja2V0IGJlZ2luPSLvu78iIGlkPSJXNU0wTXBDZWhpSHpyZVN6TlRjemtjOWQiPz4gPHg6eG1wbWV0YSB4bWxuczp4PSJhZG9iZTpuczptZXRhLyIgeDp4bXB0az0iQWRvYmUgWE1QIENvcmUgOS4xLWMwMDIgNzkuNzhiNzYzOCwgMjAyNS8wMi8xMS0xOToxMDowOCAgICAgICAgIj4gPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4gPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIgeG1sbnM6eG1wPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvIiB4bWxuczpkYz0iaHR0cDovL3B1cmwub3JnL2RjL2VsZW1lbnRzLzEuMS8iIHhtbG5zOnBob3Rvc2hvcD0iaHR0cDovL25zLmFkb2JlLmNvbS9waG90b3Nob3AvMS4wLyIgeG1sbnM6eG1wTU09Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9tbS8iIHhtbG5zOnN0RXZ0PSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvc1R5cGUvUmVzb3VyY2VFdmVudCMiIHhtbG5zOnN0UmVmPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvc1R5cGUvUmVzb3VyY2VSZWYjIiB4bWxuczp0aWZmPSJodHRwOi8vbnMuYWRvYmUuY29tL3RpZmYvMS4wLyIgeG1sbnM6ZXhpZj0iaHR0cDovL25zLmFkb2JlLmNvbS9leGlmLzEuMC8iIHhtcDpDcmVhdG9yVG9vbD0iQWRvYmUgUGhvdG9zaG9wIDI0LjEgKFdpbmRvd3MpIiB4bXA6Q3JlYXRlRGF0ZT0iMjAyMy0wOS0yNVQxODo0NzoyMiswMTowMCIgeG1wOk1vZGlmeURhdGU9IjIwMjUtMDQtMDJUMTc6MTY6MTIrMDE6MDAiIHhtcDpNZXRhZGF0YURhdGU9IjIwMjUtMDQtMDJUMTc6MTY6MTIrMDE6MDAiIGRjOmZvcm1hdD0iaW1hZ2UvcG5nIiBwaG90b3Nob3A6Q29sb3JNb2RlPSIzIiB4bXBNTTpJbnN0YW5jZUlEPSJ4bXAuaWlkOmZiYTYxMzA1LThmZmYtMjM0Ni1hYmRlLWVhYjg4N2I2ZjYwYSIgeG1wTU06RG9jdW1lbnRJRD0iYWRvYmU6ZG9jaWQ6cGhvdG9zaG9wOjY1YWEyZmZlLTU2MDYtMzQ0MS1iZDRkLTliMTZlZTYyNGRjNSIgeG1wTU06T3JpZ2luYWxEb2N1bWVudElEPSJ4bXAuZGlkOmQ0NTlmYWQyLTdkMzktOWY0MS05NDMwLTE3YjRmM2QyYzQyNiIgdGlmZjpPcmllbnRhdGlvbj0iMSIgdGlmZjpYUmVzb2x1dGlvbj0iMTAwMDAwMDAvMTAwMDAiIHRpZmY6WVJlc29sdXRpb249IjEwMDAwMDAwLzEwMDAwIiB0aWZmOlJlc29sdXRpb25Vbml0PSIyIiBleGlmOkNvbG9yU3BhY2U9IjY1NTM1IiBleGlmOlBpeGVsWERpbWVuc2lvbj0iNjAwIiBleGlmOlBpeGVsWURpbWVuc2lvbj0iMTg2Ij4gPHhtcE1NOkhpc3Rvcnk+IDxyZGY6U2VxPiA8cmRmOmxpIHN0RXZ0OmFjdGlvbj0iY3JlYXRlZCIgc3RFdnQ6aW5zdGFuY2VJRD0ieG1wLmlpZDpkNDU5ZmFkMi03ZDM5LTlmNDEtOTQzMC0xN2I0ZjNkMmM0MjYiIHN0RXZ0OndoZW49IjIwMjMtMDktMjVUMTg6NDc6MjIrMDE6MDAiIHN0RXZ0OnNvZnR3YXJlQWdlbnQ9IkFkb2JlIFBob3Rvc2hvcCAyNC4xIChXaW5kb3dzKSIvPiA8cmRmOmxpIHN0RXZ0OmFjdGlvbj0ic2F2ZWQiIHN0RXZ0Omluc3RhbmNlSUQ9InhtcC5paWQ6MmM5ZTVkYzgtM2NlZi0xNzQzLWFiMWMtNTNmYTczNjZjYjMwIiBzdEV2dDp3aGVuPSIyMDI0LTAxLTMxVDEwOjM1OjAyWiIgc3RFdnQ6c29mdHdhcmVBZ2VudD0iQWRvYmUgUGhvdG9zaG9wIDI1LjMgKFdpbmRvd3MpIiBzdEV2dDpjaGFuZ2VkPSIvIi8+IDxyZGY6bGkgc3RFdnQ6YWN0aW9uPSJzYXZlZCIgc3RFdnQ6aW5zdGFuY2VJRD0ieG1wLmlpZDphMDZmMzA0OS01ZDkwLTA3NDctOGQxZC1hZDBjZjgxMTRmMjciIHN0RXZ0OndoZW49IjIwMjQtMTAtMjhUMjA6MTU6NDFaIiBzdEV2dDpzb2Z0d2FyZUFnZW50PSJBZG9iZSBQaG90b3Nob3AgMjYuMCAoV2luZG93cykiIHN0RXZ0OmNoYW5nZWQ9Ii8iLz4gPHJkZjpsaSBzdEV2dDphY3Rpb249ImNvbnZlcnRlZCIgc3RFdnQ6cGFyYW1ldGVycz0iZnJvbSBhcHBsaWNhdGlvbi92bmQuYWRvYmUucGhvdG9zaG9wIHRvIGltYWdlL3BuZyIvPiA8cmRmOmxpIHN0RXZ0OmFjdGlvbj0iZGVyaXZlZCIgc3RFdnQ6cGFyYW1ldGVycz0iY29udmVydGVkIGZyb20gYXBwbGljYXRpb24vdm5kLmFkb2JlLnBob3Rvc2hvcCB0byBpbWFnZS9wbmciLz4gPHJkZjpsaSBzdEV2dDphY3Rpb249InNhdmVkIiBzdEV2dDppbnN0YW5jZUlEPSJ4bXAuaWlkOmE2ODFjZWQzLTlmODUtNDY0Yy1hNmE4LTA1OTk0YzFmZTU5OCIgc3RFdnQ6d2hlbj0iMjAyNC0xMC0yOFQyMDoxNTo0MVoiIHN0RXZ0OnNvZnR3YXJlQWdlbnQ9IkFkb2JlIFBob3Rvc2hvcCAyNi4wIChXaW5kb3dzKSIgc3RFdnQ6Y2hhbmdlZD0iLyIvPiA8cmRmOmxpIHN0RXZ0OmFjdGlvbj0ic2F2ZWQiIHN0RXZ0Omluc3RhbmNlSUQ9InhtcC5paWQ6ZmJhNjEzMDUtOGZmZi0yMzQ2LWFiZGUtZWFiODg3YjZmNjBhIiBzdEV2dDp3aGVuPSIyMDI1LTA0LTAyVDE3OjE2OjEyKzAxOjAwIiBzdEV2dDpzb2Z0d2FyZUFnZW50PSJBZG9iZSBQaG90b3Nob3AgMjYuNSAoV2luZG93cykiIHN0RXZ0OmNoYW5nZWQ9Ii8iLz4gPC9yZGY6U2VxPiA8L3htcE1NOkhpc3Rvcnk+IDx4bXBNTTpEZXJpdmVkRnJvbSBzdFJlZjppbnN0YW5jZUlEPSJ4bXAuaWlkOmEwNmYzMDQ5LTVkOTAtMDc0Ny04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' alt='SmartBoat Logo' style='max-width:200px; height:auto;' />"
    "</div>");

  page += F("<h2 style='color: white;'>");
  page += _apName;
  page += F("</h2>");

  page += "</h2>";
  page += FPSTR(WM_FLDSET_START);
  page += FPSTR(WM_HTTP_PORTAL_OPTIONS);
  page += FPSTR(CUSTOM_PORTAL_BUTTONS);
  page += FPSTR(WM_HTTP_PORTAL_EXIT);
  page += F("<div class=\"msg\">");

  reportStatus(page);

  page += F("</div>");
  page += FPSTR(WM_FLDSET_END);
  page += FPSTR(WM_HTTP_END);

#if ( USING_ESP32_S2 || USING_ESP32_C3 )
  request->send(200, WM_HTTP_HEAD_CT, page);

  // Fix ESP32-S2 issue with WebServer (https://github.com/espressif/arduino-esp32/issues/4348)
  delay(1);
#else

  AsyncWebServerResponse *response = request->beginResponse(200, WM_HTTP_HEAD_CT, page);
  response->addHeader(FPSTR(WM_HTTP_CACHE_CONTROL), FPSTR(WM_HTTP_NO_STORE));

#if USING_CORS_FEATURE
  // For configuring CORS Header, default to WM_HTTP_CORS_ALLOW_ALL = "*"
  response->addHeader(FPSTR(WM_HTTP_CORS), _CORS_Header);
#endif

  response->addHeader(FPSTR(WM_HTTP_PRAGMA), FPSTR(WM_HTTP_NO_CACHE));
  response->addHeader(FPSTR(WM_HTTP_EXPIRES), "-1");

  request->send(response);

#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )
}

//////////////////////////////////////////

// Wifi config page handler
void ESPAsync_WiFiManager::handleWifi(AsyncWebServerRequest *request)
{
  LOGDEBUG(F("Handle WiFi"));

  // Disable _configPortalTimeout when someone accessing Portal to give some time to config
  _configPortalTimeout = 0;

  String page = FPSTR(WM_HTTP_HEAD_START);
  page.replace("{v}", "Config ESP");

  page += FPSTR(WM_HTTP_SCRIPT);
  page += FPSTR(WM_HTTP_SCRIPT_NTP);
  page += FPSTR(WM_HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(WM_HTTP_HEAD_END);
  // 🔽 Add this logo block right after WM_HTTP_HEAD_END
  page += F("<div style='text-align:center; margin-top:10px; margin-bottom:0px;'>"
    "<img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAMgAAAA+CAYAAABuk1SaAAAACXBIWXMAAJnKAACZygHjkaQiAAALNmlUWHRYTUw6Y29tLmFkb2JlLnhtcAAAAAAAPD94cGFja2V0IGJlZ2luPSLvu78iIGlkPSJXNU0wTXBDZWhpSHpyZVN6TlRjemtjOWQiPz4gPHg6eG1wbWV0YSB4bWxuczp4PSJhZG9iZTpuczptZXRhLyIgeDp4bXB0az0iQWRvYmUgWE1QIENvcmUgOS4xLWMwMDIgNzkuNzhiNzYzOCwgMjAyNS8wMi8xMS0xOToxMDowOCAgICAgICAgIj4gPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4gPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIgeG1sbnM6eG1wPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvIiB4bWxuczpkYz0iaHR0cDovL3B1cmwub3JnL2RjL2VsZW1lbnRzLzEuMS8iIHhtbG5zOnBob3Rvc2hvcD0iaHR0cDovL25zLmFkb2JlLmNvbS9waG90b3Nob3AvMS4wLyIgeG1sbnM6eG1wTU09Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9tbS8iIHhtbG5zOnN0RXZ0PSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvc1R5cGUvUmVzb3VyY2VFdmVudCMiIHhtbG5zOnN0UmVmPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvc1R5cGUvUmVzb3VyY2VSZWYjIiB4bWxuczp0aWZmPSJodHRwOi8vbnMuYWRvYmUuY29tL3RpZmYvMS4wLyIgeG1sbnM6ZXhpZj0iaHR0cDovL25zLmFkb2JlLmNvbS9leGlmLzEuMC8iIHhtcDpDcmVhdG9yVG9vbD0iQWRvYmUgUGhvdG9zaG9wIDI0LjEgKFdpbmRvd3MpIiB4bXA6Q3JlYXRlRGF0ZT0iMjAyMy0wOS0yNVQxODo0NzoyMiswMTowMCIgeG1wOk1vZGlmeURhdGU9IjIwMjUtMDQtMDJUMTc6MTY6MTIrMDE6MDAiIHhtcDpNZXRhZGF0YURhdGU9IjIwMjUtMDQtMDJUMTc6MTY6MTIrMDE6MDAiIGRjOmZvcm1hdD0iaW1hZ2UvcG5nIiBwaG90b3Nob3A6Q29sb3JNb2RlPSIzIiB4bXBNTTpJbnN0YW5jZUlEPSJ4bXAuaWlkOmZiYTYxMzA1LThmZmYtMjM0Ni1hYmRlLWVhYjg4N2I2ZjYwYSIgeG1wTU06RG9jdW1lbnRJRD0iYWRvYmU6ZG9jaWQ6cGhvdG9zaG9wOjY1YWEyZmZlLTU2MDYtMzQ0MS1iZDRkLTliMTZlZTYyNGRjNSIgeG1wTU06T3JpZ2luYWxEb2N1bWVudElEPSJ4bXAuZGlkOmQ0NTlmYWQyLTdkMzktOWY0MS05NDMwLTE3YjRmM2QyYzQyNiIgdGlmZjpPcmllbnRhdGlvbj0iMSIgdGlmZjpYUmVzb2x1dGlvbj0iMTAwMDAwMDAvMTAwMDAiIHRpZmY6WVJlc29sdXRpb249IjEwMDAwMDAwLzEwMDAwIiB0aWZmOlJlc29sdXRpb25Vbml0PSIyIiBleGlmOkNvbG9yU3BhY2U9IjY1NTM1IiBleGlmOlBpeGVsWERpbWVuc2lvbj0iNjAwIiBleGlmOlBpeGVsWURpbWVuc2lvbj0iMTg2Ij4gPHhtcE1NOkhpc3Rvcnk+IDxyZGY6U2VxPiA8cmRmOmxpIHN0RXZ0OmFjdGlvbj0iY3JlYXRlZCIgc3RFdnQ6aW5zdGFuY2VJRD0ieG1wLmlpZDpkNDU5ZmFkMi03ZDM5LTlmNDEtOTQzMC0xN2I0ZjNkMmM0MjYiIHN0RXZ0OndoZW49IjIwMjMtMDktMjVUMTg6NDc6MjIrMDE6MDAiIHN0RXZ0OnNvZnR3YXJlQWdlbnQ9IkFkb2JlIFBob3Rvc2hvcCAyNC4xIChXaW5kb3dzKSIvPiA8cmRmOmxpIHN0RXZ0OmFjdGlvbj0ic2F2ZWQiIHN0RXZ0Omluc3RhbmNlSUQ9InhtcC5paWQ6MmM5ZTVkYzgtM2NlZi0xNzQzLWFiMWMtNTNmYTczNjZjYjMwIiBzdEV2dDp3aGVuPSIyMDI0LTAxLTMxVDEwOjM1OjAyWiIgc3RFdnQ6c29mdHdhcmVBZ2VudD0iQWRvYmUgUGhvdG9zaG9wIDI1LjMgKFdpbmRvd3MpIiBzdEV2dDpjaGFuZ2VkPSIvIi8+IDxyZGY6bGkgc3RFdnQ6YWN0aW9uPSJzYXZlZCIgc3RFdnQ6aW5zdGFuY2VJRD0ieG1wLmlpZDphMDZmMzA0OS01ZDkwLTA3NDctOGQxZC1hZDBjZjgxMTRmMjciIHN0RXZ0OndoZW49IjIwMjQtMTAtMjhUMjA6MTU6NDFaIiBzdEV2dDpzb2Z0d2FyZUFnZW50PSJBZG9iZSBQaG90b3Nob3AgMjYuMCAoV2luZG93cykiIHN0RXZ0OmNoYW5nZWQ9Ii8iLz4gPHJkZjpsaSBzdEV2dDphY3Rpb249ImNvbnZlcnRlZCIgc3RFdnQ6cGFyYW1ldGVycz0iZnJvbSBhcHBsaWNhdGlvbi92bmQuYWRvYmUucGhvdG9zaG9wIHRvIGltYWdlL3BuZyIvPiA8cmRmOmxpIHN0RXZ0OmFjdGlvbj0iZGVyaXZlZCIgc3RFdnQ6cGFyYW1ldGVycz0iY29udmVydGVkIGZyb20gYXBwbGljYXRpb24vdm5kLmFkb2JlLnBob3Rvc2hvcCB0byBpbWFnZS9wbmciLz4gPHJkZjpsaSBzdEV2dDphY3Rpb249InNhdmVkIiBzdEV2dDppbnN0YW5jZUlEPSJ4bXAuaWlkOmE2ODFjZWQzLTlmODUtNDY0Yy1hNmE4LTA1OTk0YzFmZTU5OCIgc3RFdnQ6d2hlbj0iMjAyNC0xMC0yOFQyMDoxNTo0MVoiIHN0RXZ0OnNvZnR3YXJlQWdlbnQ9IkFkb2JlIFBob3Rvc2hvcCAyNi4wIChXaW5kb3dzKSIgc3RFdnQ6Y2hhbmdlZD0iLyIvPiA8cmRmOmxpIHN0RXZ0OmFjdGlvbj0ic2F2ZWQiIHN0RXZ0Omluc3RhbmNlSUQ9InhtcC5paWQ6ZmJhNjEzMDUtOGZmZi0yMzQ2LWFiZGUtZWFiODg3YjZmNjBhIiBzdEV2dDp3aGVuPSIyMDI1LTA0LTAyVDE3OjE2OjEyKzAxOjAwIiBzdEV2dDpzb2Z0d2FyZUFnZW50PSJBZG9iZSBQaG90b3Nob3AgMjYuNSAoV2luZG93cykiIHN0RXZ0OmNoYW5nZWQ9Ii8iLz4gPC9yZGY6U2VxPiA8L3htcE1NOkhpc3Rvcnk+IDx4bXBNTTpEZXJpdmVkRnJvbSBzdFJlZjppbnN0YW5jZUlEPSJ4bXAuaWlkOmEwNmYzMDQ5LTVkOTAtMDc0Ny04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' alt='SmartBoat Logo' style='max-width:200px; height:auto;' />"
    "</div>");

    page += F("<h2 style='color: white;'>Configuration</h2>");

#if !( USING_ESP32_S2 || USING_ESP32_C3 )

  wifiSSIDscan = false;
  LOGDEBUG(F("handleWifi: Scan done"));

  if (wifiSSIDCount == 0)
  {
    LOGDEBUG(F("handleWifi: No network found"));

    page += F("No network found. Refresh to scan again.");
  }
  else
  {
    page += FPSTR(WM_HTTP_LIST_START);
    page += FPSTR(WM_FLDSET_START);
        //display networks in page
    page += networkListAsString();

    page += pager;
    page += FPSTR(WM_FLDSET_END);
    page += "<br/>";
  }

  wifiSSIDscan = true;

#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )

  page += "<small>*Hint: To reuse the saved WiFi credentials, leave SSID and PWD fields empty</small>";

  page += FPSTR(WM_HTTP_FORM_START);

#if DISPLAY_STORED_CREDENTIALS_IN_CP
  // Populate SSIDs and PWDs if valid
  page.replace("[[ssid]]",  _ssid );
  page.replace("[[pwd]]",   _pass );
  page.replace("[[ssid1]]", _ssid1 );
  page.replace("[[pwd1]]",  _pass1 );
#endif

  char parLength[2];

  page += FPSTR(WM_HTTP_PARAM_START);
  page += FPSTR(WM_FLDSET_START);

  // add the extra parameters to the form
  for (int i = 0; i < _paramsCount; i++)
  {
    if (_params[i] == NULL)
    {
      break;
    }

    String pitem;

    switch (_params[i]->getLabelPlacement())
    {
      case WFM_LABEL_BEFORE:
        pitem = FPSTR(WM_HTTP_FORM_LABEL_BEFORE);
        break;

      case WFM_LABEL_AFTER:
        pitem = FPSTR(WM_HTTP_FORM_LABEL_AFTER);
        break;

      default:
        // WFM_NO_LABEL
        pitem = FPSTR(WM_HTTP_FORM_PARAM);
        break;
    }

    if (_params[i]->getID() != NULL)
    {
      pitem.replace("{i}", _params[i]->getID());
      pitem.replace("{n}", _params[i]->getID());
      pitem.replace("{p}", _params[i]->getPlaceholder());

      snprintf(parLength, 2, "%d", _params[i]->getValueLength());

      pitem.replace("{l}", parLength);
      pitem.replace("{v}", _params[i]->getValue());
      pitem.replace("{c}", _params[i]->getCustomHTML());
    }
    else
    {
      pitem = _params[i]->getCustomHTML();
    }

    page += pitem;
  }

  if (_paramsCount > 0)
  {
    page += FPSTR(WM_FLDSET_END);
  }

  if (_params[0] != NULL)
  {
    page += "<br/>";
  }

  LOGDEBUG1(F("Static IP ="), _WiFi_STA_IPconfig._sta_static_ip.toString());

  // KH, Comment out to permit changing from DHCP to static IP, or vice versa
  // and add staticIP label in CP

  // To permit disable/enable StaticIP configuration in Config Portal from sketch. Valid only if DHCP is used.
  // You'll loose the feature of dynamically changing from DHCP to static IP, or vice versa
  // You have to explicitly specify false to disable the feature.

#if !USE_STATIC_IP_CONFIG_IN_CP

  if (_WiFi_STA_IPconfig._sta_static_ip)
#endif
  {
    page += FPSTR(WM_HTTP_STATIC_START);
    page += FPSTR(WM_FLDSET_START);

    String item = FPSTR(WM_HTTP_FORM_LABEL);

    item += FPSTR(WM_HTTP_FORM_PARAM);

    item.replace("{i}", "ip");
    item.replace("{n}", "ip");
    item.replace("{p}", "Static IP");
    item.replace("{l}", "15");
    item.replace("{v}", _WiFi_STA_IPconfig._sta_static_ip.toString());

    page += item;

    item = FPSTR(WM_HTTP_FORM_LABEL);
    item += FPSTR(WM_HTTP_FORM_PARAM);

    item.replace("{i}", "gw");
    item.replace("{n}", "gw");
    item.replace("{p}", "Gateway IP");
    item.replace("{l}", "15");
    item.replace("{v}", _WiFi_STA_IPconfig._sta_static_gw.toString());

    page += item;

    item = FPSTR(WM_HTTP_FORM_LABEL);
    item += FPSTR(WM_HTTP_FORM_PARAM);

    item.replace("{i}", "sn");
    item.replace("{n}", "sn");
    item.replace("{p}", "Subnet");
    item.replace("{l}", "15");
    item.replace("{v}", _WiFi_STA_IPconfig._sta_static_sn.toString());

#if USE_CONFIGURABLE_DNS
    //***** Added for DNS address options *****
    page += item;

    item = FPSTR(WM_HTTP_FORM_LABEL);
    item += FPSTR(WM_HTTP_FORM_PARAM);

    item.replace("{i}", "dns1");
    item.replace("{n}", "dns1");
    item.replace("{p}", "DNS1 IP");
    item.replace("{l}", "15");
    item.replace("{v}", _WiFi_STA_IPconfig._sta_static_dns1.toString());

    page += item;

    item = FPSTR(WM_HTTP_FORM_LABEL);
    item += FPSTR(WM_HTTP_FORM_PARAM);

    item.replace("{i}", "dns2");
    item.replace("{n}", "dns2");
    item.replace("{p}", "DNS2 IP");
    item.replace("{l}", "15");
    item.replace("{v}", _WiFi_STA_IPconfig._sta_static_dns2.toString());
    //***** End added for DNS address options *****
#endif

    page += item;
    page += FPSTR(WM_FLDSET_END);
    page += "<br/>";
  }

  page += FPSTR(WM_HTTP_SCRIPT_NTP_HIDDEN);
  page += FPSTR(WM_HTTP_FORM_END);
  page += FPSTR(WM_HTTP_END);

#if ( USING_ESP32_S2 || USING_ESP32_C3 )
  request->send(200, WM_HTTP_HEAD_CT, page);

  // Fix ESP32-S2 issue with WebServer (https://github.com/espressif/arduino-esp32/issues/4348)
  delay(1);
#else

  AsyncWebServerResponse *response = request->beginResponse(200, WM_HTTP_HEAD_CT, page);

  response->addHeader(FPSTR(WM_HTTP_CACHE_CONTROL), FPSTR(WM_HTTP_NO_STORE));

#if USING_CORS_FEATURE
  response->addHeader(FPSTR(WM_HTTP_CORS), _CORS_Header);
#endif

  response->addHeader(FPSTR(WM_HTTP_PRAGMA), FPSTR(WM_HTTP_NO_CACHE));
  response->addHeader(FPSTR(WM_HTTP_EXPIRES), "-1");

  request->send(response);

#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )

  LOGDEBUG(F("Sent config page"));
}

//////////////////////////////////////////

// Handle the WLAN save form and redirect to WLAN config page again
void ESPAsync_WiFiManager::handleWifiSave(AsyncWebServerRequest *request)
{
  LOGDEBUG(F("WiFi save"));

  //SAVE/connect here
  _ssid = request->arg("s").c_str();
  _pass = request->arg("p").c_str();

  _ssid1 = request->arg("s1").c_str();
  _pass1 = request->arg("p1").c_str();

  ///////////////////////

#if USE_ESP_WIFIMANAGER_NTP

  if (request->hasArg("timezone"))
  {
    _timezoneName = request->arg("timezone");   //.c_str();

    LOGDEBUG1(F("TZ ="), _timezoneName);
  }
  else
  {
    LOGDEBUG(F("No TZ arg"));
  }

#endif

  ///////////////////////

  //parameters
  for (int i = 0; i < _paramsCount; i++)
  {
    if (_params[i] == NULL)
    {
      break;
    }

    //read parameter
    String value = request->arg(_params[i]->getID()).c_str();

    //store it in array
    value.toCharArray(_params[i]->_WMParam_data._value, _params[i]->_WMParam_data._length);

    LOGDEBUG2(F("Parameter and value :"), _params[i]->getID(), value);
  }

  if (request->hasArg("ip"))
  {
    String ip = request->arg("ip");

    optionalIPFromString(&_WiFi_STA_IPconfig._sta_static_ip, ip.c_str());

    LOGDEBUG1(F("New Static IP ="), _WiFi_STA_IPconfig._sta_static_ip.toString());
  }

  if (request->hasArg("gw"))
  {
    String gw = request->arg("gw");

    optionalIPFromString(&_WiFi_STA_IPconfig._sta_static_gw, gw.c_str());

    LOGDEBUG1(F("New Static Gateway ="), _WiFi_STA_IPconfig._sta_static_gw.toString());
  }

  if (request->hasArg("sn"))
  {
    String sn = request->arg("sn");

    optionalIPFromString(&_WiFi_STA_IPconfig._sta_static_sn, sn.c_str());

    LOGDEBUG1(F("New Static Netmask ="), _WiFi_STA_IPconfig._sta_static_sn.toString());
  }

#if USE_CONFIGURABLE_DNS

  //*****  Added for DNS Options *****
  if (request->hasArg("dns1"))
  {
    String dns1 = request->arg("dns1");

    optionalIPFromString(&_WiFi_STA_IPconfig._sta_static_dns1, dns1.c_str());

    LOGDEBUG1(F("New Static DNS1 ="), _WiFi_STA_IPconfig._sta_static_dns1.toString());
  }

  if (request->hasArg("dns2"))
  {
    String dns2 = request->arg("dns2");

    optionalIPFromString(&_WiFi_STA_IPconfig._sta_static_dns2, dns2.c_str());

    LOGDEBUG1(F("New Static DNS2 ="), _WiFi_STA_IPconfig._sta_static_dns2.toString());
  }

  //*****  End added for DNS Options *****
#endif

  String page = FPSTR(WM_HTTP_HEAD_START);
  page.replace("{v}", "Credentials Saved");

  page += FPSTR(WM_HTTP_SCRIPT);
  page += FPSTR(WM_HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(WM_HTTP_HEAD_END);
  page += FPSTR(WM_HTTP_SAVED);

  page.replace("{v}", _apName);
  page.replace("{x}", _ssid);
  page.replace("{x1}", _ssid1);
  //////

  page += FPSTR(WM_HTTP_END);

  //LOGDEBUG1(F("page ="), page);

#if ( USING_ESP32_S2 || USING_ESP32_C3 )
  request->send(200, WM_HTTP_HEAD_CT, page);

  // Fix ESP32-S2 issue with WebServer (https://github.com/espressif/arduino-esp32/issues/4348)
  delay(1);
#else

  AsyncWebServerResponse *response = request->beginResponse(200, WM_HTTP_HEAD_CT, page);
  response->addHeader(FPSTR(WM_HTTP_CACHE_CONTROL), FPSTR(WM_HTTP_NO_STORE));

#if USING_CORS_FEATURE
  response->addHeader(FPSTR(WM_HTTP_CORS), _CORS_Header);
#endif

  response->addHeader(FPSTR(WM_HTTP_PRAGMA), FPSTR(WM_HTTP_NO_CACHE));
  response->addHeader(FPSTR(WM_HTTP_EXPIRES), "-1");

  request->send(response);

#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )

  LOGDEBUG(F("Sent wifi save page"));

  connect = true; //signal ready to connect/reset
}

//////////////////////////////////////////

// Handle shut down the server page
void ESPAsync_WiFiManager::handleServerClose(AsyncWebServerRequest *request)
{
  LOGDEBUG(F("Server Close"));

  String page = FPSTR(WM_HTTP_HEAD_START);
  page.replace("{v}", "Close Server");

  page += FPSTR(WM_HTTP_SCRIPT);
  page += FPSTR(WM_HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(WM_HTTP_HEAD_END);
  page += F("<div class=\"msg\">");
  page += F("My network is <b>");
  page += WiFi_SSID();
  page += F("</b><br>");
  page += F("IP address is <b>");
  page += WiFi.localIP().toString();
  page += F("</b><br><br>");
  page += F("Portal closed...<br><br>");

  //page += F("Push button on device to restart configuration server!");

  page += FPSTR(WM_HTTP_END);

#if ( USING_ESP32_S2 || USING_ESP32_C3 )
  request->send(200, WM_HTTP_HEAD_CT, page);

  // Fix ESP32-S2 issue with WebServer (https://github.com/espressif/arduino-esp32/issues/4348)
  delay(1);
#else

  AsyncWebServerResponse *response = request->beginResponse(200, WM_HTTP_HEAD_CT, page);
  response->addHeader(FPSTR(WM_HTTP_CACHE_CONTROL), FPSTR(WM_HTTP_NO_STORE));

#if USING_CORS_FEATURE
  response->addHeader(FPSTR(WM_HTTP_CORS), _CORS_Header);
#endif

  response->addHeader(FPSTR(WM_HTTP_PRAGMA), FPSTR(WM_HTTP_NO_CACHE));
  response->addHeader(FPSTR(WM_HTTP_EXPIRES), "-1");

  request->send(response);

#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )

  stopConfigPortal = true; //signal ready to shutdown config portal

  LOGDEBUG(F("Sent server close page"));
}

//////////////////////////////////////////

// Handle the info page
void ESPAsync_WiFiManager::handleInfo(AsyncWebServerRequest *request)
{
  LOGDEBUG(F("Info"));

  // Disable _configPortalTimeout when someone accessing Portal to give some time to config
  _configPortalTimeout = 0;

  String page = FPSTR(WM_HTTP_HEAD_START);
  page.replace("{v}", "Info");

  page += FPSTR(WM_HTTP_SCRIPT);
  page += FPSTR(WM_HTTP_SCRIPT_NTP);
  page += FPSTR(WM_HTTP_STYLE);
  page += _customHeadElement;

  if (connect)
    page += F("<meta http-equiv=\"refresh\" content=\"5; url=/i\">");

  page += FPSTR(WM_HTTP_HEAD_END);

  page += F("<dl>");

  if (connect)
  {
    page += F("<dt>Trying to connect</dt><dd>");
    page += wifiStatus;
    page += F("</dd>");
  }

  page += pager;
  page += F("<h2>WiFi Information</h2>");

  reportStatus(page);

  page += FPSTR(WM_FLDSET_START);
  page += F("<h3>Device Data</h3>");
  page += F("<table class=\"table\">");
  page += F("<thead><tr><th>Name</th><th>Value</th></tr></thead><tbody><tr><td>Chip ID</td><td>");

#ifdef ESP8266
  page += String(ESP.getChipId(), HEX);
#else   //ESP32

  page += String(ESP_getChipId(), HEX);
  page += F("</td></tr>");

  page += F("<tr><td>Chip OUI</td><td>");
  page += F("0x");
  page += String(getChipOUI(), HEX);
  page += F("</td></tr>");

  page += F("<tr><td>Chip Model</td><td>");
  page += ESP.getChipModel();
  page += F(" Rev");
  page += ESP.getChipRevision();
#endif

  page += F("</td></tr>");

  page += F("<tr><td>Flash Chip ID</td><td>");

#ifdef ESP8266
  page += String(ESP.getFlashChipId(), HEX);
#else   //ESP32
  // TODO
  page += F("TODO");
#endif

  page += F("</td></tr>");

  page += F("<tr><td>IDE Flash Size</td><td>");
  page += ESP.getFlashChipSize();
  page += F(" bytes</td></tr>");

  page += F("<tr><td>Real Flash Size</td><td>");

#ifdef ESP8266
  page += ESP.getFlashChipRealSize();
#else   //ESP32
  // TODO
  page += F("TODO");
#endif

  page += F(" bytes</td></tr>");

  page += F("<tr><td>Access Point IP</td><td>");
  page += WiFi.softAPIP().toString();
  page += F("</td></tr>");

  page += F("<tr><td>Access Point MAC</td><td>");
  page += WiFi.softAPmacAddress();
  page += F("</td></tr>");

  page += F("<tr><td>SSID</td><td>");
  page += WiFi_SSID();
  page += F("</td></tr>");

  page += F("<tr><td>Station IP</td><td>");
  page += WiFi.localIP().toString();
  page += F("</td></tr>");

  page += F("<tr><td>Station MAC</td><td>");
  page += WiFi.macAddress();
  page += F("</td></tr>");
  page += F("</tbody></table>");

  page += FPSTR(WM_FLDSET_END);

#if USE_AVAILABLE_PAGES
  page += FPSTR(WM_FLDSET_START);
  page += FPSTR(WM_HTTP_AVAILABLE_PAGES);
  page += FPSTR(WM_FLDSET_END);
#endif

  page += F("<p/>More information about ESPAsync_WiFiManager at");
  page += F("<p/><a href=\"https://github.com/khoih-prog/ESPAsync_WiFiManager\">https://github.com/khoih-prog/ESPAsync_WiFiManager</a>");
  page += FPSTR(WM_HTTP_END);

#if ( USING_ESP32_S2 || USING_ESP32_C3 )
  request->send(200, WM_HTTP_HEAD_CT, page);

  // Fix ESP32-S2 issue with WebServer (https://github.com/espressif/arduino-esp32/issues/4348)
  delay(1);
#else

  AsyncWebServerResponse *response = request->beginResponse(200, WM_HTTP_HEAD_CT, page);

  response->addHeader(FPSTR(WM_HTTP_CACHE_CONTROL), FPSTR(WM_HTTP_NO_STORE));

#if USING_CORS_FEATURE
  response->addHeader(FPSTR(WM_HTTP_CORS), _CORS_Header);
#endif

  response->addHeader(FPSTR(WM_HTTP_PRAGMA), FPSTR(WM_HTTP_NO_CACHE));
  response->addHeader(FPSTR(WM_HTTP_EXPIRES), "-1");

  request->send(response);

#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )

  LOGDEBUG(F("Info page sent"));
}

//////////////////////////////////////////

// Handle the state page
void ESPAsync_WiFiManager::handleState(AsyncWebServerRequest *request)
{
  LOGDEBUG(F("State-Json"));

  String page = F("{\"Soft_AP_IP\":\"");

  page += WiFi.softAPIP().toString();
  page += F("\",\"Soft_AP_MAC\":\"");
  page += WiFi.softAPmacAddress();
  page += F("\",\"Station_IP\":\"");
  page += WiFi.localIP().toString();
  page += F("\",\"Station_MAC\":\"");
  page += WiFi.macAddress();
  page += F("\",");

  if (WiFi.psk() != "")
  {
    page += F("\"Password\":true,");
  }
  else
  {
    page += F("\"Password\":false,");
  }

  page += F("\"SSID\":\"");
  page += WiFi_SSID();
  page += F("\"}");

#if ( USING_ESP32_S2 || USING_ESP32_C3 )
  request->send(200, WM_HTTP_HEAD_CT, page);

  // Fix ESP32-S2 issue with WebServer (https://github.com/espressif/arduino-esp32/issues/4348)
  delay(1);
#else

  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", page);
  response->addHeader(FPSTR(WM_HTTP_CACHE_CONTROL), FPSTR(WM_HTTP_NO_STORE));

#if USING_CORS_FEATURE
  response->addHeader(FPSTR(WM_HTTP_CORS), _CORS_Header);
#endif

  response->addHeader(FPSTR(WM_HTTP_PRAGMA), FPSTR(WM_HTTP_NO_CACHE));
  response->addHeader(FPSTR(WM_HTTP_EXPIRES), "-1");

  request->send(response);
#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )

  LOGDEBUG(F("Sent state page in json format"));
}

//////////////////////////////////////////

/** Handle the scan page */
void ESPAsync_WiFiManager::handleScan(AsyncWebServerRequest *request)
{
  LOGDEBUG(F("Scan"));

  // Disable _configPortalTimeout when someone accessing Portal to give some time to config
  _configPortalTimeout = 0;   //KH

  LOGDEBUG(F("Scan-Json"));

  String page = F("{\"Access_Points\":[");

  // KH, display networks in page using previously scan results
  for (int i = 0; i < wifiSSIDCount; i++)
  {
    if (wifiSSIDs[i].duplicate == true)
      continue; // skip dups

    if (i != 0)
      page += F(", ");

    LOGDEBUG1(F("Index ="), i);
    LOGDEBUG1(F("SSID ="), wifiSSIDs[i].SSID);
    LOGDEBUG1(F("RSSI ="), wifiSSIDs[i].RSSI);

    int quality = getRSSIasQuality(wifiSSIDs[i].RSSI);

    if (_minimumQuality == -1 || _minimumQuality < quality)
    {
      String item = FPSTR(JSON_ITEM);
      String rssiQ;

      rssiQ += quality;
      item.replace("{v}", wifiSSIDs[i].SSID);
      item.replace("{r}", rssiQ);

#if defined(ESP8266)

      if (wifiSSIDs[i].encryptionType != ENC_TYPE_NONE)
#else
      if (wifiSSIDs[i].encryptionType != WIFI_AUTH_OPEN)
#endif
      {
        item.replace("{i}", "true");
      }
      else
      {
        item.replace("{i}", "false");
      }

      page += item;
      delay(0);
    }
    else
    {
      LOGDEBUG(F("Skipping due to quality"));
    }
  }

  page += F("]}");

#if ( USING_ESP32_S2 || USING_ESP32_C3 )
  request->send(200, WM_HTTP_HEAD_CT, page);

  // Fix ESP32-S2 issue with WebServer (https://github.com/espressif/arduino-esp32/issues/4348)
  delay(1);
#else

  AsyncWebServerResponse *response = request->beginResponse(200, WM_HTTP_HEAD_JSON, page);

  response->addHeader(WM_HTTP_CACHE_CONTROL, WM_HTTP_NO_STORE);
  response->addHeader(WM_HTTP_PRAGMA, WM_HTTP_NO_CACHE);
  response->addHeader(WM_HTTP_EXPIRES, "-1");

  request->send(response);
#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )  

  LOGDEBUG(F("Sent WiFiScan Data in Json format"));
}

//////////////////////////////////////////

// Handle the reset page
void ESPAsync_WiFiManager::handleReset(AsyncWebServerRequest *request)
{
  LOGDEBUG(F("Reset"));

  String page = FPSTR(WM_HTTP_HEAD_START);
  page.replace("{v}", "WiFi Information");

  page += FPSTR(WM_HTTP_SCRIPT);
  page += FPSTR(WM_HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(WM_HTTP_HEAD_END);
  page += F("Resetting");
  page += FPSTR(WM_HTTP_END);

#if ( USING_ESP32_S2 || USING_ESP32_C3 )
  request->send(200, WM_HTTP_HEAD_CT, page);
#else

  AsyncWebServerResponse *response = request->beginResponse(200, WM_HTTP_HEAD_CT, page);

  response->addHeader(WM_HTTP_CACHE_CONTROL, WM_HTTP_NO_STORE);
  response->addHeader(WM_HTTP_PRAGMA, WM_HTTP_NO_CACHE);
  response->addHeader(WM_HTTP_EXPIRES, "-1");

  request->send(response);
#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )

  LOGDEBUG(F("Sent reset page"));
  delay(5000);

  // Temporary fix for issue of not clearing WiFi SSID/PW from flash of ESP32
  // See https://github.com/khoih-prog/ESP_WiFiManager/issues/25 and https://github.com/espressif/arduino-esp32/issues/400
  resetSettings();
  //WiFi.disconnect(true); // Wipe out WiFi credentials.
  //////

#ifdef ESP8266
  ESP.reset();
#else   //ESP32
  ESP.restart();
#endif

  delay(2000);
}

//////////////////////////////////////////
//     Custom SmartConnect buttons     ///

void ESPAsync_WiFiManager::handleAutoSmartConnect(AsyncWebServerRequest *request) {
    if (request->method() != HTTP_POST) {
        request->send(405, "text/plain", "Method Not Allowed");
        return;
    }

    if (!request->hasParam("ssid", true) || !request->hasParam("pass", true)) {
        request->send(400, "text/plain", "Missing parameters");
        return;
    }

    String ssid = request->getParam("ssid", true)->value();
    String pass = request->getParam("pass", true)->value();

    Serial.printf("📡 SmartConnect request received: SSID=%s, PASS=%s\n", ssid.c_str(), pass.c_str());

    // Mimic WiFi save behavior
    _ssid = ssid;
    _pass = pass;

    wifiSetupComplete = true;
    smartConnectEnabled = true;
    writeBoolToEEPROM(WIFI_SETUP_COMPLETE_ADDR, true);
    writeBoolToEEPROM(SC_BOOL_ADDR, true);
    writeStringToEEPROM(WIFI_SSID_ADDR, ssid);
    writeStringToEEPROM(WIFI_PASS_ADDR, pass);
    standaloneMode = false;
    writeBoolToEEPROM(STANDALONE_MODE_ADDR, false);
    smartBoatEnabled = true;
    EEPROM.write(SB_BOOL_ADDR, 1);

    // ✅ Respond *first* before any restarts
    request->send(200, "text/plain", "✅ Credentials saved. Rebooting...");

    // ✅ Only reboot *after* SmartBox disconnects
    request->onDisconnect([]() {
        Serial.println("🔌 SmartBox disconnected after provisioning. Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(100)); // Optional safety delay
        saveConfigAndRestart();         // Now it's safe
    });
}



void ESPAsync_WiFiManager::handleSmartConnect(AsyncWebServerRequest *request) {
  LOGDEBUG(F("SmartConnect mode selected"));

  // HTML form with the serial number input
  String htmlResponse = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <style>
      body {
        background-color: #222;
        font-family: Arial, sans-serif;
        display: flex;
        justify-content: center;
        align-items: center;
        height: 100vh;
      }

      .popup-content {
        background-color: #333;
        color: white;
        padding: 30px; 
        border: 2px solid #ccc;
        width: 95%;
        max-width: 500px; 
        border-radius: 24px;
        font-size: 2.2rem; 
        line-height: 1.8;
        text-align: center;
      }

      input {
        font-size: 1.8rem;
        padding: 10px;
        width: 140px;
        margin: 20px;
        border-radius: 10px;
        border: 1px solid #ccc;
      }

      button {
        padding: 10px 20px;
        font-size: 2.0rem;
        border-radius: 10px;
        background-color: #009473;
        color: white;
        border: none;
      }
    </style>
  </head>
  <body>
    <div class="popup-content">
      <p>✅ <strong>SmartConnect mode enabled.</strong></p>
      <p>Please enter the serial number:</p>
      <form action="/setSerialNumber" method="POST">
        <input type="text" name="serialNumber" placeholder="Enter Serial Number" value="stcXXXX">
        <button type="submit">Connect</button>
      </form>
      <hr style="margin: 15px 0; border: 0; border-top: 1px solid #555;">
    </div>
  </body>
  </html>
  )rawliteral";

  request->send(200, "text/html", htmlResponse);
}

void ESPAsync_WiFiManager::handleSetSerialNumber(AsyncWebServerRequest *request) {
  if (request->hasParam("serialNumber", true)) {
      String serialNumber = request->getParam("serialNumber", true)->value();
      
      // Save the serial number into the global variable
      strncpy(::serialNumber, serialNumber.c_str(), sizeof(::serialNumber) - 1);
      ::serialNumber[sizeof(::serialNumber) - 1] = '\0';

      // Update the serial number in EEPROM if needed
      writeSerialNumberToEEPROM();

      // 🚦 Set SmartConnect flags
      standaloneMode = false;
      standaloneFlag = false;
      smartConnectEnabled = true;
      resetConfig = false;

      // 💾 Persist mode flags
      writeBoolToEEPROM(STANDALONE_MODE_ADDR, false);
      writeBoolToEEPROM(SC_BOOL_ADDR, true);
      writeResetConfigFlag(false);  // ✅ Skip config portal next boot

      // Confirm the action to the user
      request->send(200, "text/html", R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head>
        <meta charset="UTF-8">
        <style>
          body {
            background-color: #222;
            font-family: Arial, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
          }

          .popup-content {
            background-color: #333;
            color: white;
            padding: 30px; 
            border: 2px solid #ccc;
            width: 95%;
            max-width: 500px; 
            border-radius: 24px;
            font-size: 2.2rem; 
            line-height: 1.8;
            text-align: center;
          }
        </style>
      </head>

      <body>
        <div class="popup-content">
          <p>✅ <strong>SmartConnect mode activated.</strong></p>
          <p>Rebooting now...</p>
          <hr style="margin: 15px 0; border: 0; border-top: 1px solid #555;">
          <p>🔄 If the LED blinks <span style="color: cyan;">cyan</span>, please refresh the portal and enter your network details manually.</p>
        </div>
        <script>
          setTimeout(() => { window.location.reload(); }, 5000);
        </script>
      </body>
      </html>
      )rawliteral");

      // Restart the system after a short delay
      restartTicker.once(1.0, []() {
          Serial.println("🔁 Restarting now (SmartConnect selected)...");
          ESP.restart();
      });
  }
} 

void ESPAsync_WiFiManager::handleStandalone(AsyncWebServerRequest *request) {
  LOGDEBUG(F("Standalone mode selected"));

  // Return an HTML form for AP Name and Password
  String htmlForm = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <style>
      body {
        background-color: #222;
        font-family: Arial, sans-serif;
        display: flex;
        justify-content: center;
        align-items: center;
        height: 100vh;
      }

      .popup-content {
        background-color: #333;
        color: white;
        padding: 40px; 
        border: 2px solid #ccc;
        width: 90%;
        max-width: 500px; 
        border-radius: 24px;
        font-size: 2.5rem; 
        line-height: 1.8;
        text-align: center;
      }

      input {
        font-size: 2rem;
        padding: 10px;
        width: 80%;
        margin: 10px;
        border-radius: 10px;
        border: 1px solid #ccc;
      }

      button {
        padding: 10px 20px;
        font-size: 2rem;
        border-radius: 10px;
        background-color:  #009473;
        color: white;
        border: none;
      }
    </style>
  </head>
  <body>
    <div class="popup-content">
      <p>✅ Standalone Mode Enabled</p>
      <form action="/setStandaloneAP" method="POST">
        <input type="text" name="apName" placeholder="AP Name" value="SmartControllerAP">
        <input type="text" name="apPass" placeholder="AP Password" value="12345678">
        <input type="text" name="webName" placeholder="Custom Webpage name.local " value="smartmodule">
        <button type="submit">Set</button>
      </form>
    </div>
  </body>
  </html>
  )rawliteral";

  request->send(200, "text/html", htmlForm);
}

void ESPAsync_WiFiManager::handleSetStandaloneAP(AsyncWebServerRequest *request) {
  if (request->hasParam("apName", true) && request->hasParam("apPass", true)) {
    String apName = request->getParam("apName", true)->value();
    String apPass = request->getParam("apPass", true)->value();
    String customWeb = request->getParam("webName", true)->value();

    // Save to global vars
    strncpy(custom_ap_name, apName.c_str(), sizeof(custom_ap_name) - 1);
    custom_ap_name[sizeof(custom_ap_name) - 1] = '\0';

    strncpy(custom_ap_pass, apPass.c_str(), sizeof(custom_ap_pass) - 1);
    custom_ap_pass[sizeof(custom_ap_pass) - 1] = '\0';

    // Save custom webname
    strncpy(webnamechar, customWeb.c_str(), sizeof(webnamechar) - 1);
    webnamechar[sizeof(webnamechar) - 1] = '\0';
    webname = String(webnamechar);

    // Save to EEPROM
    writeStringToEEPROM(CUSTOM_AP_NAME_ADDR, custom_ap_name);
    writeStringToEEPROM(CUSTOM_AP_PASS_ADDR, custom_ap_pass);
    writeCustomWebnameToEEPROM();

    // Set mode flags
    standaloneMode = true;
    smartConnectEnabled = false;
    resetConfig = false;

    writeBoolToEEPROM(STANDALONE_MODE_ADDR, true);
    writeBoolToEEPROM(SC_BOOL_ADDR, false);
    writeResetConfigFlag(false);

    // Confirmation popup
    String confirmation = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <style>
        body {
          background-color: #222;
          font-family: Arial, sans-serif;
          display: flex;
          justify-content: center;
          align-items: center;
          height: 100vh;
        }

        .popup-content {
          background-color: #333;
          color: white;
          padding: 40px; 
          border: 2px solid #ccc;
          width: 90%;
          max-width: 500px; 
          border-radius: 24px;
          font-size: 2.5rem; 
          line-height: 1.8;
          text-align: center;
        }
      </style>
    </head>
    <body>
      <div class="popup-content">
        <p>✅ <strong>Standalone AP Settings Saved</strong></p>
        <p>Rebooting now...</p>
      </div>
      <script>
        setTimeout(() => { window.location.reload(); }, 4000);
      </script>
    </body>
    </html>
    )rawliteral";

    request->send(200, "text/html", confirmation);

    restartTicker.once(1.0, []() {
      Serial.println("🔁 Restarting now (Standalone selected)...");
      ESP.restart();
    });
  } else {
    request->send(400, "text/plain", "Missing AP name or password");
  }
}

///////////////////////////////////////////

void ESPAsync_WiFiManager::handleNotFound(AsyncWebServerRequest *request)
{
  if (captivePortal(request))
  {
    // If captive portal redirect instead of displaying the error page.
    return;
  }

  String message = "File Not Found\n\n";

  message += "URI: ";
  message += request->url();
  message += "\nMethod: ";
  message += (request->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += request->args();
  message += "\n";

  for (uint8_t i = 0; i < request->args(); i++)
  {
    message += " " + request->argName(i) + ": " + request->arg(i) + "\n";
  }

#if ( USING_ESP32_S2 || USING_ESP32_C3 )
  request->send(200, WM_HTTP_HEAD_CT, message);

  // Fix ESP32-S2 issue with WebServer (https://github.com/espressif/arduino-esp32/issues/4348)
  delay(1);
#else

  AsyncWebServerResponse *response = request->beginResponse( 404, WM_HTTP_HEAD_CT2, message );

  response->addHeader(FPSTR(WM_HTTP_CACHE_CONTROL), FPSTR(WM_HTTP_NO_STORE));
  response->addHeader(FPSTR(WM_HTTP_PRAGMA), FPSTR(WM_HTTP_NO_CACHE));
  response->addHeader(FPSTR(WM_HTTP_EXPIRES), "-1");

  request->send(response);
#endif    // ( USING_ESP32_S2 || USING_ESP32_C3 )
}

//////////////////////////////////////////

/**
   HTTPD redirector
   Redirect to captive portal if we got a request for another domain.
   Return true in that case so the page handler do not try to handle the request again.
*/
bool ESPAsync_WiFiManager::captivePortal(AsyncWebServerRequest *request)
{
  if (!isIp(request->host()))
  {
    LOGINFO(F("Request redirected to captive portal"));
    LOGINFO1(F("Location http://"), toStringIp(request->client()->localIP()));

    AsyncWebServerResponse *response = request->beginResponse(302, WM_HTTP_HEAD_CT2, "");

    response->addHeader("Location", String("http://") + toStringIp(request->client()->localIP()));

    request->send(response);

    return true;
  }

  LOGDEBUG1(F("request host IP ="), request->host());

  return false;
}

//////////////////////////////////////////

// start up config portal callback
void ESPAsync_WiFiManager::setAPCallback(void(*func)(ESPAsync_WiFiManager* myWiFiManager))
{
  _apcallback = func;
}

//////////////////////////////////////////

// start up save config callback
void ESPAsync_WiFiManager::setSaveConfigCallback(void(*func)())
{
  _savecallback = func;
}

//////////////////////////////////////////

// sets a custom element to add to head, like a new style tag
void ESPAsync_WiFiManager::setCustomHeadElement(const char* element)
{
  _customHeadElement = element;
}

//////////////////////////////////////////

// if this is true, remove duplicated Access Points - defaut true
void ESPAsync_WiFiManager::setRemoveDuplicateAPs(bool removeDuplicates)
{
  _removeDuplicateAPs = removeDuplicates;
}

//////////////////////////////////////////

int ESPAsync_WiFiManager::getRSSIasQuality(const int& RSSI)
{
  int quality = 0;

  if (RSSI <= -100)
  {
    quality = 0;
  }
  else if (RSSI >= -50)
  {
    quality = 100;
  }
  else
  {
    quality = 2 * (RSSI + 100);
  }

  return quality;
}

//////////////////////////////////////////

// Is this an IP?
bool ESPAsync_WiFiManager::isIp(const String& str)
{
  for (unsigned int i = 0; i < str.length(); i++)
  {
    int c = str.charAt(i);

    if ( (c != '.') && (c != ':') && ( (c < '0') || (c > '9') ) )
    {
      return false;
    }
  }

  return true;
}

//////////////////////////////////////////

// IP to String
String ESPAsync_WiFiManager::toStringIp(const IPAddress& ip)
{
  String res = "";

  for (int i = 0; i < 3; i++)
  {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }

  res += String(((ip >> 8 * 3)) & 0xFF);

  return res;
}

//////////////////////////////////////////

#ifdef ESP32
// We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
// SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
// Have to create a new function to store in EEPROM/SPIFFS for this purpose

String ESPAsync_WiFiManager::getStoredWiFiSSID()
{
  if (WiFi.getMode() == WIFI_MODE_NULL)
  {
    return String();
  }

  wifi_ap_record_t info;

  if (!esp_wifi_sta_get_ap_info(&info))
  {
    return String(reinterpret_cast<char*>(info.ssid));
  }
  else
  {
    wifi_config_t conf;

    esp_wifi_get_config(WIFI_IF_STA, &conf);

    return String(reinterpret_cast<char*>(conf.sta.ssid));
  }

  return String();
}

//////////////////////////////////////////

String ESPAsync_WiFiManager::getStoredWiFiPass()
{
  if (WiFi.getMode() == WIFI_MODE_NULL)
  {
    return String();
  }

  wifi_config_t conf;

  esp_wifi_get_config(WIFI_IF_STA, &conf);

  return String(reinterpret_cast<char*>(conf.sta.password));
}

//////////////////////////////////////////

uint32_t getChipID()
{
  uint64_t chipId64 = 0;

  for (int i = 0; i < 6; i++)
  {
    chipId64 |= ( ( (uint64_t) ESP.getEfuseMac() >> (40 - (i * 8)) ) & 0xff ) << (i * 8);
  }

  return (uint32_t) (chipId64 & 0xFFFFFF);
}

//////////////////////////////////////////

uint32_t getChipOUI()
{
  uint64_t chipId64 = 0;

  for (int i = 0; i < 6; i++)
  {
    chipId64 |= ( ( (uint64_t) ESP.getEfuseMac() >> (40 - (i * 8)) ) & 0xff ) << (i * 8);
  }

  return (uint32_t) (chipId64 >> 24);
}

#endif

//////////////////////////////////////////

#endif    // ESPAsync_WiFiManager_Impl_h
