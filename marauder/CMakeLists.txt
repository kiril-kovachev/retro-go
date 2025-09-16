cmake_minimum_required(VERSION 3.5)
set(COMPONENTS "main retro-go marauder bootloader esptool_py")
set(RG_ENABLE_NETWORKING 1)
include(../base.cmake)
project(marauder)


