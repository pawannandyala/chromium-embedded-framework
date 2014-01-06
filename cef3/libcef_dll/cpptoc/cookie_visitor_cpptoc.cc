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

#include "libcef_dll/cpptoc/cookie_visitor_cpptoc.h"


// MEMBER FUNCTIONS - Body may be edited by hand.

int CEF_CALLBACK cookie_visitor_visit(struct _cef_cookie_visitor_t* self,
    const struct _cef_cookie_t* cookie, int count, int total,
    int* deleteCookie) {
  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  DCHECK(self);
  if (!self)
    return 0;
  // Verify param: cookie; type: struct_byref_const
  DCHECK(cookie);
  if (!cookie)
    return 0;
  // Verify param: deleteCookie; type: bool_byref
  DCHECK(deleteCookie);
  if (!deleteCookie)
    return 0;

  // Translate param: cookie; type: struct_byref_const
  CefCookie cookieObj;
  if (cookie)
    cookieObj.Set(*cookie, false);
  // Translate param: deleteCookie; type: bool_byref
  bool deleteCookieBool = (deleteCookie && *deleteCookie)?true:false;

  // Execute
  bool _retval = CefCookieVisitorCppToC::Get(self)->Visit(
      cookieObj,
      count,
      total,
      deleteCookieBool);

  // Restore param: deleteCookie; type: bool_byref
  if (deleteCookie)
    *deleteCookie = deleteCookieBool?true:false;

  // Return type: bool
  return _retval;
}


// CONSTRUCTOR - Do not edit by hand.

CefCookieVisitorCppToC::CefCookieVisitorCppToC(CefCookieVisitor* cls)
    : CefCppToC<CefCookieVisitorCppToC, CefCookieVisitor, cef_cookie_visitor_t>(
        cls) {
  struct_.struct_.visit = cookie_visitor_visit;
}

#ifndef NDEBUG
template<> long CefCppToC<CefCookieVisitorCppToC, CefCookieVisitor,
    cef_cookie_visitor_t>::DebugObjCt = 0;
#endif

