/***************************************************************************
 * Archivo dlgServer_utils.c
 * Autor: Pablo Peluffo @ spymovl SRL 2008
 * Descripcion: Funciones de uso general del programa de comunicaciones de
 * los dataloggers SP4000.
 *
 * Funciones definidas:
 * 	void daemonize( void)
 * 	void F_openlog(void)
 *		void F_syslog(const char *fmt,...)
 *		ssize_t readline(int fd, void *vptr, size_t maxlen)
 * 	void sp4001InitDate(void)
 *		int selectDlg(char *dlgStr)
 *		float analogToValues(char *dlgId, unsigned int channel, int analogValue)
 * 	void getSystemTime (char *argfecha, char *arghora)
 *
 */
/**************************************************************************/

#include "dlgServerV2.h"

/*--------------------------------------------------------------------------*/

void daemonize( void)
{
	int i;
	pid_t pid;

	if ( ( pid = fork()) != 0)
		exit (0);					/* El parent termina */

	/* Continua el primer child  que se debe volver lider de sesion y no tener terminal */
	setsid();

	signal(SIGHUP, SIG_IGN);

	if ( (pid = fork()) != 0)
		exit(0);					/* El primer child termina. Garantiza que el demonio no pueda
									 * adquirir una terminal
									*/
	/* Continua el segundo child */
	chdir("/");
	umask(0);

	for(i=0; i<MAXFD; i++)	/* Cerramos todos los descriptores de archivo que pueda haber heredado */
		close(i);

}

/*--------------------------------------------------------------------------*/

void F_openlog(void)
{
/*	openlog( "/var/log/sp4001.log", LOG_PID, LOG_USER);
	openlog( systemParameters.logfileName, LOG_PID, LOG_USER);
	El primer argumento de openlog es el TAG que se le va a poner a los mensajes.
	Entonces lo configuramos para de ahi poder determinar en que archivo se va a loguear.
	Esto se configura en /etc/rsyslog.conf
	La parte estatica de este string es el programname
*/
	openlog( "dlgServerV2", LOG_PID, LOG_USER);
	F_syslog( "\nStarting dlgServerV2 R1.0...\n");

}
/*--------------------------------------------------------------------------*/

void F_syslog(const char *fmt,...)
{
/* Loguea por medio del syslog la linea de texto con el formato pasado
 * Por ahora solo imprimimos en pantalla
 * Tener en cuenta que en caso de los forks esto puede complicarse por
 * lo cual hay que usar el syslog
 * La funcion la definimos del tipo VARADIC (leer Manual de glibC.) para
 * poder pasar un numero variable de argumentos tal como lo hace printf.
 *
 * Siempre loguea en syslog. Si NO es demonio tambien loguea en stderr
*/
	va_list ap;
	char buf[MAXLINE - 1], printbuf[MAXLINE - 1];
	int i,j;

	va_start(ap, fmt);

	/* Imprimimos en el buffer los datos pasados en argumento con formato
    * Usamos vsnprintf porque es mas segura que vsprintf ya que controla
    * el tamaño del buffer.
    * Agregamos siempre un \n.
  */
	vsnprintf(buf, MAXLINE, fmt, ap);

	/* Elimino los caracteres  \r y \n para no distorsionar el formato. */
	/* Agrego un textStamp para usar en el syslog */
	i=0;
	j=0;
	while ( buf[i] != '\0') {
		if ( (buf[i] == '\n') || (buf[i] == '\r')) {
			i++;
		} else {
			printbuf[j++] = buf[i++];
		}
	}
	printbuf[j++] = '\n';
	printbuf[j++] = '\0';

	/* Si el programa se transformo en demonio uso syslog, sino el dato lo
      manda tambien como stream al stdout
	*/
	syslog((LOG_INFO | LOG_USER), printbuf);

	/* Si el programa NO es demonio, muestro el log por el stdout */
	if (systemParameters.daemonMode == 0) {
		fflush(stdout);
		fputs(printbuf, stdout);
		fflush(stdout);
	}

}
/*--------------------------------------------------------------------------*/

ssize_t readline(int fd, void *vptr, size_t maxlen)
{
/* Usamos las funciones I/O standard UNIX y no las ANSI porque
   no queremos leer en modo buffereado sino poder procesar c/caracter
   en la medida que lo vamos recibiendo.
	No guardo el CR LF
*/
	ssize_t n, rc;
	char c, *ptr;

	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		again:
			if ( ( rc = read(fd, &c, 1)) == 1) {		/* Leo del socket y paso de a un caracter */
				*ptr++ = c;
				if (c == '\n')
					break;			/* Linea completa: almaceno el newline como fgets() */
			} else if (rc == 0) {
				if ( n == 1)
					return(0);		/* EOF, no lei ningun dato */
				else
					break;			/* EOF pero lei datos */
			} else {
				if (errno == EINTR) {
					goto again;
				}
				return (-1);		/* Errro: la valiable errno la seteo la funcion read() */
			}
	}

	*ptr = 0;	/* null terminate como fgets() */
	return(n);
}
/*--------------------------------------------------------------------*/

