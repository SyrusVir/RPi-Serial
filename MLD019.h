//#include <pigpio.h>

typedef enum mld_mode {
    MLD_HARDWARE,
    MLD_SOFTWARE
} mld_mode_t;

typedef struct MldDriver {
    mld_mode_t mode;
    int serial_handle;

} mld_t;

typedef struct MldMessage {
    char checksum;  //LSB
    char datum3;
    char datum2;
    char datum1;
    char header;    //MSB
} mld_msg_t;

typedef union MldMessageUnion
{
    mld_msg_t msg_struct;
    char msg_arr[5];
    long long msg_hex: 40;
} mld_msg_u;


//Sending/Receiving commands
int mldSendMsg(mld_t mld, mld_msg_u msg);
mld_msg_u mldRecvMsg(mld_t mld);

//Converting messages to/from string
char* mldMsgToString(char* buff, mld_msg_u msg);    //Convert hex number to MLD command string
mld_msg_u mldStringToMsg(char* str);

/**
mldLinkControl

mldReadRTC

mldCaseTemp

mldVLD

mldStatus

mldBoardTemp

mldDigitalInputMonitor

mldSerialNum

mldHWSWMode

mldCheckConfig

mldTrigSource

mldSetInterlock

mldSetEnable

mldSetPRR
**/
