/*
 * cbmdisk - network enabled, sd card based IEEE-488 CBM floppy emulator 
 * Copyright (C) 2015 Guenter Bartsch
 * 
 * Most of the code originates from:
 *
 * NODISKEMU - SD/MMC to IEEE-488 interface/controller
 * Copyright (c) 2015 Nils Eilers. 
 *
 * which is based on:
 *
 * sd2iec by Ingo Korb (et al.), http://sd2iec.de
 * Copyright (C) 2007-2014  Ingo Korb <ingo@akana.de>
 *
 * Inspired by MMC2IEC by Lars Pontoppidan et al.
 * FAT filesystem access based on code from ChaN and Jim Brain, see ff.c|h.
 *
 * Network code is based on ETH_M32_EX 
 * Copyright (C) 2007 by Radig Ulrich <mail@ulrichradig.de>
 *
 * JiffyDos send based on code by M.Kiesel
 * Fat LFN support and lots of other ideas+code by Jim Brain 
 * Final Cartridge III fastloader support by Thomas Giesel 
 * Original IEEE488 support by Nils Eilers 
 * FTP server and most of the IEEE 488 FSM implementation by G. Bartsch.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*----------------------------------------------------------------------------
 Copyright:      Radig Ulrich  mailto: mail@ulrichradig.de
 Author:         Radig Ulrich
 Remarks:        
 known Problems: none
 Version:        24.10.2007
 Description:    Ethernet Stack

 Dieses Programm ist freie Software. Sie können es unter den Bedingungen der 
 GNU General Public License, wie von der Free Software Foundation veröffentlicht, 
 weitergeben und/oder modifizieren, entweder gemäß Version 2 der Lizenz oder 
 (nach Ihrer Option) jeder späteren Version. 

 Die Veröffentlichung dieses Programms erfolgt in der Hoffnung, 
 daß es Ihnen von Nutzen sein wird, aber OHNE IRGENDEINE GARANTIE, 
 sogar ohne die implizite Garantie der MARKTREIFE oder der VERWENDBARKEIT 
 FÜR EINEN BESTIMMTEN ZWECK. Details finden Sie in der GNU General Public License. 

 Sie sollten eine Kopie der GNU General Public License zusammen mit diesem 
 Programm erhalten haben. 
 Falls nicht, schreiben Sie an die Free Software Foundation, 
 Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA. 
------------------------------------------------------------------------------*/

#ifndef _STACK_H
	#define _STACK_H

#include <uart.h>

//#define DEBUG printf	//mit Debugausgabe
#define DEBUG(...) 			//ohne Debugausgabe

#define MAX_APP_ENTRY 5

struct
{
	volatile unsigned char data_present : 1;
	volatile unsigned char timer	    : 1;
	volatile unsigned char no_reset		: 1;
}eth;

typedef struct
{
	unsigned int port;		// Port
	void(*fp)(unsigned char);  	// Zeiger auf auszuführende Funktion
} TCP_PORT_ITEM;

typedef struct
{
	unsigned int port;		// Port
	void(*fp)(unsigned char);  	// Zeiger auf auszuführende Funktion
} UDP_PORT_ITEM;

TCP_PORT_ITEM TCP_PORT_TABLE[MAX_APP_ENTRY];
UDP_PORT_ITEM UDP_PORT_TABLE[MAX_APP_ENTRY];
	
#define HTONS(n) (unsigned int)((((unsigned int) (n)) << 8) | (((unsigned int) (n)) >> 8))
#define HTONS32(x) ((x & 0xFF000000)>>24)+((x & 0x00FF0000)>>8)+((x & 0x0000FF00)<<8)+((x & 0x000000FF)<<24)

#define NTOHS(x) HTONS(x)
#define NTOHS32(x) HTONS32(x)

extern unsigned char myip[4];
extern unsigned char netmask[4];
extern unsigned char router_ip[4];
extern unsigned char broadcast_ip[4];


extern unsigned int IP_id_counter;

#if defined (__AVR_ATmega32__)
	#define MAX_TCP_ENTRY 5	
	#define MAX_UDP_ENTRY 5
	#define MAX_ARP_ENTRY 5
	#define MTU_SIZE 600
#endif

