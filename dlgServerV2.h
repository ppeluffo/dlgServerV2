
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <wait.h>
#include <argp.h>
#include <syslog.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <argp.h>
#include <time.h>

#define MAXFD 64

#define SA struct sockaddr
#define LISTENQ 1024
#define MAXCONFIGLINE 255
#define MAXLENPARAMETERS 64
#define MAXAVLRCVDLINE 128

#define MAXLINE 2560
#define MAXSQLCMDSTRING	 2560
#define MAXLOGSTRING	 2560

#define ALARMTIMEOUT	10

#define NODEBUG	0
#define LOWDEBUG	1
#define WARNDEBUG	2
#define FULLDEBUG	3

#define ERROR FALSE
#define OK	  TRUE

/*--------------------------------------------------------------------------*/

MYSQL *conn;
MYSQL_STMT *stmt_main, *stmt_values;

char logBuffer[MAXLOGSTRING];
char alarmMsgBuffer[32];
char txMsgBuffer[32];
char mysqlCmd[MAXSQLCMDSTRING];

struct {
	char fullDate[32];
	int anio;
	int mes;
	int dia;
} sysFecha;

struct {
	char fullHora[32];
	char hh;
	char mm;
	char ss;
} sysHora;

struct {
	int anio;
	int mes;
	int dia;
	char hh;
	char mm;
	char ss;

} dataFechaHora;

time_t sysClock;
struct tm *cur_time;

int fd_log;

#define MAXCHANNELS		20
#define FRAME_FIELDS	(4 + MAXCHANNELS )

typedef struct st_chValues {
	int fieldPos;
	char magName[20];
	char magUnits[10];
	char tbMColValName[20];
	char tbMColMagName[20];
	char tbMColDispName[20];
	float M;
	float B;
	float val;
	float mag;
};

struct  {
	struct st_chValues pList[MAXCHANNELS];
	int nroParametros;
} dlgConfParam;

struct {
	int daemonMode;
	int debugLevel;
	int timerkeepalive;
	int timerdlgoffline;
	char tcp_port[MAXLENPARAMETERS];
	char dbaseName[MAXLENPARAMETERS];
	char dbaseUser[MAXLENPARAMETERS];
	char dbasePassword[MAXLENPARAMETERS];
	char logfileName[MAXLENPARAMETERS];
	char configFile[MAXLENPARAMETERS];
} systemParameters;

struct {
	char dlgRcvdFrame[MAXLINE];
	char dlgId[7];
	char frameType[2];
	char fechaHoraData[32];
	int version;

	char fecha[10];
	char hora[10];
	int valores[MAXCHANNELS];
	float fvalores[MAXCHANNELS];

} st_dlgFrame;

char dlgId[7];
FILE *fdLogFile;
int fd;

/*--------------------------------------------------------------------------*/
/* Prototipos de funciones sp4000 */

void parseConfigLine(char *linea);
void readConfigFile(void);
void printConfigParameters(void);

void bdInit(void);

void daemonize();
void F_openlog(void);
void F_syslog(const char *fmt,...);
ssize_t readline(int fd, void *vptr, size_t maxlen);
void getSystemTime (char *argfecha, char *arghora);

void sendAckFrame(int connfd);
void sendKeepAlive(int connfd);
void checkDlgVersion(void);
void pInitFrame(int connfd);
int readDlgConfig(void);
void pDataFrame(void);
void expandBitFields(void);
void getFechaHoraData( char *fecha, char *hora);
void pAckFrame(void);

/*--------------------------------------------------------------------------*/
