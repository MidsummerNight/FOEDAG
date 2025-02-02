/*
Copyright 2021 The Foedag team

GPL License

Copyright (c) 2021 The Open-Source FPGA Foundation

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "CommandLine.h"
#include "Foedag.h"
#include "MainWindow/Session.h"
#include "MainWindow/main_window.h"

QWidget* mainWindowBuilder(FOEDAG::CommandLine* cmd,
                           FOEDAG::TclInterpreter* interp) {
  return new FOEDAG::MainWindow{interp};
}

FOEDAG::GUI_TYPE getGuiType(const bool& withQt, const bool& withQml) {
  if (!withQt) return FOEDAG::GUI_TYPE::GT_NONE;
  if (withQml)
    return FOEDAG::GUI_TYPE::GT_QML;
  else
    return FOEDAG::GUI_TYPE::GT_WIDGET;
}

int main(int argc, char** argv) {
  Q_INIT_RESOURCE(compiler_resources);
  FOEDAG::CommandLine* cmd = new FOEDAG::CommandLine(argc, argv);
  cmd->processArgs();

  FOEDAG::GUI_TYPE guiType = getGuiType(cmd->WithQt(), cmd->WithQml());

  FOEDAG::Foedag* foedag =
      new FOEDAG::Foedag(cmd, mainWindowBuilder, registerAllFoedagCommands);

  return foedag->init(guiType);
}
