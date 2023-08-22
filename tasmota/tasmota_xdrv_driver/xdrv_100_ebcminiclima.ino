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
enum EBC_STATE { NEXT_SERNUM, NEXT_VALS, NEXT_DATE, NEXT_TIME, NEXT_DUMP, NEXT_START, NEXT_STOP, IDLE};

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
    uint32_t lastDate;
    uint32_t lastTime;
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
    char firmwareVer[FIRMWARE_VER_LEN]= "V0.0.0";
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
  "SendMQTT";

void (* const MyProjectCommand[])(void) PROGMEM = {
  &CmdVals, &CmdSernum, &CmdDate, &CmdStart, &CmdStop, &CmdSimulate,&CmdEbcStatus,&CmdSendMQTT};

void CmdVals(void) {
  AddLog(LOG_LEVEL_INFO, cmd_vals);
  ebcSerial->write(cmd_vals, 5);
  ebcstatus.ebcstate = NEXT_VALS;
  AddLog(LOG_LEVEL_DEBUG, "next state VALS %d", ebcstatus.ebcstate);
  ResponseCmndDone();
}

void CmdDate(void) {
  AddLog(LOG_LEVEL_INFO, cmd_date);
  ebcSerial->write(cmd_date, 5);
  ebcstatus.ebcstate = NEXT_DATE;
  ResponseCmndDone();
}

void CmdStart(void) {
  AddLog(LOG_LEVEL_INFO, cmd_start);
  ebcSerial->write(cmd_start, 5);
  ebcstatus.ebcstate = NEXT_START;
  ResponseCmndDone();
}

void CmdStop(void) {
  AddLog(LOG_LEVEL_INFO, cmd_stop);
  ebcSerial->write(cmd_stop, 5);
  ebcstatus.ebcstate = NEXT_STOP;
  ResponseCmndDone();
}
void CmdSernum(void) {
  AddLog(LOG_LEVEL_INFO, cmd_sernum);
  ebcSerial->write(cmd_sernum, 7);
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
  if(state == true)
    ebcSerial->write(cmd_start);
  else 
    ebcSerial->write(cmd_stop);
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
        case NEXT_SERNUM:
        //#000975 M 120509.03 Set:15 10 25 01 01 +00 02\r",46);
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
            AddLog(LOG_LEVEL_DEBUG_MORE,"START %s", ebc_buffer);
          if(!strcmp(cmd_start, ebc_buffer))
            return; //handle echo
          ebcStartStop(true);
          break;
        case NEXT_STOP:
            AddLog(LOG_LEVEL_DEBUG_MORE,"STOP %s", ebc_buffer);
            if(!strcmp(cmd_stop, ebc_buffer))
              return; //handle echo
            ebcStartStop(false);
            break;
        default:
        break;
        ebcstatus.ebcstate = IDLE;
      }

    ResponseCmndDone();
    // DEBUG_SENSOR_LOG(PSTR("TFmini Plus: crc error"));
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
        WSContentSend_PD("{s}setpoint {m}%d{e}{s}rhCorrection {m}%d{e}{s}Hysteresis {m}%d{e}{s}humidity {m}%d{e}{s}temperature {m}%d{e}{s}Serial number {m}%s{e}{s}FW version{m}%s{e}", ebcstatus.setpoint, ebcstatus.rhCorrection,
ebcstatus.hysteresis, ebcstatus.humidity, ebcstatus.temperature, ebcstatus.serialNumber, ebcstatus.firmwareVer);
        //WSContentSend_PD(PSTR("{s}Firmware version{m}%s {e}"), ebcstatus.firmwareVer);
        //WSContentSend_P(PSTR("{s}P{m}%s {e}"), ebcstatus.firmwareVer);
        //WSContentSend_PD(PSTR("{s}one value{e}"));
      }
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
        /*case FUNC_WEB_APPEND:
          AddLog(LOG_LEVEL_DEBUG,"FUNC_WEB_APPEND called");
					      WSContentSend_PD( PSTR("{s}EBC stato{m}%s{e}"),  ebcstatus.running);
      WSContentSend_PD( PSTR("{s}EBC Versione firmware{m}%s{e}"),  ebcstatus.firmwareVer);
      WSContentSend_PD( PSTR("{s}EBC Numero di serie{m}%s{e}"),  ebcstatus.serialNumber);
        break;*/
        case FUNC_COMMAND:
          result = DecodeCommand(MyProjectCommands, MyProjectCommand);
          if (ebcstatus.simulator) {
             AddLog(LOG_LEVEL_DEBUG, PSTR("Into simulator"));
             AddLog(LOG_LEVEL_DEBUG, PSTR("state %d"), ebcstatus.ebcstate);
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
            }
          }
          break;
        case FUNC_LOOP:
          ebcProcessSerialData();
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