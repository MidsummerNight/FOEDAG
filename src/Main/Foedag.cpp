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

#if defined(_MSC_VER)
#include <direct.h>
#include <process.h>
#else
#include <sys/param.h>
#include <unistd.h>
#endif

#include <string.h>
#include <sys/stat.h>
extern "C" {
#include <tcl.h>
}

#include <QApplication>
#include <QGuiApplication>
#include <QLabel>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Command/CommandStack.h"
#include "CommandLine.h"
#include "Main/Foedag.h"
#include "MainWindow/Session.h"
#include "MainWindow/main_window.h"
#include "Tcl/TclInterpreter.h"
#include "qttclnotifier.hpp"

FOEDAG::Session* GlobalSession;

using namespace FOEDAG;

bool Foedag::initGui() {
  // Gui mode with Qt Widgets
  int argc = m_cmdLine->Argc();
  QApplication app(argc, m_cmdLine->Argv());
  FOEDAG::TclInterpreter* interpreter =
      new FOEDAG::TclInterpreter(m_cmdLine->Argv()[0]);
  FOEDAG::CommandStack* commands = new FOEDAG::CommandStack(interpreter);
  QWidget* mainWin = nullptr;
  if (m_mainWinBuilder) {
    mainWin = m_mainWinBuilder(m_cmdLine, interpreter);
  }
  GlobalSession =
      new FOEDAG::Session(mainWin, interpreter, commands, m_cmdLine);
  GlobalSession->setGuiType(GUI_TYPE::GT_WIDGET);

  registerBasicGuiCommands(GlobalSession);
  if (m_registerTclFunc) {
    m_registerTclFunc(GlobalSession->MainWindow(), GlobalSession);
  }

  QtTclNotify::QtTclNotifier::setup();  // Registers notifier with Tcl

  // Tell Tcl to run Qt as the main event loop once the interpreter is
  // initialized
  Tcl_SetMainLoop([]() { QApplication::exec(); });

  // --replay <script> Gui replay, register test
  if (!GlobalSession->CmdLine()->GuiTestScript().empty()) {
    interpreter->evalGuiTestFile(GlobalSession->CmdLine()->GuiTestScript());
  }

  // Tcl_AppInit
  auto tcl_init = [](Tcl_Interp* interp) -> int {
    // --script <script>
    if (!GlobalSession->CmdLine()->Script().empty()) {
      Tcl_EvalFile(interp, GlobalSession->CmdLine()->Script().c_str());
    }
    // --cmd \"tcl cmd\"
    if (!GlobalSession->CmdLine()->TclCmd().empty()) {
      Tcl_EvalEx(interp, GlobalSession->CmdLine()->TclCmd().c_str(), -1, 0);
    }
    // --replay <script> Gui replay, invoke test
    if (!GlobalSession->CmdLine()->GuiTestScript().empty()) {
      std::string proc = "call_test";
      Tcl_EvalEx(interp, proc.c_str(), -1, 0);
    } else {
      Tcl_EvalEx(interp, "gui_start", -1, 0);
    }
    return 0;
  };

  // exit tcl after last window is closed
  QObject::connect(qApp, &QApplication::lastWindowClosed, [interpreter]() {
    Tcl_EvalEx(interpreter->getInterp(), "exit", -1, 0);
  });

  // Start Loop
  Tcl_MainEx(argc, m_cmdLine->Argv(), tcl_init, interpreter->getInterp());

  delete GlobalSession;
  return 0;
}

