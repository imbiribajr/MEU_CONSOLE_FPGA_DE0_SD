@echo off
setlocal
set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"
set "APP_DIR=%~dp0"
set "APP_DIR_SLASH=%APP_DIR:\=/%"
set "APP_DIR_CYG=/cygdrive/%APP_DIR_SLASH:~0,1%%APP_DIR_SLASH:~2%"
set "LINKER_SCRIPT=./linker_flashboot.x"

call "%NIOS_SHELL%" sh -c "cd \"%APP_DIR_CYG%\" && nios2-download -r -g main.elf --accept-bad-sysid && nios2-terminal"
endlocal
