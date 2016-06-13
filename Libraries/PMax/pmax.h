#pragma once
#include <stdio.h>
#include "FixedSizeQueue.h"
#include "MemoryMap.h"

#define  MAX_BUFFER_SIZE 250
#define  MAX_SEND_BUFFER_SIZE 15
#define  MAX_SEND_QUEUE_DEPTH 15
#define  PACKET_TIMEOUT_DEFINED 2000
#define  MAX_ZONE_COUNT 31

//This Pin is a temporary pin for powerlink (this app). Does not have to match any of user or installer codes.
//If your pin is 1234, you need to return 0x1234 (this is strange, as 0x makes it hex, but the only way it works).
#define POWERLINK_PIN 0x3622;

class PowerMaxAlarm;

enum PmaxCommand
{
    Pmax_ACK,
    Pmax_PING,
    Pmax_GETEVENTLOG,
    Pmax_DISARM,
    Pmax_ARMHOME,
    Pmax_ARMAWAY,
    Pmax_ARMAWAY_INSTANT,
    Pmax_REQSTATUS,
    Pmax_ENROLLREPLY,
    Pmax_INIT,
    Pmax_RESTORE,
    Pmax_DL_START,
    Pmax_DL_GET,
    Pmax_DL_EXIT, //stop download mode
    Pmax_DL_PANELFW,
    Pmax_DL_SERIAL,
    Pmax_DL_ZONESTR
};

enum ZoneEvent
{
    ZE_None,
    ZE_TamperAlarm,
    ZE_TamperRestore,
    ZE_Open,
    ZE_Closed,
    ZE_Violated,
    ZE_PanicAlarm,
    ZE_RFJamming,
    ZE_TamperOpen,
    ZE_CommunicationFailure,
    ZE_LineFailure,
    ZE_Fuse,
    ZE_NotActive,
    ZE_LowBattery,
    ZE_ACFailure,
    ZE_FireAlarm,
    ZE_Emergency,
    ZE_SirenTamper,
    ZE_SirenTamperRestore,
    ZE_SirenLowBattery,
    ZE_SirenACFail
};

enum SystemStatus
{
    SS_Disarm = 0x00,
    SS_Exit_Delay = 0x01,
    SS_Exit_Delay2 = 0x02,
    SS_Entry_Delay = 0x03,
    SS_Armed_Home = 0x04,
    SS_Armed_Away = 0x05,
    SS_User_Test = 0x06,
    SS_Downloading = 0x07,
    SS_Programming = 0x08,
    SS_Installer = 0x09,
    SS_Home_Bypass = 0x0A,
    SS_Away_Bypass = 0x0B,
    SS_Ready = 0x0C,
    SS_Not_Ready = 0x0D
};

enum PmAckType
{
    ACK_1,
    ACK_2
};

//this abstract class is used by DumpToJson API
//it allows to redirect JSON to file, console, www output
class IOutput
{
public:
    virtual void write(const char* str) = 0;
    
    void writeQuotedStr(const char* str);
    void writeJsonTag(const char* name, bool value, bool addComma = true);
    void writeJsonTag(const char* name, int value, bool addComma = true);
    void writeJsonTag(const char* name, const char* value, bool addComma = true, bool quoteValue = true);
};

class ConsoleOutput : public IOutput
{
public:
    void write(const char* str);
};

struct PlinkCommand {
    unsigned char buffer[MAX_SEND_BUFFER_SIZE];
    int size;
    const char* description;
    void (*action)(PowerMaxAlarm* pm, const struct PlinkBuffer *);
};

struct PlinkBuffer {
    unsigned char buffer[MAX_BUFFER_SIZE];
    int size;
};

struct ZoneState {
    bool lowBattery; //battery needs replacing
    bool tamper;     //someone tampered with the device
    bool doorOpen;   //door is open (either intrusion or not redy to arm)
    bool bypased;    //user temporarly disabled this zone
    bool active;     //commication with one is OK
};

struct Zone {
    bool enrolled;       //PowerMax knows about this zone (it's configured)
    char name[0x11];     //Will be dowloaded from PowerMax eprom
    unsigned char zonetype;
    unsigned char sensorid;
    const char* sensortype;
    const char* autocreate;

    ZoneState stat;      //basic state of the zone

    ZoneEvent lastEvent; //last event recodred for this zone
    unsigned long lastEventTime;

    void DumpToJson(IOutput* outputStream);
};

struct PmQueueItem
{
    unsigned char buffer[MAX_SEND_BUFFER_SIZE];
    int bufferLen;

    const char* description;
    unsigned char expectedRepply;
    const char* options;
};

struct PmConfig
{
    bool parsedOK;

    char installerPin[5];
    char masterInstallerPin[5];
    char powerLinkPin[5];
    char userPins[48][5];

    //telephone numbers to call:
    char phone[4][15]; //15: max 14 digits + NULL

    char serialNumber[15];
    char eprom[17];
    char software[17];

    unsigned char partitionCnt;

    //panel max capabilities (not actual count used):
    unsigned char maxZoneCnt;
    unsigned char maxCustomCnt;
    unsigned char maxUserCnt;
    unsigned char maxPartitionCnt;
    unsigned char maxSirenCnt;
    unsigned char maxKeypad1Cnt;
    unsigned char maxKeypad2Cnt;
    unsigned char maxKeyfobCnt;

    PmConfig()
    {
        Init();
    }

    void Init()
    {
        memset(this, 0, sizeof(PmConfig));
    }

