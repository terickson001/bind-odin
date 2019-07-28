@echo off

:: Make sure this is a decent name and not generic
set exe_name=bind.exe

:: Debug = 0, Release = 1
set release_mode=1
set compiler_flags= -nologo -Oi -Gm- -MP -GS- -EHsc- -Z7

set compiler_warnings= ^
	-W4 ^
	-wd4100 -wd4101 -wd4127 -wd4189 ^
	-wd4201 -wd4204 ^
	-wd4456 -wd4457 -wd4480 ^
	-wd4512 -wd4244 -wd4129

set compiler_includes=-Iinclude -Ilib
set libs= ^
	kernel32.lib find_vs.lib

set linker_flags= -incremental:no -opt:ref -subsystem:console -debug
set compiler_settings=%compiler_includes% %compiler_flags% %compiler_warnings%
set linker_settings=%libs% %linker_flags%

del %exe_name% > NUL 2> NUL
del *.pdb > NUL 2> NUL
del *.ilk > NUL 2> NUL

cl %compiler_settings% /TP /LD /DBUILD_DLL "lib\find_vs.cpp" ^
	/link Advapi32.lib Ole32.lib OleAut32.lib

cl %compiler_settings% /TC "src\*.c" ^
	/link %linker_settings% -OUT:%exe_name%

del *.obj > NUL 2> NUL

:end_of_build