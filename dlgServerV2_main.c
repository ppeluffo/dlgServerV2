/***************************************************************************
 * Programa: dlgServerV2
 *
 * Servidor de comunicaciones para que los datalggers de la seria SP4000
 * puedan conectarse a un servidor por medio de sockets.
 * Deriva de avlserver (manejo de sockets) y del sp4001 (manejo de dataloggers)
 *
 * Modificaciones:
 * --------------
 * 2017-Feb-15
 * Para cumplir la nueva normativa de DINAMA, creo una tabla identica a tbMain pero la llamo
 * tbMain_auxDinama para insertar lo mismo que en tbMain pero poder manipularla sin problema.
 * Para esto modifico el archivo dlgServerV2_frames, la funcion pDataFrame.
 * 
 * 2013-Ago-30:
 * Utilizo una estructura para almacenar los datos de c/frame en vez de un puntero a un array de strings.
 *
 * 2013-Ago-10:
 * Incorporo el campo disponibilidad en la base de datos en las tablas tbParseConf, tbMain, tbMainOnline
 * por lo tanto modifico las consultas a la BD para tener en cuenta el nuevo campo.
 * En particular este codigo lo utiliza por ahora solo UTE en las 2 versiones del protocolo de los
 * dataloggers, por lo tanto borro todo lo innecesario.
 * Los nuevos frames (V2012) pueden tener hasta 20 (MAXCHANNELS) canales de datos.
 * --------------------------------------------------------------------------------------------------------
 *
 *
 * 1- Los factores de conversion los leo al recibir el frame de init. De este modo
 * independizo el programa de la cantidad de dataloggers de la red.
 *
 * 2- Cambio la forma de manejar la fecha y hora utilizando un campo DATETIME de modo
 * que simplifique la consulta.
 *
 * 3- Inserto en la BD los valores de las entradas digitales. (ute->pozos)
 *
 * 4- Mejoro el manejo de los mensajes de debug.
 *
 * 5- Agrego que cada nueva conexion, el child que la atiende lo deja registrado en un
 * archivo con el nombre del datalogger, y en su interior una sola linea con el pid
 * Esto me va a servir para luego conectarme al remoto.
 *
 * 6- Que espere lo necesario para arrancar hasta que el socket este libre.
 *    (por los TIME_WAIT)
 *---------------------------------------------------------------------------
 * Pendiente:
 * 5- Trasmitir frames a cada datalogger
 *
 ----------------------------------------------------------------------------
 * Modificaciones 2009-10-30:
 * Cambio todo para referirme a dlg8ch.
 * 1- Logueo los inits.
 * 2- Cambio el formato de datos fecha/hora en la BD para que sea datetime.
 * 3- Cambio el formato de tablas.
 * 4- Agrego una tabla para manejar solo los on-line.
 ----------------------------------------------------------------------------
 * Modificaciones 2012-06-22 (dlgServer)
 * 1- El parseo de campos lo hago por :, para que sea compatible con las versiones
 *    anteriores.
 * 2- Todos los frames de la nueva version tienen como separadores , de modo que
 *    en los keepAlive, dependiendo de datalogger conectado es el separador que mando.
  ----------------------------------------------------------------------------
 * Modificaciones 2012-07-05 (dlgServer)
 * 1- Compatibilizo los frames con los de los dataloggers viejos.
 *    INIT: >MT01:$:HW:sp4000R2/8,OS:FRTOS4.1.3 FW:dlg2010ute2.0d 20110225,95<
 *    DATA: >DL05:%:120301,000134,765,375,375,3,810,231,451,542,255,0,0<
 *
 *--------------------------------------------------------------------------------
 * Frames
 *
 	 Init 2012 >MDP001,$,HW=BC+LbjU3,OS=Linux Deb 3.2.0-psp7,FW=dlg2012v1.0 20120426,00<
     DATA      >MDP001,%,130425,040215,240,290,26,164,0,739,4,334,12,30,14,27,0,0,1,1,1,1,60<

     Init 2010 >MDP001:$:HW:sp4000R2,OS:FRTOS4.1.3 FW:dlg2010ute2.0a 20101212,68<
     DATA:     >MDP001:%:120301,000134,765,375,375,3,810,231,451,542,255,0,0<

 *
 ***************************************************************************/