void getSystemTime (char *argfecha, char *arghora)
{
int anio, mes, dia, hora, mins, secs;
char dd[3], mm[3], yy[3], hh[3], min[3], seg[3];
struct tm *Sys_T = NULL;

    time_t Tval = 0;
    Tval = time(NULL);
    Sys_T = localtime(&Tval);

	anio = Sys_T->tm_year - 100;
	mes = Sys_T->tm_mon + 1;
	dia = Sys_T->tm_mday;
	hora = Sys_T->tm_hour;
	mins = Sys_T->tm_min;
	secs = Sys_T->tm_sec;

	/* Convierto de nuevo a string */
	if (hora > 9) {
		sprintf(hh, "%d", hora);
	} else {
		sprintf(hh, "0%d", hora);
	}

	if (mins > 9) {
		sprintf(min, "%d", mins);
	} else {
		sprintf(min, "0%d", mins);
	}

	if (secs > 9) {
		sprintf(seg, "%d", secs);
	} else {
		sprintf(seg, "0%d", secs);
	}

	if (dia > 9) {
		sprintf(dd, "%d", dia);
	} else {
		sprintf(dd, "0%d", dia);
	}

	if (mes > 9) {
		sprintf(mm, "%d", mes);
	} else {
		sprintf(mm, "0%d", mes);
	}

	if (anio > 9) {
		sprintf(yy, "%d", anio);
	} else {
		sprintf(yy, "0%d", anio);
	}

	sprintf(argfecha,"%s%s%s",yy, mm, dd);
	sprintf(arghora,"%s%s%s",hh, min, seg);
}

/*--------------------------------------------------------------------*/

