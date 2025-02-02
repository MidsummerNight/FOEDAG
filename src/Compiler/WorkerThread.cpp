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

#include "Compiler/WorkerThread.h"

#include <mutex>
#include <thread>

using namespace FOEDAG;

std::set<WorkerThread*> ThreadPool::threads;

WorkerThread::WorkerThread(const std::string& threadName,
                           Compiler::Action action, Compiler* compiler)
    : m_threadName(threadName), m_action(action), m_compiler(compiler) {
  ThreadPool::threads.insert(this);
}

WorkerThread::~WorkerThread() {}

bool WorkerThread::start() {
  bool result = true;
  m_compiler->start();
  m_thread = new std::thread([=] {
    m_compiler->Compile(m_action);
    m_compiler->finish();
  });
  return result;
}

bool WorkerThread::stop() {
  m_compiler->Stop();
  m_thread->join();
  delete m_thread;
  m_thread = nullptr;
  return true;
}
