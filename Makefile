CC=/opt/amiga/bin/m68k-amigaos-gcc
CFLAGS=-std=c99 -O0
#works
VNETCC=/opt/amiga/bin/m68k-amigaos-gcc
VNET_CFLAGS=-std=c99 -O0 -Wall -fstack-protector-all -Wno-pointer-sign
VNET_Includes=-I/opt/amiga/m68k-amigaos/ndk-include/

#WIP
#VNETCC=/usr/bin/m68k-linux-gnu-gcc
#VNETCC=/opt/amiga/bin/vc
#VNET_Includes=-I/opt/amiga/m68k-amigaos/ndk-include/
#VNET_CFLAGS=-c99 -stack-check -dontwarn=64 -lamiga -lauto -I/opt/amiga/m68k-amigaos/ndk-include/
TARGET=amigaServer
VNetAmigaServer=VNetServerAmiga
AMIGA_SRC=	Amiga/VNetServerAmiga.c \
			Amiga/VNetServerThread.c \
			Amiga/VNetDiscoveryThread.c \
			Amiga/VNetUtil.c \
			Amiga/protocol.c \
			Amiga/DirectoryList.c \
			Amiga/SendFile.c \
			Amiga/ReceiveFile.c \
			Amiga/MakeDir.c \
			Amiga/VolumeList.c

${VNetAmigaServer}: 
	$(VNETCC) ${VNET_CFLAGS} ${VNET_Includes} -I./Amiga/ -I. ${AMIGA_SRC} -lamiga -o ${VNetAmigaServer}
	
all: clean ${VNetAmigaServer}
	
clean:
	rm -f ${VNetAmigaServer}