#if defined (__AVR_ATmega644__) || defined (__AVR_ATmega644P__)
	#define MAX_TCP_ENTRY 5	
	#define MAX_UDP_ENTRY 5
	#define MAX_ARP_ENTRY 5
	#define MTU_SIZE 1200
#endif

#if defined (__AVR_ATmega128__)
	#define MAX_TCP_ENTRY 5	
	#define MAX_UDP_ENTRY 5
	#define MAX_ARP_ENTRY 5
	#define MTU_SIZE 1200
#endif

#if defined (__AVR_ATmega1284__) || defined (__AVR_ATmega1284P__)
	#define MAX_TCP_ENTRY 10	
	#define MAX_UDP_ENTRY 10
	#define MAX_ARP_ENTRY 10
	#define MTU_SIZE 1600
#endif


#define ARP_REPLY_LEN		60
#define ICMP_REPLY_LEN		98
#define ARP_REQUEST_LEN		42

extern unsigned char eth_buffer[MTU_SIZE+1];

#define TCP_TIME_OFF 		0xFF
#define TCP_MAX_ENTRY_TIME	3
#define MAX_TCP_PORT_OPEN_TIME 30 //30sekunden
#define MAX_TCP_ERRORCOUNT	5

#define ARP_MAX_ENTRY_TIME 100 //100sec.


#define MAX_WINDOWS_SIZE (MTU_SIZE-100)

struct arp_table
{
	volatile unsigned char arp_t_mac[6];
	volatile unsigned long arp_t_ip;
	volatile unsigned int arp_t_time;
};

struct tcp_table
{
	volatile unsigned long ip				;
	volatile unsigned int  src_port		;
	volatile unsigned int  dest_port		;
	volatile unsigned long ack_counter	;
	volatile unsigned long seq_counter	;
	volatile unsigned char status			;
	volatile unsigned int  app_status		;
	volatile unsigned char time			;
	volatile unsigned char error_count	;
	volatile unsigned char first_ack	:1	;
};

typedef struct
{
    unsigned char ip1[4];
    volatile unsigned char no;
    volatile unsigned char result;
}PING_STRUCT;

extern PING_STRUCT ping;
//----------------------------------------------------------------------------
//Prototypen
unsigned int  htons(unsigned int val);
unsigned long htons32(unsigned long val);
unsigned int  ntohs(unsigned int val);
unsigned long ntohs32(unsigned long val);
void stack_init (void);
void check_packet (void);
void eth_get_data (void);

void new_eth_header (unsigned char *,unsigned long);

char arp_entry_search (unsigned long);
void arp_reply (void);
void arp_entry_add (void);
char arp_request (unsigned long);

void make_ip_header (unsigned char *,unsigned long);
void icmp_send (unsigned long,unsigned char,unsigned char,unsigned int,unsigned int);
unsigned int checksum (unsigned char *,unsigned int,unsigned long);

void udp_socket_process(void);

void tcp_entry_add (unsigned char *);
void tcp_socket_process(void);
char tcp_entry_search (unsigned long ,unsigned int);
void tcp_Port_close (unsigned char);
void tcp_port_open (unsigned long, unsigned int, unsigned int);
void tcp_index_del (unsigned char);
void create_new_tcp_packet(unsigned int,unsigned char);
void create_new_udp_packet(	unsigned int,unsigned int,unsigned int,unsigned long);

void find_and_start (unsigned char index);
void tcp_timer_call (void);
void arp_timer_call (void);
void add_tcp_app (unsigned int, void(*fp1)(unsigned char));
void kill_tcp_app (unsigned int port);
void add_udp_app (unsigned int, void(*fp1)(unsigned char));
void kill_udp_app (unsigned int port);
void change_port_tcp_app (unsigned int, unsigned int);
void pinging (void);

#define ETHER_OFFSET			0x00
#define ARP_OFFSET				0x0E
#define IP_OFFSET				0x0E
#define ICMP_OFFSET				0x22
#define ICMP_DATA				0x2A
#define TCP_OFFSET				0x22
#define UDP_OFFSET				0x22

struct arp_table arp_entry[MAX_ARP_ENTRY];
struct tcp_table tcp_entry[MAX_TCP_ENTRY+1];

