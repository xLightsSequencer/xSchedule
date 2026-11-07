#!/bin/bash
# Linux sanity check in a throwaway container (needs only Docker):
#   docker run --rm -v "<repo>:/src" ubuntu:24.04 bash /src/xSchedule/tests/linux_check.sh
# Installs the distro wxWidgets (3.2) to build and run the schedule unit tests and to
# compile-check the GUI files that the Linux CI build would otherwise only find later.
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq g++ make libwxgtk3.2-dev >/dev/null

cd /src/xSchedule/tests
rm -rf build
make test

cd /src/xSchedule
INCLUDES="-I../xlights/include -I../xlights/src-core -I../xlights/src-core/utils -I../xlights/src-ui-wx -I../xlights/src-ui-wx/shared/utils -I../xlights/common -I../dependencies -I../dependencies/spdlog/include"
for f in ModernUI.cpp Holidays.cpp Schedule.cpp ScheduleDialog.cpp SchedulePreview.cpp ScheduleCalendar.cpp CalendarPanel.cpp ShowBackups.cpp RestoreBackupDialog.cpp; do
    echo "syntax check $f"
    g++ -std=gnu++20 -fsyntax-only $INCLUDES $(wx-config --cxxflags) "$f"
done
echo "linux check passed"
