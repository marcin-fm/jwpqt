# Microsoft Developer Studio Project File - Name="JWPce_CE" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (WCE x86) Application" 0x8301
# TARGTYPE "Win32 (WCE ARM) Application" 0x8501
# TARGTYPE "Win32 (WCE PPC) Application" 0x8401
# TARGTYPE "Win32 (WCE SH3) Application" 0x8101
# TARGTYPE "Win32 (WCE SH4) Application" 0x8601
# TARGTYPE "Win32 (WCE MIPS) Application" 0x8201
# TARGTYPE "Win32 (WCE MIPSFP) Application" 0x8701

CFG=JWPce_CE - Win32 (WCE MIPS) Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "JWPce_CE.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "JWPce_CE.mak" CFG="JWPce_CE - Win32 (WCE MIPS) Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "JWPce_CE - Win32 (WCE MIPS) Release" (based on "Win32 (WCE MIPS) Application")
!MESSAGE "JWPce_CE - Win32 (WCE MIPS) Debug" (based on "Win32 (WCE MIPS) Application")
!MESSAGE "JWPce_CE - Win32 (WCE SH4) Release" (based on "Win32 (WCE SH4) Application")
!MESSAGE "JWPce_CE - Win32 (WCE SH4) Debug" (based on "Win32 (WCE SH4) Application")
!MESSAGE "JWPce_CE - Win32 (WCE SH3) Release" (based on "Win32 (WCE SH3) Application")
!MESSAGE "JWPce_CE - Win32 (WCE SH3) Debug" (based on "Win32 (WCE SH3) Application")
!MESSAGE "JWPce_CE - Win32 (WCE ARM) Release" (based on "Win32 (WCE ARM) Application")
!MESSAGE "JWPce_CE - Win32 (WCE ARM) Debug" (based on "Win32 (WCE ARM) Application")
!MESSAGE "JWPce_CE - Win32 (WCE MIPSFP) Release" (based on "Win32 (WCE MIPSFP) Application")
!MESSAGE "JWPce_CE - Win32 (WCE MIPSFP) Debug" (based on "Win32 (WCE MIPSFP) Application")
!MESSAGE "JWPce_CE - Win32 (WCE x86) Release" (based on "Win32 (WCE x86) Application")
!MESSAGE "JWPce_CE - Win32 (WCE x86) Debug" (based on "Win32 (WCE x86) Application")
!MESSAGE "JWPce_CE - Win32 (WCE PPC) Release" (based on "Win32 (WCE PPC) Application")
!MESSAGE "JWPce_CE - Win32 (WCE PPC) Debug" (based on "Win32 (WCE PPC) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath "H/PC Ver. 2.00"

!IF  "$(CFG)" == "JWPce_CE - Win32 (WCE MIPS) Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WMIPSRel"
# PROP BASE Intermediate_Dir "WMIPSRel"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WMIPSRel"
# PROP Intermediate_Dir "WMIPSRel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /machine:MIPS /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /machine:MIPS /nodefaultlib:"$(CENoDefaultLib)" /out:"WMIPSRel/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "MIPS" /d "_MIPS_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "NDEBUG"
# ADD RSC /l 0x409 /r /d "MIPS" /d "_MIPS_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "NDEBUG"
CPP=clmips.exe
# ADD BASE CPP /nologo /M$(CECrt) /W3 /O2 /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "NDEBUG" /D "MIPS" /D "_MIPS_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /QMRWCE /c
# ADD CPP /nologo /M$(CECrt) /W3 /O2 /D "NDEBUG" /D "MIPS" /D "_MIPS_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /YX /QMRWCE /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE MIPS) Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WMIPSDbg"
# PROP BASE Intermediate_Dir "WMIPSDbg"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WMIPSDbg"
# PROP Intermediate_Dir "WMIPSDbg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /debug /machine:MIPS /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /debug /machine:MIPS /nodefaultlib:"$(CENoDefaultLib)" /out:"WMIPSDbg/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "MIPS" /d "_MIPS_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "DEBUG"
# ADD RSC /l 0x409 /r /d "MIPS" /d "_MIPS_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "DEBUG"
CPP=clmips.exe
# ADD BASE CPP /nologo /M$(CECrtDebug) /W3 /Zi /Od /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "DEBUG" /D "MIPS" /D "_MIPS_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /QMRWCE /c
# ADD CPP /nologo /M$(CECrtDebug) /W3 /Zi /Od /D "DEBUG" /D "MIPS" /D "_MIPS_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /YX /QMRWCE /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE SH4) Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WCESH4Rel"
# PROP BASE Intermediate_Dir "WCESH4Rel"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WCESH4Rel"
# PROP Intermediate_Dir "WCESH4Rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /machine:SH4 /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /machine:SH4 /nodefaultlib:"$(CENoDefaultLib)" /out:"WCESH4Rel/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "SHx" /d "SH4" /d "_SH4_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "NDEBUG"
# ADD RSC /l 0x409 /r /d "SHx" /d "SH4" /d "_SH4_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "NDEBUG"
CPP=shcl.exe
# ADD BASE CPP /nologo /Qsh4 /MC /W3 /O2 /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "NDEBUG" /D "SHx" /D "SH4" /D "_SH4_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /c
# ADD CPP /nologo /Qsh4 /MC /W3 /O2 /D "NDEBUG" /D "SHx" /D "SH4" /D "_SH4_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /YX /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE SH4) Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WCESH4Dbg"
# PROP BASE Intermediate_Dir "WCESH4Dbg"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WCESH4Dbg"
# PROP Intermediate_Dir "WCESH4Dbg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /debug /machine:SH4 /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /debug /machine:SH4 /nodefaultlib:"$(CENoDefaultLib)" /out:"WCESH4Dbg/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "SHx" /d "SH4" /d "_SH4_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "DEBUG"
# ADD RSC /l 0x409 /r /d "SHx" /d "SH4" /d "_SH4_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "DEBUG"
CPP=shcl.exe
# ADD BASE CPP /nologo /Qsh4 /MC /W3 /Zi /Od /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "DEBUG" /D "SHx" /D "SH4" /D "_SH4_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /c
# ADD CPP /nologo /Qsh4 /MC /W3 /Zi /Od /D "DEBUG" /D "SHx" /D "SH4" /D "_SH4_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /YX /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE SH3) Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WCESH3Rel"
# PROP BASE Intermediate_Dir "WCESH3Rel"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WCESH3Rel"
# PROP Intermediate_Dir "WCESH3Rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /machine:SH3 /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /machine:SH3 /nodefaultlib:"$(CENoDefaultLib)" /out:"WCESH3Rel/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "SHx" /d "SH3" /d "_SH3_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "NDEBUG"
# ADD RSC /l 0x409 /r /d "SHx" /d "SH3" /d "_SH3_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "NDEBUG"
CPP=shcl.exe
# ADD BASE CPP /nologo /M$(CECrt) /W3 /O2 /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "NDEBUG" /D "SHx" /D "SH3" /D "_SH3_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /c
# ADD CPP /nologo /M$(CECrt) /W3 /O2 /D "NDEBUG" /D "SHx" /D "SH3" /D "_SH3_" /D "WINCE_HPC" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /YX /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE SH3) Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WCESH3Dbg"
# PROP BASE Intermediate_Dir "WCESH3Dbg"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WCESH3Dbg"
# PROP Intermediate_Dir "WCESH3Dbg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /debug /machine:SH3 /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /debug /machine:SH3 /nodefaultlib:"$(CENoDefaultLib)" /out:"WCESH3Dbg/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "SHx" /d "SH3" /d "_SH3_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "DEBUG"
# ADD RSC /l 0x409 /r /d "SHx" /d "SH3" /d "_SH3_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "DEBUG"
CPP=shcl.exe
# ADD BASE CPP /nologo /M$(CECrtDebug) /W3 /Zi /Od /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "DEBUG" /D "SHx" /D "SH3" /D "_SH3_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /c
# ADD CPP /nologo /M$(CECrtDebug) /W3 /Zi /Od /D "DEBUG" /D "SHx" /D "SH3" /D "_SH3_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /YX /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE ARM) Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WCEARMRel"
# PROP BASE Intermediate_Dir "WCEARMRel"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WCEARMRel"
# PROP Intermediate_Dir "WCEARMRel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 coredll.lib commctrl.lib /nologo /machine:ARM /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /machine:ARM /nodefaultlib:"$(CENoDefaultLib)" /out:"WCEARMRel/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "ARM" /d "_ARM_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "NDEBUG"
# ADD RSC /l 0x409 /r /d "ARM" /d "_ARM_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "NDEBUG"
CPP=clarm.exe
# ADD BASE CPP /nologo /MC /W3 /O2 /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "NDEBUG" /D "ARM" /D "_ARM_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /c
# ADD CPP /nologo /MC /W3 /O2 /D "NDEBUG" /D "ARM" /D "_ARM_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /YX /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE ARM) Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WCEARMDbg"
# PROP BASE Intermediate_Dir "WCEARMDbg"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WCEARMDbg"
# PROP Intermediate_Dir "WCEARMDbg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 coredll.lib commctrl.lib /nologo /debug /machine:ARM /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /debug /machine:ARM /nodefaultlib:"$(CENoDefaultLib)" /out:"WCEARMDbg/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "ARM" /d "_ARM_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "DEBUG"
# ADD RSC /l 0x409 /r /d "ARM" /d "_ARM_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "DEBUG"
CPP=clarm.exe
# ADD BASE CPP /nologo /MC /W3 /Zi /Od /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "DEBUG" /D "ARM" /D "_ARM_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /c
# ADD CPP /nologo /MC /W3 /Zi /Od /D "DEBUG" /D "ARM" /D "_ARM_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /YX /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE MIPSFP) Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WMIPSFPRel"
# PROP BASE Intermediate_Dir "WMIPSFPRel"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WMIPSFPRel"
# PROP Intermediate_Dir "WMIPSFPRel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /machine:MIPS /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /machine:MIPS /nodefaultlib:"$(CENoDefaultLib)" /out:"WMIPSFPRel/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "MIPS" /d "_MIPS_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "NDEBUG"
# ADD RSC /l 0x409 /r /d "MIPS" /d "_MIPS_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "NDEBUG"
CPP=clmips.exe
# ADD BASE CPP /nologo /QMFWCE /MC /W3 /O2 /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "NDEBUG" /D "MIPS" /D "_MIPS_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /c
# ADD CPP /nologo /QMFWCE /MC /W3 /O2 /D "NDEBUG" /D "MIPS" /D "_MIPS_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /YX /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE MIPSFP) Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WMIPSFPDbg"
# PROP BASE Intermediate_Dir "WMIPSFPDbg"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WMIPSFPDbg"
# PROP Intermediate_Dir "WMIPSFPDbg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /debug /machine:MIPS /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /debug /machine:MIPS /nodefaultlib:"$(CENoDefaultLib)" /out:"WMIPSFPDbg/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "MIPS" /d "_MIPS_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "DEBUG"
# ADD RSC /l 0x409 /r /d "MIPS" /d "_MIPS_" /d UNDER_CE=$(CEVersion) /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d "UNICODE" /d "DEBUG"
CPP=clmips.exe
# ADD BASE CPP /nologo /QMFWCE /MC /W3 /Zi /Od /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "DEBUG" /D "MIPS" /D "_MIPS_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /c
# ADD CPP /nologo /QMFWCE /MC /W3 /Zi /Od /D "DEBUG" /D "MIPS" /D "_MIPS_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /YX /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE x86) Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WCEX86Rel"
# PROP BASE Intermediate_Dir "WCEX86Rel"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WCEX86Rel"
# PROP Intermediate_Dir "WCEX86Rel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 $(CEx86Corelibc) commctrl.lib coredll.lib /nologo /machine:IX86 /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 $(CEx86Corelibc) commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /machine:IX86 /nodefaultlib:"$(CENoDefaultLib)" /out:"WCEX86Rel/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "x86" /d "_i386_" /d "_x86_" /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d UNDER_CE=$(CEVersion) /d "UNICODE" /d "NDEBUG"
# ADD RSC /l 0x409 /r /d "x86" /d "_i386_" /d "_x86_" /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d UNDER_CE=$(CEVersion) /d "UNICODE" /d "NDEBUG"
CPP=cl.exe
# ADD BASE CPP /nologo /ML /W3 /O2 /D "x86" /D "_i386_" /D "_x86_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "NDEBUG" /D "i_386_" /D "_MBCS" /Gs8192 /GF /c
# ADD CPP /nologo /ML /W3 /O2 /D "x86" /D "_i386_" /D "_x86_" /D "NDEBUG" /D "i_386_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /Gs8192 /GF /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE x86) Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WCEX86Dbg"
# PROP BASE Intermediate_Dir "WCEX86Dbg"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WCEX86Dbg"
# PROP Intermediate_Dir "WCEX86Dbg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 $(CEx86Corelibc) commctrl.lib coredll.lib /nologo /debug /machine:IX86 /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 $(CEx86Corelibc) commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /debug /machine:IX86 /nodefaultlib:"$(CENoDefaultLib)" /out:"WCEX86Dbg/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "x86" /d "_i386_" /d "_x86_" /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d UNDER_CE=$(CEVersion) /d "UNICODE" /d "DEBUG"
# ADD RSC /l 0x409 /r /d "x86" /d "_i386_" /d "_x86_" /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d UNDER_CE=$(CEVersion) /d "UNICODE" /d "DEBUG"
CPP=cl.exe
# ADD BASE CPP /nologo /MLd /W3 /Zi /Od /D "x86" /D "_i386_" /D "_x86_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "DEBUG" /D "i_386_" /D "_MBCS" /Gs8192 /GF /c
# ADD CPP /nologo /MLd /W3 /Zi /Od /D "x86" /D "_i386_" /D "_x86_" /D "DEBUG" /D "i_386_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /Gs8192 /GF /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE PPC) Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WCEPPCRel"
# PROP BASE Intermediate_Dir "WCEPPCRel"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WCEPPCRel"
# PROP Intermediate_Dir "WCEPPCRel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /machine:PPC /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /machine:PPC /nodefaultlib:"$(CENoDefaultLib)" /out:"WCEPPCRel/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "ppc" /d "_ppc_" /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d UNDER_CE=$(CEVersion) /d "UNICODE" /d "NDEBUG"
# ADD RSC /l 0x409 /r /d "ppc" /d "_ppc_" /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d UNDER_CE=$(CEVersion) /d "UNICODE" /d "NDEBUG"
CPP=clppc.exe
# ADD BASE CPP /nologo /M$(CECrt) /W3 /O2 /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "NDEBUG" /D "PPC" /D "_PPC_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /c
# ADD CPP /nologo /M$(CECrt) /W3 /O2 /D "NDEBUG" /D "PPC" /D "_PPC_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /YX /c

!ELSEIF  "$(CFG)" == "JWPce_CE - Win32 (WCE PPC) Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WCEPPCDbg"
# PROP BASE Intermediate_Dir "WCEPPCDbg"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WCEPPCDbg"
# PROP Intermediate_Dir "WCEPPCDbg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /debug /machine:PPC /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib ceshell.lib ole32.lib /nologo /debug /machine:PPC /nodefaultlib:"$(CENoDefaultLib)" /out:"WCEPPCDbg/JWPce.exe" /subsystem:$(CESubsystem) /STACK:65536,4096
# SUBTRACT LINK32 /pdb:none /nodefaultlib
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /r /d "ppc" /d "_ppc_" /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d UNDER_CE=$(CEVersion) /d "UNICODE" /d "DEBUG"
# ADD RSC /l 0x409 /r /d "ppc" /d "_ppc_" /d _WIN32_WCE=$(CEVersion) /d "$(CEConfigName)" /d UNDER_CE=$(CEVersion) /d "UNICODE" /d "DEBUG"
CPP=clppc.exe
# ADD BASE CPP /nologo /M$(CECrtDebug) /W3 /Zi /Od /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "DEBUG" /D "PPC" /D "_PPC_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /YX /c
# ADD CPP /nologo /M$(CECrtDebug) /W3 /Zi /Od /D "DEBUG" /D "PPC" /D "_PPC_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "_MBCS" /D "WINCE" /D "WINCETEMP" /D "WINCE_HPC" /YX /c

!ENDIF 

# Begin Target

# Name "JWPce_CE - Win32 (WCE MIPS) Release"
# Name "JWPce_CE - Win32 (WCE MIPS) Debug"
# Name "JWPce_CE - Win32 (WCE SH4) Release"
# Name "JWPce_CE - Win32 (WCE SH4) Debug"
# Name "JWPce_CE - Win32 (WCE SH3) Release"
# Name "JWPce_CE - Win32 (WCE SH3) Debug"
# Name "JWPce_CE - Win32 (WCE ARM) Release"
# Name "JWPce_CE - Win32 (WCE ARM) Debug"
# Name "JWPce_CE - Win32 (WCE MIPSFP) Release"
# Name "JWPce_CE - Win32 (WCE MIPSFP) Debug"
# Name "JWPce_CE - Win32 (WCE x86) Release"
# Name "JWPce_CE - Win32 (WCE x86) Debug"
# Name "JWPce_CE - Win32 (WCE PPC) Release"
# Name "JWPce_CE - Win32 (WCE PPC) Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\jwp_cach.cpp
DEP_CPP_JWP_C=\
	".\jwp_cach.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_clip.cpp
DEP_CPP_JWP_CL=\
	".\jwp_cach.h"\
	".\jwp_clip.h"\
	".\jwp_conf.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_conv.cpp
DEP_CPP_JWP_CO=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_stat.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_dict.cpp
DEP_CPP_JWP_D=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_dict.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_edit.cpp
DEP_CPP_JWP_E=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_flio.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_info.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_file.cpp
DEP_CPP_JWP_F=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_dict.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_info.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_lkup.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_stat.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_find.cpp
DEP_CPP_JWP_FI=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_find.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_flio.cpp
DEP_CPP_JWP_FL=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_file.h"\
	".\jwp_flio.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_stat.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_font.cpp
DEP_CPP_JWP_FO=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_help.cpp
DEP_CPP_JWP_H=\
	".\jwp_conf.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_options.h"\
	".\jwp_prnt.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_info.cpp
DEP_CPP_JWP_I=\
	".\jwp_cach.h"\
	".\jwp_clip.h"\
	".\jwp_conf.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_info.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_inpt.cpp
DEP_CPP_JWP_IN=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_stat.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_inst.cpp
DEP_CPP_JWP_INS=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_inst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_prnt.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_jisc.cpp
DEP_CPP_JWP_J=\
	".\jwp_cach.h"\
	".\jwp_cp1250.dat"\
	".\jwp_cp1251.dat"\
	".\jwp_cp1252.dat"\
	".\jwp_cp1253.dat"\
	".\jwp_cp1254.dat"\
	".\jwp_cp1255.dat"\
	".\jwp_cp1256.dat"\
	".\jwp_cp1257.dat"\
	".\jwp_cp1258.dat"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_ukan.dat"\
	".\jwp_umis.dat"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_klst.cpp
DEP_CPP_JWP_K=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_info.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_lkup.cpp
DEP_CPP_JWP_L=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_info.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_lkup.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_misc.cpp
DEP_CPP_JWP_M=\
	".\jwp_cach.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_para.cpp
DEP_CPP_JWP_P=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_prnt.cpp
DEP_CPP_JWP_PR=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_stat.cpp
DEP_CPP_JWP_S=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_stat.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_undo.cpp
DEP_CPP_JWP_U=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_stat.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=.\jwp_wnce.cpp
DEP_CPP_JWP_W=\
	".\jwp_options.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# Begin Source File

SOURCE=".\jwpce-ce.rc"
# End Source File
# Begin Source File

SOURCE=.\jwpce.cpp
DEP_CPP_JWPCE=\
	".\jwp_cach.h"\
	".\jwp_clip.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_dict.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_find.h"\
	".\jwp_flio.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_info.h"\
	".\jwp_inpt.h"\
	".\jwp_inst.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_lkup.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_stat.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\jwp_cach.h
# End Source File
# Begin Source File

SOURCE=.\jwp_clip.h
# End Source File
# Begin Source File

SOURCE=.\jwp_conf.h
# End Source File
# Begin Source File

SOURCE=.\jwp_conv.h
# End Source File
# Begin Source File

SOURCE=.\jwp_dict.h
# End Source File
# Begin Source File

SOURCE=.\jwp_edit.h
# End Source File
# Begin Source File

SOURCE=.\jwp_file.h
# End Source File
# Begin Source File

SOURCE=.\jwp_find.h
# End Source File
# Begin Source File

SOURCE=.\jwp_flio.h
# End Source File
# Begin Source File

SOURCE=.\jwp_font.h
# End Source File
# Begin Source File

SOURCE=.\jwp_help.h
# End Source File
# Begin Source File

SOURCE=.\jwp_info.h
# End Source File
# Begin Source File

SOURCE=.\jwp_inpt.h
# End Source File
# Begin Source File

SOURCE=.\jwp_inst.h
# End Source File
# Begin Source File

SOURCE=.\jwp_jisc.h
# End Source File
# Begin Source File

SOURCE=.\jwp_klst.h
# End Source File
# Begin Source File

SOURCE=.\jwp_lkup.h
# End Source File
# Begin Source File

SOURCE=.\jwp_misc.h
# End Source File
# Begin Source File

SOURCE=.\jwp_options.h
# End Source File
# Begin Source File

SOURCE=.\jwp_para.h
# End Source File
# Begin Source File

SOURCE=.\jwp_prnt.h
# End Source File
# Begin Source File

SOURCE=.\jwp_stat.h
# End Source File
# Begin Source File

SOURCE=.\jwp_undo.h
# End Source File
# Begin Source File

SOURCE=.\jwp_wnce.h
# End Source File
# Begin Source File

SOURCE=.\jwpce.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\alticon.ico
# End Source File
# Begin Source File

SOURCE=.\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\bitmap2.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00001.bmp
# End Source File
# Begin Source File

SOURCE=".\bushu-button.bmp"
# End Source File
# Begin Source File

SOURCE=.\charinfo.bmp
# End Source File
# Begin Source File

SOURCE=.\dictionary.bmp
# End Source File
# Begin Source File

SOURCE=.\fileicon.ico
# End Source File
# Begin Source File

SOURCE=.\fourcorners.bmp
# End Source File
# Begin Source File

SOURCE=.\hpctools.bmp
# End Source File
# Begin Source File

SOURCE=".\hsradicals-ce.bmp"
# End Source File
# Begin Source File

SOURCE=.\hsradicals.bmp
# End Source File
# Begin Source File

SOURCE=.\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\kanjiconvert.bmp
# End Source File
# Begin Source File

SOURCE=.\mainicon.ico
# End Source File
# Begin Source File

SOURCE=.\projicon.ico
# End Source File
# Begin Source File

SOURCE=".\rad-button.bmp"
# End Source File
# Begin Source File

SOURCE=".\radicals-ce.bmp"
# End Source File
# Begin Source File

SOURCE=.\radicals.bmp
# End Source File
# Begin Source File

SOURCE=.\radlooku.bmp
# End Source File
# Begin Source File

SOURCE=".\skip-button.bmp"
# End Source File
# Begin Source File

SOURCE=.\skiptype.bmp
# End Source File
# Begin Source File

SOURCE=.\strokes.bmp
# End Source File
# End Group
# Begin Source File

SOURCE=.\changes.txt
# End Source File
# End Target
# End Project
