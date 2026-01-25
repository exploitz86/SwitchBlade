add_repositories("switch-repo https://github.com/exploitz86/switch-repo.git")
add_repositories("zeromake-repo https://github.com/exploitz86/xrepo.git")


includes("toolchain/*.lua")

add_defines(
    'BRLS_RESOURCES="romfs:/"',
    "YG_ENABLE_EVENTS",
    "STBI_NO_THREAD_LOCALS", 
    "BOREALIS_USE_DEKO3D"
)

add_rules("mode.debug", "mode.release")

add_requires("borealis", {repo = "switch-repo"}, "deko3d", "libcurl", "libarchive", "bzip2", "zlib", "liblzma", "lz4", "libexpat", "libzstd")

local version = "1.0.1"

target("SwitchBlade")
    set_kind("binary")
    if not is_plat("cross") then
        return
    end

    -- Build forwarder and TegraExplorer before main app and copy to romfs
    before_build(function (target)
        -- Build forwarder only if not already built
        if not os.exists("forwarder/forwarder.nro") then
            print("Building forwarder...")
            os.exec("make -C forwarder clean")
            os.exec("make -C forwarder")
        else
            print("Forwarder already built, skipping...")
        end

        print("Copying forwarder.nro to romfs...")
        os.cp("forwarder/forwarder.nro", "resources/forwarder.nro")

        -- Build TegraExplorer only if not already built
        if not os.exists("TegraExplorer/output/TegraExplorer.bin") then
            print("Building TegraExplorer...")
            os.exec("make -C TegraExplorer")
        else
            print("TegraExplorer already built, skipping...")
        end

        print("Copying TegraExplorer.bin to romfs as switchblade_rcm.bin...")
        os.cp("TegraExplorer/output/TegraExplorer.bin", "resources/switchblade_rcm.bin")
    end)

    set_arch("aarch64")
    add_rules("switch")
    set_toolchains("devkita64")
    set_languages("c++17")
    
    set_values("switch.name", "SwitchBlade")
    set_values("switch.author", "eXploitz")
    set_values("switch.version", version)
    set_values("switch.romfs", "resources")
    set_values("switch.icon", "resources/icon/icon-256.jpg")

    -- Pass version as compiler define so C++ code can use APP_VERSION
    add_defines('APP_VERSION="' .. version .. '"')

    -- SimpleIniParser
    add_files("lib/ini/source/SimpleIniParser/*.cpp")
    add_includedirs("lib/ini/include")
    add_includedirs("lib/ini/include/SimpleIniParser")

    add_files("source/**.cpp")
    add_includedirs("include")
    add_packages("borealis", "deko3d", "libcurl", "libarchive", "bzip2", "zlib", "liblzma", "lz4", "libexpat", "libzstd")
