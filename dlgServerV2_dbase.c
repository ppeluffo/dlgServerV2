/***************************************************************************
 * Archivo dlgServer_dbase.c
 * Autor: Pablo Peluffo @ spymovl SRL 2008
 * Descripcion: Funciones de manejo de la base de datos del programa de
 * comunicaciones de los dataloggers SP4000.
 * Inicialmente usamos MySQL 5.X
 * Tomamos como base el archivo sp4001_dbase.c
 *
 */
/**************************************************************************/

#include "dlgServerV2.h"

void bdMySqlInit(void);
void bdMySqlConnect(void);
void bdMySqlStmtInit(void);

/*--------------------------------------------------------------------------*/
void bdInit(void)
{
	bdMySqlInit();
	bdMySqlConnect();
	bdMySqlStmtInit();
}

/*--------------------------------------------------------------------------*/
void bdMySqlInit(void)
{
/* Inicializo la mysql. En caso de no poder aborta el programa y loguea
 * La secuencia de funciones es la extraida de "Writing MySql programs using C".
*/

	mysql_library_init(0,NULL,NULL);

	/* Inicializo el sistema de BD */
	conn = mysql_init(NULL);
	bzero(logBuffer, sizeof(logBuffer));
	if (conn == NULL) {
		sprintf(logBuffer, "ERROR_002: No puedo inicializar MySql. No puedo continuar\n");
		F_syslog("%s", logBuffer);
		exit (1);
	} else {
		sprintf(logBuffer, "MySql inicializada.. OK.\n");
		F_syslog("%s", logBuffer);
	}
}

/*--------------------------------------------------------------------------*/

void bdMySqlConnect(void)
{
/* Intento conectarme a la BD con el usuario y el passwd pasado en los args */

	bzero(logBuffer, sizeof(logBuffer));
	if (mysql_real_connect(conn, NULL, systemParameters.dbaseUser, systemParameters.dbasePassword, systemParameters.dbaseName, 0, NULL, 0) == NULL ) {
		sprintf(logBuffer, "ERROR_003: No puedo conectarme a la BD. No puedo continuar\n");
		F_syslog("%s", logBuffer);
		exit (1);
	} else {
		sprintf(logBuffer, "Conectado a la BD.. OK.\n");
		F_syslog("%s", logBuffer);
	}
}

/*--------------------------------------------------------------------------*/

void bdMySqlStmtInit(void)
{
	/* Inicializo las consultas */
	stmt_main = mysql_stmt_init(conn);
	bzero(logBuffer, sizeof(logBuffer));
	if (!stmt_main) {
		sprintf(logBuffer, "ERROR_004: No puedo inicializar la statement MAIN. No puedo continuar\n");
		F_syslog("%s", logBuffer);
		exit (1);
	} else {
		sprintf(logBuffer, "Inicializo la statement MAIN.. OK.\n");
		F_syslog("%s", logBuffer);
	}

}

/*--------------------------------------------------------------------------*/

