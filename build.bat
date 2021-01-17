@echo off
pushd 
IF NOT EXIST ..\build mkdir ..\build
pushd ..\build
cl -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -Zi ..\code\hh_e10.cpp user32.lib gdi32.lib
popd