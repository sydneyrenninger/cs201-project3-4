# Nota Hospital ER Simulation - CS 201 Project 3
#
#   make            build the SFML GUI            -> ./hospital
#   make run        build and run the GUI
#   make cli        build the console version     -> ./hospital_cli
#   make test       build and run the tests       -> ./hospital_tests
#   make data       generate the larger synthetic dataset -> data/*_large.csv
#   make run-large run the GUI on the larger dataset
#   make clean      remove build output
#
# SFML 2.5+ or 3.x is required for the GUI only. If it is not installed in a
# standard location, point SFML_DIR at it, e.g.
#   make SFML_DIR=C:/SFML-2.6.2
#   make SFML_DIR=/opt/homebrew/opt/sfml

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -Iinclude
CPPFLAGS += -Iinclude -MMD -MP

BUILD := build

# Engine code shared by the GUI, the console version and the tests.
CORE_SRC := src/PersonKey.cpp src/SimTime.cpp src/Patient.cpp src/Doctor.cpp \
            src/DataLoader.cpp src/Hospital.cpp
GUI_SRC  := src/Gui.cpp src/main.cpp
CLI_SRC  := src/cli_main.cpp
TEST_SRC := tests/test_main.cpp
GEN_SRC  := tools/generate_data.cpp

obj = $(addprefix $(BUILD)/,$(notdir $(1:.cpp=.o)))
CORE_OBJ := $(call obj,$(CORE_SRC))
GUI_OBJ  := $(call obj,$(GUI_SRC))
CLI_OBJ  := $(call obj,$(CLI_SRC))
TEST_OBJ := $(call obj,$(TEST_SRC))
GEN_OBJ  := $(call obj,$(GEN_SRC))

# ---- SFML ----------------------------------------------------------------
SFML_LIBS := -lsfml-graphics -lsfml-window -lsfml-system
# -isystem (not -I) so warnings inside SFML's own headers are not shown.
ifneq ($(SFML_DIR),)
  SFML_CPPFLAGS := -isystem $(SFML_DIR)/include
  SFML_LDFLAGS  := -L$(SFML_DIR)/lib
endif

# ---- Platform differences ------------------------------------------------
ifeq ($(OS),Windows_NT)
  EXE := .exe
  # Git Bash / MSYS2 put their own libstdc++-6.dll on the PATH, which a g++
  # build from a different toolchain can pick up and crash at startup. The
  # console programs link the runtime in statically to avoid that. (The GUI
  # cannot, it uses SFML's DLLs.)
  STATIC := -static
  # GNU make on Windows uses sh if it is on the PATH (MSYS2, Git Bash),
  # otherwise cmd.exe, which has no rm/mkdir -p.
  ifeq ($(shell sh -c "echo sh" 2>&1),sh)
    MKDIR_BUILD := mkdir -p $(BUILD)
    CLEAN_CMD   := rm -rf $(BUILD) hospital$(EXE) hospital_cli$(EXE) hospital_tests$(EXE) generate_data$(EXE)
  else
    MKDIR_BUILD := if not exist $(BUILD) mkdir $(BUILD)
    CLEAN_CMD   := if exist $(BUILD) rmdir /S /Q $(BUILD) & del /Q /F hospital$(EXE) hospital_cli$(EXE) hospital_tests$(EXE) generate_data$(EXE) 2>NUL
  endif
else
  EXE :=
  STATIC :=
  MKDIR_BUILD := mkdir -p $(BUILD)
  CLEAN_CMD   := rm -rf $(BUILD) hospital hospital_cli hospital_tests generate_data
  ifeq ($(shell uname -s),Darwin)
    # Homebrew installs outside the default search path on Apple Silicon.
    ifeq ($(SFML_DIR),)
      BREW_SFML := $(shell brew --prefix sfml 2>/dev/null)
      ifneq ($(BREW_SFML),)
        SFML_CPPFLAGS := -isystem $(BREW_SFML)/include
        SFML_LDFLAGS  := -L$(BREW_SFML)/lib -Wl,-rpath,$(BREW_SFML)/lib
      endif
    endif
  endif
endif

GUI_BIN  := hospital$(EXE)
CLI_BIN  := hospital_cli$(EXE)
TEST_BIN := hospital_tests$(EXE)
GEN_BIN  := generate_data$(EXE)

# ---- Targets -------------------------------------------------------------
.PHONY: all gui cli test run run-cli data run-large clean

all: gui
gui: $(GUI_BIN)
cli: $(CLI_BIN)

$(GUI_BIN): $(CORE_OBJ) $(GUI_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(SFML_LDFLAGS) $(SFML_LIBS)

$(CLI_BIN): $(CORE_OBJ) $(CLI_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(STATIC)

$(TEST_BIN): $(CORE_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(STATIC)

$(GEN_BIN): $(GEN_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(STATIC)

# Writes data/doctors_large.csv and data/patients_large.csv (same seed, same files).
data: $(GEN_BIN)
	./$(GEN_BIN)

run-large: $(GUI_BIN) data
	./$(GUI_BIN) data/doctors_large.csv data/patients_large.csv

test: $(TEST_BIN)
	./$(TEST_BIN)

run: $(GUI_BIN)
	./$(GUI_BIN)

run-cli: $(CLI_BIN)
	./$(CLI_BIN)

# Only the GUI files need the SFML headers.
$(BUILD)/Gui.o $(BUILD)/main.o: CPPFLAGS += $(SFML_CPPFLAGS)

vpath %.cpp src tests tools

$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD):
	$(MKDIR_BUILD)

clean:
	$(CLEAN_CMD)

-include $(wildcard $(BUILD)/*.d)
