# cbmdisk

A simple sd card based IEEE-488 CBM floppy emulator 
Copyright (C) 2015, 2019 Guenter Bartsch

Most of the code originates from:

NODISKEMU - SD/MMC to IEEE-488 interface/controller
Copyright (c) 2015 Nils Eilers. 

which is based on:

sd2iec by Ingo Korb (et al.), http://sd2iec.de
Copyright (C) 2007-2014  Ingo Korb <ingo@akana.de>

Inspired by MMC2IEC by Lars Pontoppidan et al.
FAT filesystem access based on code from ChaN and Jim Brain, see ff.c|h.

JiffyDos send based on code by M.Kiesel
Fat LFN support and lots of other ideas+code by Jim Brain
Final Cartridge III fastloader support by Thomas Giesel
Original IEEE488 support by Nils Eilers
FTP server and most of the IEEE 488 FSM implementation by G. Bartsch.

License: Schematic: LGPLv2, Firmware: GPL (version 2 only, see individual source files for details)

Very much work in progress at the moment. 

# NODISKEMU

FIXME:This file still needs to be expanded. A lot.
FIXME: sprinkle mentions of IEEE488 where appropiate
FIXME: Rewrite this README to better suit NODISKEMU's needs

Introduction:
=============
NODISKEMU is firmware, used in hardware designs like MMC2IEC, SD2IEC, or uIEC,
that allows the Commodore serial bus to access removable storage devices
(MMC, SD, CF) - think of it as a 1541 with a modern storage medium instead
of disks. The project was inspired by (and uses a few bits of code from)
MMC2IEC[1] by Lars Pontoppidan and once ran on the same hardware before it
grew too big for the ATmega32 used there.

Currently, the firmware provide good DOS and file-level compatibility with CBM
drives, but much work remains.  Unless specifically noted, anything that tries
to execute code on the 1541 will not work, this includes every software
fastloader.

[1] Homepage: http://pontoppidan.info/lars/index.php?proj=mmc2iec

Please note: Whenever this file talks about "D64 images" the text applies to
all Dxx image types, i.e. D64/D71/D81/DNP unless specifically noted.

If you are the author of a program that needs to detect NODISKEMU for
some reason, DO NOT use M-R for this purpose. Use the UI command
instead and check the message you get for "nodiskemu" instead.


Supported commands:
===================
- General notes:
  Any command not listed below is currently not supported.

- Directory filters:
  To show only directories, both =B (CMD-compatible) and =D can be used.
  On a real Commodore drive D matches everything.
  To include hidden files in the directory, use *=H - on a 1541 this doesn't
  do anything. NODISKEMU marks hidden files with an H after the lock mark,
  i.e. "PRG<H" or "PRG H".

  CMD-style "short" and "long" directory listings with timestamps are supported
  ("$=T"), including timestamp filters. Please read a CMD manual for the syntax
  until this file is updated.

- Partition directory:
  The CMD-style partition directory ($=P) is supported, including filters
  ($=P:S*). All partitions are listed with type "FAT", although this could
  change to "NAT" later for compatibility.

- CD/MD/RD:
  Subdirectory access is compatible to the syntax used by the CMD drives,
  although drive/partition numbers are completely ignored.

  Quick syntax overview:
    CD:_         changes into the parent dir (_ is the left arrow on the C64)
    CD_          dito
    CD:foo       changes into foo
    CD/foo       dito
    CD//foo      changes into \foo
    CD/foo/:bar  changes into foo\bar
    CD/foo/bar   dito

  You can use wildcards anywhere in the path. To change into an D64
  image the image file must be the last component in the path, either
  after a slash or a colon character.

  MD uses a syntax similiar to CD and will create the directory listed
  after the colon (:) relative to any directory listed before it.

    MD/foo/:bar  creates bar in foo
    MD//foo/:bar creates bar in \foo

  RD can only remove subdirectories of the current directory.

    RD:foo       deletes foo

  CD is also used to mount/unmount image files. Just change into them
  as if they were a directory and use CD:_ (left arrow on the C64) to leave.
  Please note that image files are detected by file extension and file size
  and there is no reliable way to see if a file is a valid image file.

- CP, C<Shift-P>
  This changes the current partition, see "Partitions" below for details.

