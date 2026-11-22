add_rules("mode.debug", "mode.release")

-- 使用官方 stm32-devel addon：
-- 它提供 gnu-rm 工具链、CMSIS 启动文件/SystemInit，并根据 board 自动生成链接脚本。
-- add_addons("stm32-devel")

includes("@addon/stm32-devel/board")

target("voice")
    add_rules("@addon/stm32-devel/app")
    set_languages("c99")

    -- STM32F103C8: 中容量产品，使用标准外设库，外部晶振 8MHz
    add_defines("STM32F10X_MD", "USE_STDPERIPH_DRIVER", "HSE_VALUE=8000000")

    -- StdPeriph 头文件搜索路径
    -- 注意：add_includedirs 不会展开 ** 通配符，子目录必须显式列出，如：
    -- "Hardware",
    -- "Hardware/LED",
    -- "Hardware/USART"
    add_includedirs(
        "Start",
        "Library",
        "System",
        "System/sys",
        "User",
        "Hardware",
        "Hardware/LED",
        "Hardware/USART"
    )

    -- set output filepath of .elf and .hex
    set_targetdir("Objects/")

    -- StdPeriph 外设库与应用代码
    -- 启动文件和 SystemInit 由 addon 的 CMSIS 设备包提供，不要重复添加
    -- 使用 dir/**.c 递归匹配该目录及其所有子目录下的 .c 文件
    -- 不要直接写 "**.c"，否则会把 Start/ 下由 addon 提供的文件也加进来导致重复定义
    add_files(
        "Library/**.c",
        "System/**.c",
        "User/**.c",
        "Hardware/**.c"
    )

--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro definition
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--
