Import("env")

# esptool 5 prints Unicode progress bars. On Windows terminals using cp1252,
# that can make PlatformIO uploads hang or fail while writing flash.
env["ENV"]["PYTHONIOENCODING"] = "utf-8"
env["ENV"]["PYTHONUTF8"] = "1"
