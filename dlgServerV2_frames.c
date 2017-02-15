/***************************************************************************
 * Archivo dlgServer_frames.c
 * Autor: Pablo Peluffo @ spymovl SRL 2008
 * Descripcion:
 *
 */
/**************************************************************************/

#include "dlgServerV2.h"

/*--------------------------------------------------------------------*/
void sendAckFrame(int connfd)
{
	/* A cada DATA Frame respondo con un ACK */

char fecha[8], hora[8];

	getSystemTime(&fecha,&hora);
	bzero(txMsgBuffer, sizeof(txMsgBuffer));
	switch (st_dlgFrame.version) {
		case 2012:
			sprintf (txMsgBuffer, ">DL00,!,%s,%s<\n",fecha, hora);
			break;
		case 2010:
			sprintf (txMsgBuffer, ">DL00:!:%s,%s<\n",fecha, hora);
			break;
	}

	write(connfd,txMsgBuffer, sizeof(txMsgBuffer) );
	/* log ?? */
	if (systemParameters.debugLevel >= WARNDEBUG ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer,"[%s][%d] Send ACK: %s", st_dlgFrame.dlgId, getpid(), txMsgBuffer );
		F_syslog("%s", logBuffer);
	}
}
/*-------------------------------------------------------------------*/
void sendKeepAlive(int connfd)
{
	/* Envio un frame de KeepAlive */

char fecha[8], hora[8];

	/* Calculo la  hora del sistema */
	getSystemTime(&fecha,&hora);
	bzero(txMsgBuffer, sizeof(txMsgBuffer));
	switch (st_dlgFrame.version) {
		case 2012:
			sprintf (txMsgBuffer, ">DL00,?,%s,%s<\n",fecha, hora);
			break;
		case 2010:
			sprintf (txMsgBuffer, ">DL00:?:%s,%s<\n",fecha, hora);
			break;
		default:
			sprintf(logBuffer,"ERROR: Send keepAlive no puedo determinar version." );
			F_syslog("%s", logBuffer);
			break;
	}

	write(connfd, txMsgBuffer, sizeof(txMsgBuffer) );
	/* log ?? */
	if (systemParameters.debugLevel >= WARNDEBUG ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer,"[%s][%d] Send KALIVE: %s", st_dlgFrame.dlgId, getpid(), txMsgBuffer );
		F_syslog("%s", logBuffer);
	}
}
/*--------------------------------------------------------------------*/
void checkDlgVersion(void)
{
	/* Determino la version del firmware de datalogger */
	st_dlgFrame.version = 2010;

	if ( strstr( st_dlgFrame.dlgRcvdFrame, "FW=2012")) {
		st_dlgFrame.version = 2012;
	}

	if (strstr( st_dlgFrame.dlgRcvdFrame, "FW=dlg2012")) {
		st_dlgFrame.version = 2012;
	}

	if (systemParameters.debugLevel >= WARNDEBUG ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf (logBuffer, "[%s][%d][VERSION %d]", st_dlgFrame.dlgId, getpid(), st_dlgFrame.version );
		F_syslog("%s", logBuffer);
	}
}
/*--------------------------------------------------------------------*/
void pInitFrame(int connfd)
{

	/* Frame recibido 2012 :>MDP001,$,HW=BC+LbjU3,OS=Linux Deb 3.2.0-psp7,FW=dlg2012v1.0 20120426,00< */
    /* Frame recibido 2010 >DL01:$:HW:sp4000R2,OS:FRTOS4.1.3 FW:dlg2010ute2.0a 20101212, 68< */

	/* Lo generan los dataloggers al abrir el socket */

	/* El servidor le pone la fecha y hora antes de ingresarlo en la BD */
	sysClock = time (NULL);
	cur_time = localtime(&sysClock);
	strftime (sysFecha.fullDate, 32, "%F %T", cur_time);

	/* Logueamos el frame */
	bzero(logBuffer, sizeof(logBuffer));
	sprintf (logBuffer, "[%s][%d] RCVD(INIT)=>[Rcvd=%s][type=%s]", st_dlgFrame.dlgId, getpid(), st_dlgFrame.dlgRcvdFrame, st_dlgFrame.frameType);
	F_syslog("%s", logBuffer);

	/* Guardo los datos en la BD */
	/* Inserto en la BD:tb_datosInit */
	sprintf(mysqlCmd,"INSERT INTO tbInits (dlgId, fechaHora, rcvdFrame) VALUES (\"%s\", \"%s\", \"%s\")", st_dlgFrame.dlgId, sysFecha.fullDate, st_dlgFrame.dlgRcvdFrame );
	if (mysql_query (conn, mysqlCmd) != 0) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf (logBuffer, "[%s] ERROR_015: No puedo ejecutar stmt INIT: %s",st_dlgFrame.dlgId, mysql_stmt_error(stmt_main) );
		F_syslog("%s", logBuffer);
		bzero(logBuffer, sizeof(logBuffer));
		sprintf (logBuffer, "[%s] ERROR_015 [STMT=%s]", st_dlgFrame.dlgId, mysqlCmd );
		F_syslog("%s", logBuffer);
	} else {
		if (systemParameters.debugLevel >= LOWDEBUG ) {
			bzero(logBuffer, sizeof(logBuffer));
			sprintf(logBuffer, "[%s] InsertInit OK", st_dlgFrame.dlgId );
			F_syslog("%s", logBuffer);
		}
	}

	/* Leo los factores de conversion para el datalogger dado */
	if ( readDlgConfig() == ERROR ) {
		/* El datalogger no esta definido en la base: corto. */
		bzero(logBuffer, sizeof(logBuffer));
		sprintf (logBuffer, "[%s] ERROR_016->Datalogger no definido en tabla de configuracion", st_dlgFrame.dlgId);
		F_syslog("%s", logBuffer);
		shutdown(connfd, 2);
		exit(1);
	}
}
/*--------------------------------------------------------------------*/
void pDataFrame(void)
{
/* Frame con datos. En el parametro ya vienen parseados los campos
 * Debo ingresarlo en la base Main, MainOnline y loguear los datos
 * Al hacer los INSERT, determino si la magnitud esta disponible o no y
 * agrego dicho campo.
 */

char tmp[10];
int i, fieldPos;
int bufPos;
char dispQuery[MAXSQLCMDSTRING];
/*
 *  // Si quiero debugear y ver como quedo el frame parseado.
	fieldPos = 0;
	while ( (token = u[fieldPos]) != NULL ) {
		printf("F%d=[%s]\n", fieldPos, token);
		fieldPos++;
	}
	return;
*/

	/* PASO 1: Determino la fecha y hora del sistema */
	sysClock = time (NULL);
	cur_time = localtime(&sysClock);
	bzero(tmp, strlen(tmp));
	strftime (sysFecha.fullDate, 32, "%F %T", cur_time);

	/* Hallo la fechaHoraData del frame en el formato DATETIME */
	getFechaHoraData(st_dlgFrame.fecha, st_dlgFrame.hora);

	/* PASO 2: Logueo el frame recibido */
	if (systemParameters.debugLevel >= LOWDEBUG ) {
		bzero(logBuffer, sizeof(logBuffer));
		sprintf (logBuffer, "[%s][%d] RCVD(DAT)=>[Rcvd=%s]", st_dlgFrame.dlgId, getpid(), st_dlgFrame.dlgRcvdFrame);
		F_syslog("%s", logBuffer);
	}

	/* Logueo el frame parseado */
	bufPos = 0;
	fieldPos = 0;
	bzero(logBuffer, sizeof(logBuffer));
	bufPos += sprintf(logBuffer + bufPos, "[%s][%d] PARSED=> \n", st_dlgFrame.dlgId, getpid());
	bufPos += sprintf(logBuffer + bufPos, "[%s]", st_dlgFrame.frameType);
	bufPos += sprintf(logBuffer + bufPos, "[%s]", st_dlgFrame.fecha);
	bufPos += sprintf(logBuffer + bufPos, "[%s]", st_dlgFrame.hora);
	bufPos += sprintf(logBuffer + bufPos, "=>");
	for (i=0; i < MAXCHANNELS; i++) {
		bufPos += sprintf(logBuffer + bufPos, "[%d]", st_dlgFrame.valores[i]);
	}
	bufPos += sprintf(logBuffer + bufPos, "\n");
	F_syslog("%s",logBuffer);

	/* PASO 3: Convierto los valores a magnitudes */
	for (i=0; i< dlgConfParam.nroParametros; i++) {
		// Valor del conversor A/D
		dlgConfParam.pList[i].val = st_dlgFrame.valores[dlgConfParam.pList[i].fieldPos];
		// Magnitud.
		dlgConfParam.pList[i].mag = dlgConfParam.pList[i].val * dlgConfParam.pList[i].M + dlgConfParam.pList[i].B;
		bzero(logBuffer, sizeof(logBuffer));
		sprintf(logBuffer, "DEBUG:[I=%d][VAL=%.2f][MAG=%.2f][M=%.3f][B=%.3f]", i, dlgConfParam.pList[i].val, dlgConfParam.pList[i].mag,dlgConfParam.pList[i].M , dlgConfParam.pList[i].B );
		F_syslog("%s", logBuffer);
	}

	bufPos = 0;
	bzero(logBuffer, sizeof(logBuffer));
	bufPos += sprintf(logBuffer + bufPos, "[%s][%d] PAIRS=>  ", st_dlgFrame.dlgId, getpid());
	for (i=0; i< dlgConfParam.nroParametros; i++) {
		bufPos += sprintf(logBuffer + bufPos, "{[%d] %d=>%.5s,%s=%.2f %s=%.2f %.5s} ", i, dlgConfParam.pList[i].fieldPos, dlgConfParam.pList[i].magName, dlgConfParam.pList[i].tbMColValName,  dlgConfParam.pList[i].val, dlgConfParam.pList[i].tbMColMagName,dlgConfParam.pList[i].mag,  dlgConfParam.pList[i].magUnits);
	}

	if (systemParameters.debugLevel >= LOWDEBUG ) {
		F_syslog("%s", logBuffer);
	}

	/* PASO 4: Inserto en tbMain */
	bufPos = 0;
	bzero(mysqlCmd, sizeof(mysqlCmd));

	// Armo el sql string "INSERT INTO tbMain ( values list ) VALUES (...."
	bufPos += sprintf(mysqlCmd + bufPos, "INSERT INTO tbMain (dlgId,fechaHoraData,uxTsData,fechaHoraSys,uxTsSys");
	for (i=0; i< dlgConfParam.nroParametros; i++) {
		bufPos += sprintf(mysqlCmd + bufPos, ",%s,%s,%s", dlgConfParam.pList[i].tbMColValName, dlgConfParam.pList[i].tbMColMagName,dlgConfParam.pList[i].tbMColDispName );
	}
	bufPos += sprintf(mysqlCmd + bufPos, ",rcvdFrame) VALUES (\"%s\",\"%s\",UNIX_TIMESTAMP(fechaHoraData),\"%s\",UNIX_TIMESTAMP(fechaHoraSys)", st_dlgFrame.dlgId,st_dlgFrame.fechaHoraData,sysFecha.fullDate);

	// Termino el sql string con los VALUES .....)"
	for (i=0; i< dlgConfParam.nroParametros; i++) {
		bzero(dispQuery, sizeof(dispQuery));
		sprintf(dispQuery, "(SELECT disp FROM tbDlgParserConf WHERE dlgId LIKE \"%s\" AND fieldPos=%d)",st_dlgFrame.dlgId, dlgConfParam.pList[i].fieldPos);
		bufPos += sprintf(mysqlCmd + bufPos, ",%.2f,%.2f,%s", dlgConfParam.pList[i].val, dlgConfParam.pList[i].mag, dispQuery);
	}

	bufPos += sprintf(mysqlCmd + bufPos, ",\"%s\");",st_dlgFrame.dlgRcvdFrame );

	bzero(logBuffer, sizeof(logBuffer));
	if (mysql_query (conn, mysqlCmd) != 0) {
		sprintf (logBuffer, "[%s][%d] ERROR_012a: No puedo ejecutar stmt Main: %s",st_dlgFrame.dlgId, getpid(), mysql_stmt_error(stmt_main) );
		F_syslog("%s", logBuffer);
		sprintf (logBuffer, "[%s][%d] ERROR_012a: [STMT=%s]", st_dlgFrame.dlgId, getpid(), mysqlCmd );
		F_syslog("%s", logBuffer);
	} else {
		if (systemParameters.debugLevel >=LOWDEBUG ) {
			sprintf(logBuffer, "[%s][%d] InsertMain OK", st_dlgFrame.dlgId,getpid());
			//sprintf (logBuffer, "[%s][%d] DEBUG: [STMT=%s]", st_dlgFrame.dlgId, getpid(), mysqlCmd );
			F_syslog("%s", logBuffer);
		}
	}

	//sprintf (logBuffer, "[%s][%d] DEBUG: [STMT=%s]", st_dlgFrame.dlgId, getpid(), mysqlCmd );
	//F_syslog("%s", logBuffer);

	/* PASO 5: Inserto en tbMainOnline */
	bufPos = 0;
	bzero(mysqlCmd, sizeof(mysqlCmd));

	// Armo el sql string "INSERT INTO tbMainOnline ( values list ) VALUES (...."
	bufPos += sprintf(mysqlCmd + bufPos, "INSERT INTO tbMainOnline (dlgId,fechaHoraData,fechaHoraSys");
	for (i=0; i< dlgConfParam.nroParametros; i++) {
		bufPos += sprintf(mysqlCmd + bufPos, ",%s,%s,%s", dlgConfParam.pList[i].tbMColValName, dlgConfParam.pList[i].tbMColMagName,dlgConfParam.pList[i].tbMColDispName );
	}
	bufPos += sprintf(mysqlCmd + bufPos, ",rcvdFrame) VALUES (\"%s\",\"%s\",\"%s\"", st_dlgFrame.dlgId,st_dlgFrame.fechaHoraData,sysFecha.fullDate);

	// Termino el sql string con los VALUES .....)"
	for (i=0; i< dlgConfParam.nroParametros; i++) {
		bzero(dispQuery, sizeof(dispQuery));
		sprintf(dispQuery, "(SELECT disp FROM tbDlgParserConf WHERE dlgId LIKE \"%s\" AND fieldPos=%d)",st_dlgFrame.dlgId, dlgConfParam.pList[i].fieldPos);
		bufPos += sprintf(mysqlCmd + bufPos, ",%.2f,%.2f,%s", dlgConfParam.pList[i].val, dlgConfParam.pList[i].mag, dispQuery);
	}

	bufPos += sprintf(mysqlCmd + bufPos, ",\"%s\");",st_dlgFrame.dlgRcvdFrame );


	bzero(logBuffer, sizeof(logBuffer));
	if (mysql_query (conn, mysqlCmd) != 0) {
		sprintf (logBuffer, "[%s][%d] ERROR_012b: No puedo ejecutar stmt MainOnline: %s",st_dlgFrame.dlgId, getpid(), mysql_stmt_error(stmt_main) );
		F_syslog("%s", logBuffer);
		sprintf (logBuffer, "[%s][%d] ERROR_012b: [STMT=%s]", st_dlgFrame.dlgId, getpid(), mysqlCmd );
		F_syslog("%s", logBuffer);
	} else {
		if (systemParameters.debugLevel >=LOWDEBUG ) {
			sprintf(logBuffer, "[%s][%d] InsertOnline OK", st_dlgFrame.dlgId, getpid());
			F_syslog("%s", logBuffer);
		}
	}

	/* Dejo en tbMainOnline un solo registro */
	/* Borro todos los registros de la tabla main_online distintos al ultimo insertado */
	bzero(mysqlCmd, sizeof(mysqlCmd));
	sprintf(mysqlCmd,"DELETE FROM tbMainOnline WHERE dlgId LIKE \"%s\" AND fechaHoraSys < \"%s\";" , st_dlgFrame.dlgId, sysFecha.fullDate );
	bzero(logBuffer, sizeof(logBuffer));
	if (mysql_query (conn, mysqlCmd) != 0) {
		sprintf (logBuffer, "[%s][%d] ERROR_012c: No puedo ejecutar stmt MainOnline (DELETE): %s",st_dlgFrame.dlgId, getpid(), mysql_stmt_error(stmt_main) );
		F_syslog("%s", logBuffer);
		sprintf (logBuffer, "[%s][%d] ERROR_012c: [STMT=%s]", st_dlgFrame.dlgId, getpid(), mysqlCmd );
		F_syslog("%s", logBuffer);
	} else {
		if (systemParameters.debugLevel >=LOWDEBUG ) {
			sprintf(logBuffer, "[%s][%d] DeleteMainOnline OK", st_dlgFrame.dlgId, getpid());
			F_syslog("%s", logBuffer);
		}
	}

}
/*--------------------------------------------------------------------*/
void expandBitFields(void)
{
/* Frame con datos. En el parametro ya vienen parseados los campos
 *
 * El tema es que en el campo din vienen los 8 bits de las entradas digitales
 * en una sola palabra por lo que debo abrirla en 8 campos.
 * >DLid:%:YYMMDD,hhmmss,a0,a1,a2,a3,a4,a5,a6,a6,din,c0,c1<
 * se trasnforma en
 * >DLid:%:YYMMDD,hhmmss,a0,a1,a2,a3,a4,a5,a6,a6,d0,d1,d2,d3,d4,d5,d6,d7,c0,c1<
 *
 */

int i, din, num;


	if (st_dlgFrame.version == 2012) {
		return;
	}

	// Los primeros 8 valores ( a0..a7) quedan como estan. { valores[0]..valores[7] }
	// El siguiente campo din lo expando en 8 { valores[8]..valores[15] ]
	// Muevo antes el resto de los campos.
	st_dlgFrame.valores[16] = st_dlgFrame.valores[9];
	st_dlgFrame.valores[17] = st_dlgFrame.valores[10];
	st_dlgFrame.valores[18] = st_dlgFrame.valores[11];
	st_dlgFrame.valores[19] = st_dlgFrame.valores[12];

	// Expando el campo digital din
	din = st_dlgFrame.valores[8];
	num = (din & 0x01) >> 0; st_dlgFrame.valores[15] = num;
	num = (din & 0x02) >> 1; st_dlgFrame.valores[14] = num;
	num = (din & 0x04) >> 2; st_dlgFrame.valores[13] = num;
	num = (din & 0x08) >> 3; st_dlgFrame.valores[12] = num;
	num = (din & 0x10) >> 4; st_dlgFrame.valores[11] = num;
	num = (din & 0x20) >> 5; st_dlgFrame.valores[10] = num;
	num = (din & 0x40) >> 6; st_dlgFrame.valores[9] = num;
	num = (din & 0x80) >> 7; st_dlgFrame.valores[8] = num;

	// Si quiero debugear y ver como quedo el frame parseado.
	for ( i=0; i < MAXCHANNELS; i++) {
		printf("[%d]", st_dlgFrame.valores[i]);
	}
	printf("\n");

	return;

}
/*--------------------------------------------------------------------*/
void pAckFrame(void)
{
	/* Frame de ACK re: Solo Logueo ya que es la respuesta al echo request / resync mandada por el server */
	bzero(logBuffer, sizeof(logBuffer));
	sprintf (logBuffer, "[%s][%d] RCVD(KA)=> [Rcvd=%s][type=%s]", st_dlgFrame.dlgId, getpid(), st_dlgFrame.dlgRcvdFrame, st_dlgFrame.frameType);
	if (systemParameters.debugLevel >= LOWDEBUG ) {
		F_syslog("%s", logBuffer);
	}
	return;

}
/*--------------------------------------------------------------------*/

