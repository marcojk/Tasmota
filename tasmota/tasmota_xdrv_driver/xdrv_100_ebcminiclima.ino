/*
  xdrv_100_ebcminiclima.ino EBC10/12 support
*/

#ifdef USE_EBCMINICLIMA
/*********************************************************************************************\
 * My IoT Device example3
 *
 *
\*********************************************************************************************/


#define XDRV_100 100
const char HTTP_BTN_MENU_MI32[] PROGMEM = "<p><form action='m32' method='get'><button>EBC Dashboard</button></form></p>";


#ifndef nitems
#define nitems(_a)		(sizeof((_a)) / sizeof((_a)[0]))
#endif

#define EBC_SPEED           9600
#define SERIAL_NUMBER_LEN   13
#define FIRMWARE_VER_LEN    13
// Serial buffer length for incoming data
#define EBC_MAX_DATA_LEN 128

/*M/S/E/e (M.. EBC10/11/12 master
S.. EBC10/11/12 slave,
E.. EBCeasy master
e.. EBCeasy slave*/

const char cmd_sernum[] PROGMEM ="sernum\r";
const char cmd_start[] PROGMEM ="start\r";
const char cmd_stop[] PROGMEM ="stop\r";
const char cmd_vals[] PROGMEM ="vals\r";
const char cmd_date[] PROGMEM ="date\r";
const char cmd_time[] PROGMEM ="time\r";
const char cmd_setpoint[] PROGMEM ="#setPoint\r";

#define D_PRFX_EBCMINICLIMA "EBC"

enum EBC_MODEL { EBC_10_11_12_MASTER, EBC_10_11_12_SLAVE, EBCEASY_MASTER, EBCEASY_SLAVE };
enum EBC_STATE { NEXT_SERNUM, NEXT_VALS, NEXT_SETPOINT, NEXT_DATE, NEXT_TIME, NEXT_DUMP, NEXT_START, NEXT_STOP, IDLE};

struct EBC_MODEL_MAP {
	enum EBC_MODEL	 linkMode;
	const char		*name;
};
static const struct EBC_MODEL_MAP ebcLinkMode[] = {
	{ EBC_10_11_12_MASTER,		        "EBC10/11/12 master"},
	{ EBC_10_11_12_SLAVE,		        "EBC10/11/12 slave" },
	{ EBCEASY_MASTER,		            "EBCeasy master"	},
	{ EBCEASY_SLAVE,		            "EBCeasy slave"	},
};
#define EBC1X_SENSOR_ALARM      0x40
#define EBC1X_PUMP              0x20
#define EBC1X_PUMP_OUT          0x10
#define EBC1X_BOTTLE_ALARM      0x04
#define EBC1X_HUMIDITY_LOW      0x02
#define EBC1X_HUMIDITY_HIGH     0x01
#define EBCEASY_SENSOR_ALARM    0x40
#define EBCEASY_CONTROLFLAP     0x20
#define EBCEASY_INTERNAL_TEM    0x04
#define EBCEASY_HUMIDITY_HIGH   0x02
#define EBCEASY_HUMIDITY_LOW    0x01

#include <TasmotaSerial.h>

char ebc_buffer[EBC_MAX_DATA_LEN + 1];

// Software and hardware serial pointers
TasmotaSerial *ebcSerial = nullptr;

/*********************************************************************************************\
 * My IoT Device Functions
\*********************************************************************************************/

// This variable will be set to true after initialization
bool initSuccess = false;

char * payload = nullptr;
size_t payload_size = 100;
char * topic = nullptr;
size_t topic_size = 30;

struct ebcStatus {
    //uint32_t lastDate;
    //uint32_t lastTime;
    char lastDate[9]; //00.00.00
    char lastTime[6]; //11:59
    bool running;
    bool simulator;
    bool inited;
    uint8_t setpoint;
    uint8_t setpointHumidityH;
    uint8_t setpointHumidityL;
    uint8_t alarmdelay;
    uint8_t interval;
    int8_t  rhCorrection;
    uint8_t hysteresis;
    uint8_t humidity;
    uint8_t temperature;
    int8_t  t_cond;
    int8_t  t_cool;
    char serialNumber[SERIAL_NUMBER_LEN];
    char firmwareVer[FIRMWARE_VER_LEN];
    enum EBC_STATE ebcstate;
    enum EBC_MODEL model;
    uint8_t alarms;
} ebcstatus;