//IP Protocol Types
#define	PROT_ICMP				0x01	//zeigt an die Nutzlasten enthalten das ICMP Prot
#define	PROT_TCP				0x06	//zeigt an die Nutzlasten enthalten das TCP Prot.
#define	PROT_UDP				0x11	//zeigt an die Nutzlasten enthalten das UDP Prot.	

//Defines für IF Abfrage
#define IF_MYIP 				(ip->IP_Destaddr==*((unsigned long*)&myip[0]))
#define IP_UDP_PACKET 			(ip->IP_Proto == PROT_UDP)
#define IP_TCP_PACKET 			(ip->IP_Proto == PROT_TCP)
#define IP_ICMP_PACKET 			(ip->IP_Proto == PROT_ICMP)

//Defines für if Abfrage
#define ETHERNET_IP_DATAGRAMM	ethernet->EnetPacketType == 0x0008
#define ETHERNET_ARP_DATAGRAMM 	ethernet->EnetPacketType == 0x0608

#define IP_VERS_LEN 			20
#define TCP_HDR_LEN 			20
#define UDP_HDR_LEN				8
#define ETH_HDR_LEN 			14
#define TCP_DATA_START			(IP_VERS_LEN + TCP_HDR_LEN + ETH_HDR_LEN)
#define UDP_DATA_START			(IP_VERS_LEN + UDP_HDR_LEN + ETH_HDR_LEN)
#define UDP_DATA_END_VAR        (ETH_HDR_LEN + ((eth_buffer[IP_PKTLEN]<<8)+eth_buffer[IP_PKTLEN+1]) - UDP_HDR_LEN + 8)

#define	TCP_HDRFLAGS_FIX		0x2E
#define TCP_DATA_START_VAR		(ETH_HDR_LEN + IP_VERS_LEN + ((eth_buffer[TCP_HDRFLAGS_FIX] & 0xF0) >>2))

#define	IP_PKTLEN				0x10
#define TCP_DATA_END_VAR 		(ETH_HDR_LEN + IP_VERS_LEN + ((eth_buffer[IP_PKTLEN]<<8)+eth_buffer[IP_PKTLEN+1]) - ((eth_buffer[TCP_HDRFLAGS_FIX] & 0xF0) >>2))

//TCP Flags
#define  FIN_FLAG				0x01	//Datenübertragung beendet (finished)
#define  SYN_FLAG				0x02	//Verbindungs aufbauen (synchronize)
#define  RST_FLAG				0x04	//Verbindung neu initialisieren (reset)
#define  PSH_FLAG				0x08	//Datenübergabe an Anwendung beschleunigen (push)
#define  ACK_FLAG				0x10	//Datenübertragung bestätigen (acknowledged)

//----------------------------------------------------------------------------
//Aufbau eines Ethernetheader
//
//
//
//
struct Ethernet_Header	{
	unsigned char EnetPacketDest[6];	//MAC Zieladresse 6 Byte
	unsigned char EnetPacketSrc[6];	//MAC Quelladresse 6 Byte
	unsigned int EnetPacketType;		//Nutzlast 0x0800=IP Datagramm;0x0806 = ARP
};

//----------------------------------------------------------------------------
//Aufbau eines ARP Header
//	
//	2 BYTE Hardware Typ				|	2 BYTE Protokoll Typ	
//	1 BYTE Länge Hardwareadresse	|	1 BYTE Länge Protokolladresse
//	2 BYTE Operation
//	6 BYTE MAC Adresse Absender		|	4 BYTE IP Adresse Absender
//	6 BYTE MAC Adresse Empfänger	|	4 BYTE IP Adresse Empfänger	
struct ARP_Header	{
		unsigned int ARP_HWType;		//Hardware Typ enthält den Code für Ethernet oder andere Link Layer
		unsigned int ARP_PRType;		//Protokoll Typ enthält den Code für IP o. anderes Übertragungsprotokoll
		unsigned char ARP_HWLen;		//Länge der Hardwareadresse enthält 6 für 6 Byte lange MAC Adressen
		unsigned char ARP_PRLen;		//Länge der Protocolladresse enthält 4 für 4 Byte lange IP Adressen
		unsigned int ARP_Op;			//Enthält Code der signalisiert ob es sich um eine Anfrage o. Antwort handelt
		unsigned char ARP_SHAddr[6];	//Enthält die MAC Adresse des Anfragenden  
		unsigned long ARP_SIPAddr;    //Enthält die IP Adresse des Absenders
		unsigned char ARP_THAddr[6];	//MAC Adresse Ziel, ist in diesem Fall 6 * 00,da die Adresse erst noch herausgefunden werden soll (ARP Request)
		unsigned long ARP_TIPAddr;    //IP Adresse enthält die IP Adresse zu der die Kommunikation aufgebaut werden soll 
};

