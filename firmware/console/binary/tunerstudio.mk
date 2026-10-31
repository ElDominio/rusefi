
TUNERSTUDIO_SRC_CPP = $(PROJECT_DIR)/console/binary/tunerstudio_io.cpp \
	$(PROJECT_DIR)/console/binary/tunerstudio_io_serial.cpp \
	$(PROJECT_DIR)/console/binary/tunerstudio_io_serial_ports.cpp \
	$(PROJECT_DIR)/console/binary/generated/live_data.cpp \
	$(PROJECT_DIR)/console/binary/ts_can_channel.cpp \
	$(PROJECT_DIR)/console/binary/serial_can.cpp \
	$(PROJECT_DIR)/console/binary/tunerstudio.cpp \
	$(PROJECT_DIR)/console/binary/tunerstudio_calibration_channel.cpp \
	$(PROJECT_DIR)/console/binary/tunerstudio_commands.cpp \
	$(PROJECT_DIR)/console/binary/bluetooth.cpp \
	$(PROJECT_DIR)/console/binary/signature.cpp \
	$(PROJECT_DIR)/console/binary/trigger_scope.cpp \


# 64-bit POSIX simulator (simulator/posix shim): no ChibiOS SerialDriver, TS runs on plain TCP sockets.
ifeq ($(POSIX_SIM),yes)
TUNERSTUDIO_SRC_CPP := $(filter-out \
	$(PROJECT_DIR)/console/binary/tunerstudio_io_serial.cpp \
	$(PROJECT_DIR)/console/binary/tunerstudio_io_serial_ports.cpp, \
	$(TUNERSTUDIO_SRC_CPP))
TUNERSTUDIO_SRC_CPP += $(PROJECT_DIR)/console/binary/tunerstudio_io_posix.cpp
endif