/*
  Optional: if you need to pass any command for your device
  Commands are issued in Console or Web Console
  Commands:


  comando 1
  comando
*/

const char MyProjectCommands[] PROGMEM = D_PRFX_EBCMINICLIMA "|"
  "vals|"
  "sernum|"
  "date|"
  "start|"
  "stop|"
  "simulate|"
  "status|"
  "setpoint|"
  "SendMQTT";

void (* const MyProjectCommand[])(void) PROGMEM = {
  &CmdVals, &CmdSernum, &CmdDate, &CmdStart, &CmdStop, &CmdSimulate, &CmdEbcStatus, &CmdSetPoint, &CmdSendMQTT};

void CmdVals(void) {
  AddLog(LOG_LEVEL_INFO, cmd_vals);
  ebcSerial->write(cmd_vals, strlen(cmd_vals));
  ebcstatus.ebcstate = NEXT_VALS;
  AddLog(LOG_LEVEL_DEBUG, "next state VALS %d", ebcstatus.ebcstate);
  ResponseCmndDone();
}

void CmdSetPoint(void) {
  char cmd[20];
  AddLog(LOG_LEVEL_INFO, cmd_setpoint);
   if (XdrvMailbox.data_len > 0) {
    if (XdrvMailbox.payload > 0) {
      char *p;
      uint32_t i = 0;
      uint32_t param[4] = { 0 };
      for (char *str = strtok_r(XdrvMailbox.data, ", ", &p); str && i < 4; str = strtok_r(nullptr, ", ", &p)) {
        param[i] = strtoul(str, nullptr, 0);
        i++;
      }
      AddLog(LOG_LEVEL_DEBUG, PSTR("EBC: params %08x %08x %08x %08x"), param[0], param[1], param[2], param[3]);
      if(param[0] >= 15 && param[0] <= 85 ) { //setpoint00
        if( param[0] <= ebcstatus.setpointHumidityL + 5 )
          param[0] = ebcstatus.setpointHumidityL + 5;
        else if( param[0] >= ebcstatus.setpointHumidityH - 5)
          param[0] = ebcstatus.setpointHumidityH - 5;
        snprintf_P(cmd,sizeof(cmd),PSTR("#setPoint%02d%02d\r"), 0, param[0]);
        AddLog(LOG_LEVEL_DEBUG,"Setpoint humidity: %s", cmd);
        ebcSerial->write(cmd, strlen(cmd));
      }
      else {
        AddLog(LOG_LEVEL_DEBUG, "formato errato setpoint 0");
        return;
      }
      if(param[1] >= 15 && param[1] <= 90 && param[2] >=5 && param[2] <=80 && param[1] >= param[2] ) { //setpoint01 setpoint02
        snprintf_P(cmd,sizeof(cmd),PSTR("#setPoint%02d%02d\r"), 1, param[1]);
        ebcSerial->write(cmd, strlen(cmd));
        snprintf_P(cmd,sizeof(cmd),PSTR("#setPoint%02d%02d\r"), 2, param[1]);
        ebcSerial->write(cmd, strlen(cmd));
        AddLog(LOG_LEVEL_DEBUG,"Setpoint alarmH: %s alarmL: %d", param[1], param[2]);
      }
      else if (param[1] != 0 && param[2] != 0) {
        AddLog(LOG_LEVEL_DEBUG, "formato errato setpoint 1 2");
        return;
      }
    
    }
   }
  //ebcSerial->write(cmd_date, strlen(cmd_setpoint));
  ebcstatus.ebcstate = NEXT_SETPOINT;
  ResponseCmndDone();
}