- C:
  File copy command, should be CMD compatible. The syntax is
   C[partition][path]:targetname=[[partition][path]:]sourcename[,[[p][p]:]sourcename...]
  You can use this command to copy multiple files into a single target
  file in which case all source files will be appended into the target
  file. Parsing restarts for every source file name which means that
  every source name is assumed to be relative to the current directory.
  You can use wildcards in the source names, but only the first
  file matching will be copied.

  Copying REL files should work, but isn't tested well. Mixing REL and
  non-REL files in an append operation isn't supported.

- D
  Direct sector access, this is a command group introduced by NODISKEMU.
  Some Commodore drives use D for disk duplication between two drives
  in the same unit, an attempt to use that command with NODISKEMU should
  result in an error message.

  D has three subcommands: DI (Info), DR (Read) and DW (Write).
  Each of those commands requires a buffer to be opened (similiar
  to U1/U2), but due to the larger sector size of the storage devices
  used by NODISKEMU it needs to be a large buffer of size 2 (512 bytes)
  or larger. The exception is the DI command with page set to 0,
  its result will always fir into a standard 256 byte buffer.
  If you try to use one of the commands with a buffer that is too
  small a new error message is returned, "78,BUFFER TOO SMALL,00,00".

  In the following paragraphs the secondary address that was used
  to open the buffer is called "bufchan".

  - DI
    In BASIC notation the command format is
      "DI"+chr$(bufchan)+chr$(device)+chr$(page)

    "device" is the number of the physical device to be queried,
    "page" the information page to be retrieved. Currently the
    only page implemented is page 0 which will return the
    following data structure:
     1 byte : Number of valid bytes in this structure
              This includes this byte and is meant to provide
              backwards compatibility if this structure is extended
              at a later time. New fields will always be added to
              the end so old programs can still read the fields
              they know about.
     1 byte : Highest diskinfo page supported
              Always 0 for now, will increase if more information
              pages are added (planned: Complete ATA IDENTIFY
              output for IDE and CSD for SD)
     1 byte : Disk type
              This field identifies the device type, currently
              implemented values are:
                0  IDE
                2  SD
                3  (reserved)
     1 byte : Sector size divided by 256
              This field holds the sector size of the storage device
              divided by 256.
     4 bytes: Number of sectors on the device 
              A little-endian (same byte order as the 6502) value
              of the number of sectors on the storage device.
              If there is ever a need to increase the reported
              capacity beyond 2TB (for 512 byte sectors) this
              field will return 0 and a 64-bit value will be added
              to this diskinfo page.

    If you want to determine if there is a device that responds
    to a given number, read info page 0 for it. If there is no
    device present that corresponds to the number you will see
    a DRIVE NOT READY error on the error channel and the
    "number of valid bytes" entry in the structure will be 0.

    Do not assume that device numbers are stable between releases
    and do not assume that they are continuous either. To scan
    for all present devices you should query at least 0-7 for now,
    but this limit may increase in later releases.

  - DR/DW
    In BASIC notation the command format would be
      "DR"+chr$(bufchan)+chr$(device)
          +chr$(sector AND 255)
          +chr$((sector/256) AND 255)
          +chr$((sector/65536) AND 255)
          +chr$((sector/16777216) AND 255)
    (or "DW" instead of "DR)
    but this won't work on the C64 because AND does not accept
    parameters larger than 32767. The principle should be clear
    though, the last four bytes are a 32 bit sector number in
    little-endian byte order.

    DR reads the sector to the buffer, DW writes the contents
    of the buffer to the sector. Both commands will update the
    error channel if an error occurs, for DR the 20,READ ERROR
    was chosen to represent read errors; for write problems
    during DW it sets 25,WRITE ERROR for errors and 26,WRITE
    PROTECT ON if the device is read-only.

- G-P
  Get partition info, see CMD FD/HD manual for details. The reported
  information is partially faked, feedback is welcome.

- P
  Positioning doesn't just work for REL files but also for regular
  files on a FAT partition. When used for regular files the format
  is "P"+chr$(channel)+chr$(lo)+chr$(midlo)+chr$(midhi)+chr$(hi)
  which will seek to the 0-based offset hi*2^24+midhi*65536+256*midlo+lo
  in the file. If you send less than four bytes for the offset, the
  missing bytes are assumed to be zero.

- N:
  Format works only if a D64 image is already mounted. This command will
  be ignored for DNP images unless the current directory is the root
  directory of the DNP image.

- R
  Renaming files should work the same as it does on CMD drives, although
  the errors flagged for invalid characters in the name may differ.

- S:
  Name matching is fully supported, directories are ignored.
  Scratching of multiple files separated by , is also supported with no
  limit to the number of files except for the maximum command line length
  (usually 100 to 120 characters).

- T-R and T-W
  If your hardware features RTC support the commands T-R (time read) and T-W
  (time write) are available. If the RTC isn't present, both commands return
  30,SYNTAX ERROR,00,00; if the RTC is present but not set correctly T-R will
  return 31,SYNTAX ERROR,00,00.

  Both commands expect a fourth character that specifies the time format
  to be used. T-W expects that the new time follows that character
  with no space or other characters inbetween. For the A, B and D
  formats, the expected input format is exactly the same as returned
  by T-R with the same format character; for the I format the day of
  week is ignored and calculated based on the date instead.

  The possible formats are:
   - "A"SCII: "SUN. 01/20/08 01:23:45 PM"+CHR$(13)
      The day-of-week string can be any of "SUN.", "MON.", "TUES", "WED.",
      "THUR", "FRI.", "SAT.". The year field is modulo 100.

   - "B"CD or "D"ecimal:
     Both these formats use 9 bytes to specify the time. For BCD everything
     is BCD-encoded, for Decimal the numbers are sent/parsed as-is.
      Byte 0: Day of the week (0 for sunday)
           1: Year (modulo 100 for BCD; -1900 for Decimal, i.e. 108 for 2008)
           2: Month (1-based)
           3: Day (1-based)
           4: Hour   (1-12)
           5: Minute (0-59)
           6: Second (0-59)
           7: AM/PM-Flag (0 is AM, everything else is PM)
           8: CHR$(13)

      When the time is set a year less than 80 is interpreted as 20xx.

   - "I"SO 8601 subset: "2008-01-20T13:23:45 SUN"+CHR$(13)
     This format complies with ISO 8601 and adds a day of week
     abbreviation using the same table as the A format, but omitting
     the fourth character. When it is used with T-W, anything beyond
     the seconds field is ignored and the day of week is calculated
     based on the specified date. The year must always be specified
     including the century if this format is used to set the time.
     To save space, NODISKEMU only accepts this particular date/time
     representation when setting the time with T-WI and no other ISo
     8601-compliant representation.

- U0
  Device address changing with "U0>"+chr$(new address) is supported,
  other U0 commands are currently not implemented.

- U1/U2/B-R/B-W
  Block reading and writing is fully supported while a D64 image is mounted.

- B-P
  Supported, not checked against the original rom at all.

- UI+/UI-
  Switching the slightly faster bus protocol for the VC20 on and off works,
  it hasn't been tested much though.

- UI/UJ
  Soft/Hard reset - UI just sets the "73,..." message on the error channel,
  UJ closes all active buffers but doesn't reset the current directory,
  mounted image, swap list or anything else.

- U<Shift-J>
  Real hard reset - this command causes a restart of the AVR processor
  (skipping the bootloader if installed). <Shift-J> is character code 202.

- X: Extended commands. If you use JiffyDOS, you can send them by using
  @"X..." - without quotes you'll just receive an error.

  - XEnum    Sets the "file extension mode". This setting controls if
             files on FAT are written with an x00 header and extension or not.
             Possible values for num are:
               0: Never write x00 format files.
               1: Write x00 format files for SEQ/USR/REL, but not for PRG
               2: Always write x00 format files.
               3: Use SEQ/USR/REL file extensions, no x00 header
               4: Same as 3, but also for PRG
             If you set mode 3 or 4, extension hiding is automatically enabled.
             This setting can be saved in the EEPROM using XW, the default
             value is 1.

             For compatibility with existing programs that write D64 files,
             PRG files that have D64, D41, D71, D81, DNP or M2I as an extension
             will always be written without an x00 header and without
             any additional PRG file extension.

  - XE+/XE-  Enable/disable extension hiding. If enabled, files in FAT with
             a PRG/SEQ/USR/REL extension will have their extension removed
             and the file type changed to the type specified by the file
             extension - e.g. APPLICATION.PRG will become a PRG file named
             "APPLICATION", "README.SEQ" will become a SEQ file named "README".
             This flag can be saved in the EEPROM using XW, the default
             value is disabled (-).

  - XInum    Switches the display mode for mountables files (i.e. disk images
             and M2I). num can be 0, in which case the file will be shown
             with its normal type in the directory or 1 which will show all
             mountable files as DIRectory entries (but they can still be
             accessed as files too) or 2 in which case they will show up
             twice - once with its normal type and once as directory.
             The default value is 0 and this setting can be stored
             permanently using XW.

             It may be useful to set it to 1 or 2 when using software that
             was originally written for CMD devices and which wouldn't
             recognize disk images/M2I files as mountable on its own.
             However, due to limitations of the current implementation of
             the CD command such software may still fail to mount a disk
             image with this option enabled.

  - X*+/X*-  Enable/disable 1581-style * matching. If enabled, characters
             after a * will be matched against the end of the file name.
             If disabled, any characters after a * will be ignored.
             This flag can be saved in the EEPROM using XW, the default value
             is enabled (+).

  - XDdrv=val Configure drives.  On ATA-based units or units with multiple
             drive types, this command can be used to enable or reorder
             the drives.  drv is the drive slot (0-7), while val is one
             of:
                0: Master ATA device
                1: Slave ATA device
                4: Primary SD/MMC device
                5: Secondary SD/MMC device
                6: (reserved)
               15: no device

             Note that only devices supported by the specific hardware
             can be selected.  Unsupported device types will return an
             error if requested.  Also, note that you cannot select a device
             in multiple drive slots.  Finally, while it is possible to
             re-order ATA devices using this functionality, it is strongly
             discouraged.  Use the master/slave jumpers on the ATA devices
             instead.  To reset the drive configuration, set all drive slots
             to "no device".  This value can be permanently saved in the
             EEPROM using XW.

    XD?      View the current drive configuration.  Example result:
             "03,D:00=04:01=00:02=01,10,01".  The track indicates the
             current device address, while the sector indicates extended
             drive configuration status information.

  - X        X without any following characters reports the current state
             of all extended parameters via the error channel, similiar
             to DolphinDOS. Example result: "03,J-:C152:E01+:B+:*+,08,00"
             The track indicates the current device address.

  - XS:name  Set up a swap list - see "Changing Disk Images" below.
    XS       Disable swap list

  - XR:name  Set the file used for file-based M-R emulation.
    XR       Disable file-based M-R emulation.
             See "M-R, M-W, M-E" below. This setting can be
             permanently saved in the EEPROM using XW.

  - XW       Store configuration to EEPROM
             This commands stores the current configuration in the EEPROM.
             It will automatically be read when the AVR is reset, so
             any changes you made will persist even after turning off
             the hardware.

             The stored configuration include the extension mode,
             drive configuration and the current device address.
             If you have changed the device address by software,
             NODISKEMU will power up with that address unless
             you have changed the device address jumpers (if available) to
             a different setting than the one active at the time the
             configuration was saved. You can think of this feature as
             changing the meaning of one specific setting of the jumpers
             to a different address if this sounds logical enough to you.

             The "hardware overrides software overrides hardware" priority
             was chosen to allow accessing NODISKEMU even when it is soft-
             configured for a device number that is already taken by
             another device on the bus without having to remove that
             device to reconfigure NODISKEMU (e.g. when using a C128D).

  - X?       Extended version query
             This commands returns the extended version string which
             consists of the version, the processor type set at build time
             and the suffix of the configuration file (usually corresponds
             to the short name of the hardware NODISKEMU was compiled for).

- M-R, M-W, M-E
  When no file is set up using XR, M-R will check a small internal
  table of common drive-detection addresses and return data that
  forces most of the supported fast loaders into a compatible mode
  (e.g. 1541 mode for Dreamload and ULoad Model 3, disabled fastloader
  for Action Replay 6). If the address is not recognized, more-or-less
  random data will be returned.

  Unfortunately GEOS reads rather large parts of the drive rom using
  M-R to detect the drive, which cannot be reasonably added into the
  internal table. To enable the GEOS drive detection to work properly
  with NODISKEMU and to allow switching between 1541/71/81 modes,
  file-based M-R emulation has been implemented. If a file has been
  set up as M-R data source using the XR command, its contents will be
  returned for M-R commands that try to read an address in the range
  of $8000-$ffff. The rom file should be a copy of the rom contents of
  a 1541/71/81 drive (any headers will be skipped automatically), its
  name must be 16 characters or less. When an M-R command is received,
  the file will be searched in three locations on the storage medium:
    1) in the current directory of the current partition
    2) in the root directory of the current partition
    3) in the root directory of the first partition

  The internal emulation table will be used if the file wasn't found
  in any of those locations or an error occured while reading
  it. Please be aware that the rom file is ONLY used for M-R
  commands. Except for some very specific situations where drive
  detection fails (e.g. GEOS) it will probably decrease compatibility
  of NODISKEMU because most of the implemented fast loaders will only
  recognize the 1541 variation of the loader.

  Memory writing knows about the address used for changing the device
  address on a 1541 and will change the address of NODISKEMU to the
  requested value. It will also check if the transmitted data
  corresponds to any of the known software fastloaders so the correct
  emulation code can be used when M-E is called.

