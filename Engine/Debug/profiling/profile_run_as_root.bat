@echo off

echo Please run this as root!
echo In the properties, under Linker/Advanced, make sure “Profile” is set to “Enable"

set path=%path%;C:\Program Files (x86)\Microsoft Visual Studio 9.0\Team Tools\Performance Tools
vsinstr.exe Engine.exe
vsperfcmd /start:trace /output:Engine.vsp
Engine.exe
vsperfcmd /shutdown
vsperfreport Engine.vsp /output:.\result /summary:all