void CmdDate(void) {
  AddLog(LOG_LEVEL_INFO, cmd_date);
  ebcSerial->write(cmd_start, strlen(cmd_date));
  ebcstatus.ebcstate = NEXT_DATE;
  ResponseCmndDone();
}

void CmdStart(void) {
  AddLog(LOG_LEVEL_INFO, cmd_start);
  ebcSerial->write(cmd_start, strlen(cmd_start));
  ebcstatus.ebcstate = NEXT_START;
  ResponseCmndDone();
}

void CmdStop(void) {
  AddLog(LOG_LEVEL_INFO, cmd_stop);
  ebcSerial->write(cmd_stop, strlen(cmd_stop));
  ebcstatus.ebcstate = NEXT_STOP;
  ResponseCmndDone();
}
void CmdSernum(void) {
  AddLog(LOG_LEVEL_INFO, cmd_sernum);
  ebcSerial->write(cmd_sernum, strlen(cmd_sernum));
  ebcstatus.ebcstate = NEXT_SERNUM;
  ResponseCmndDone();
}

void CmdSimulate(void) {
  ebcstatus.simulator ? ebcstatus.simulator = false : ebcstatus.simulator = true;
  AddLog(LOG_LEVEL_INFO, "Toggle EBC internal simulator: %d", ebcstatus.simulator);
  ResponseCmndDone();
}

void CmdEbcStatus(void) {
AddLog(LOG_LEVEL_INFO,
"setpoint %d\nrhCorrection %d\nhysteresis %d\nhumidity %d\ntemperature %d\nSerial number %s\n FW ver%s", ebcstatus.setpoint, ebcstatus.rhCorrection,
ebcstatus.hysteresis, ebcstatus.humidity, ebcstatus.temperature, ebcstatus.serialNumber, ebcstatus.firmwareVer
);
  ResponseCmndDone();
}

void CmdSendMQTT(void) {
  AddLog(LOG_LEVEL_INFO, PSTR("Sending MQTT message."));

  snprintf_P(topic, topic_size, PSTR("tasmota/embcminiclima"));

  snprintf_P(payload, payload_size,
            PSTR("{\"" D_JSON_TIME "\":\"%s\",\"name\":\"My Project\"}"),
            GetDateAndTime(DT_LOCAL).c_str()
  );

  // retain = true
  MqttPublishPayload(topic, payload, strlen(payload), false);

  ResponseCmndDone();
}

/*
  AddLog(LOG_LEVEL_INFO, PSTR("Help: Accepted commands - Say_Hello, SendMQTT, Help"));
  ResponseCmndDone();
}
*/
/*********************************************************************************************\
 * Tasmota Functions
\*********************************************************************************************/
//  if (*mserv == '*') { mserv = xPSTR(EMAIL_SERVER); }

void ebcSendMailAlarms ()
{
  uint8_t alarms;
  if(ebcstatus.model == EBC_10_11_12_MASTER || ebcstatus.model == EBC_10_11_12_SLAVE) {
        if( (alarms & EBC1X_SENSOR_ALARM) != (ebcstatus.alarms & EBC1X_SENSOR_ALARM));

      }
}

void ebcCheckAlarms(uint8_t alarms) {
  if(ebcstatus.alarms != alarms) {
    ebcstatus.alarms = alarms;
    ebcSendMailAlarms();
  }

}
void ebcProcessSerialData (void)
{
    uint8_t data;
    bool dataReady;
    if (ebcSerial)
    {
        while (ebcSerial->available() > 0)
        {
            data = ebcSerial->read();
            dataReady = ebcAddData((char)data);
            if (dataReady)
            {
                AddLog(LOG_LEVEL_DEBUG,PSTR("DATA READY"));
                ebcProcessData();
            }
        }
    }
}

