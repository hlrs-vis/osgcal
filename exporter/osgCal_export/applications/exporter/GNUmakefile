TOPDIR = ../..
include $(TOPDIR)/Make/makedefs

ifeq ($(OS),MINGW)
3DSMAX_PATH="`reg query "HKLM\Software\Autodesk\3dsMax\9.0\MAX-1:409" //v Installdir \
              | grep REG_SZ \
              | sed "s%.*REG_SZ.\(.*\)%\1%g"`"

SCRIPT_PATH=$(3DSMAX_PATH)/scripts
UTILITY_SCRIPTS_PATH=$(3DSMAX_PATH)/scripts/cal3D_export

default: all

all:
		echo $(SCRIPT_PATH)
		install -d $(SCRIPT_PATH)
		install export_cal3D.ms $(SCRIPT_PATH)
		install -d $(UTILITY_SCRIPTS_PATH)
		install csv_edit.py $(UTILITY_SCRIPTS_PATH)
		install export_textures_and_prepare_model.bat $(UTILITY_SCRIPTS_PATH)
		install export_textures.py $(UTILITY_SCRIPTS_PATH)

clean:
	rm -f *~
else
all clean debug install default:
endif
