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
const char cmd_setpoint[] PROGMEM ="#setPoint";

enum EBC_MODEL { EBC_10_11_12_MASTER, EBC_10_11_12_SLAVE, EBCEASY_MASTER, EBCEASY_SLAVE };
enum EBC_STATE { NEXT_SERNUM, NEXT_VALS, NEXT_DATE, NEXT_TIME, NEXT_DUMP, IDLE};

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
    uint8_t setpointHumidityH;
    uint8_t setpointHumidityL;
    uint8_t alarmdelay;
    int8_t  rhCorrection;
    uint8_t hysteresis;
    uint8_t humidity;
    uint8_t temperature;
    int8_t  t_cond;
    int8_t  t_cool;
    char serialNumber[SERIAL_NUMBER_LEN];
    char firmwareVer[FIRMWARE_VER_LEN];
    enum EBC_STATE ebcstate;
    const char *model;
} ebcstatus;

/* 
  Optional: if you need to pass any command for your device 
  Commands are issued in Console or Web Console
  Commands:


  comando 1
  comando
*/

const char MyProjectCommands[] PROGMEM = "|"  // No Prefix
  "vals|"
  "sernum|"
  "date|"
  "ebcsimulate|"
  "SendMQTT";

void (* const MyProjectCommand[])(void) PROGMEM = {
  &CmdVals, &CmdSernum, &CmdDate, &CmdSendMQTT};

void CmdVals(void) {
  AddLog(LOG_LEVEL_INFO, cmd_vals);
  ebcSerial->write(cmd_vals, 5);
  ebcstatus.ebcstate = NEXT_VALS;
  ResponseCmndDone();
}

void CmdDate(void) {
  AddLog(LOG_LEVEL_INFO, cmd_date);
  ebcSerial->write(cmd_date, 5);
  ebcstatus.ebcstate = NEXT_DUMP;
  ResponseCmndDone();
}

void CmdSernum(void) {
  AddLog(LOG_LEVEL_INFO, cmd_sernum);
  ebcSerial->write(cmd_sernum, 7);
  ebcstatus.ebcstate = NEXT_SERNUM;
  ResponseCmndDone();
}

void CmdSimulate(void) {
AddLog(LOG_LEVEL_INFO, "Toggle EBC internal simulator");
  ebcstatus.simulator != ebcstatus.simulator;
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

void ebcProcessData(void) {
//////////todo
 
    AddLog(LOG_LEVEL_DEBUG,PSTR("Line from serial: %s"),ebc_buffer);
    switch (ebcstatus.ebcstate) {
        case NEXT_VALS:
          AddLog(LOG_LEVEL_DEBUG,"VALS %s", ebc_buffer);
          ebcstatus.temperature = TextToInt(ebc_buffer);
          break;
        case NEXT_SERNUM:
          AddLog(LOG_LEVEL_DEBUG,"SERNUM %s", ebc_buffer);
          break;
        case NEXT_DATE:
          AddLog(LOG_LEVEL_DEBUG,"DATE %s", ebc_buffer);
          break;
        default:
        break;
    }
    ebcstatus.ebcstate = IDLE;

    ResponseCmndDone();
    // DEBUG_SENSOR_LOG(PSTR("TFmini Plus: crc error"));
}

void ebcInit(void)
{
    ebcstatus.inited = false;
    AddLog(LOG_LEVEL_DEBUG, "EBC Miniclima init...");
     // Software serial init needs to be done here as earlier (serial) interrupts may lead to Exceptions
    ebcSerial = new TasmotaSerial(Pin(GPIO_EBC_RX), Pin(GPIO_EBC_TX), 1);
    if (ebcSerial->begin(4800, SERIAL_8N1)) {
      AddLog(LOG_LEVEL_DEBUG, "serial started");
        /*if (ebcSerial->hardwareSerial()) {
        ClaimSerial();
        }*/
        ebcstatus.inited = true;
        ebcstatus.simulator = false;
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


bool Xdrv100(uint32_t function) {
  bool result = false;

  if (FUNC_INIT == function) {
    ebcInit();
    } else if (ebcstatus.inited) {
      switch (function) {
        case FUNC_EVERY_SECOND:
          if(ebcstatus.inited) {
             WSContentSend_Temp("test", ebcstatus.temperature );
             

          }
          if (ebcstatus.simulator) {
            switch (ebcstatus.ebcstate) {
              case NEXT_VALS:
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
        case FUNC_COMMAND:
          result = DecodeCommand(MyProjectCommands, MyProjectCommand);
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