bool ebcAddData(char nextChar)
{
    // Buffer position
    static uint8_t currentIndex = 0;
    // Store data into buffer at position
    /* if ((currentIndex >0) && (0x0D == Tfmp_buffer[currentIndex-1]) && (0x59 == nextChar))
    {
        currentIndex = 1;
    } */
    if (0x0D == nextChar) {
      ebc_buffer[currentIndex] = '\0';
      currentIndex = 0;
      return true;
    }
    if (0x0A == nextChar)
        return false;

    ebc_buffer[currentIndex] = nextChar;
    currentIndex++;
    // Check for too many data
    if (currentIndex > EBC_MAX_DATA_LEN)
    {
        // Terminate buffer and reset position
        ebc_buffer[EBC_MAX_DATA_LEN] = '\0';
        currentIndex = 0;
        return true;
    }
    return false;
}

void ebcStartStop( bool state) {
  if(state == true) {
    ebcSerial->write(cmd_start,strlen(cmd_start));
    ebcstatus.ebcstate = NEXT_START;
  }
  else{
    ebcSerial->write(cmd_stop, strlen(cmd_start);
    ebcstatus.ebcstate = NEXT_STOP;
  }
}

void ebcProcessData(void) {
    AddLog(LOG_LEVEL_DEBUG,PSTR("Line from serial: %s"),ebc_buffer);
    switch (ebcstatus.ebcstate) {
        case NEXT_VALS:
          AddLog(LOG_LEVEL_DEBUG_MORE,"VALS %s", ebc_buffer);
          char runstop[10];
          uint8_t alarms;
          if(!strcmp(cmd_vals, ebc_buffer))
            return; //handle echo
          sscanf( ebc_buffer, "%s %d %d %d %d %d", runstop, &ebcstatus.humidity, &ebcstatus.temperature, &ebcstatus.t_cond, &ebcstatus.t_cool, &alarms );
            if(!strcmp(PSTR("Running"), runstop))
              ebcstatus.running = true;
            else if(!strcmp(PSTR("Standby"), runstop))
              ebcstatus.running = false;
          ebcCheckAlarms(alarms);
          break;
        case NEXT_SETPOINT:
          AddLog(LOG_LEVEL_DEBUG_MORE,"SETPOINT %s", ebc_buffer);
          if(!strcmp(PSTR("#*************"), ebc_buffer))
            return; //handle echo
          if(ebc_buffer[0] == '?') {
            ResponseCmndError();
            return;
          }
        break;
        case NEXT_SERNUM:
          AddLog(LOG_LEVEL_DEBUG_MORE,"SERNUM %s", ebc_buffer);
          if(!strcmp(cmd_sernum, ebc_buffer))
            return; //handle echo
          char model;
          char fill[10];
          sscanf( ebc_buffer, "%s %s %s %[^:]:%d %d %d %d %d %d %d", ebcstatus.serialNumber, &model, ebcstatus.firmwareVer, fill, &ebcstatus.setpoint, &ebcstatus.setpointHumidityH, &ebcstatus.setpointHumidityL, &ebcstatus.alarmdelay, &ebcstatus.interval, &ebcstatus.rhCorrection, &ebcstatus.hysteresis);
          break;
        case NEXT_DATE:
          AddLog(LOG_LEVEL_DEBUG_MORE,"DATE %s", ebc_buffer);
          if(!strcmp(cmd_date, ebc_buffer))
            return; //handle echo
          break;
        case NEXT_START:
            AddLog(LOG_LEVEL_DEBUG,"START %s", ebc_buffer);
          if(!strcmp(cmd_start, ebc_buffer))
            return; //handle echo
          if(strstr(ebc_buffer, PSTR("Start"))) {
            AddLog(LOG_LEVEL_DEBUG, "in start");
            ebcParseDateTime(ebc_buffer);
            }
            else if (strstr(ebc_buffer, PSTR("Set"))) {
              AddLog(LOG_LEVEL_DEBUG, "in set");
              ebcParsePeriodicData(ebc_buffer);
              AddLog(LOG_LEVEL_DEBUG, "out set");
              ebcstatus.running = 1;
            }
          else {
            AddLog(LOG_LEVEL_DEBUG,"calling startstop from ebcprocessdata");
            ebcStartStop(true);
            return;
          }
          break;
        case NEXT_STOP:
            AddLog(LOG_LEVEL_DEBUG,"STOP %s", ebc_buffer);
            if(!strcmp(cmd_stop, ebc_buffer))
              return; //handle echo
            if(strstr(ebc_buffer, PSTR("Stop"))) {
              ebcParseDateTime(ebc_buffer);
              ebcstatus.running = 0;
            }
            else {
              AddLog(LOG_LEVEL_DEBUG,"calling startstop from ebcprocessdata");
              ebcStartStop(false);
              return;
            }
            break;
        default:
          ebcParsePeriodicData(ebc_buffer);
          break;
        ebcstatus.ebcstate = IDLE;
      }
    ResponseCmndDone();
}

void ebcInit(void)
{
    ebcstatus.inited = false;
    AddLog(LOG_LEVEL_DEBUG, PSTR("EBC Miniclima init..."));
    if(!PinUsed(GPIO_EBC_RX) || !PinUsed(GPIO_EBC_TX))
    {
      AddLog(LOG_LEVEL_DEBUG,PSTR("EBC Module not configured"));
      return;
    }
     // Software serial init needs to be done here as earlier (serial) interrupts may lead to Exceptions
    ebcSerial = new TasmotaSerial(Pin(GPIO_EBC_RX), Pin(GPIO_EBC_TX), 1);
    if (ebcSerial->begin(9600, SERIAL_8N1)) {
      AddLog(LOG_LEVEL_DEBUG, PSTR("EBC ser start"));
        /*if (ebcSerial->hardwareSerial()) {
        ClaimSerial();
        }*/
        ebcstatus.inited = true;
        ebcstatus.simulator = false;
    }
    else AddLog(LOG_LEVEL_DEBUG, PSTR("EBC ser NOT started"));
}

void ebcParseDateTime(char * buffer) {
  strncpy(ebcstatus.lastDate, buffer, 8);
  ebcstatus.lastDate[8] = 0;
  strncpy(ebcstatus.lastTime, buffer+10, 5);
  ebcstatus.lastTime[5];
}

/*    bool simulator;
    bool inited;
    uint8_t setpoint;
    uint8_t setpointHumidityH;
    uint8_t setpointHumidityL;
    uint8_t alarmdelay;
    uint8_t interval;
    int8_t  rhCorrection;
    uint8_t hysteresis;
    uint8_t humidity;
    uint8_t temperature;
    int8_t  t_cond;
    int8_t  t_cool;
    */
void ebcParsePeriodicData(char * buffer) {
  if (buffer != NULL) {
    if( buffer[0] == 'S' && buffer[1] == 'e') { //periodic dopo "start"
      //Set:76 51 71 01 11 +00 03 59 27 +28 +28 00
      sscanf( buffer, "%[^:]:%d %d %d %d %d %d %d %d %d %d %d", &ebcstatus.setpoint, &ebcstatus.setpointHumidityL, &ebcstatus.setpointHumidityH, 
      &ebcstatus.alarmdelay, &ebcstatus.interval, &ebcstatus.rhCorrection, &ebcstatus.humidity, &ebcstatus.temperature, &ebcstatus.t_cond, &ebcstatus.t_cool, &ebcstatus.alarms);
      return;
    }
/*    <RH value> <temperature> <t@cond>
<t@cool> <alarm byte> Set: <setvalue>
<lower alarm limit> <upper alarm limit>
<1.alarm delay> <interval> <+/-RH
correction>
    54 28 +28 +29 00 Set:15 10
25 01 01 +00
*/
    if( buffer[17] == 'S' && buffer[18] == 'e') { //periodic dopo "stop"
      //54 28 +28 +29 00 Set:15 10 25 01 01 +00
      AddLog(LOG_LEVEL_DEBUG,"into set after stop");
      sscanf( buffer, "%d %d %d %d %d %[^:]:%d %d %d %d %d %d", 
      &ebcstatus.humidity, &ebcstatus.temperature, &ebcstatus.t_cond, &ebcstatus.t_cool, &ebcstatus.alarms,
      &ebcstatus.setpoint, &ebcstatus.setpointHumidityL, &ebcstatus.setpointHumidityH,
      &ebcstatus.alarmdelay, &ebcstatus.interval, &ebcstatus.rhCorrection);
      return;
    }
  }
}
void MyProjectInit()
{


  /*
    Here goes My Project setting.
    Usually this part is included into setup() function
  */



  payload = (char*)calloc(payload_size, sizeof(char));
  topic = (char*)calloc(topic_size, sizeof(char));


  if (!payload || !topic) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("My Project: out of memory"));
    return;
  }

  initSuccess = true;

  AddLog(LOG_LEVEL_DEBUG, PSTR("My Project init is successful..."));

}