    void DumpToJson(IOutput* outputStream);
    int GetMasterPinAsHex() const;
};

class PowerMaxAlarm
{
    //Flags with[*] are one-shot notifications of last event
    //For example when user arms away - bit 6 will be set
    //When system will deliver 'last 10 sec' notification - bit 6 will be cleared

    //bit 0: Ready if set
    //bit 1: [?] Alert in Memory if set
    //bit 2: [?] Trouble if set
    //bit 3: [?] Bypass On if set
    //bit 4: [*] Last 10 seconds of entry or exit delay if set
    //bit 5: [*] Zone event if set 
    //bit 6: [*] Arm, disarm event 
    //bit 7: [?] Alarm event if set
    unsigned char flags;

    //overall system status
    SystemStatus stat;

    //status of all zones (0 is not used, system)
    Zone zone[MAX_ZONE_COUNT];

    //config downloaded from PM, parsed using ProcessSettings
    PmConfig m_cfg;

    //used to detect when PowerMax stops talking to us, that will trigger re-establish comms message
    time_t lastIoTime;

    FixedSizeQueue<PmQueueItem, MAX_SEND_QUEUE_DEPTH> m_sendQueue;

    bool m_bEnrolCompleted;
    bool m_bDownloadMode;
    int m_iPanelType;
    int m_iModelType;
    bool m_bPowerMaster;
    PmAckType m_ackTypeForLastMsg;

    //used to store data downloaded from PM
    MemoryMap m_mapMain;
    MemoryMap m_mapExtended;

    PlinkCommand m_lastSentCommand;
    unsigned long m_ulLastPing;
public:

#ifdef _MSC_VER
    void IZIZTODO_testMap();
#endif

    void Init();
    void SendNextCommand();
    void clearQueue(){ m_sendQueue.clear(); }
    
    bool sendCommand(PmaxCommand cmd);
    void handlePacket(PlinkBuffer  * commandBuffer);
   
    static bool isBufferOK(const PlinkBuffer* commandBuffer);
    const char* getZoneName(unsigned char zoneId);

    unsigned int getEnrolledZoneCnt() const;
    unsigned long getSecondsFromLastComm() const;
    void DumpToJson(IOutput* outputStream);

private:
    void addPin(unsigned char* bufferToSend, int pos = 4, bool useMasterCode = false);

    bool isFlagSet(unsigned char id) const { return (flags & 1<<id) != 0; }
    bool isAlarmEvent() const{  return isFlagSet(7); }
    bool isZoneEvent()  const{  return isFlagSet(5); }

    void Format_SystemStatus(char* tpbuff, int buffSize);

    //buffer is coppied, description and options need to be in pernament addressess (not something from stack)
    bool QueueCommand(const unsigned char* buffer, int bufferLen, const char* description, unsigned char expectedRepply = 0x00, const char* options = NULL);
    void PowerLinkEnrolled();
    void ProcessSettings();

    int ReadMemoryMap(const unsigned char* msg, unsigned char* buffOut, int buffOutSize);
    void WriteMemoryMap(int iPage, int iIndex, const unsigned char* sData, int sDataLen);

    bool sendBuffer(const unsigned char * data, int bufferSize);
    void sendBuffer(struct PlinkBuffer * Buff);
    static PmAckType calculateAckType(const unsigned char* deformattedBuffer, int bufferLen);

    void StartKeepAliveTimer();
    void StopKeepAliveTimer();

public:
    static void PmaxStatusUpdateZoneBat(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxStatusUpdatePanel(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxStatusUpdateZoneBypassed(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxStatusUpdateZoneTamper(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxStatusChange(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxStatusUpdate(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxEventLog(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxAccessDenied(PowerMaxAlarm* pm, const PlinkBuffer  * Bufff);
    static void PmaxAck(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxTimeOut(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxStop(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxEnroll(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxPing(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxPanelInfo(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxDownloadInfo(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
    static void PmaxDownloadSettings(PowerMaxAlarm* pm, const PlinkBuffer  * Buff);
};

/* This section specifies OS specific functions. */
/* Implementation for Windows (MSVS) is provided */
/* If you compile for other OS provide your own. */
#ifndef LOG_INFO
#define	LOG_EMERG	0	/* system is unusable */
#define	LOG_ALERT	1	/* action must be taken immediately */
#define	LOG_CRIT	2	/* critical conditions */
#define	LOG_ERR		3	/* error conditions */
#define	LOG_WARNING	4	/* warning conditions */
#define	LOG_NOTICE	5	/* normal but significant condition */
#define	LOG_INFO	6	/* informational */
#define	LOG_DEBUG	7	/* debug-level messages */
#endif
#define	LOG_NO_FILTER 0 /* message always outputed */
#define DEBUG(x,...) os_debugLog(x, false,__FUNCTION__,__LINE__,__VA_ARGS__);
#define DEBUG_RAW(x,...) os_debugLog(x, true,__FUNCTION__,__LINE__,__VA_ARGS__);
int log_console_setlogmask(int mask);

bool os_serialPortInit(const char* portName);
int  os_serialPortRead(void* writePos, int bytesToRead);
int  os_serialPortWrite(const void* dataToWrite, int bytesToWrite);
bool os_serialPortClose();
void os_usleep(int microseconds);

int os_cfg_getPacketTimeout();

void os_debugLog(int priority, bool raw, const char *function, int line,const char *format, ...);
void os_strncat_s(char* dst, int dst_size, const char* src);

unsigned long os_getCurrentTimeSec();