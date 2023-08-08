/*
  xdrv_100_ebcminiclima.ino EBC10/12 support
*/

#define USE_EBCMINICLIMA
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
#define TFMP_MAX_DATA_LEN 128

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
const char cmd_start[] PROGMEM ="#setPoint";

enum EBC_MODEL = { EBC_10_11_12_MASTER, EBC_10_11_12_SLAVE, EBCEASY_MASTER, EBCEASY_SLAVE };

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
#define EBC1X_PUMP_OUT          0x20
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

char ebc_buffer[TFMP_MAX_DATA_LEN + 1];

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

struct ebcStatus ebcstatus = {
    uint32_t lastDate;
    uint32_t lastTime;
    bool running;
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
    const char *model;

}

/* 
  Optional: if you need to pass any command for your device 
  Commands are issued in Console or Web Console
  Commands:


  comando 1
  comando
*/

const char MyProjectCommands[] PROGMEM = "|"  // No Prefix
  "vars|" 
  "sernum|"
  "date|"
  "SendMQTT";

void (* const MyProjectCommand[])(void) PROGMEM = {
  &CmdVars, &CmdDate, &CmdSernum, &CmdSendMQTT};

void CmdVars(void) {
  AddLog(LOG_LEVEL_INFO, PSTR("vars"));
  ebcSerial.write(cmd_vars, 5);
  ResponseCmndDone();
}

void CmdDate(void) {
  AddLog(LOG_LEVEL_INFO, PSTR("date"));
  ebcSerial.write(cmd_date, 5);
  ResponseCmndDone();
}

void CmdSernum(void) {
  AddLog(LOG_LEVEL_INFO, PSTR("sernum"));
  ebcSerial.write(cmd_sernum, 7);
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


  AddLog(LOG_LEVEL_INFO, PSTR("Help: Accepted commands - Say_Hello, SendMQTT, Help"));
  ResponseCmndDone();
}

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
    if (0x0D == nextChar)
        return true;
    if (0x0A == nextChar)
        return false;
    Tfmp_buffer[currentIndex] = nextChar;
    currentIndex++;
    // Check for too many data
    if (currentIndex > TFMP_MAX_DATA_LEN)
    {
        // Terminate buffer and reset position
        Tfmp_buffer[TFMP_MAX_DATA_LEN] = '\0';
        currentIndex = 0;
        return true;
    }
    return false;
}

void ebcProcessData(void) {
    // check crc sum
    uint16_t crc = 0;
    if ( '\0' != ebc_buffer[EBC_MAX_DATA_LEN] )
        return;
//////////todo 
    for (int i = 0; i < TFMP_MAX_DATA_LEN - 1; ++i) {
        crc += (uint16_t)Tfmp_buffer[i];
    }
    /*if (!strcmp(ebc_buffer, cmd_vals)) {
         AddLog(LOG_LEVEL_INFO, ebc_buffer);
    }*/
    
    // DEBUG_SENSOR_LOG(PSTR("TFmini Plus: crc error"));
}

static const char *
miel_hvac_map_byval__(uint8_t byte,
    const struct miel_hvac_map *m, size_t n)
{
	const struct miel_hvac_map *e;
	size_t i;

	for (i = 0; i < n; i++) {
		e = &m[i];
		if (byte == e->byte)
			return (e->name);
	}

	return (NULL);
}
void EBCInit(void)
{
    ebcstatus.inited = false;
    AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("EBC Miniclima init..."));
     // Software serial init needs to be done here as earlier (serial) interrupts may lead to Exceptions
    ebcSerial = new TasmotaSerial(Pin(GPIO_EBC_RX)), Pin(GPIO_EBC_TX)), 1);
    if (ebcSerial->begin(9600, SERIAL_8N1)) {
        if (ebcSerial->hardwareSerial()) {
        ClaimSerial();
        }
        ebstatus.inited = true;
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
          #ifdef WEB_SERVER
            
          #endif
          AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("Every second"));
          break;
        case FUNC_COMMAND:
          AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("Calling Command..."));
          result = DecodeCommand(MyProjectCommands, MyProjectCommand);
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


#endif  // USE_EBCMINICLIMA