void EBCShow(bool json) {
      if (json) {
      //  ResponseAppend_P(PSTR(",\"EBC miniClima\":{\"" D_JSON_VOLTAGE "\":%d}"), &magic);
      } else {
        WSContentSend_PD("{s}setpoint {m}%d{e}{s}rhCorrection {m}%d{e}{s}Hysteresis {m}%d{e}{s}humidity {m}%d{e}{s}temperature {m}%d{e}{s}Serial number {m}%s{e}{s}FW version{m}%s{e}{s}Stato{m}%s{e}",
        ebcstatus.setpoint, ebcstatus.rhCorrection,
ebcstatus.hysteresis, ebcstatus.humidity, ebcstatus.temperature, ebcstatus.serialNumber, ebcstatus.firmwareVer, ebcstatus.running ? PSTR("Attivo") : PSTR("Arrestato"));
        //WSContentSend_PD(PSTR("{s}Firmware version{m}%s {e}"), ebcstatus.firmwareVer);
        //WSContentSend_P(PSTR("{s}P{m}%s {e}"), ebcstatus.firmwareVer);
        //WSContentSend_PD(PSTR("{s}one value{e}"));
      }
}

//#ifdef USE_WEBSERVER

void LscMcAddFuctionButtons(void) {
  //WSContentSend_P(HTTP_TABLE100);
  //WSContentSend_P(PSTR("<tr>"));
  WSContentSend_P(HTTP_MSG_SLIDER_GRADIENT,  // Cold Warm
    PSTR("a"),             // a - Unique HTML id
    PSTR("#ffffff"), PSTR("#73a1"),  // 6500k in RGB (White) to 2500k in RGB (Warm Yellow)
    1,               // sl1
    1, 100,        // Range color temperature
    0,
    'h', 0);         // sp0 - Value id releated to lc("h0", value) and WebGetArg("h0", tmp, sizeof(tmp));

//  WSContentSend_P(PSTR("</tr></table>"));
  WSContentSend_P(HTTP_TABLE100);
  WSContentSend_P(PSTR("<tr>"));
  WSContentSend_P(PSTR("<td style='width:%d%%'><button onclick='la(\"&ebc=1\");'>%s</button></td>"), 50,   // &ebc is related to WebGetArg("lsc", tmp, sizeof(tmp));
  PSTR("Avvia"));
  WSContentSend_P(PSTR("<td style='width:%d%%'><button onclick='la(\"&ebc=0\");'>%s</button></td>"), 50,   // &ebc is related to WebGetArg("lsc", tmp, sizeof(tmp));
  PSTR("Ferma"));
  WSContentSend_P(PSTR("</tr></table>"));
  /*
  uint32_t rows = 1;
  uint32_t cols = 8;
  for (uint32_t i = 0; i < 8; i++) {
    if (strlen(SettingsText(SET_BUTTON1 + i +1))) {
      rows <<= 1;
      cols >>= 1;
      break;
    }
  }
  WSContentSend_P(HTTP_TABLE100);
  WSContentSend_P(PSTR("<tr>"));
  char number[4];
  uint32_t idx = 0;
  for (uint32_t i = 0; i < rows; i++) {
    if (idx > 0) { WSContentSend_P(PSTR("</tr><tr>")); }
    for (uint32_t j = 0; j < cols; j++) {
      idx++;
      WSContentSend_P(PSTR("<td style='width:%d%%'><button onclick='la(\"&lsc=%d\");'>%s</button></td>"),  // &lsc is related to WebGetArg("lsc", tmp, sizeof(tmp));
        100 / cols,
        idx -1,
        (strlen(SettingsText(SET_BUTTON1 + idx))) ? SettingsText(SET_BUTTON1 + idx) : itoa(idx, number, 10));
    }
  }
  WSContentSend_P(PSTR("</tr></table>"));
  */
}