int readDlgConfig(void)
{

	/* Leo los datos de configuracion de la base de datos, para el datalogger dado.

	mysql> describe tbDlgParserConf;
	+--------------+-------------+------+-----+---------+----------------+
	| Field        | Type        | Null | Key | Default | Extra          |
	+--------------+-------------+------+-----+---------+----------------+
	| id           | bigint(20)  | NO   | PRI | NULL    | auto_increment |
	| dlgId        | varchar(10) | NO   |     | NULL    |                |
	| fieldPos     | int(11)     | NO   |     | NULL    |                |
	| magName      | varchar(20) | YES  |     | NULL    |                |
    | magUnits     | varchar(10) | YES  |     |         |                |
	| tbMCol       | int(11)     | NO   |     | NULL    |                |
	| M            | float(6,3)  | NO   |     | NULL    |                |
	| B            | float(6,3)  | NO   |     | NULL    |                |
	| tipo         | varchar(10) | NO   |     | NULL    |                |
	| disp         | int(11)     | NO   |     | 1       |                |
	+--------------+-------------+------+-----+---------+----------------+

	*/

	MYSQL_RES *res_set;
	MYSQL_ROW row;
	int i;
	int fieldPos;
	char strName[20];
	char magName[20], magUnits[10];
	int tbMCol;
	float M,B;

	/* Consula SQL
	 * No leo la disponibilidad ya que este parametro lo leo en cada INSERT
	 */
	bzero(mysqlCmd, sizeof(mysqlCmd));
	sprintf(mysqlCmd,"SELECT fieldPos, magName, magUnits, tbMCol, M, B FROM tbDlgParserConf WHERE dlgId LIKE \"%s\" ORDER BY fieldPos;", st_dlgFrame.dlgId);
	if (mysql_query (conn, mysqlCmd) != 0) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf (logBuffer, "[%s] ERROR_006: No puedo hacer el query para leer la configuracion. %s", st_dlgFrame.dlgId, mysql_stmt_error(stmt_main) );
		F_syslog("%s", logBuffer);
		bzero(logBuffer, sizeof(logBuffer));
		sprintf (logBuffer, "[%s] ERROR_006: [STMT=%s].No puedo continuar.", st_dlgFrame.dlgId, mysqlCmd );
		F_syslog("%s", logBuffer);
		exit (1);
	}

	res_set = mysql_store_result(conn);
	if (systemParameters.debugLevel > WARNDEBUG ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "[%s] BD Lectura de configuracion.. OK.\n", st_dlgFrame.dlgId);
		F_syslog("%s", logBuffer);
	}

	/* Inicializo la estructura de configuracion */
	memset (&dlgConfParam, '\0', sizeof(dlgConfParam));

	/* Leo los parametros */
	i=0;
	while ( (row = mysql_fetch_row(res_set)) != NULL )  {

		// Valores leidos
		fieldPos = atoi(row[0]);		// Posicion dentro del frame
		strcpy(magName, row[1]);		// Nombre de la magnitud
		strcpy(magUnits, row[2]);		// Unidades
		tbMCol = atoi(row[3]);			// Columna en las tablas tbMainX donde va ( valX,magX,dispX)
		M = atof(row[4]);				// Pendiente
		B = atof(row[5]);				// Offset

		// Cada elemento de la lista contiene todos los parametros de c/magnitud
		dlgConfParam.pList[i].fieldPos = fieldPos;
		strcpy(dlgConfParam.pList[i].magName, magName);
		strcpy(dlgConfParam.pList[i].magUnits, magUnits);

		bzero(strName, sizeof(strName));
		sprintf( strName,"val%d",tbMCol );
		strcpy(dlgConfParam.pList[i].tbMColValName, strName);

		bzero(strName, sizeof(strName));
		sprintf( strName,"mag%d",tbMCol );
		strcpy(dlgConfParam.pList[i].tbMColMagName, strName);

		bzero(strName, sizeof(strName));
		sprintf( strName,"disp%d",tbMCol );
		strcpy(dlgConfParam.pList[i].tbMColDispName, strName);

		dlgConfParam.pList[i].M = M;
		dlgConfParam.pList[i].B = B;
		i++;
	}

	dlgConfParam.nroParametros = i;

	/* log ?? */
	if (systemParameters.debugLevel > WARNDEBUG ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "-------------------------------------------------------------------------------------------------------------\n");
		F_syslog("%s", logBuffer);
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "   |   DLG   | FPos|     magName     |  magUnits  | tbMColVal | tbMColMag | tbMColDisp |    M     |    B    |\n");
		F_syslog("%s", logBuffer);
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "---|---------|-----|-----------------|------------|-----------|-----------|------------|----------|---------\n");
		F_syslog("%s", logBuffer);

		for (i=0; i<dlgConfParam.nroParametros; i++) {
			sprintf(logBuffer,"%02d%8s%7d%20s%10s%12s%12s%12s %7.3f %10.3f\n", i, st_dlgFrame.dlgId, dlgConfParam.pList[i].fieldPos, dlgConfParam.pList[i].magName, dlgConfParam.pList[i].magUnits, dlgConfParam.pList[i].tbMColValName, dlgConfParam.pList[i].tbMColMagName,dlgConfParam.pList[i].tbMColDispName, dlgConfParam.pList[i].M, dlgConfParam.pList[i].B);
			F_syslog("%s", logBuffer);
			// %12s   , dlgConfParam.pList[i].tbMColDispName, dlgConfParam.pList[i].M, dlgConfParam.pList[i].B
			//sprintf(logBuffer,"%20s%10s",dlgConfParam.pList[i].magName, dlgConfParam.pList[i].magUnits);
			//sprintf(logBuffer,"%12s%12s%12s", dlgConfParam.pList[i].tbMColValName, dlgConfParam.pList[i].tbMColMagName, dlgConfParam.pList[i].tbMColDispName );
			//sprintf(logBuffer,"\t%7.3f\t%10.3f\n",dlgConfParam.pList[i].M, dlgConfParam.pList[i].B);
		}
	}
	return(OK);
}

/*--------------------------------------------------------------------*/

void getFechaHoraData( char *fecha, char *hora)
{

/* Convierte la fecha y hora del frame a un formato apto para la BD */

char dd[3], mm[3], yy[3];
char hh[3], min[3], sec[3];

    /* dataFecha */
    memcpy(yy, &fecha[0], 2); yy[2] = '\0';
    memcpy(mm, &fecha[2],2); mm[2] = '\0';
    memcpy(dd, &fecha[4],2); dd[2] = '\0';

    /* dataHora */
    memcpy(hh, &hora[0],2); hh[2] = '\0';
    memcpy(min, &hora[2],2); min[2] = '\0';
    memcpy(sec, &hora[4],2); sec[2] = '\0';

    sprintf(st_dlgFrame.fechaHoraData, "%4d-%02d-%02d %02d:%02d:%02d ", (2000+atoi(yy)), atoi(mm), atoi(dd) , atoi(hh), atoi(min), atoi(sec));

//	printf("%s/%s/%s %s:%s:%s\n",yy,mm,dd,hh,min,sec);
//	printf("FECHA=%s, HORA=%s, FULL=%s\n", fecha, hora, st_dlgFrame.fechaHoraData);

}

/*--------------------------------------------------------------------*/
