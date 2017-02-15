# Makefile del proyecto dlg2012sbc
# -----------------------------
#
SRCS = dlgServerV2_config.c dlgServerV2_dbase.c dlgServerV2_frames.c dlgServerV2_main.c dlgServerV2_utils.c
OBJS = $(SRCS:.c=.o)
CC=gcc -O0 -g3 -Wall -c -fmessage-length=0
CFLAGS=-I/usr/include -I/usr/include/mysql -O0 -g3 -Wall -c -fmessage-length=0
#LD=gcc -L/usr/lib/x86_64-linux-gnu
LD=gcc
LDFLAGS= -lpthread -ldl -lm -lrt -lmysqlclient
EXE=dlgServerV2
STRIP=strip
FILE=file
RM=rm
#
#---------------------------------------------------------------------------------
#
all: $(EXE)

# Compilacion
$(EXE): $(OBJS) 
#	$(LD) $(LDFLAGS) $(OBJS) -o $(EXE)
	$(LD) -o $(EXE) $(OBJS) $(LDFLAGS)
	#cp $(EXE) $(EXE).exe
	$(STRIP) $(EXE)
	$(FILE) $(EXE)

# Con esta regla, cada vez que cambie el .h debo regenerar todos los .o
$(OBJS): dlgServerV2.h

# Generacion de todos los objetos
# suffix rule: regla para convertir los .c en .o
# $@ es la variable que representa el objetivo (target) de la regla, en este caso el archivo .o
# $< es la variable que representa el nombre del archivo que causo la accion ( el percusor del target), en este caso el archivo .c
#
.c .o:
		$(CC) $(CFLAGS) $< -o $@

#
clean:
	$(RM) -f $(EXE) $(EXE).exe *.o *.d *.gch
	$(RM) -f *.~
	
	
#---------------------------------------------------------------------------------