void LscMcWebGetArg(void) {
  char tmp[8];                               // WebGetArg numbers only
  WebGetArg(PSTR("ebc"), tmp, sizeof(tmp));  // 0 - 7 functions
  if(strlen(tmp)) {
    AddLog(LOG_LEVEL_DEBUG,"st sp");
  //char *tmp1 = tmp;
  //for ( ; *tmp1; ++tmp1) *tmp1 = tolower(*tmp1); //to lowercase
  if(tmp[0] == '1' )
    ExecuteWebCommand("ebcstart");
  else if(tmp[0] == '0' )
    ExecuteWebCommand("ebcstop");
  }
  WebGetArg(PSTR("h0"), tmp, sizeof(tmp));  // 0 - 7 functions
  if(strlen(tmp)) {
    uint32_t function = atoi(tmp);
    AddLog(LOG_LEVEL_DEBUG,"%s", tmp);
    char cmd[30];
    snprintf_P(cmd, sizeof(cmd), PSTR("ebcsetpoint %d,0,0,0"), function);
    ExecuteWebCommand(cmd);
    
  }
}

//#endif  // USE_WEBSERVER
void MI32HandleWebGUI(void){
  WSContentSend_P("{s}mi32handleweb{e}");
}

bool Xdrv100(uint32_t function) {
  bool result = false;

  if (FUNC_INIT == function) {
    ebcInit();
    } else if (ebcstatus.inited) {
      switch (function) {
        case FUNC_EVERY_SECOND:
          break;
        case FUNC_WEB_SENSOR:
          EBCShow(0);
          break;
        case FUNC_COMMAND:
          result = DecodeCommand(MyProjectCommands, MyProjectCommand);
          if (ebcstatus.simulator) {
             switch (ebcstatus.ebcstate) {
              case NEXT_VALS:
                //AddLog(LOG_LEVEL_DEBUG, "writing to serial Running 48 29 +34 +30 00\r");
                ebcSerial->write("Running 48 29 +34 +30 00\r",25);
              break;
              case NEXT_SERNUM:
                ebcSerial->write("#000975 M 120509.03 Set:15 10 25 01 01 +00 02\r",46);
              break;
              case NEXT_DATE:
                ebcSerial->write("11.06.07\r",9);
              break;
              case NEXT_STOP:
                ebcSerial->write("11.06.08 13:30 Stop\r");
                ebcSerial->write("54 28 +28 +29 00 Set:15 10 25 01 01 +00\r");
              break;
              case NEXT_START:
                ebcSerial->write("11.06.08 13:30 Start\r");
                ebcSerial->write("Set:15 10 25 01 01 +00 53 28 +28 +29 00\r");
              break;
              default:
              break;
            }
          }
          break;
        case FUNC_LOOP:
          ebcProcessSerialData();
          break;
        case FUNC_WEB_ADD_MAIN_BUTTON:
          LscMcAddFuctionButtons();
          //WSContentSend_P(HTTP_BTN_MENU_MI32);
          break;
        case FUNC_WEB_ADD_HANDLER:
           WebServer_on(PSTR("/m32"), MI32HandleWebGUI);
          break;
        case FUNC_WEB_GET_ARG:
          LscMcWebGetArg();
        break;
        default:
          break;
      }
    }
  return result;
}

void MyProjectProcessing(void)
{

  /*
    Here goes My Project code.
    Usually this part is included into loop() function
  */

}


#endif  // USE_MY_PROJECT_EX3