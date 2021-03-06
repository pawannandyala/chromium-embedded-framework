// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was generated by the CEF translator tool. If making changes by
// hand only do so within the body of existing method and function
// implementations. See the translator.README.txt file in the tools directory
// for more information.
//

#ifndef CEF_LIBCEF_DLL_CTOCPP_READ_HANDLER_CTOCPP_H_
#define CEF_LIBCEF_DLL_CTOCPP_READ_HANDLER_CTOCPP_H_
#pragma once

#ifndef BUILDING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed DLL-side only")
#else  // BUILDING_CEF_SHARED

#include "include/cef_stream.h"
#include "include/capi/cef_stream_capi.h"
#include "libcef_dll/ctocpp/ctocpp.h"

// Wrap a C structure with a C++ class.
// This class may be instantiated and accessed DLL-side only.
class CefReadHandlerCToCpp
    : public CefCToCpp<CefReadHandlerCToCpp, CefReadHandler,
        cef_read_handler_t> {
 public:
  explicit CefReadHandlerCToCpp(cef_read_handler_t* str)
      : CefCToCpp<CefReadHandlerCToCpp, CefReadHandler, cef_read_handler_t>(
          str) {}
  virtual ~CefReadHandlerCToCpp() {}

  // CefReadHandler methods
  virtual size_t Read(void* ptr, size_t size, size_t n) OVERRIDE;
  virtual int Seek(int64 offset, int whence) OVERRIDE;
  virtual int64 Tell() OVERRIDE;
  virtual int Eof() OVERRIDE;
  virtual bool MayBlock() OVERRIDE;
};

#endif  // BUILDING_CEF_SHARED
#endif  // CEF_LIBCEF_DLL_CTOCPP_READ_HANDLER_CTOCPP_H_

