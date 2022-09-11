/* JPOS2INS.CMD
   14-Jan-98

      Creates/removes Desktop objects and modifies CONFIG.SYS for
       JP Software's "4DOS", "4OS2", and "Take Command for OS/2"

         Copyright 1998, JP Software Inc., All Rights Reserved.
   
Parameters:
"4DOS" "directory" "CreateFolder"
       "directory" "RemoveFolder"

"4OS2" "directory" "UpdatePaths" "config.in" "config.out" "file.log" "cmd.new"
       "directory" "RestorePaths" "config.in" "config.out" "shell.old" "spec.old"
       "directory" "UpdateINI" "ini.in" "ini.out"
       "directory" "CreateFolder"
       "directory" "RemoveFolder"
	
"TCMD" "directory" "UpdatePaths" "config.in" "config.out"
       "directory" "RestorePaths" "config.in" "config.out"
       "directory" "UpdateINI" "ini.in" "ini.out"
       "directory" "CreateFolder"
       "directory" "RemoveFolder"

Notes: 
1) File names that are not qualified will use the "directory" parameter
2) Parameters are not checked for validity
3) Double quotes are REQUIRED around parameters as shown above

Modifications:
10/24/97 EWL - Add quotes to 4StartPath and TCStartPath if directories
                contain non-alphanumeric characters.
01/14/98 EWL - Allow unqualified file names:  Now add "directory" parameter
                to file name if unqualified.
             - Allow abbreviations for commands (ie. "CF" for "CreateFolder")
*/

CALL RxFuncAdd 'SysLoadFuncs', 'REXXUTIL', 'SysLoadFuncs';
CALL SysLoadFuncs;

container  = '<WP_DESKTOP>'
jpFolder = '<WP_JP_CMDPRMPT>'
os2ExeId = '<WP_JP_4OS2_EXE>'
os2HelpId = '<WP_JP_4OS2_HELP>'
dosExeId = '<WP_JP_4DOS_EXE>'
dosHelpId = '<WP_JP_4DOS_HELP>'
tcmdExeId = '<WP_JP_TCMDOS2_EXE>'
tcmdHelpId = '<WP_JP_TCMDOS2_HELP>'

/*
 ****************************************************************************
 ** MAIN
 ****************************************************************************
*/
/* Want case sensitive arguments for HPFS directory names */
PARSE ARG '"' product '"' '"' comDir '"' '"' command '"' '"' parm1 '"' '"' parm2 '"' '"' parm3 '"' '"' parm4 '"' 

/* Shift case of command and platform */
PARSE UPPER VALUE command WITH command
PARSE UPPER VALUE product WITH product

