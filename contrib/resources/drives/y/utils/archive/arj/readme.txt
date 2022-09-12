
   ARJ version 2.78 Open-Source                            June 23rd, 2005


   INTRODUCTION

      To meet  the data  archiving  needs, ARJ Software  Russia delivers a
      range of its own  products based  on the code of  original ARJ. From
      the  beginning,  our  goal  was  to  retain  the  functionality  and
      compatibility with  the  original ARJ  for DOS, but we  also provide
      features  and  enhancements  that  are a  must for certain  tasks or
      certain platforms where no ARJ has been before.


   NEW FEATURES AND DIFFERENCES FROM THE ORIGINAL ARJ

      ("-" indicates a  missing feature, "*" - a different  operation and
       "+" stands for a feature unique for our implementation)

      -  There may be a  significant  performance drawback  when accessing
         files on volumes with no write-back cache.

      *  The "-hm" options  are  compatible  with their ARJ implementation
         hence  a  temporary  swap  file  is  created  if  the  file  list
         exceeds  3000  files.  To keep  the  entire list  in  memory, use
         -hm65000. This  is  useful if you are running  a non-DOS version,
         have  an   adequate  amount  of   RAM  and   therefore   are  not
         constrained with RAM occupied by file lists.

      *  The ARJ display  program, ARJ$DISP, has been  renamed to ARJDISP.
         If you are using a custom  display module, you  have to rename it
         as well.

      *  "-t1g"  can  really  be  used  as  a   shortcut  for "-t1gf",  as
         documented but not implemented in the original ARJ.

      *  Parameters  accepting numeric  quantities (such as "-v" or "-2i")
         tolerate  both  decimal and  hexadecimal values. To denote a  hex
         value, place  "0x" in  front  of it, as  in "-2i0x1000" (equal to
         "-2i4096").

         The  multipliers 'K'  and  'M' have  been  supplemented  with 'G'
         (giga-) and three currently reserved quantities: 'T' (tera-), 'P'
         (peta-) and 'E' (exa-). All  modifiers imply  a decimal power, so
         "-v1G" is equivalent to "-v1000000000" or "-v1000M".

         These two changes may require a review of the existing ARJ setup,
         as marshalling certain switches together ("-jd0x1") may no longer
         have  the  desired  effect ("-jd0", "-x1"). This does  not affect
         the parameters like "-m4" or "-a1" where the digit is a modifier,
         not a free-form value, and thus will go unnoticed for most of the
         practical configurations.

      *  Comments specified with "-jz" or "-z" will retain  their original
         layout in the archive, without replacing their last character for
         newline.

      +  "ARJ a -d1" will delete  files without asking for  permission, as
         "ARJ m". "ARJ a -d2" will  truncate  files, rather than  deleting
         them, which is usually suggested for keeping hard links.

      +  The "-_" option can be used to convert filenames into lower case.
         When  adding  files, the  filenames  stored  in the  archive  are
         folded  down  into  lowercase. When  extracting files, a  similar
         conversion is carried out for filenames being restored. No checks
         for duplicate filenames are made. On case-sensitive file systems,
         the "-jt1" switch is not operable in conjunction with "-_".

      +  The "-h#" option  has been  improved to  allow  custom  date/time
         formats.  A  custom  format  is  specified  by  putting a  format
         sequence   right   after  the "-h#".  The  following   characters
         represent date/time macros:

          Y = year,       M = month,      D = day
          h = hour,       m = minute,     s = second
          N = day of year

          (note that these are case-sensitive)

         All  other  characters,  as  well as  those going  beyond  format
         limits (4  digits for year, 2 digits for all  other fields),  are
         treated as delimiters. Examples:

         ARJ a project- -h#YYYYMMDD              (project-19991022.arj)
         ARJ a backup- -h#MM-DD_hh-mm-ss    (backup-10-22_23-57-16.arj)
         ARJ a specs -h#YY                                (specs99.arj)
         ARJ a logs_ -h#NNN                              (logs_295.arj)
         ARJ a test -h#YYYYYYYY                      (testYYYY1999.arj)

      +  The "-ha" has been improved. Now, when  used  in conjuction  with
         an archiving  command, it does  not mark read-only  files as such
         in archive.  This simplifies  archiving from  CD-ROM media  where
         virtualized read-only  attribute  is  forced  by  respective IFS
         driver.

      +  The  "-2a"  option  is  implemented  in  ARJ  due  to  a  popular
         demand from  FidoNet system  operators.  Basically it  acts  like
         "-jo",  with   the   exception  that  file  names,  and  not  the
         extensions,  are  "serialized". Consider  having  a  file  called
         "FILE995N.TXT" and  an archive that contains  the  same  file. If
         extracted  with  the "-2a"  option,  the  file  will  be  written
         to  "FILE9950.TXT", if  you extract it  again,  it'll  be  called
         "FILE9951.TXT",  and   so   on   up   to   "FILE9959.TXT",   then
         "FILE9960.TXT". And  after  "FILE9999.TXT" ARJ  will  start  with
         "FIL00000.TXT". This option allows you  to extract  one  file  to
         100000000  unique  names.  It's  essential  to  system  operators
         since multiple mail  packets with  the same  name may  come  from
         different systems.

         NOTES:
           1. It'll be wise to  include  this  option in the  script  that
              unpacks the ARCmail  packets and  NOT  in ARJ_SW environment
              variable. This  option is  a  security  measure  for systems
              running in  unattended  mode, and  will  only confuse you if
              enabled by default.
           2. There's a security  hole: a file  called  "9999.XXX" or  so,
              will not  be  overwritten.  However, all  subsequent  writes
              will be  redirected  into  file  "0000.XXX". So, files  with
              9s in the beginning have  less  chances of being  preserved.
              Hopefully such situation is unlikely for FidoNet systems.
           3. There is  another  option, "-jo1", to  serialize  filenames,
              however  its   operation   is  different.  The  volume  must
              support  long  filenames  in   order  to  use  this  option,
              moreover,  it's  not   suitable  for  dealing  with  FidoNet
              ARCmail.

      +  "-2d" enforces  the header  compatibility mode. In this mode, the
         archive   header   format   corresponds  to   the   original  ARJ
         specification, besides this, "MS-DOS" is  stamped as the host OS,
         to prevent the "Binary file from a different OS" warning messages
         when unpacking  the archive  in DOS. "-2d1" retains  the enhanced
         header format, but makes the archive comment display correctly in
         DOS.

      +  "-2f" can be  used  to  apply  the archive  comment to the  first
         volume only, and to strip it out for subsequent volumes.

      +  "-2i" is akin  to "-jx" but acts on  the .ARJ being processed. It
         skips unconditionally the given  number of bytes at the beginning
         of the archive. Its primary uses are  to recover severely damaged
         archives  or  extract  ARJ   files  contained   within  some  raw
         file system. Only the first  archive being  processed is affected
         by this switch; subsequent archives (e.g. multivolume) assume -2i
         of zero.

      +  "-2k" option forces 2-digit display of year in lists. This can be
         helpful if the 3-digit  year format used for dates beyond 2100 is
         confusing.

         Alternatively, "-2k1" uses a non-ambiguous  format  that is  both
         easy to read and  information-packed.  The dates are  represented
         by two digits if  the year  is 1970 to 2069, and in three  digits
         if it's 2070 or beyond.

         Examples:
                         15.07.1990  15.07.2040  15.07.2090  15.07.2103
         Default:          90-07-15    40-07-15    90-07-15   103-07-15
         -2k:              90-07-15    40-07-15    90-07-15    03-07-15
         -2k1:             90-07-15    40-07-15   090-07-15   103-07-15

      +  "-2r" tells ARJ to store  directory  attributes first, then store
         its contents. This is the order  that was used  by default in ARJ
         prior to 2.76. It is useful  when the archive is  to be extracted
         in an older version of ARJ to avoid directory  overwrite prompts.
         Upon extraction, it forces ARJ to ask if directory attributes are
         to  be  overwritten (by default, ARJ  will  always overwrite  the
         directory attributes without asking for confirmation).

      +  With no ARJ_SW  specified, ARJ  looks for  a file  named  ARJ.CFG
         in its home  directory. If found, this file  will  be parsed  and
         used  as  a  standard  ARJ  configuration  file (see  manual  for
         details). For UNIX platforms, this  has been changed to search in
         certain standard locations instead of home directory, see the ARJ
         for UNIX notes for further reference.

      +  REARJ v 2.42.05 and higher accepts the "T" modifier in REARJ.CFG,
         which means that it should take care to delete the output archive
         itself if rearchiving fails.


   EXTENDED ATTRIBUTES HANDLING

      Beginning  with  version  2.62.10,  the  extended  attributes  (also
      referenced to  as  EAs) can  be  backed  up  and   restored  without
      needing  any external  utilities. This is  achieved  by  compressing
      and  storing EAs as a part  of file  header. ARJ  supports SAA-style
      EAs under OS/2 and Windows NT.

      Restrictions on EA support:

      *  The multivolume restart  feature (-jn) will not  work  if EAs are
         enabled. You'll  have to  disable EAs  with -2e  prior  to  using
         -jn, or to recreate the archive if the EAs are precious.

      *  Hollow mode archives do not support EAs.

      *  Under Windows NT, extended attributes cannot be overwritten. That
         is, if the EA data is appended to  a file which  already contains
         EAs at the time of unarchiving, the file will retain its original
         EAs.

      A set of new options has been introduced to let  the user control EA
      handling:

      *  "-2c" restricts  EA  handling  to  critical  EAs  only.  Archived
         non-critical  EAs  will not  be  restored.  When  an  archive  is
         created, only critical EAs will be saved.

      *  "-2e" specifies EA inclusion  filter. With no  parameters  given,
         it disables EA  handling at  all. Otherwise, an  expression  that
         follows  it  is  interpreted  as  a   wildcard  that   limits  EA
         inclusion to a  particular EAs. Multiple  options can be  entered
         to represent a set of EA names but list files are not allowed.
         Examples:

         ARJ a test

         In this example, all EAs will be preserved.

         ARJ a no_eas -2e
         ARJ x no_eas -2e

         EAs will neither be packed nor restored.

         ARJ a documents -2e.LONGNAME

         In this case, only .LONGNAME EAs will be handled.

         ARJ a test -2e.CLASSINFO -2e.ICON*

         .CLASSINFO and .ICON* (i.e. .ICON, .ICON1, .ICONPOS) EAs  will be
         be packed and restored.

         It's wise  to specify "-2e.*" when backing up  your OS/2  desktop
         or  configuration  files.  The system  EAs  start with  dot (".")
         while application EAs start with application name.

      *  "-2l" allows to  convert  .LONGNAME  extended  attributes  (these
         represent icon titles used in  WPS) to file names, when possible.
         This feature  simplifies moving document  files away from an OS/2
         system installed on a FAT volume.

         If  the icon  title (and  so  the  extended  attribute)  contains
         line breaks, wildcard characters  or other symbols, real filename
         will be used instead and the .LONGNAME EA will be preserved.

         This option is ignored during extraction. "-2e" and "-2x" have no
         effect  on  this  option  (but  .LONGNAME EAs are  not  saved  if
         .LONGNAME EA handling is implicitly or explicitly disabled).

      *  "-2x" specifies an  exclusion  filter. It must be followed  by an
         exclusion  EA name  specification. The  rules  are  the  same  as
         with "-2e". Also, the  two options  may  work together, providing
         both an inclusion and an exclusion rule. For example:

         ARJ a backup_ -r -p1 -h#2 -2e.* -2x.FED* c:\projects

         may be used to  create  regular back-ups of your  work directory,
         including all system EAs  but  excluding  EAs  created  with FED
         (Fast Editor  Lite, an  editor written  by Sergey I. Yevtushenko,
         evsi@naverex.kiev.ua) - that program  does not follow traditional
         EA naming  conventions  and  uses  system-alike  EAs  for  anchor
         position marks.

      Extended  attributes are  also  supported in ARJSFXV self-extractors
      where they are stored using the  same  technology as with  usual ARJ
      archives.

      The presence of EAs  is  indicated  by a "(EA: ...)" message when  a
      file is packed. Note that this size  may differ from  the one  given
      when the file  is unpacked - the  former is  the EA  structure  size
      and the latter is the space allocated  for EA storage. The number of
      EAs and the size of EA structure is  also displayed when the archive
      is listed with "ARJ v" command.


   FREQUENTLY ASKED QUESTIONS

      Q: Third-party applications can't handle ARJ for DOS archives!
      A: Try to disable extended attributes (-2e), DTA/DTC  storage (-j$),
         hard  links  (-2h),  and   enforce  the  DOS  mode  (-2d).   Many
         applications   are  incapable  of  handling  new  archive  format
         (although  this  format is  fully compliant  with the  documented
         guidelines). Known  examples  of such  applications  include File
         Commander v 2.11, Norton Commander v 5.00 and WinRAR v 2.60.

      Q: Extended attribute  sizes reported  by  InfoZIP and  ARJ  differ.
         What's the cause?
      A: As we have  stated earlier, ARJ reports  the size of its internal
         EA storage structure as the  EA size when archiving files. Across
         various platforms   (OS/2-16, OS/2-32, NT)  there are  various EA
         structures.  The system is   questioned for  the  actual  EA size
         during archiving.

      Q: How can I back up my OS/2 Workplace Shell folders, preserving the
         icons?
      A: Since  folders are  represented  with directories, you'll have to
         enable directory storage with -a1 or -hbfd.

      Q: The EAs have vanished after I used ARJ/DOS to update an archive.
      A: Current versions  of ARJ/DOS and ARJ32, as well as ARJ/2 prior to
         2.62.10, strip  the  extended  headers when  any kind  of archive
         update occurs.

      Q: I want  to  create  single-volume  self-extracting  archive  that
         supports EAs but ARJ/2 uses ARJSFX instead of ARJSFXV.
      A: You need to  force  use of  ARJSFXV/2. The best way for  it is to
         specify an arbitrary large value for volume size, e.g. -va.

      Q: How can I create an installer for my OS/2 product with ARJ?
      A: ARJSFX/2 is  able  to run OS/2  commands after unpacking archive.
         Try this: create a  script you want  to to be  invoked  after the
         installation completes.

         e.g., INSTALL2.CMD:
         =======
          /* REXX */

          if RxFuncQuery('SysLoadFuncs') then do
           call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
           call SysLoadFuncs
          end

          say "Installation has completed, creating desktop object..."
          call SysCreateObject ....
         ========
         and so on...

         Now create an archive comment with the first line as follows:
         )) \InstallDir\ -b -x -y -!INSTALL2.CMD

         Call it, for example, CMT.ANS. Now create an ARJSFX archive:

         ARJ a PACKAGE.EXE -je -r -a -jm -zCMT.ANS -xCMT.ANS

         You'll make  a  self-extracting  archive  with  an  automatically
         invoked installation program.

      Q: How can I distinguish between ARJ/ARJZ/ARJ32 archives?
      A: Use the ARJ V  command. The "Host OS" field contains  the type of
         host OS. The  "Revision"  field  may  be used  to  determine  the
         archiver version that added the file:

                 1 = ARJ versions earlier than 0.14,
                     ARJZ compatibility mode (-md is less than 26624)
                 2 = ARJ v 0.14...0.20
                 3 = ARJ v 1.00...2.22
                 4 = ARJ v 2.30, X1
                 5 = ARJ v 2.39a, 2.39b
                 6 = ARJ v 2.39c...2.41
                 7 = ARJ v 2.42a...2.50a
                 8 = ARJ v 2.55...2.61, ARJ/2 v 2.61
                 9 = ARJ v 2.62, ARJ/2 v 2.62, ARJ32 v 3.00
                10 = ARJ v 2.70 and higher, ARJ/2 v 2.62.10 and higher
                11 = ARJ with UNIX support (2.77/3.10 and higher)
                50 = ARJZ with maximum distance up to 32K
                51 = ARJZ with maximum distance up to 64K
               100 = ARJ32 v 3.00b...3.01
               101 = ARJ32 v 3.02 and higher

         Notes:

         1. ARJ versions  that created the  Revision 1 header have  used a
            different method 4 compression. Support for it  was dropped in
            versions 1.xx. Such  archives may  be incorrectly processed by
            ARJ.

         2. ARJ v 2.76.07 and higher can  read the  newer UNIX time format
            of  ARJ  v  3.10/2.77. The  intention was  to make  the stable
            versions compatible with it.


   INFORMATION FOR DEVELOPERS

      The new UNIX time  format  can be identified  by "Host OS" equal  to
      "UNIX" or "NeXT", and "arj_nbr" greater than or equal to 11. In this
      case, all of the time fields in the  corresponding header are in the
      UNIX  time  format, i.e. 4-byte  value specifying number  of seconds
      passed since 01/01/1970, 00:00:00 UTC.

      By other means, the header format  is 100% compatible  with standard
      ARJ format, but we  utilize the  extended  header fields. Here  is a
      brief overview of this  technology. The extended header layout is as
      follows (all values are little-endian):

      Bytes  Description
      -----  -----------
          1  Extended header ID.
          1  Continuation  flag. If set to  0, marks the  end of  block
             chain so the header data can be concatenated and processed.
             Also it provides a way of checking for trashed blocks.
          ?  Header data.

      A standard CRC32 of the whole header, including the ID but excluding
      header size field,  is  appended to  it. It is strongly  recommended
      that the CRC is verified before any further processing occurs.

      ID 0x45 ('E') == Extended attributes
      ------------------------------------

      This is only valid if  the arj_nbr (header revision number) is 10 or
      greater.

      The packed  EA  block is a  complex  structure  that  can span  over
      multiple  volumes. In  case of  such  spanning,  separate  parts  of
      the block  are stored  in  separate extended  headers  on  different
      volumes and they must be joined together when the last block is read
      (it's recognized   by  EXTFILE_FLAG being clear). The layout of  the
      packed block follows:

      Bytes  Description
      -----  -----------
          1  Compression method (0...4, may differ from the  one found  in
             the file header)
          2  Unpacked EA data size in bytes
          4  CRC32 of unpacked EA data
          ?  Raw packed EA data

      The raw  packed data  may  be decompressed  using the  original  ARJ
      algorithms. In the case when the file is encrypted, the packed block
      is also  encrypted (but the  garble  routine is  reinitialized  when
      compression  of the EA occurs). The password modifier is the same as
      for the  first file section. After  decompression, the  following EA
      structure will exist:

      Bytes  Description
      -----  -----------
          2  Total number of EA records
          ?  Extended attribute records

      The extended attribute records are merged altogether. They should be
      processed sequentially. A single record  represents a single EA, and
      no EA can be represented twice. The layout is as follows:

      Bytes  Description
      -----  -----------
          1  fEA byte (may indicate a critical EA)
          1  Size of extended attribute name
          2  Size of extended attribute value
          ?  Extended attribute name (not ASCIIZ)
          ?  Extended attribute value (binary data)

      Even if the file is  a text  one, the EAs must be  handled as binary
      data during  compression and  extraction. When EAs are spanned  over
      multiple volumes, neither  the packed  block header  is repeated nor
      the compression  is  restarted (actually, the whole block  including
      its header is created in memory and later split to volumes).

      ID 0x4F ('O') == Owner information
      ----------------------------------

      Contains the owner  information, and possibly, group information, in
      character form. Valid if arj_nbr is 11 or greater.

      Bytes  Description
      -----  -----------
          1  Owner's name length.
          ?  Owner's name (non-ASCIIZ)
             OR
             Owner's name (ASCIIz), followed by
          ?  Group name (non-ASCIIz)

      ID 0x55 ('U') == UNIX special files
      -----------------------------------

      The UNIX special file entry is  a complex  freeform structure, valid
      if arj_nbr is 11 or greater:

      Bytes  Description
      -----  -----------
          1  Special file descriptor:
             Bits 0...4 = data size (0 allowed, 31 has a special meaning)
             Bits 5...7 = type:
                          000 = pipe
                          001 = hard link
                          002 = symbolic link
                          003 = block device
                          004 = character device
          ?  If the size field in descriptor  contained 31, then two bytes
             here contain the size, otherwise there is no area between the
             descriptor and data.
          ?  Raw data. Size is reported by the descriptor or the dedicated
             size field. Format:

		 Pipe:	Empty (size is zero).
		 Link:  Target file (non-ASCIIZ).
               Device:	The dev_t  structure  in the  host OS' format  and
                        endia order.

      ID 0x6F ('o') == Owner information (numeric)
      --------------------------------------------

      Contains the owner's UID and GID. Valid if arj_nbr is 11 or greater.

      Bytes  Description
      -----  -----------
          1  Data length (must be 8 for the current implementation).
          4  Owner's UID (little-endian).
          4  Owner's GID (little-endian).


   COMPATIBILITY ISSUES

      ARJ has  been  briefly tested  on the  following platforms  and file
      systems:

      DOS (FAT)

        MS-DOS v 2.11, 3.20, 3.21, 4.01, 5.00, 6.00, 6.20, 6.22
        PC DOS v 6.30, 7.00
        Windows 95, 98
        Windows NT Workstation v 3.51, 4.00

      OS/2 (FAT, HPFS, Ext2FS, JFS, NTFS)

        Microsoft OS/2 v 1.21
        IBM OS/2 v 1.30, 2.00, 2.10, 3.00, 4.00, 4.50
        OS/2 subsystem in Windows NT v 3.51, 4.00, Windows 2000

      Win32 (FAT, HPFS, NTFS)

        Windows 95, 98, ME
        Windows NT v 3.51, 4.00, Windows 2000, Windows XP

      Linux (UMSDOS, HPFS, Ext2FS, Ext3FS, JFS)

        Linux v 2.2.13/19-20, 2.4.5/18-23, 2.6.3
        glibc v 2.1.2, 2.1.3, 2.2.3

      FreeBSD (FAT, UFS, HPFS)

        FreeBSD v 3.4/STABLE

      QNX (QNX4FS, FAT)

        QNX v 6.2.1/PE Patch B


      End of document
