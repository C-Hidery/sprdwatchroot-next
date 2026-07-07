/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * sprdwatchroot-next Copyright (C) 2026 Ryan Crepa
 */
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
#include <thread>
#include <new>
#include <cstdlib>
#include <signal.h>
#include "core/file_io.h"
#include "third_party/nlohmann/json.hpp"
using json = nlohmann::json;