workspace "bind"
    configurations { "Debug", "Release" }

    filter "Configurations:Debug"
        defines "DEBUG"
        optimize "Off"
        symbols "On"
        
    filter "Configurations:Release"
        optimize "Full"
        symbols "Off"
    
project "bind_find_vs"
    kind "SharedLib"
    language "C++"
    location "build"
    
    targetname "find_vs"

    defines { "BUILD_DLL" }
    files "./lib/find_vs.h"
    links { "Advapi32.lib", "Ole32.lib", "OleAut32.lib" }

project "bind"
    kind "ConsoleApp"
    language "C"
    location "build"
    
    targetname "bind"
    targetdir "."
    
    includedirs { "./include", "./lib" }
    files { "./src/*.c" }

    filter "system:linux"
        links { "pthread", "dl" }
        
    filter "system:windows"
    links { "bind_find_vs", "kernel32.lib" }

newaction {
   trigger = "clean",
   description = "Clean generated files",
   execute = function ()
      os.remove("Makefile")
      os.rmdir("build")
   end
}
