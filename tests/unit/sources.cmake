# Production sources compiled into xschedule_testlib.
#
# Keep this to what the tests need: add a file when a test starts using it (or when
# the linker asks for it). Paths are the same files the application builds.
set(XS_TEST_SOURCES
    # scheduling
    ${XS_SRC}/Schedule.cpp
    ${XS_SRC}/ScheduleDialog.cpp
    ${XS_SRC}/City.cpp

    # output processing (frame data manipulation)
    ${XS_SRC}/OutputProcess.cpp
    ${XS_SRC}/OutputProcessColourOrder.cpp
    ${XS_SRC}/OutputProcessDeadChannel.cpp
    ${XS_SRC}/OutputProcessDim.cpp
    ${XS_SRC}/OutputProcessDimWhite.cpp
    ${XS_SRC}/OutputProcessExcludeDim.cpp
    ${XS_SRC}/OutputProcessGamma.cpp
    ${XS_SRC}/OutputProcessRemap.cpp
    ${XS_SRC}/OutputProcessReverse.cpp
    ${XS_SRC}/OutputProcessSet.cpp
    ${XS_SRC}/OutputProcessSustain.cpp
    ${XS_SRC}/OutputProcessThreeToFour.cpp

    # blending / misc
    ${XS_SRC}/Blend.cpp
    ${XS_SRC}/md5.cpp

    # xSchedule's no-op Discovery (referenced by some output classes)
    ${XS_SRC}/DiscoveryStub.cpp

    # xLights core: the outputs layer (OutputManager decodes start channels)
    ${XL_ROOT}/src-core/outputs/ArtNetOutput.cpp
    ${XL_ROOT}/src-core/outputs/Controller.cpp
    ${XL_ROOT}/src-core/outputs/ControllerEthernet.cpp
    ${XL_ROOT}/src-core/outputs/ControllerNull.cpp
    ${XL_ROOT}/src-core/outputs/ControllerSerial.cpp
    ${XL_ROOT}/src-core/outputs/DDPOutput.cpp
    ${XL_ROOT}/src-core/outputs/DMXOutput.cpp
    ${XL_ROOT}/src-core/outputs/E131Output.cpp
    ${XL_ROOT}/src-core/outputs/GenericSerialOutput.cpp
    ${XL_ROOT}/src-core/outputs/IPOutput.cpp
    ${XL_ROOT}/src-core/outputs/KinetOutput.cpp
    ${XL_ROOT}/src-core/outputs/LorController.cpp
    ${XL_ROOT}/src-core/outputs/LorControllers.cpp
    ${XL_ROOT}/src-core/outputs/LOROptimisedOutput.cpp
    ${XL_ROOT}/src-core/outputs/LOROutput.cpp
    ${XL_ROOT}/src-core/outputs/NullOutput.cpp
    ${XL_ROOT}/src-core/outputs/OPCOutput.cpp
    ${XL_ROOT}/src-core/outputs/OpenDMXOutput.cpp
    ${XL_ROOT}/src-core/outputs/OpenPixelNetOutput.cpp
    ${XL_ROOT}/src-core/outputs/Output.cpp
    ${XL_ROOT}/src-core/outputs/OutputManager.cpp
    ${XL_ROOT}/src-core/outputs/PixelNetOutput.cpp
    ${XL_ROOT}/src-core/outputs/RenardOutput.cpp
    ${XL_ROOT}/src-core/outputs/serial.cpp
    ${XL_ROOT}/src-core/outputs/SerialOutput.cpp
    ${XL_ROOT}/src-core/outputs/TestPreset.cpp
    ${XL_ROOT}/src-core/outputs/TwinklyOutput.cpp
    ${XL_ROOT}/src-core/outputs/xxxEthernetOutput.cpp
    ${XL_ROOT}/src-core/outputs/xxxSerialOutput.cpp
    ${XL_ROOT}/src-core/outputs/ZCPPOutput.cpp
    ${XL_ROOT}/src-core/controllers/BaseController.cpp
    ${XL_ROOT}/src-core/controllers/ControllerCaps.cpp
    ${XL_ROOT}/src-core/controllers/Falcon.cpp
    ${XL_ROOT}/src-core/models/Pixels.cpp
    ${XL_ROOT}/src-core/utils/AppCallbacks.cpp
    ${XL_ROOT}/src-core/utils/CurlManager.cpp
    ${XL_ROOT}/src-core/utils/FileUtils.cpp
    ${XL_ROOT}/src-core/utils/ip_utils.cpp
    ${XL_ROOT}/src-core/utils/Parallel.cpp
    ${XL_ROOT}/src-core/utils/RangeWorkPool.cpp
    ${XL_ROOT}/src-core/utils/xlExceptionDescribe.cpp
    ${XL_ROOT}/src-core/utils/JobPool.cpp
    ${XL_ROOT}/src-core/utils/LayoutObjectId.cpp
    ${XL_ROOT}/src-core/utils/LayoutRenameLog.cpp
    ${XL_ROOT}/src-core/utils/ShowFileGuard.cpp
    ${XL_ROOT}/src-core/utils/SpecialOptions.cpp
    ${XL_ROOT}/src-core/utils/string_utils.cpp
    ${XL_ROOT}/src-core/utils/TraceLog.cpp
    ${XL_ROOT}/src-core/utils/UtilFunctions.cpp
    ${XL_ROOT}/src-ui-wx/shared/utils/wxUtilities.cpp
    ${XS_REPO_ROOT}/dependencies/pugixml/src/pugixml.cpp
)
