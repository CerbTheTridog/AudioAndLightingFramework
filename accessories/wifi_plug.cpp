#include "wifi_plug.h"
#include <stdio.h>
#include <stdlib.h>

// TODO: Not call external script?
// TODO: Better features

static const char* tplink_exe = TPLINK_EXE;


int turn_on_wifi_plug(const std::string &hostname)
{
    const char* tplink_smartplug_flag = "on";
    char cmd_buf[256];

    sprintf(cmd_buf, "%s -t %s -c %s", tplink_exe, hostname.c_str(), tplink_smartplug_flag);
    return system(cmd_buf);
}

int turn_off_wifi_plug(const std::string &hostname)
{
    const char* tplink_smartplug_flag = "off";
    char cmd_buf[256];
    sprintf(cmd_buf, "%s -t %s -c %s", tplink_exe, hostname.c_str(), tplink_smartplug_flag);
    return system(cmd_buf);
}