/* Remove '\' from end of directory if it exists */
comDir = STRIP(comDir)
IF (RIGHT(comDir,1) == '\') THEN
  comDir = LEFT(comDir, LENGTH(comDir) - 1)

/* Add drive and directory to filename parameters that don't have one */
parm1 = Qualify('"'comDir'"', '"'parm1'"')
parm2 = Qualify('"'comDir'"', '"'parm2'"')
parm3 = Qualify('"'comDir'"', '"'parm3'"')
parm4 = Qualify('"'comDir'"', '"'parm4'"')

mainRC = 1

IF product == '4DOS' THEN DO
  productName = '4DOS'
  IF (command == 'CREATEFOLDER') | (command == 'CF') THEN DO
    mainRC = CreateJPFolder()
    container = jpFolder
  
    IF mainRC = 0 THEN
      mainRC = Create4DOSObjects()

    SAY 'Press any key to continue ...'
    SYSGETKEY(NOECHO)

  END
  ELSE IF (command == 'REMOVEFOLDER') | (command == 'RF') THEN DO
    mainRC = Remove4DOSObjects()
    SAY 'Press any key to continue ...'
    SYSGETKEY(NOECHO)
  END
  ELSE
    CALL ArgError
END  /* 4DOS */

ELSE IF product == '4OS2' THEN DO
  productName = '4OS2'
  IF (command == 'UPDATEPATHS') | (command == 'UP') THEN
    mainRC = UpdatePaths()
  ELSE IF (command == 'RESTOREPATHS') | (command == 'RP') THEN
    mainRC = RestorePaths()
  ELSE IF (command == 'UPDATEINI') | (command == 'UI') THEN
    mainRC = UpdateINI()
  ELSE IF (command == 'CREATEFOLDER') | (command == 'CF') THEN DO
    mainRC = CreateJPFolder()
    container = jpFolder
  
    IF mainRC = 0 THEN
      mainRC = Create4OS2Objects()
	 
  END
  ELSE IF (command == 'REMOVEFOLDER') | (command == 'RF') THEN
    mainRC = Remove4OS2Objects()
  ELSE
    CALL ArgError
END  /* 4OS2 */

ELSE IF product == 'TCMD' THEN DO
  productName = 'Take Command for OS/2'
  IF (command == 'UPDATEPATHS') | (command == 'UP') THEN 
    mainRC = UpdatePaths()
  ELSE IF (command == 'RESTOREPATHS') | (command == 'RP') THEN 
    mainRC = RestorePaths()
  ELSE IF (command == 'UPDATEINI') | (command == 'UI') THEN
    mainRC = UpdateINI()
  ELSE IF (command == 'CREATEFOLDER') | (command == 'CF') THEN DO
    mainRC = CreateJPFolder()
    container = jpFolder
  
    IF mainRC = 0 THEN
      mainRC = CreateTCMDObjects()
	 
    /* Look for 4DOS and add to folder? */
  
  END
  ELSE IF (command == 'REMOVEFOLDER') | (command == 'RF') THEN
    mainRC = RemoveTCMDObjects()
  ELSE
    CALL ArgError
END  /* TCMD */
ELSE DO 
  CALL ArgError
END

EXIT mainRC
/*
 ****************************************************************************
 ** END MAIN
 ****************************************************************************
*/

UpdatePaths:

  SAY ''
  CALL CHAROUT ,'Adding ' || productName || ' directory to CONFIG.SYS paths'
  IF product == '4OS2' THEN DO
    CALL LINEOUT ,' and'
    CALL CHAROUT ,'adding ' || productName || ' executable to OS2_SHELL ' ||,
	               'and COMSPEC variables'
  END
  
  configSys = parm1
  configSysTemp = parm2
  logFile = parm3
  cmdFile = parm4

  /* Delete output file name; otherwise LINEOUT will append to file */
  delRC = SysFileDelete(configSysTemp)

  PARSE UPPER VALUE comDir WITH comDirUp
  holdDir = comDir || ';'
  holdDirUp = comDirUp || ';'
  
  /* Flags */
  libFound = 0
  bookFound = 0
  dFound = 0
  shellFound = 0
  specFound = 0
 

  DO WHILE LINES(configSys) > 0
    /* Read one line of file */
    inputLine = LINEIN(configSys) 
    
    /* Get path key and info */
    PARSE VALUE inputLine WITH envarKey '=' envarInfo     
	 PARSE UPPER VALUE envarKey WITH envarKey
	 PARSE UPPER VALUE envarInfo WITH envarInfoUp
    
    envarKey = STRIP(envarKey)
    pathFound = 0
    envarFound = 0
    
    /* Check to see if this is one of the lines we want to modify */
    IF (envarKey == 'LIBPATH') THEN DO 
      libFound = 1
      pathFound = 1
    END
    IF (envarKey == 'SET BOOKSHELF') THEN DO
      bookFound = 1
      pathFound = 1
    END
    IF (envarKey == 'SET DPATH') THEN DO
      dFound = 1
      pathFound = 1
    END

    IF product == '4OS2' THEN DO
      IF (envarKey == 'SET OS2_SHELL') THEN DO 
	     currShell = envarInfo 
        shellFound = 1
        envarFound = 1
      END
      IF (envarKey == 'SET COMSPEC') THEN DO 
	     currSpec = envarInfo 
        specFound = 1
        envarFound = 1
      END
    END  /* IF 4OS2 */
    

    /* Add our directory if it is not already in path */
    IF ((pathFound = 1) & (POS(holdDirUp, envarInfoUp) = 0)) THEN DO     
      /* Remove trailing spaces */
      inputLine = STRIP(inputLine,'t')

      /* IF path does not end in ';' or '=', add ';' to end */
      IF ((RIGHT(inputLine,1) \== ';') & (RIGHT(inputLine,1) \== '=')) THEN
        inputLine = inputLine || ';'  

      /* Add our path to end */
      inputLine = inputLine || holdDirUp

    END  /* pathFound */

    IF (envarFound = 1) THEN DO
      /* Write our environment var instead */
      inputLine = envarKey || '=' || cmdFile
    END  /* envarFound */
       
    /* IF none of the keywords were found, inputLine is unchanged */
    CALL LINEOUT configSysTemp,inputLine
  
  END  /* DO while LINES > 0 */

  
  /* IF any paths not found */
  IF ((libFound = 0) | (bookFound = 0) | (dFound = 0)) THEN DO
    /* Add the path variables that we did not find in the file */
    IF (libFound = 0) THEN
      CALL LINEOUT configSysTemp,'LIBPATH=' || holdDirUp
    IF (bookFound = 0) THEN
      CALL LINEOUT configSysTemp,'SET BOOKSHELF=' || holdDirUp
    IF (dFound = 0) THEN
      CALL LINEOUT configSysTemp,'SET DPATH=' || holdDirUp
  END  /* IF any paths not found */
     
  /* If 4OS2 install and either envar not found */
  IF ((product == '4OS2') & ((shellFound = 0) | (specFound = 0))) THEN DO
    /* Add the environment variables that we did not find in the file */
    IF (shellFound = 0) THEN
      CALL LINEOUT configSysTemp,'SET OS2_SHELL=' || cmdFile
    IF (specFound = 0) THEN
      CALL LINEOUT configSysTemp,'SET COMSPEC=' || cmdFile
  END  /* If either envar not found */

  /* Close files */ 
  CALL LINEOUT configSys
  CALL LINEOUT configSysTemp

  /* Write current OS2_SHELL and COMSPEC to log only if they don't reference 
      the same copy of 4OS2.EXE (eg. when someone runs install twice for the
      the same version)

     Shareware installation has no logging; need to handle a null log file
      name
   */
  IF (logfile \== '')  THEN DO
    IF ((shellFound = 1) & (POS(comDirUp||'\4OS2.EXE', UCase(currShell)) = 0)) THEN DO     
      CALL WriteLog logFile, 'OS2_SHELL', currShell 
    END
    IF ((specFound = 1) & (POS(comDirUp||'\4OS2.EXE', UCase(currShell)) = 0)) THEN DO     
      CALL WriteLog logFile, 'COMSPEC', currSpec 
    END
  END
  
  CALL charout ,'... OK!'
  SAY ''

RETURN 0


/****************************************************************************/
WriteLog:
  
  PARSE ARG logFileName , matchParm , valueParm 
  PARSE UPPER VALUE matchParm WITH matchParm
  
  n = 0
  itemFound = 0
  dataArray. = '' 
  
  DO WHILE LINES(logFileName) > 0
    /* Read one line of file */
    inputData = LINEIN(logFileName) 
    
    /* Get key and info */
    PARSE VALUE inputData WITH logKey '=' logInfo     
	 PARSE UPPER VALUE logKey WITH logKey
    
    logKey = STRIP(logKey)
    
    /* Check to see if this is the line we want to modify */
    IF (logKey == matchParm) THEN DO 
      /* Write our environment var instead */
      inputData = logKey || '=' || valueParm
      itemFound = 1
    END  
       
    /* Store the line in the array
       If none of the keywords were found, inputLine is unchanged 
     */
    dataArray.n = inputData
    n = n + 1
  
  END  /* DO while LINES > 0 */
  
  /* Close file */ 
  CALL LINEOUT logFileName
  
  /* If not found */
  IF (itemFound = 0) THEN DO
    /* Add item to end of file */
    CALL LINEOUT logFileName, matchParm || '=' || valueParm
  END
  ELSE DO
    /* Erase log file and rewrite */
	 nn = 0
	 IF (SysFileDelete(logFileName) = 0) THEN DO WHILE (nn < n)
      CALL LINEOUT logFileName, dataArray.nn
		nn = nn + 1
	 END
	 ELSE DO 
	   /* Error */
      SAY 'ERROR: Could not modify the installation log file'
    END  
  END

  /* Close file */ 
  CALL LINEOUT logFileName
  
RETURN 0


/****************************************************************************/
RestorePaths:

  SAY ''
  CALL CHAROUT ,'Removing ' || productName || ' directory from ' ||,
                'CONFIG.SYS paths'
  IF product == '4OS2' THEN DO
    CALL LINEOUT ,' and'
    CALL charout ,'removing ' || productName || ' from OS2_SHELL and ' ||,
                  'COMSPEC variables'
  END
  
  configSys = parm1
  configSysTemp = parm2
  shellLine = parm3
  specLine = parm4
  holdDir = comDir || ';'
  
  rcRestorePaths = 0
  
  /* Delete output file name; otherwise LINEOUT will append to file */
  delRC = SysFileDelete(configSysTemp)

  IF product == '4OS2' THEN DO
    /* Verify that the old OS2_SHELL and COMSPEC files exist */
    PARSE VALUE shellLine WITH holdFile .
    IF STREAM(STRIP(holdFile), C, QUERY EXISTS) \= '' THEN
      shellFound = 1
    ELSE
      shellFound = 0

    PARSE VALUE specLine WITH holdFile .
    IF STREAM(STRIP(holdFile), C, QUERY EXISTS) \= '' THEN
      specFound = 1
    ELSE
      specFound = 0
  END  /* If 4OS2 */
 
  PARSE UPPER VALUE holdDir WITH holdDirUp
  
  DO WHILE LINES(configSys) > 0
    /* Read one line of file */
    inputLine = LINEIN(configSys) 
	 
    PARSE UPPER VALUE inputLine WITH inputLineUp
  
    /* Get path key and info */
    PARSE VALUE inputLineUp WITH envarKey '=' envarInfo     
    
    envarKey = STRIP(envarKey)
    
    /* If this is one of the paths we want to modify */
    IF (envarKey == 'LIBPATH') | ,
       (envarKey == 'SET BOOKSHELF') | ,
       (envarKey == 'SET DPATH') THEN DO
      
      /* Find our directory (case insensitive) */
      envarPos = POS(holdDirUp, inputLineUp)
      
      IF (envarPos \= 0) THEN DO
        inputLine = DELSTR(inputLine, envarPos, LENGTH(holdDirUp))
      END  
 
    END  /* Path found */

    IF product == '4OS2' THEN DO
      /* If we find an OS2_SHELL or COMSPEC line, replace it only if we know
          that the old file exists, otherwise flag it as an error
      */
      IF (envarKey == 'SET OS2_SHELL') THEN DO
        IF (shellFound = 1) THEN
          inputLine = envarKey || '=' || shellLine
        ELSE
          rcRestorePaths = 1
      END

      IF (envarKey == 'SET COMSPEC') THEN DO
        IF (specFound = 1) THEN
          inputLine = envarKey || '=' || specLine
        ELSE
          rcRestorePaths = 1
      END
    END  /* If 4OS2 */
       
    /* IF none of the keywords were found, inputLine is unchanged */
    CALL LINEOUT configSysTemp, inputLine

  END  /* DO while LINES > 0 */
  
  /* Close files */ 
  CALL LINEOUT configSys
  CALL LINEOUT configSysTemp

  IF rcRestorePaths = 0 THEN
    CALL charout ,'... OK!'
  ELSE
    CALL charout ,'... ERROR!'

  SAY ''

RETURN rcRestorePaths


/****************************************************************************/
UpdateINI:

  iniFile = parm1
  iniFileTemp = parm2
  
  SAY ''
  CALL CHAROUT ,'Updating ' || iniFile
  
  /* Delete output file name; otherwise LINEOUT will append to file */
  delRC = SysFileDelete(iniFileTemp)

  /* Check to see if we have [4OS2] as first non-blank, non-comment line */
  IF (product == '4OS2') THEN DO
    searchDone = 0
    foundSection = 0

    DO WHILE (LINES(iniFile) > 0) & (searchDone = 0)
      inputLine = STRIP(LINEIN(iniFile))

      /* Read until we hit the first line to contain text other than a
         comment
      */
      IF (inputLine \== '') & (LEFT(inputLine, 1) \== ';') THEN DO
        searchDone = 1
        PARSE UPPER VALUE inputLine WITH '[' sectionName ']'
        IF (sectionName == '4OS2') THEN
          foundSection = 1
      END

    END  /* DO while LINES > 0 */

    /* If we did not find [4OS2], write it to the first line of output file */
    IF foundSection = 0 THEN
      CALL LINEOUT iniFileTemp, '[4OS2]'

    /* Close input file */
    CALL LINEOUT iniFile
  END  /* If 4OS2 */

  /* 10/24/97 EWL - Add quotes to 4StartPath and TCStartPath if directories
                     contain special characters
  */ 
  IF COMPARE(comDir, TRANSLATE(comDir,," ,=+<>|", "X")) \= 0 THEN
    comDir = '"' || comDir || '"'

  DO WHILE LINES(iniFile) > 0
    /* Read one line of file */
    inputLine = LINEIN(iniFile) 
	 
    /* Get path key and info */
    PARSE VALUE inputLine WITH envarKey '=' envarInfo     
    PARSE UPPER VALUE envarKey WITH envarKey 
    
    envarKey = STRIP(envarKey)
    envarInfo = STRIP(envarInfo)
    
    /* If this is one of the directives we want to modify */
    IF (product == '4OS2') & (envarKey == '4STARTPATH') THEN
      inputLine = '4StartPath=' || comDir
    
    IF (product == 'TCMD') & (envarKey == 'TCSTARTPATH') THEN
      inputLine = 'TCStartPath=' || comDir
    
    IF envarKey == 'HISTWINLEFT' THEN
      inputLine = 'PopupWinLeft=' || envarInfo
    ELSE IF envarKey == 'HISTWINTOP' THEN
      inputLine = 'PopupWinTop=' || envarInfo
    ELSE IF envarKey == 'HISTWINWIDTH' THEN
      inputLine = 'PopupWinWidth=' || envarInfo
    ELSE IF envarKey == 'HISTWINHEIGHT' THEN
      inputLine = 'PopupWinHeight=' || envarInfo
    ELSE IF envarKey == 'HISTWINCOLORS' THEN
      inputLine = 'PopupWinColors=' || envarInfo

    /* If none of the keywords were found, inputLine is unchanged */
    CALL LINEOUT iniFileTemp, inputLine

  END  /* DO while LINES > 0 */
  
  /* Close files */ 
  CALL LINEOUT iniFile
  CALL LINEOUT iniFileTemp

  CALL charout ,'... OK!'
  SAY ''

RETURN 0


/****************************************************************************/
CreateJPFolder:

  /* Create/update the JP command prompts folder */
  classname = 'WPFolder'
  objectname = 'JP Software'||'0d0A'x||'Command Processors'
  msgtext = 'JP Software Command Processors folder'
  location = container
  setup = 'ICONFILE='||comDir||'\JPFOLDER.ICO;'||,
          'ICONNFILE=1,'||comDir||'\JPOPNFLD.ICO;'||,
          'OBJECTID='jpFolder||';'
  CALL DOCreateObject
RETURN RESULT


/****************************************************************************/
Create4DOSObjects:

  /* Create/update the main executable */
  classname = 'WPProgram'
  objectname = '4DOS Window'
  msgtext = '4DOS Window'
  location = container
  setup = 'EXENAME=*;'||,
          'ICONFILE='||comDir||'\4DOSOS2.ICO;'||,
          'STARTUPDIR='||comDir||';'||,
          'PROGTYPE=WINDOWEDVDM;'||,
          'OBJECTID='dosExeId||';'||,
          'SET DOS_SHELL='||comDir||'\4DOS.COM '||comdir||' /P;'
  CALL DOCreateObject
  
  IF RESULT = 0 THEN DO
    /* Create/update the help object */
    classname = 'WPProgram'
    objectname = '4DOS Help'
    msgtext = '4DOS Help'
    location = container
    setup = 'EXENAME=*;'||,
            'PARAMETERS=/C '||comDir||'\4HELP.EXE;'||,
            'PROGTYPE=WINDOWEDVDM;'||,
            'OBJECTID='dosHelpId||';'||,
            'SET DOS_SHELL='||comDir||'\4DOS.COM '||comdir||' /P;'
    CALL DOCreateObject
  END
RETURN RESULT


/****************************************************************************/
Create4OS2Objects:

  /* Create/update the main executable */
  classname = 'WPProgram'
  objectname = '4OS2 Window'
  msgtext = '4OS2 program file'
  location = container
  setup = 'EXENAME='||comDir||'\4OS2.EXE;'||,
          'ICONFILE='||comDir||'\4OS2.ICO;'||,
          'STARTUPDIR='||comDir||';'||,
          'PROGTYPE=WINDOWABLEVIO;'||,
          'OBJECTID='os2ExeId||';'
  CALL DOCreateObject
  
  IF RESULT = 0 THEN DO
    /* Create/update the help object */
    classname = 'WPProgram'
    objectname = '4OS2 Help'
    msgtext = '4OS2 help file'
    location = container
    setup = 'EXENAME=VIEW.EXE;'||,
            'PARAMETERS='||comDir||'\4OS2.INF;'||,
            'PROGTYPE=PM;'||,
            'OBJECTID='os2HelpId||';'
    CALL DOCreateObject
  END
RETURN RESULT


/****************************************************************************/
CreateTCMDObjects:
  /* Create/update the main executable */
  classname = 'WPProgram'
  objectname = 'Take Command'||'0d0A'x||'for OS/2'
  msgtext = 'Take Command for OS/2 program file'
  location = container
  setup = 'EXENAME='||comDir||'\TCMDOS2.EXE;'||,
          'STARTUPDIR='||comDir||';'||,
          'PROGTYPE=PM;'||,
          'OBJECTID='tcmdExeId||';'
  CALL DOCreateObject

  IF RESULT = 0 THEN DO
    /* Create/update the help object */
    classname = 'WPProgram'
    objectname = 'Take Command'||'0d0A'x||'for OS/2 Help'
    msgtext = 'Take Command for OS/2 help file'
    location = container
    setup = 'EXENAME=VIEW.EXE;'||,
            'PARAMETERS='||comDir||'\TCMDOS2.INF;'||,
            'PROGTYPE=PM;'||,
            'OBJECTID='tcmdHelpId||';'
    CALL DOCreateObject
  END

RETURN RESULT


/****************************************************************************/
RemoveFolder:
  /* Delete the folder */
  msgtext = 'JP Software Command Processors folder'
  object = jpFolder
  
  CALL DODestroyObject

  IF (RESULT = 0) THEN
    holdRC = 0
  ELSE  
    holdRC = 1

RETURN holdRC


/****************************************************************************/
Remove4DOSObjects:
  /* Delete the objects */
  msgtext = '4DOS Window'
  object = dosExeId
  CALL DODestroyObject
  holdRC = RESULT
  
  msgtext = '4DOS Help'
  object = dosHelpId
  CALL DODestroyObject

  IF (RESULT \= 0) | (holdRC \= 0) THEN
    holdRC = 1

RETURN holdRC


/****************************************************************************/
Remove4OS2Objects:
  /* Delete the objects */
  msgtext = '4OS2 program file'
  object = os2ExeId
  CALL DODestroyObject
  holdRC = RESULT
  
  msgtext = '4OS2 help file'
  object = os2HelpId
  CALL DODestroyObject

  IF (RESULT \= 0) | (holdRC \= 0) THEN
    holdRC = 1

RETURN holdRC


/****************************************************************************/
RemoveTCMDObjects:
  /* Delete the objects */
  msgtext = 'Take Command for OS/2 program file'
  object = tcmdExeId
  CALL DODestroyObject
  holdRC = RESULT
  
  msgtext = 'Take Command for OS/2 help file'
  object = tcmdHelpId
  CALL DODestroyObject

  IF (RESULT \= 0) | (holdRC \= 0) THEN
    holdRC = 1

RETURN holdRC


/****************************************************************************/
DOCreateObject:
  SAY ''
  CALL charout, 'Creating/updating object:  ' msgtext
  rc = SysCreateObject(classname, objectname, location, setup, 'U')
  IF rc <> 0 THEN DO
    CALL charout ,'... OK!'
    objRC = 0
  END
  ELSE DO
    CALL charout ,'... ERROR #'rc
    objRC = 1
  END

  SAY '';
RETURN objRC


/****************************************************************************/
DODestroyObject:
  SAY ''
  CALL charout, 'Removing object:  ' msgtext
  rc = SysDestroyObject(object)
  IF rc <> 0 THEN DO
    CALL charout ,'... OK!'
    objRC = 0
  END
  ELSE DO
    CALL charout ,'... ERROR #'rc
    objRC = 1
  END

  SAY '';
RETURN objRC


/****************************************************************************/
ArgError:
  SAY ''
  SAY 'This program can only be run by the JP Software INSTALL program.'
  SAY 'It will not work if you attempt to execute it manually.'
RETURN


/****************************************************************************/
UCase:
  PARSE UPPER ARG ucReturn
RETURN ucReturn


/****************************************************************************/
Qualify:
  PARSE ARG '"'qDir'"', '"'qFile'"'

  /* If filename is not fully qualified, add directory to filename */
  IF (SUBSTR(qFile,2,1) \== ':') THEN 
    qualFile = qDir || '\' || qFile
  ELSE
    qualFile = qFile

RETURN qualFile
