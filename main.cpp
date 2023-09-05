#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

int zTravel=100;
int xTravel=-5000;
int yTravel=-5000;


int readline(HANDLE hConn, char buf[256], size_t maxlen);

void sendCommand(HANDLE hComm, const char *cmd);

void expectResponse(HANDLE hComm, const char *expect);

void sendCancel(HANDLE handle);

void reset(HANDLE hCOmm);

void getStatus(HANDLE hComm, char *buf, int len);

void homeAxis(HANDLE hComm, const char *axis, int travel, int feed);

DWORD writeToSerialPort(HANDLE hComm, char * data, int length)
{

    DWORD dwBytesRead = 0;
    if(!WriteFile(hComm, data, length, &dwBytesRead, NULL)){
        printf("Invalid i/o: %d,\r\n", GetLastError());
    }
    return dwBytesRead;

}

DWORD readFromSerialPort(HANDLE hComm, char * buffer, int buffersize)
{
    DWORD dwBytesRead = 0;
    if(!ReadFile(hComm, buffer, buffersize, &dwBytesRead, NULL)){
        //handle error
    }
    return dwBytesRead;
}

//const char * EOL = "\r";

int main(int argc, char **argv) {
    char cmd[2048];
    HANDLE hComm;
    DCB dcbSerialParams = {0};
    COMMTIMEOUTS timeouts = {0};

    char buf[1024];

    char portname[32];
    sprintf(portname, R"(\\.\%s)",argv[1]);

//    FILE* com = fopen(portname,"rw");
//
//    if (!com) {
//        fprintf(stderr,"cannot open %s",portname);
//        return 9;
//    }
//

//    printf("sending $$\n"); fflush(stdout);
//    fprintf(com,"$$\n");
//    printf("reading...\n"); fflush(stdout);
//    fgets(buf,sizeof(buf),com);


    hComm = CreateFile(portname,
                       GENERIC_READ | GENERIC_WRITE,
                       0,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,//FILE_FLAG_OVERLAPPED,
                       NULL);


    if (hComm == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Invalid handle: %d,\r\n", GetLastError());
    } else {
        fprintf(stderr, "port opened\n");
    }



    // set device parameters (38400 baud, 1 start bit,
    // 1 stop bit, no parity)
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (GetCommState(hComm, &dcbSerialParams) == 0) {
        fprintf(stderr, "Error getting device state\n");
        CloseHandle(hComm);
        return 1;
    }

    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    if (SetCommState(hComm, &dcbSerialParams) == 0) {
        fprintf(stderr, "Error setting device parameters\n");
        CloseHandle(hComm);
        return 1;
    }

    // Set COM port timeout settings
//    timeouts.ReadIntervalTimeout = 500;
//    timeouts.ReadTotalTimeoutConstant = 500;
//    timeouts.ReadTotalTimeoutMultiplier = 10;
//    timeouts.WriteTotalTimeoutConstant = 500;
//    timeouts.WriteTotalTimeoutMultiplier = 10;
//    if(SetCommTimeouts(hComm, &timeouts) == 0)
//    {
//        fprintf(stderr, "Error setting timeouts\n");
//        CloseHandle(hComm);
//        return 1;
//    }



    reset(hComm);

    homeAxis(hComm, "Z", zTravel,2000);
    homeAxis(hComm, "Z", zTravel,30);

    homeAxis(hComm, "X", xTravel,2000);
    homeAxis(hComm, "X", xTravel,30);

    homeAxis(hComm, "Y", yTravel,2000);
    homeAxis(hComm, "Y", yTravel,30);

    sendCommand(hComm,"$21=1");
    return(0);


}

void homeAxis(HANDLE hComm, const char *axis, int travel, int feed) {
    char cmd[1024];
    char buf[8192];

    int dir=(travel>0)?1:-1;

    sendCommand(hComm, "$21=1");
    expectResponse(hComm, "ok");

    sprintf(cmd, "$J=G21G91%s%dF%d", axis, travel, feed);
    sendCommand(hComm, cmd);
    expectResponse(hComm, "ok");

    fprintf(stderr, "checking for limit\n");
    expectResponse(hComm, "ALARM:1");
    expectResponse(hComm, "[MSG:Reset to continue]");

    reset(hComm);

    sendCommand(hComm, "$21=0");
    expectResponse(hComm, "ok");
    sprintf(cmd, "$J=G21G91%s%dF2000", axis, -1*dir*3);
    sendCommand(hComm, cmd);
    expectResponse(hComm, "ok");

    sendCommand(hComm, "?");
    expectResponse(hComm, "ok");

    *buf = 0;
    while (strstr(buf, "Idle") == 0) {
        getStatus(hComm, buf, sizeof(buf));
        fprintf(stderr,"%s\n", buf);
        usleep(200*1000);
    }



}

void getStatus(HANDLE hComm, char *buf, int len) {
    sendCommand(hComm, "?");
    readline(hComm,buf,len);
    expectResponse(hComm,"ok");
}

void reset(HANDLE hComm) {
    sendCancel(hComm);
    expectResponse(hComm,"");
    sendCommand(hComm,"$X");
    expectResponse(hComm, "ok");
}

void sendCancel(HANDLE hComm) {
    char buf[8];
    memset(buf,0,sizeof(buf));
    sprintf(buf,"%c",24);

    fprintf(stderr,"Sending cancel...\n");
    writeToSerialPort(hComm,buf,1);
    FlushFileBuffers(hComm);
}

void sendCommand(HANDLE hComm, const char *cmd) {
    char buf[16384];

    int len=strlen(cmd);
    memset(buf,0,sizeof(buf));
    sprintf(buf,"%s\r",cmd);
    fprintf(stderr, "Command: %s\n", buf);

    writeToSerialPort(hComm,buf,len+1);
    FlushFileBuffers(hComm);


}

void expectResponse(HANDLE hComm, const char *expect) {
    char buf[16384];

    do  {
        int rs=readline(hComm, buf, sizeof(buf));
        fprintf(stderr,"%s\n", buf);
    } while (strcmp(buf,expect));
}

int readline(HANDLE hComm, char buf[256], size_t maxlen) {
    char tmpstr[8];
    int bytesRead=0;

    memset(buf,0,maxlen);
    while (bytesRead<(maxlen - 1)) {
        auto rd=readFromSerialPort(hComm, tmpstr, 1);
        if (rd==0) continue;
        if (*tmpstr=='\r') {
            continue;
        }
        if (*tmpstr=='\n') {
            break;
        }

        buf[bytesRead]=*tmpstr;
        ++bytesRead;
    }
    return bytesRead;
}