bool Foedag::initQmlGui() {
  // Gui mode with QML
  int argc = m_cmdLine->Argc();
  QApplication app(argc, m_cmdLine->Argv());
  FOEDAG::TclInterpreter* interpreter =
      new FOEDAG::TclInterpreter(m_cmdLine->Argv()[0]);
  FOEDAG::CommandStack* commands = new FOEDAG::CommandStack(interpreter);

  MainWindowModel* windowModel = new MainWindowModel(interpreter);

  QQmlApplicationEngine engine;
  engine.addImportPath(QStringLiteral("qrc:/"));
  const QUrl url(QStringLiteral("qrc:/mainWindow.qml"));
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &app,
      [url](QObject* obj, const QUrl& objUrl) {
        if (!obj && url == objUrl) QCoreApplication::exit(-1);
      },
      Qt::QueuedConnection);

  engine.rootContext()->setContextProperty(QStringLiteral("windowModel"),
                                           windowModel);

  engine.load(url);

  GlobalSession =
      new FOEDAG::Session(nullptr, interpreter, commands, m_cmdLine);
  GlobalSession->setGuiType(GUI_TYPE::GT_QML);
  GlobalSession->setWindowModel(windowModel);

  registerBasicGuiCommands(GlobalSession);
  if (m_registerTclFunc) {
    m_registerTclFunc(GlobalSession->MainWindow(), GlobalSession);
  }

  QtTclNotify::QtTclNotifier::setup();  // Registers notifier with Tcl

  // Tell Tcl to run Qt as the main event loop once the interpreter is
  // initialized
  Tcl_SetMainLoop([]() { QApplication::exec(); });

  // --replay <script> Gui replay, register test
  if (!GlobalSession->CmdLine()->GuiTestScript().empty()) {
    interpreter->evalGuiTestFile(GlobalSession->CmdLine()->GuiTestScript());
  }

  // Tcl_AppInit
  auto tcl_init = [](Tcl_Interp* interp) -> int {
    // --script <script>
    if (!GlobalSession->CmdLine()->Script().empty()) {
      Tcl_EvalFile(interp, GlobalSession->CmdLine()->Script().c_str());
    }
    // --cmd \"tcl cmd\"
    if (!GlobalSession->CmdLine()->TclCmd().empty()) {
      Tcl_EvalEx(interp, GlobalSession->CmdLine()->TclCmd().c_str(), -1, 0);
    }
    // --replay <script> Gui replay, invoke test
    if (!GlobalSession->CmdLine()->GuiTestScript().empty()) {
      std::string proc = "call_test";
      Tcl_EvalEx(interp, proc.c_str(), -1, 0);
    }
    return 0;
  };

  // Start Loop
  Tcl_MainEx(argc, m_cmdLine->Argv(), tcl_init, interpreter->getInterp());

  delete GlobalSession;
  return 0;
}

bool Foedag::init(GUI_TYPE guiType) {
  bool result;
  switch (guiType) {
    case GUI_TYPE::GT_NONE:
      result = initBatch();
      break;
    case GUI_TYPE::GT_WIDGET:
      result = initGui();
      break;
    case GUI_TYPE::GT_QML:
      result = initQmlGui();
      break;
    default:
      break;
  }
  return result;
}

bool Foedag::initBatch() {
  // Batch mode
  FOEDAG::TclInterpreter* interpreter =
      new FOEDAG::TclInterpreter(m_cmdLine->Argv()[0]);
  FOEDAG::CommandStack* commands = new FOEDAG::CommandStack(interpreter);
  GlobalSession =
      new FOEDAG::Session(m_mainWin, interpreter, commands, m_cmdLine);
  GlobalSession->setGuiType(GUI_TYPE::GT_NONE);

  registerBasicBatchCommands(GlobalSession);
  if (m_registerTclFunc) {
    m_registerTclFunc(nullptr, GlobalSession);
  }
  std::string result = interpreter->evalCmd("puts \"Tcl only mode\"");
  // --script <script>
  if (!m_cmdLine->Script().empty())
    result = interpreter->evalFile(m_cmdLine->Script());
  if (result != "") {
    std::cout << result << '\n';
  }

  // Tcl_AppInit
  auto tcl_init = [](Tcl_Interp* interp) -> int {
    // --script <script>
    if (!GlobalSession->CmdLine()->Script().empty()) {
      Tcl_EvalFile(interp, GlobalSession->CmdLine()->Script().c_str());
    }
    // --cmd \"tcl cmd\"
    if (!GlobalSession->CmdLine()->TclCmd().empty()) {
      Tcl_EvalEx(interp, GlobalSession->CmdLine()->TclCmd().c_str(), -1, 0);
    }
    // --replay <script> Gui replay, invoke test
    if (!GlobalSession->CmdLine()->GuiTestScript().empty()) {
      std::string proc = "call_test";
      Tcl_EvalEx(interp, proc.c_str(), -1, 0);
    }
    return 0;
  };

  // Start Loop
  int argc = m_cmdLine->Argc();
  Tcl_MainEx(argc, m_cmdLine->Argv(), tcl_init, interpreter->getInterp());

  delete GlobalSession;
  return 0;
}
