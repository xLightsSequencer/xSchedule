# Builds and runs the schedule unit tests on Linux/macOS: make test
CXX ?= g++
WX_CONFIG ?= wx-config
CXXFLAGS += -std=gnu++20 -O2 -I../../xlights/include -I../../xlights/src-core/utils -I../../xlights/src-core \
            -I../../xlights/src-ui-wx -I../../xlights/src-ui-wx/shared/utils -I../../xlights/common -I../../dependencies -I../../dependencies/spdlog/include \
            $(shell $(WX_CONFIG) --cxxflags)
SOURCES = ScheduleTests.cpp ../Schedule.cpp ../SchedulePreview.cpp ../ShowBackups.cpp ../City.cpp ../Holidays.cpp

build/xScheduleTests: $(SOURCES) ../Schedule.h ../SchedulePreview.h ../SettingsFiles.h ../ShowBackups.h ../Holidays.h ../City.h
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@ $(shell $(WX_CONFIG) --libs std,xml)

test: build/xScheduleTests
	./build/xScheduleTests

clean:
	rm -rf build

.PHONY: test clean