//----------------------------------------------------------------------------
//Aufbau eines IP Datagramms (B=BIT)
//	
//4B Version	|4B Headergr.	|8B Tos	|16B Gesamtlänge in Bytes	
//16B Identifikation			|3B Schalter	|13B Fragmentierungsposition
//8B Time to Live	|8B Protokoll	|16B Header Prüfsumme 
//32B IP Quelladresse
//32B IB Zieladresse
struct IP_Header	{
	unsigned char	IP_Vers_Len;	//4 BIT Die Versionsnummer von IP, 
									//meistens also 4 + 4Bit Headergröße 	
	unsigned char	IP_Tos;			//Type of Service
	unsigned int	IP_Pktlen;		//16 Bit Komplette Läng des IP Datagrams in Bytes
	unsigned int	IP_Id;			//ID des Packet für Fragmentierung oder 
									//Reassemblierung
	unsigned int	IP_Frag_Offset;	//wird benutzt um ein fragmentiertes 
									//IP Packet wieder korrekt zusammenzusetzen
	unsigned char	IP_ttl;			//8 Bit Time to Live die lebenszeit eines Paket
	unsigned char	IP_Proto;		//Zeigt das höherschichtige Protokoll an 
									//(TCP, UDP, ICMP)
	unsigned int	IP_Hdr_Cksum;	//Checksumme des IP Headers
	unsigned long	IP_Srcaddr;		//32 Bit IP Quelladresse
	unsigned long	IP_Destaddr;	//32 Bit IP Zieladresse
};

//----------------------------------------------------------------------------
//Aufbau einer ICMP Nachricht
//	
//8 BIT Typ	|8 BIT Code	|16 BIT Prüfsumme	
//Je nach Typ			|Nachricht unterschiedlich
//Testdaten
//
//8 BIT Typ = 0 Echo Reply oder 8 Echo request	
struct ICMP_Header {
		unsigned char     	ICMP_Type;		//8 bit typ identifiziert Aufgabe der Nachricht 
											//0 = Echo Reply 8 = Echo Request
		unsigned char     	ICMP_Code;		//8 Bit Code enthält Detailangaben zu bestimmten Typen
		unsigned int     	ICMP_Cksum;		//16 Bit Prüfsumme enthält die CRC Prüfsumme
		unsigned int     	ICMP_Id;		//2 byte Identifizierung
		unsigned int     	ICMP_SeqNum;	//Sequenznummer
};

//----------------------------------------------------------------------------
//TCP Header Layout
//
//
struct TCP_Header {
	unsigned int 	TCP_SrcPort;	//der Quellport für die Verbindung (Socket)
	unsigned int 	TCP_DestPort;	//der Zielport für die Verbindung (Socket)
	unsigned long	TCP_Seqnum;	//numerierter Bytestrom
	unsigned long	TCP_Acknum;	//numerierter Bytestrom (Gegenrichtung)
	unsigned char  TCP_Hdrlen;
	unsigned char 	TCP_HdrFlags;//4Bit Länge des Headers in 32 Bit schritten rest Flags
	unsigned int	TCP_Window;	//Fenstergröße max. 64K Byte
	unsigned int	TCP_Chksum;	//Enthält eine Prüfsumme über Header und Datenbereich
	unsigned int	TCP_UrgentPtr;	//16 Bit für dringende Übertragung
};

//----------------------------------------------------------------------------
//UDP Header Layout
//
//
struct UDP_Header {
	unsigned int 	udp_SrcPort;	//der Quellport für die Verbindung (Socket)
	unsigned int 	udp_DestPort;	//der Zielport für die Verbindung (Socket)
	unsigned int   udp_Hdrlen;
	unsigned int	udp_Chksum;	//Enthält eine Prüfsumme über Header und Datenbereich
};

#endif // #define _STACK_H
