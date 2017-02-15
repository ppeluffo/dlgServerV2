/***************************************************************************
 * Archivo dlgServer_config.c
 *
 * Autor: Pablo Peluffo @ spymovl SRL 2012
 * Descripcion: Modulo de configuracion del programa de comunicaciones de
 * los dataloggers.
 *
 * Rutinas de lectura del archivo de configuracion del programa
 * El archivo de configuracion por defecto se llama
 * "/etc/dlg2012Server.conf"
 *
 * TCP_PORT=2601
 * DBASENAME=dlgDb
 * DBASEUSER=root
 * DBASEPASS=spymovil
 * DEBUG=1
 * DAEMONMODE=1
 * TIMERDLGOFFLINE=180
 * TIMERKEEPALIVE=60
 *
 */
/**************************************************************************/

#include "dlgServerV2.h"

/*--------------------------------------------------------------------------*/
void readConfigFile(void)
{
/* Lee el archivo de configuracion (default o pasado como argumento del programa
   Extraemos todos los parametros operativos y los dejamos en una estructura de
   configuracion.
   Si DEBUG=1 hacemos un printf al archivo de log de todos los parametros
*/
	FILE *configFile_fd;
	char linea[MAXCONFIGLINE];

	if (  (configFile_fd = fopen(systemParameters.configFile, "r" ) ) == NULL ) {
		F_syslog("ERROR_001: No es posible abrir el archivo de configuracion");
		exit(1);
	}

	bzero(&systemParameters, sizeof(systemParameters));

	/* Valores por defecto */
	systemParameters.daemonMode = 0;
	systemParameters.timerdlgoffline = 120;
	systemParameters.timerkeepalive = 120;
	strcpy(systemParameters.dbaseName, "dlgDb");
	strcpy(systemParameters.dbaseUser, "root");
	strcpy(systemParameters.dbasePassword, "spymovil");
	strcpy(systemParameters.logfileName, "/var/log/dlgServerV2.log");

	/* Leo el archivo de configuracion para reconfigurarme */
	while ( fgets( linea, MAXCONFIGLINE, configFile_fd)  != NULL ) {
		parseConfigLine(linea);
	}

}

/*--------------------------------------------------------------------------*/
void parseConfigLine(char *linea)
{
/* Tenemos una linea del archivo de configuracion.
 * Si es comentario ( comienza con #) la eliminamos
 * Si el primer caracter es CR, LF, " " la eliminamos.
*/
	char *index;
	int largo;

	if (linea[0] == '#') return;

	/* Busco los patrones de configuracion y los extraigo */

	/* Puerto TCP donde escucha el server las conexiones entrantes de los dataloggers */
	if ( strstr ( linea, "TCP_PORT=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			bzero(systemParameters.tcp_port, strlen(systemParameters.tcp_port));
			strncpy(systemParameters.tcp_port, index, (largo));
		}
		return;
	}

	/* Nombre del archivo de log: (debe estar configurado el syslog_ng */
	if ( strstr ( linea, "LOGFILENAME=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			bzero(systemParameters.logfileName, strlen(systemParameters.logfileName));
			strncpy(systemParameters.logfileName, index, (largo));
		}
		return;
	}

	/* Nombre de la base de datos mysql */
	if ( strstr ( linea, "DBASENAME=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			bzero(systemParameters.dbaseName, strlen(systemParameters.dbaseName));
			strncpy(systemParameters.dbaseName, index, (largo));
		}
		return;
	}

	/* Usuario de la base de datos */
	if ( strstr ( linea, "DBASEUSER=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			bzero(systemParameters.dbaseUser, strlen(systemParameters.dbaseUser));
			strncpy(systemParameters.dbaseUser, index, (largo));
		}
		return;
	}

	/* contraseña de la base de datos */
	if ( strstr ( linea, "DBASEPASS=") != NULL ) {
		index = strchr(linea, '=') + 1;
		if (strchr(linea, ';') != NULL ) {
			largo = strchr(linea, ';') - index;
			bzero(systemParameters.dbasePassword, strlen(systemParameters.dbasePassword));
			strncpy(systemParameters.dbasePassword, index, (largo));
		}
		return;
	}

	/* Si trabaja en modo demonio o consola */
	if ( strstr ( linea, "DAEMON=") != NULL ) {
		index = strchr(linea, '=') + 1;
		systemParameters.daemonMode = atoi(index);
		return;
	}

	/* Nivel de debug */
	if ( strstr ( linea, "DEBUGLEVEL=") != NULL ) {
		index = strchr(linea, '=') + 1;
		systemParameters.debugLevel = atoi(index);
		return;
	}

	/* Timeout para determinar que un datalogger esta desconectado */
	if ( strstr ( linea, "TIMERDLGOFFLINE=") != NULL ) {
		index = strchr(linea, '=') + 1;
		systemParameters.timerdlgoffline = atoi(index);
		return;
	}

	/* Periodo cada cuanto se manda un ping a los dataloggers */
	if ( strstr ( linea, "TIMERKEEPALIVE=") != NULL ) {
		index = strchr(linea, '=') + 1;
		systemParameters.timerkeepalive = atoi(index);
		return;
	}

}
/*--------------------------------------------------------------------------*/
void printConfigParameters(void)
{
	if (systemParameters.debugLevel > 0) {
		F_syslog("TCP_PORT=%s", systemParameters.tcp_port );
		F_syslog("DBASENAME=%s", systemParameters.dbaseName);
		F_syslog("DBASEUSER=%s", systemParameters.dbaseUser);
		F_syslog("DBASEPASSWD=%s", systemParameters.dbasePassword);
		F_syslog("LOGFILENAME=%s", systemParameters.logfileName);
		F_syslog("DAEMON=%d", systemParameters.daemonMode);
		F_syslog("DEBUGLEVEL=%d", systemParameters.debugLevel);
		F_syslog("TIMERKEEPALIVE=%d", systemParameters.timerkeepalive);
		F_syslog("TIMERDLGOFFLINE=%d", systemParameters.timerdlgoffline);
		F_syslog("----------------------------");
		F_syslog("dlgServerV2 running...");
	}

}
/*--------------------------------------------------------------------------*/
