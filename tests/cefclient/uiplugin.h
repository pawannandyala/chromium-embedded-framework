// Copyright (c) 2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this implementation are borrowed from webkit\default_plugin\
// plugin_impl.h

#pragma once

#include "cef_nplugin.h"

extern NPNetscapeFuncs* g_uibrowser;

NPError API_CALL NP_UIGetEntryPoints(NPPluginFuncs* pFuncs);
NPError API_CALL NP_UIInitialize(NPNetscapeFuncs* pFuncs);
NPError API_CALL NP_UIShutdown(void);

// Function called to modify the rotation value.
void ModifyRotation(float value);
// Function called to reset the rotation value.
void ResetRotation();