# Microsoft Developer Studio Project File - Name="jwpce" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101
# TARGTYPE "Win32 (WCE SH3) Application" 0x8101
# TARGTYPE "Win32 (WCE MIPS) Application" 0x8201

CFG=jwpce - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "jwpce.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "jwpce.mak" CFG="jwpce - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "jwpce - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "jwpce - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "jwpce - Win32 (WCE MIPS) Release" (based on "Win32 (WCE MIPS) Application")
!MESSAGE "jwpce - Win32 (WCE MIPS) Debug" (based on "Win32 (WCE MIPS) Application")
!MESSAGE "jwpce - Win32 (WCE SH3) Release" (based on "Win32 (WCE SH3) Application")
!MESSAGE "jwpce - Win32 (WCE SH3) Debug" (based on "Win32 (WCE SH3) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath "Desktop"

!IF  "$(CFG)" == "jwpce - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "jwpce___"
# PROP BASE Intermediate_Dir "jwpce___"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comctl32.lib imm32.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "jwpce__0"
# PROP BASE Intermediate_Dir "jwpce__0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comctl32.lib imm32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

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
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /machine:MIPS /subsystem:$(CESubsystem)
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib /nologo /machine:MIPS /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem)
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
# ADD BASE CPP /nologo /ML /W3 /O2 /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "NDEBUG" /D "MIPS" /D "_MIPS_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /YX /QMRWCE /c
# ADD CPP /nologo /M$(CECrt) /W3 /O1 /D "NDEBUG" /D "MIPS" /D "_MIPS_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "WINCE" /D "WINCETEMP" /YX /QMRWCE /c
PFILE=pfile.exe
# ADD BASE PFILE COPY
# ADD PFILE COPY

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /debug /machine:MIPS /subsystem:$(CESubsystem)
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 comctl32.lib commctrl.lib coredll.lib comdlg32.lib /nologo /debug /machine:MIPS /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem)
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
# ADD BASE CPP /nologo /MLd /W3 /Zi /Od /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "DEBUG" /D "MIPS" /D "_MIPS_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /YX /QMRWCE /c
# ADD CPP /nologo /M$(CECrtDebug) /W3 /Zi /Od /D "DEBUG" /D "MIPS" /D "_MIPS_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "WINCE" /YX /QMRWCE /c
PFILE=pfile.exe
# ADD BASE PFILE COPY
# ADD PFILE COPY

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WCESHRel"
# PROP BASE Intermediate_Dir "WCESHRel"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WCESHRel"
# PROP Intermediate_Dir "WCESHRel"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /machine:SH3 /subsystem:$(CESubsystem)
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 commctrl.lib coredll.lib /nologo /machine:SH3 /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem)
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
# ADD BASE CPP /nologo /ML /W3 /O2 /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "NDEBUG" /D "SHx" /D "SH3" /D "_SH3_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /YX /c
# ADD CPP /nologo /M$(CECrt) /W3 /O2 /D "NDEBUG" /D "SHx" /D "SH3" /D "_SH3_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "WINCE" /D "WINCETEMP" /YX /c
PFILE=pfile.exe
# ADD BASE PFILE COPY
# ADD PFILE COPY

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WCESHDbg"
# PROP BASE Intermediate_Dir "WCESHDbg"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WCESHDbg"
# PROP Intermediate_Dir "WCESHDbg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LINK32=link.exe
# ADD BASE LINK32 commctrl.lib coredll.lib /nologo /debug /machine:SH3 /subsystem:$(CESubsystem)
# SUBTRACT BASE LINK32 /pdb:none /nodefaultlib
# ADD LINK32 comctl32.lib commctrl.lib coredll.lib comdlg32.lib /nologo /debug /machine:SH3 /nodefaultlib:"$(CENoDefaultLib)" /subsystem:$(CESubsystem)
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
# ADD BASE CPP /nologo /MLd /W3 /Zi /Od /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D "DEBUG" /D "SHx" /D "SH3" /D "_SH3_" /D UNDER_CE=$(CEVersion) /D "UNICODE" /YX /c
# ADD CPP /nologo /M$(CECrtDebug) /W3 /Zi /Od /D "DEBUG" /D "SHx" /D "SH3" /D "_SH3_" /D _WIN32_WCE=$(CEVersion) /D "$(CEConfigName)" /D UNDER_CE=$(CEVersion) /D "UNICODE" /D "WINCE" /YX /c
PFILE=pfile.exe
# ADD BASE PFILE COPY
# ADD PFILE COPY

!ENDIF 

# Begin Target

# Name "jwpce - Win32 Release"
# Name "jwpce - Win32 Debug"
# Name "jwpce - Win32 (WCE MIPS) Release"
# Name "jwpce - Win32 (WCE MIPS) Debug"
# Name "jwpce - Win32 (WCE SH3) Release"
# Name "jwpce - Win32 (WCE SH3) Debug"
# Begin Source File

SOURCE=.\alticon.ico
# End Source File
# Begin Source File

SOURCE=.\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\changes.txt
# End Source File
# Begin Source File

SOURCE=.\fileicon.ico
# End Source File
# Begin Source File

SOURCE=.\ico00001.ico
# End Source File
# Begin Source File

SOURCE=.\jwp_cach.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_cach.h
# End Source File
# Begin Source File

SOURCE=.\jwp_clip.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_clip.h
# End Source File
# Begin Source File

SOURCE=.\jwp_conf.h
# End Source File
# Begin Source File

