@echo off

:: Make sure this is a decent name and not generic
set exe_name=bind.exe

:: Debug = 0, Release = 1
set release_mode=1
set compiler_flags= -nologo -Oi -Gm- -MP -GS- -EHsc- -Z7


rem if %release_mode% EQU 0 ( rem Debug
rem 	set compiler_flags=%compiler_flags% -Od -MDd -Z7
rem ) else ( rem Release
rem 	set compiler_flags=%compiler_flags% -O2 -MT -Z7 -DNO_ARRAY_BOUNDS_CHECK
rem )

set compiler_warnings= ^
	-W4 ^
	-wd4100 -wd4101 -wd4127 -wd4189 ^
	-wd4201 -wd4204 ^
	-wd4456 -wd4457 -wd4480 ^
	-wd4512 -wd4244 -wd4129

set compiler_includes=-Iinclude -Ilib
set libs= ^
	kernel32.lib find_vs.lib

set linker_flags= -incremental:no -opt:ref -subsystem:console

if %release_mode% EQU 0 ( rem Debug
	set linker_flags=%linker_flags% -debug
) else ( rem Release
	set linker_flags=%linker_flags% -debug
)

set compiler_settings=%compiler_includes% %compiler_flags% %compiler_warnings%
set linker_settings=%libs% %linker_flags%

del *.pdb > NUL 2> NUL
del *.ilk > NUL 2> NUL

cl %compiler_settings% /TP /LD /DBUILD_DLL "lib\find_vs.cpp" ^
	/link Advapi32.lib Ole32.lib OleAut32.lib

cl %compiler_settings% /TC "src\*.c" ^
	/link %linker_settings% -OUT:%exe_name%

del *.obj > NUL 2> NUL

:end_of_build