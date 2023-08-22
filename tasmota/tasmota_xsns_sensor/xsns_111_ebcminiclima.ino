/*
  xsns_10_bh1750.ino - BH1750 ambient light sensor support for Tasmota

  Copyright (C) 2021  Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_EBCMINICLIMA_N

#define XSNS_111                          111


void EBCShow(bool json) {
      int magic = 111;
      if (json) {
        ResponseAppend_P(PSTR(",\"EBC miniClima\":{\"" D_JSON_VOLTAGE "\":%d}"), &magic);
      } else {
        AddLog(LOG_LEVEL_DEBUG, "web show 1");
        WSContentSend_PD(PSTR("{s}PD{m}%s {e}"), ebcstatus.firmwareVer);
        WSContentSend_P(PSTR("{s}P{m}%s {e}"), ebcstatus.firmwareVer);
        //WSContentSend_PD(PSTR("{s}one value{e}"));
      }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns111(uint32_t function) {
  
  bool result = false;

  if (FUNC_INIT == function) {
    if(ebcstatus.inited == true) {
      AddLog(LOG_LEVEL_DEBUG,"sns111: found ebc instance");
    }
    else AddLog(LOG_LEVEL_DEBUG,"sns111: not found ebc instance");
  }
  else if (1) {
    //
    switch (function) {
      case FUNC_WEB_SENSOR:
        EBCShow(0);
        break;
      case FUNC_EVERY_SECOND:
        //AddLog(LOG_LEVEL_DEBUG,"sns 1 sec");
        //EBCShow(0);
        break;
      case FUNC_JSON_APPEND:
        EBCShow(1);
        break;
    }
  }
  return result;
}

#endif  // USE_BH1750