SOURCE=.\jwp_conv.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_CO=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_edit.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_CO=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_edit.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_conv.h
# End Source File
# Begin Source File

SOURCE=.\jwp_dict.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_D=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_dict.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_D=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_dict.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_dict.h
# End Source File
# Begin Source File

SOURCE=.\jwp_edit.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_E=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_E=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_edit.h
# End Source File
# Begin Source File

SOURCE=.\jwp_file.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_file.h
# End Source File
# Begin Source File

SOURCE=.\jwp_find.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_FI=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_find.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_FI=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_find.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_find.h
# End Source File
# Begin Source File

SOURCE=.\jwp_flio.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_FL=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_file.h"\
	".\jwp_flio.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_FL=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_conv.h"\
	".\jwp_file.h"\
	".\jwp_flio.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_flio.h
# End Source File
# Begin Source File

SOURCE=.\jwp_font.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_font.h
# End Source File
# Begin Source File

SOURCE=.\jwp_help.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_H=\
	".\jwp_conf.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_options.h"\
	".\jwp_prnt.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

DEP_CPP_JWP_H=\
	".\jwp_conf.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_options.h"\
	".\jwp_prnt.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_H=\
	".\jwp_conf.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_options.h"\
	".\jwp_prnt.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

DEP_CPP_JWP_H=\
	".\jwp_conf.h"\
	".\jwp_font.h"\
	".\jwp_help.h"\
	".\jwp_options.h"\
	".\jwp_prnt.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_help.h
# End Source File
# Begin Source File

SOURCE=.\jwp_info.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_I=\
	".\jwp_cach.h"\
	".\jwp_clip.h"\
	".\jwp_conf.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_I=\
	".\jwp_cach.h"\
	".\jwp_clip.h"\
	".\jwp_conf.h"\
	".\jwp_edit.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_info.h
# End Source File
# Begin Source File

SOURCE=.\jwp_inpt.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_inpt.h
# End Source File
# Begin Source File

SOURCE=.\jwp_inst.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_INS=\
	".\jwp_conf.h"\
	".\jwp_font.h"\
	".\jwp_inst.h"\
	".\jwp_options.h"\
	".\jwp_prnt.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_INS=\
	".\jwp_conf.h"\
	".\jwp_font.h"\
	".\jwp_inst.h"\
	".\jwp_options.h"\
	".\jwp_prnt.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_inst.h
# End Source File
# Begin Source File

SOURCE=.\jwp_jisc.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_J=\
	".\jwp_cach.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

DEP_CPP_JWP_J=\
	".\jwp_cach.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_uext.dat"\
	".\jwp_ukan.dat"\
	".\jwp_umis.dat"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_J=\
	".\jwp_cach.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

DEP_CPP_JWP_J=\
	".\jwp_cach.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_inpt.h"\
	".\jwp_jisc.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_uext.dat"\
	".\jwp_ukan.dat"\
	".\jwp_umis.dat"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_jisc.h
# End Source File
# Begin Source File

SOURCE=.\jwp_klst.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_K=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

DEP_CPP_JWP_K=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_K=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

DEP_CPP_JWP_K=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
	".\jwp_file.h"\
	".\jwp_font.h"\
	".\jwp_jisc.h"\
	".\jwp_klst.h"\
	".\jwp_misc.h"\
	".\jwp_options.h"\
	".\jwp_para.h"\
	".\jwp_prnt.h"\
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_klst.h
# End Source File
# Begin Source File

SOURCE=.\jwp_lkup.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_L=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
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
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

DEP_CPP_JWP_L=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_L=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
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
	".\jwp_undo.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

DEP_CPP_JWP_L=\
	".\jwp_cach.h"\
	".\jwp_conf.h"\
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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_lkup.h
# End Source File
# Begin Source File

SOURCE=.\jwp_misc.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_M=\
	".\jwp_cach.h"\
	".\jwp_edit.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_M=\
	".\jwp_cach.h"\
	".\jwp_edit.h"\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_misc.h
# End Source File
# Begin Source File

SOURCE=.\jwp_options.h
# End Source File
# Begin Source File

SOURCE=.\jwp_para.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_P=\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_P=\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_para.h
# End Source File
# Begin Source File

SOURCE=.\jwp_prnt.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_PR=\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_PR=\
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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_prnt.h
# End Source File
# Begin Source File

SOURCE=.\jwp_stat.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_stat.h
# End Source File
# Begin Source File

SOURCE=.\jwp_undo.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_undo.h
# End Source File
# Begin Source File

SOURCE=.\jwp_wnce.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

DEP_CPP_JWP_W=\
	".\jwp_options.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

DEP_CPP_JWP_W=\
	".\jwp_options.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

DEP_CPP_JWP_W=\
	".\jwp_options.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

DEP_CPP_JWP_W=\
	".\jwp_options.h"\
	".\jwp_wnce.h"\
	".\jwpce.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwp_wnce.h
# End Source File
# Begin Source File

SOURCE=.\jwpce.cpp

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

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
	

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

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
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\jwpce.h
# End Source File
# Begin Source File

SOURCE=.\jwpce.rc

!IF  "$(CFG)" == "jwpce - Win32 Release"

!ELSEIF  "$(CFG)" == "jwpce - Win32 Debug"

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE MIPS) Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "jwpce - Win32 (WCE SH3) Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\mainicon.ico
# End Source File
# Begin Source File

SOURCE=.\projicon.ico
# End Source File
# End Target
# End Project
