# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/hgkq/esp/v5.1.2/esp-idf/components/bootloader/subproject"
  "D:/0.Studio_Android/AndroidStudioProjects/temperatureMonitoring/build/bootloader"
  "D:/0.Studio_Android/AndroidStudioProjects/temperatureMonitoring/build/bootloader-prefix"
  "D:/0.Studio_Android/AndroidStudioProjects/temperatureMonitoring/build/bootloader-prefix/tmp"
  "D:/0.Studio_Android/AndroidStudioProjects/temperatureMonitoring/build/bootloader-prefix/src/bootloader-stamp"
  "D:/0.Studio_Android/AndroidStudioProjects/temperatureMonitoring/build/bootloader-prefix/src"
  "D:/0.Studio_Android/AndroidStudioProjects/temperatureMonitoring/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/0.Studio_Android/AndroidStudioProjects/temperatureMonitoring/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/0.Studio_Android/AndroidStudioProjects/temperatureMonitoring/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