Large buffers:
==============
To support commands which directly access the storage devices support
for larger buffers was added. A large buffer can be allocated by
opening a file named "##<d>" (exactly three characters" with <d> replaced
by a single digit specifying the number of 256-byte buffers to be
chained into one large buffer - e.g. "##2" for a 512 byte buffer,
"##4" for 1024 bytes etc. Unlike a standard buffer where the read/write
pointer is set to byte 1, a large buffer will start with the r/w pointer
pointing to byte 0 because that seems to be more sensible to the author.

If there aren't enough free buffers to support the size you requested
a 70,NO CHANNEL message is set in the error channel and no file is
opened. If the file name isn't exactly three bytes long a standard
buffer ("#") will be allocated instead for compatibility.

The B-P command supports a third parameter that holds the high byte
of the buffer position, For example, "B-P 9 4 1" positions to byte
260 (1*256+4) of the buffer on secondary address 9.

No command is implemented at this time which uses large buffers.

Long File Names:
================
Long file names (i.e names not within the 8.3 limits) are supported on
FAT, but for compatibility reasons the 8.3 name is used if the long
name exceeds 16 characters. If you use anything but ASCII characters
on the PC or their PETSCII equivalents on the Commodore you may
get strange characters on the other system because the LFN use
unicode characters on disk, but NODISKEMU parses only the low byte
of each character in the name.

EEPROM file system
==================
*WARNING*: The EEPROM file system is a newly-implemented file system
that may still contain bugs. Do not store data on it that you cannot
affort to lose. Always make sure that you have a backup. Also, the
format may change in later releases, so please expect that the
partition may need to be erased in the future.

Devices running NODISKEMU always have an EEPROM to store the system
configuration, but on some devices this EEPROM is much larger than
required. To utilize the empty space on these devices (currently any
microcontroller with at least 128K of flash), a special EEPROM file
system has been implemented. This can for example be used to store a
small file browser or fast loader so it can be used independent of the
storage medium that is currently inserted.

The EEPROM file system will always register itself on the last
partition number (see "Partitions" below). You can check the list of
partitions ("$=P") to find the current partition number of the EEPROM
file system or use the alias function (see below) to access it.

To simplify calculations, block numbers on the EEPROMFS are calculated
using 256 bytes per block instead of the usual 254 bytes as used by
Commodore drives. Internally, the allocation is even more fine-grained
(using 64 byte sectors), which means that the number of free blocks
shown on an empty file system may be less than the sum of the number
of blocks of all files on a full file system.

The EEPROM file system does not support subdirectories. It can be
formatted using the N: command as usual, but the disk name and ID are
ignored. The capacity of the EEPROM file system varies between
devices: On AVR devices it is 3.25 KBytes and at most 8 files can be
stored on it. On a2iec, the file system can hold 7 KBytes and at most
16 files can be stored on it. The actual number of files that can be
stored depends on the length of the files, longer files need more than
one directory entry.


Partitions:
===========
NODISKEMU features a multi-partition support similiar to that of the CMD
drives. The partitions (which may be on separate drives for some hardware
configurations) are accessed using the drive number of the commands
sent from the computer and are numbered starting with 1. Partition 0
is a special case: Because most software doesn't support drive numbers
or always sends drive number 0, this partition points to the currently
selected partition. By default, accesses to partition 0 will access
partition 1, this can be changed by sending "CP<num>" over the command
channel with <num> being an ASCII number from 1 to 255. "C<Shift-P"
(0x42 0xd0) works the same, but expects a binary partition number as the
third character of the command.

To allow a "stable" access to the EEPROM file system no matter how
many partitions are currently available, a special character has been
introduced that will always access the EEPROM file system (if
available). When NODISKEMU sees a "!" character where it expects a
partition number and the "!" character is directly followed by a colon
(i.e. "!:"), it will access the EEPROMFS if available. Direct access
using the assigned partition number is of course still
available. Additionally "$!" will always load the directory of the
EEPROM file system partition (if available), similar to "$1" to "$9"
for partitions 1 to 9.

Software fastloaders:
=====================
Note: Using NODISKEMU without an external crystal or similiar precise
      clock source is not a supported configuration.
      If you try that anyway, be prepared to suffer from random
      data corruption. You have been warned.
      Some fastloader implementations will actively refuse to work
      if you use an unsuitable clock source.

  Turbodisk
  ---------
  Turbodisk is detected by the CRC of its 493 byte long floppy code and
  the M-E address 0x0303. The same code seems to be used under various names,
  among them "Turbodisk" (both 2.1 and 2.2) and "Fast-Load".
  It is not known if there is an NTSC-compatible version of this fastloader.

  Final Cartridge III
  -------------------
  Both the fast loader and the fast saver of Final Cartridge III are supported.
  The FC3 is both PAL and NTSC compatible.

  The slightly different fastloader used for files freezed with the FC3
  is also supported.

  EXOS V3 and The Beast System
  ----------------------------
  Both supported, the loader used by these kernals is very similiar to
  the FC3 fast loader.

  Action Replay 6
  ---------------
  The AR6 reads a byte from the drive rom to check which fastloader it should
  use. When file-based M-R emulation is disabled NODISKEMU returns a value that
  should force the cartridge to use the standard kernal loader instead of its
  many fastloaders/-savers. This means that accessind NODISKEMU with
  file-based rom emulation enabled will fail because the cartridge
  will enable fastloader that will probably not be recognized.

  Currently the only recognized AR6 fastloader and fastsaver are the
  ones for the 1581.

  Dreamload
  ---------
  Dreamload uses direct track/sector access, so it is only supported
  on D64 or similiar disk image formats. As NODISKEMU has to wait for commands
  from the C64 constantly the disk change buttons may become unresponsive,
  try multiple times if you need to. Dreamload is a "captive" fastloader,
  NODISKEMU stay in Dreamload mode until it receives a "quit loader" command
  from the C64. To force NODISKEMU to resume normal operation, hold the disk
  change button until the red LED turns on (just like sleep mode).

  Please note that Dreamload does not work with more than one device on the
  serial bus due to the way it uses the ATN line.

  ULoad Model 3
  -------------
  ULoad Model 3 uses direct track/sector access, so it is only supported
  on D64 or similiar disk image formats. Currently there is exactly one
  supported variant of ULoad Model 3, which is the one used by
  Ultima 3 Gold. There are no other known variants at this time, but
  this may change.

  If you are a coder and want to use ULoad Model 3 in your own program,
  either configure it to produce the same drive code as U3Gold or
  contact me so we can work out a way to trigger ULoad M3 support
  without uploading any drive code at all.

  G.I. Joe Loader
  ---------------
  Said to be the most-ripped IRQ loader. Unfortunately this is a
  "captive" fastloader similiar to dreamload (but not restricted
  to disk images because it is file name-based) and there is no
  reliable way to detect if the computer has been reset to switch
  back to the standard protocol. To exit this loader, hold down
  the disk change button until the red LED turns on, just like
  sleep mode.

  Epyx FastLoad Cartridge
  -----------------------
  ONLY the fast loader from this cartridge is supported, no
  disk editor/copier/whatever functions.

  GEOS
  ----
  GEOS 2.0 can be booted from D64 images made from original disks
  as well as D41/71/81 images created using geoMakeBoot (make sure to
  Configure the system for a 1541/1571/1581 before using geoMakeBoot).
  When file-based M-R emulation is disabled, GEOS may detect NODISKEMU as
  a 1541 or 1581, depending on the version of Configure used. This may
  cause the system to fail to boot, e.g. if NODISKEMU is detected as a 1581
  while booting from a D64 disk image. It is recommended to set up file-
  based M-R emulation when using GEOS to avoid these problems.

  GEOS 1.3 may or may not work - it boots, but wasn't tested in-depth.
  Gateway seems to work but was not tested beyond booting it from a D64
  image.

  Using the buttons for changing the current disk image is supported,
  but do make sure that you only access disk images that the drive
  type that is selected in GEOS would support (i.e. D64 for a 1541,
  D64/D71 for a 1571 and D81 for a 1581).

  Wheels
  ------
  Wheels can be booted from any disk image type it supports. The correct
  rom emulation file (XR) MUST be set, especially for CMD HD emulation.

  Do not use the disk change feature to change disk images when HD emulation
  is in use - Wheels does not check for disk changes on that drive!
  For other drive types the restrictions on disk image type of GEOS also
  apply to Wheels.

  ELoad Version 1
  ---------------
  This loader was made for EasyProg but may also be used in other programs.
  It detects and supports the NODISKEMU natively.

JiffyDOS:
=========
The JiffyDOS protocol has very relaxed timing constraints compared to
Turbodisk, but still not as relaxed as the standard Commodore IEC protocol.

x00 files:
==========
P00/S00/U00/R00 files are transparently supported, that means they show
up in the directory listing with their internal file name instead of the
FAT file name. Renaming them only changes the internal name. The XE
command defines if x00 extensions are used when writing files, by
default NODISKEMU uses them for SEQ/USR/REL files but not for PRG.
Parsing of x00 files is always enabled even when writing them is not.

x00 files are recognized by checking both the extension of the file
(P/S/U/R with a two-digit suffix) and the header signature.

Disk Images:
============
Disk images are recognized by their file extension (.D64, .D41, .D71, .D81,
.DNP) and their file size (must be one of 174848, 175531, 349696, 351062,
819200 or a multiple of 65536 for DNP). If the image has an error info block
appended it will be used to simulate read errors. Writing to a sector with
an error will always work, but it will not clear the indicated error.
D81 images with error info blocks are not supported.

Warning: There is at least one program out there (DirMaster v2.1/Style by
THE WIZ) which generates broken DNP files. The usual symptom is that
moving from a subdirectory that was created with this program back to
its parent directory using CD:_ (left arrow) sets the current directory
not to the parent directory, but to an incorrect sector instead. A
workaround for this problem in NODISKEMU would require an unreasonable
amount of system resources, so it is recommended to avoid creating
subdirectories with this version of DirMaster. It is possible to fix
this problem using a hex editor, but the exact process is beyond the scope
of this document.

REL files:
==========
Partial REL file support is implemented. It should work fine for existing
files, but creating new files and/or adding records to existing files
may fail. REL files in disk images are not supported yet, only as files
on a FAT medium. When x00 support is disabled the first byte of a REL
file is assumed to be the record length.

Changing Disk Images
====================
Because some programs require more than one disk side there is support
for changing the currently mounted disk image with two buttons called
NEXT and PREV connected to the AVR.

If your circuit doesn't have disk change pins/buttons you might be able to
add it yourself. In all cases the buttons need to connect the given
pins of the chip to ground.

- For the original MMC2IEC ("larsp"):
  The NEXT button is in input PA4, the PREV button is on PA5.
  PA4 is pin 36 on the DIL version of the controller or pin 33 on the
  surface-mount version. PA5 is pin 35 on DIL, pin 32 on SMD.

- For Shadowolf's MMC2IEC 1.x PCBs ("sw1"):
  The NExT button is on input PC4, the PREV button is on PC3.
  PC4 is pin 26 on the DIL version of the controller or pin 23 on the
  surface-mount version. PC3 is pin 25 on DIL, pin 22 on SMD.

- For Shadowolf's NODISKEMU 1.x PCBs ("sw2"):
  The two required pins are available on the pin header which runs
  parallel to the long side of the board. In the documentation of the
  board, the NEXT button is named "DISKSWITCH", the PREV button is
  named "RESERVE".

- Any other circuit without disk change pin on a convenient connector
  somewhere and no button dedicated to that function: Please check
  with the supplier of the board and read config.h in the sources
  to find out how to connect it.

To use this functionality, you can either create a swap list file
yourself or let NODISKEMU create one for you.

Creating a swap list
--------------------
A swap list is a text file with one line per disk image or directory
you want to be able to change into. You are not limited to using disk
images, a swap list file may also refer to standard directories on the
SD card or anything else the CD command of NODISKEMU will accept.

The swap list file is relatively tolerant against multiple styles of
line-endings, NODISKEMU should be able to parse the file no matter if you
create it on a Windows system, Unix or even the C64 itself - as a side
effect, empty lines are also ignored. By default NODISKEMU assumes that
the file is encoded in ASCII (for files created on a PC or similar),
but if the first line of the file exactly reads "#PETSCII" (in hex: 23
50 45 54 53 43 49 49), file names are assumed to be encoded in PETSCII
instead and this marker line is skipped.

An example swap list file could look like this:
=== example 1 ===
FOO.D64
BAR.D64
BAZ.D64
=== end of example 1 ===

=== example 2 ===
//NEATGAME/:DISK1A.D64
//NEATGAME/:DISK1B.D64
//NEATGAME/:DISK2A.D64
//NEATGAME/:DISK2B.D64
=== end of example 2 ===

The swap list is enabled by sending "XS:filename" over the command
channel with filename being the name of the swap list. A list
activated in this way stays active until you explicitly disable it
again by sending "XS" on the command channel or you manually activate
another swap list with "XS:otherfilename".

Since the manual activation of swap lists is still a bit of a hassle,
NODISKEMU will automatically try to activate a swap list named
"AUTOSWAP.LST" in the current directory if you use the disk change
buttons while no swap list is active. A swap list enabled in this way
behaves almost exactly as a swap list enabled with XS, but it
auto-deactivates when a CD (change directory) command is received by
NODISKEMU. This way a different AUTOSWAP.LST file is always correctly
recognized after you have changed into a different directory.

NODISKEMU can even auto-generate a swap list for you that contains all
disk images (e.g. D64/D71/D81/DNP) in the current directory if no
AUTOSWAP.LST is present in this directory. To do so, change into the
directory that you want scanned and use the HOME function (see below).
NODISKEMU will then create a file called AUTOSWAP.GEN and activate it as
if it was the standard AUTOSWAP.LST, including its auto-deactivation
features. The AUTOSWAP.GEN file will never be recognized the same way
as AUTOSWAP.LST, so you will need to either rename the file
(R:AUTOSWAP.LST=AUTOSWAP.GEN) or ask NODISKEMU to generate it again by
using the HOME function in the same directory if you want to use it
again. This mode of operation was chosen to avoid the accidental
destruction of pre-existing AUTOSWAP.LST files and to allow NODISKEMU to
recognize newly-added disk images in the directory without manually
removing the generated swap list.

Please note that using the swap-list auto generation feature will
currently leave an empty AUTOSWAP.GEN file on the card if no disk
images were found. This may be fixed in the future.

Using a swap list
-----------------
Navigation in a swap list is really simple: Press the NEXT button to
activate the next line of the file or the PREV button to activate the
previous line of the file. Both of these buttons wrap to the other end
of the file if they hit the beginning/end of the list. You can also
hit both buttons together to trigger the HOME function which jumps to
the first entry of the swap list.

NODISKEMU will confirm each of these three actions with a specific
flashing pattern on the device's LEDs. The pattern first flashed both
the red and green LEDs on for a short moment, then it flashes either
one or both of them. For the NEXT function, the green LED flashes; for
the PREV function the red LED flashes and for HOME both LEDs flash.

If any of these three functions is activated without an active swap
list and NODISKEMU finds an AUTOSWAP.LST file, they will all be treated
as the HOME function: The first line of the file is active and the
red and green LEDs both flash twice. The same happens when an
AUTOSWAP.GEN file is created, although the flashing pattern may not be
very discernible because of the preceding card activity.


Sleep Mode:
===========
If you hold the disk change button down for two seconds, NODISKEMU will
enter "sleep mode". In this mode it doesn't listen to the bus at all
until you hold down the disk change button for two seconds again
which resumes normal operation. Sleep mode allows you to keep
NODISKEMU connected to the serial bus even when you load something
from a different drive that uses a fast loader that doesn't
work with more than one device on the bus.

While sleep mode is active, the red LED will be on and the green LED
will be off.

Card detection test:
====================
Because some SD slots seem to suffer from bad/unreliable card detect
switches a test mode for this has been implemented on the units that
have SD card support. If you hold down the PREV button during powerup,
the red (dirty) LED will reflect the card detect status - if the LED
is on the card detect switch is closed. Please note that this does not
indicate successful communication with the card but merely that the
mechanical switch in the SD card slot is closed.

On units with two NODISKEMU-controlled LEDs, the green (busy) LED will
indicate the state of the write protect switch - if the LED is lit,
the write protection is on. Due to the way the write protect notch
works on SD cards, the indication is only valid when the card is fully
inserted into the slot.

To exit from the diagnostic mode, power-cycle the device or push the
NEXT button once.

Other important notes:
======================
- When you hold down the disk change (forward) button during power
  on the software will use default values instead of those stored
  in the EEPROM.
- File overwrite (@foo) is implemented by deleting the file first.
- File sizes in the directory are in blocks (of 254 bytes), but
  the blocks free message actually reports free clusters. It is
  a compromise of compatibility, accuracy and code size.
- If known, the low byte of the next line link pointer of the directory
  listing will be set to (filesize MOD 254)+2, so you can calculate the
  true size of the file if required. The 2 is added so it can never be
  mistaken for an end marker (0) or for the default value (1, used by at
  least the 1541 and 1571 disk drives).
- If your hardware supports more than one SD card, changing either one
  will reset the current partition to 1 and the current directory of
  all partitions to the root drive. Doing this just for the card that
  was changed would cause lots of problems if the number of partitions
  on the previous and the newly inserted cards are different.