/*--------------------------------------------------------------------------*/

#include "dlgServerV2.h"

int timerKeepAlive, timerDlgOffLine;
int listenfd, connfd;

void sig_alarm(int signo);
void sig_chld(int signo);
void rxFramesProcess(int sockfd);
void decodeFrame(void);

const char *argp_program_version = "dlgServerV2 V1.1a @20170215";
const char *argp_program_bug_address ="<spymovil@spymovil.com>";
static char doc[] = "dlgServerV2: Servidor de comunicaciones multasking de DLGs";

/* A description of the arguments we accept. */
static char args_doc[] = "";

/* The options we understand. */
static struct argp_option options[] = {
   {"config",   'c', "/etc/dlgServerV2.conf", 0, "Archivo de configuracion" },
   { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments
{
	char *config_file;
};

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
/* Get the input argument from argp_parse, which we
   know is a pointer to our arguments structure. */
struct arguments *arguments = state->input;

	switch (key)
    {
    case 'c':
    	arguments->config_file = arg;
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

/*--------------------------------------------------------------------*/
int main(int argc, char **argv)
{
	pid_t childpid;
	socklen_t clilen;
	struct sockaddr_in cliaddr, servaddr;
	int bindTimes;
	struct arguments arguments;

	/* Analizamos los parametros pasados en la linea de comandos */
	/* Por defecto el archivo de configuracion es: */
	arguments.config_file = "/etc/dlgServerV2.conf";

	/* Parse our arguments; every option seen by parse_opt will
	be reflected in arguments. */
	argp_parse (&argp, argc, argv, 0, 0, &arguments);
	/* Abrimos el log, leo el archivo de configuracion y muestro los
	parametros con los cuales voy a trabajar
	*/
	strcpy(systemParameters.configFile,  arguments.config_file);

	/* Abrimos el log, leo el archivo de configuracion y muestro los
        parametros con los cuales voy a trabajar
	*/
	readConfigFile();
	F_openlog();
	printConfigParameters();

	if (systemParameters.daemonMode == 1)
		daemonize();

	/* Inicializo todo lo referente a la BD */
	bdInit();

	/* Preparo la estructura TCP para escuchar los dataloggers */
	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "Opening socket (waiting TIME_WAIT state)...");
	F_syslog("%s", logBuffer);

	listenfd = socket (AF_INET, SOCK_STREAM, 0);
	bzero (&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons( atoi(systemParameters.tcp_port) );

	bindTimes = 20;
	while (bindTimes-- > 0) {
		if ( bind (listenfd, &servaddr, sizeof(servaddr)) == 0) break;
		sleep(10);
	}
	if (bindTimes == 0) {
		F_syslog( "%s", "\nNo puedo hacer el bind...\n");
		exit (1);
	}

	bzero(logBuffer, sizeof(logBuffer));
	sprintf(logBuffer, "Bind OK..");
	F_syslog("%s", logBuffer);

	listen (listenfd, LISTENQ);

	/* Instalamos el handler de la señal de terminacion de los child */
	signal (17, sig_chld);

	/* Instalamos el handler de la señal de keepalive (temporizador) */
    signal (SIGALRM, sig_alarm);

	/* Ciclo ppal: esperamos nuevas conexiones y creamos un child que
        atienda a c/u.
	*/
	F_syslog( "%s", "\nEsperando nuevas conexiones...\n");

	for (;;) {
		clilen = sizeof(cliaddr);
		if ( (connfd = accept(listenfd, &cliaddr, &clilen) ) < 0 ) {
			if (errno == EINTR)
				continue;
			else
				F_syslog( "%s", "ERROR_008: Accept error...");
		}

		if ( (childpid = fork()) == 0) {
			/* Child process */
			F_syslog( "[%d] Nuevo child process.", getpid());
			close (listenfd);

			/* habilito el keepAlive */
			alarm(ALARMTIMEOUT);
			timerDlgOffLine = ( systemParameters.timerdlgoffline / ( 1.0 * ALARMTIMEOUT ));
			timerKeepAlive = ( systemParameters.timerkeepalive / ( 1.0 * ALARMTIMEOUT ));

			/* Me conecto e inicializo la BD.
			 * Cada child abre una nueva conexion a la BD
			 * para evitar usar descriptores zombies
			 */
			bdInit();

			/* Paso a escuchar datos recibidos por el socket y procesarlas */
			rxFramesProcess(connfd);
			exit(0);
		}

		/* El parent close el socket conectado */
		close (connfd);
	}
}
/*--------------------------------------------------------------------*/

void sig_alarm(int signo)
{
/* Handler que maneja el envio de los keepAlive a los DLG remotos y la
 * espera de los timersOffline.
 * Al crearse el proceso child que atiende al DLG se setea para generar
 * una señal de alarma y asi invocar periodicamente a este handler
 * que es quien genera el frame.
*/
	/* ECHO REQUEST */
	/* Cuando expira este timer envio un keepAlive al DLG. */
	if (timerKeepAlive-- == 0) {
		sendKeepAlive(connfd);
		/* Reinicio el timer */
		timerKeepAlive = ( systemParameters.timerkeepalive / (1.0 * ALARMTIMEOUT ));
	}

	/* Cuando expira este timer es que el DLG no ha enviado nada durante
		un tiempo largo: Cierro el socket y salgo.
	*/
	if (timerDlgOffLine-- == 0) {
		F_syslog( "[%s][%d] MsgTO: Supongo DLG offline; shutdown", st_dlgFrame.dlgId, getpid());
		shutdown(connfd, 2);
		exit(1);
	}

	alarm(ALARMTIMEOUT);

}
/*--------------------------------------------------------------------*/

void sig_chld(int signo)
{

/* Handler de la señal SIGCHLD para que al terminar los child no
   se transformen en zombies
   Cuando un child termina manda un SIGCHLD al parent. Si este no
   la procesa (la ignora) entonces el child se vuelve un zombie.
   Con esto hacemos entonces la disposicion de la señal.
*/
	pid_t pid;
	int stat;

	while ( (pid = waitpid(-1, &stat, WNOHANG) ) > 0 )
		F_syslog( "[%s][%d] Child termino", st_dlgFrame.dlgId, pid);
	return;

}

/*--------------------------------------------------------------------*/

void rxFramesProcess(int sockfd)
{
/* Procesa los frames enviados por los DLG en un loop permanente. Solo
 * se sale (return) en caso de error o que el socket se cierre
*/

ssize_t n;
int version;

	for(;;) {

		/* Inicializo la estructura de datos donde voy a recibir el frame */
		//bzero(&st_dlgFrame.dlgRcvdFrame, sizeof(st_dlgFrame.dlgRcvdFrame));
		version = st_dlgFrame.version;
		bzero(&st_dlgFrame, sizeof(st_dlgFrame));
		st_dlgFrame.version = version;

		if ( ( n = readline(sockfd, st_dlgFrame.dlgRcvdFrame, MAXAVLRCVDLINE) ) == 0) {
			return;	/* El avl cerro la conexion */
		}
		else if ( n == -1) {
			/* Error en el socket al leer: lo cierro */
			F_syslog("[%s][%d] %s", st_dlgFrame.dlgId, getpid(), "ERROR_009: Error de lectura del socket");
			shutdown(sockfd,2);
			exit(1);
		}

		/* Si estoy aqui es que tengo una linea completa: la parseo */

		/* Recibi un frame: reseteo el timer de dlgOffLine */
		timerDlgOffLine = ( systemParameters.timerdlgoffline / (1.0 * ALARMTIMEOUT ));

		/* Chequeo los delimitadores */
		if (strchr(st_dlgFrame.dlgRcvdFrame, '>') == NULL ) {
			if (systemParameters.debugLevel >= WARNDEBUG ) {
				F_syslog("[%s][%d] ERROR_010: FORMAT ERROR %s", st_dlgFrame.dlgId, getpid(), st_dlgFrame.dlgRcvdFrame);
			}
				continue;
		}
		if (strchr(st_dlgFrame.dlgRcvdFrame, '<') == NULL ) {
			if (systemParameters.debugLevel >= WARNDEBUG ) {
				F_syslog("[%s][%d] ERROR_011: FORMAT ERROR %s", st_dlgFrame.dlgId, getpid(), st_dlgFrame.dlgRcvdFrame);
			}
			continue;
		}

		/* Pruebas con telnet */
		if ( strstr(st_dlgFrame.dlgRcvdFrame, "quit\0") != NULL) {
			shutdown(sockfd,2);
			exit(0);
		}

		/* Los delimitadores estan bien: paso a analizar el frame */
		decodeFrame();

	}
}
/*--------------------------------------------------------------------*/

void decodeFrame(void)
{
char *token, cp[MAXLINE];
char parseDelimiters[] = ">:,<";
int i;

	/* Copio el frame recibido a un temporal que va a usar el strtok */
	strcpy(cp,st_dlgFrame.dlgRcvdFrame);

	/* DlgId */
	token = strtok(cp, parseDelimiters);
	strcpy( st_dlgFrame.dlgId, token);

	/* Frame Type */
	token = strtok(NULL, parseDelimiters);
	strcpy( st_dlgFrame.frameType, token);

	// A partir de aqui puedo saber que hacer con el frame.

	// INIT.
	/* >MDP001,$,HW=BC+LbjU3,OS=Linux Deb 3.2.0-psp7,FW=dlg2012v1.0 20120426,00< */
	/* >DL02:$:HW:sp4000R2,OS:FRTOS4.1.3 FW:dlg2010ute2.0a 20101212, 68< */
	if ( st_dlgFrame.frameType[0] == '$' ) {
		checkDlgVersion();
		pInitFrame(connfd);
		/* Siempre confirmo el frame recibido */
		sendAckFrame(connfd);
		/* A cada init respondo con un KeepAlive para sincronizar pronto la fecha y hora del datalogger remoto */
		sendKeepAlive(connfd);
		return;
	}

	// RESYNC ACKs
	/*>DL01,?< */
	if ( st_dlgFrame.frameType[0] == '?' ) {
		pAckFrame();
		return;
	}

	// DATA
	/* >MDP001,%,120622,140719,179,166,156,150,144,145,161,171,10,10,10,10,0,0,1,1,1,1,10< */
	/* >DLid:%:YYMMDD,hhmmss,a0,a1,a2,a3,a4,a5,a6,a7,din,c0,c1<CR   */
	/* >DL02:%:120301,000134,765,375,375,3,810,231,451,542,214,0,0< */
	if ( st_dlgFrame.frameType[0] == '%' ) {

		/* Fecha */
		token = strtok(NULL, parseDelimiters);
		strcpy( st_dlgFrame.fecha, token);

		/* Hora */
		token = strtok(NULL, parseDelimiters);
		strcpy( st_dlgFrame.hora, token);

		/* Datos */
		i=0;
		while ( (token = strtok(NULL, parseDelimiters)) != NULL ) {
			st_dlgFrame.valores[i++] = atoi(token);
		}

		expandBitFields();
		pDataFrame();
		/* Confirmo la recepcion del mensaje */
		sendAckFrame(connfd);
		return;
	}

	// ERROR
	bzero(logBuffer, sizeof(logBuffer));
	sprintf (logBuffer, "[%s][%d] FRAME DESCONOCIDO [Rcvd=%s]", st_dlgFrame.dlgId, getpid(), st_dlgFrame.dlgRcvdFrame);
	if (systemParameters.debugLevel >= WARNDEBUG ) {
		F_syslog("%s", logBuffer);
	}
	return;

}

/*--------------------------------------------------------------------*/
