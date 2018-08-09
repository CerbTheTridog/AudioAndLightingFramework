#ifndef __WIFI_PLUG_H__
#define __WIFI_PLUG_H__

#include <string>

// TODO: Not call external script?
// TODO: Better features

#define ALF_DIR    "/home/pi/AudioAndLightingFramework"
#define TPLINK_DIR ALF_DIR"/accessories/tplink-smartplug-master"
#define TPLINK_EXE TPLINK_DIR"/tplink_smartplug.py"
int turn_on_wifi_plug(const std::string &hostname);

int turn_off_wifi_plug(const std::string &hostname);
#endif
