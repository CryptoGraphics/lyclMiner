-- ----------------------------------------------------------------------------
-- Setup addional paths if headers and libs are in different place.(Optional)
function setupJansson()
    includedirs { "" }
    libdirs { "" }
    
    links "jansson"
end

function setupCurl()
    includedirs { "" }
    libdirs { "" }
    
    links "curl"
end

function setupOpenCL()
    -- Alternative: opencl-headers package
    includedirs { "/opt/rocm/opencl/include/" }
    -- ROCm and/or AMDGPU-Pro    
    libdirs { "/opt/rocm/opencl/lib/x86_64/",
              "/opt/amdgpu-pro/lib/x86_64-linux-gnu/" }
    
    links "OpenCL"
end
-- ----------------------------------------------------------------------------


workspace "lyclMinerWorkspace"
    architecture "x86_64"
    location "build"

    -- setup configurations
    configurations { "Release", "Debug" }
    filter { "configurations:Release" }
        defines { "NDEBUG" }
        optimize "On"
    filter { "configurations:Debug" }
        defines { "DEBUG" }
        symbols "On"
    filter { }

    -- dependencies
    setupOpenCL()
    setupJansson()
    setupCurl()

    -- project configuration
    project "lyclMiner"
        kind "ConsoleApp"
        language "C++"
        location "build/lyclMiner"
        
        targetdir "bin"
        
        cppdialect "C++11"
        
        includedirs { "src", "." }

        filter { "system:Windows" }
            system "windows"
            -- mingw-w64
            links { "Ws2_32" }
        filter { "system:Linux" }
            system "linux"
            -- linux gcc
            links { "crypto", "pthread" }
        filter { }

        files { "src/lyclApplets/*.hpp",
                "src/lyclApplets/*.cpp",
                "src/lyclCore/*.hpp",
                "src/lyclCore/*.cpp",
                "src/lyclHostValidators/*.hpp",
                "src/lyclHostValidators/*.cpp",
                "src/main.cpp" }
