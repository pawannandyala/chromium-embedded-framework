// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// A portion of this file was generated by the CEF translator tool.  When
// making changes by hand only do so within the body of existing function
// implementations. See the translator.README.txt file in the tools directory
// for more information.
//

#include "libcef_dll/cpptoc/domevent_listener_cpptoc.h"
#include "libcef_dll/ctocpp/domevent_ctocpp.h"


// MEMBER FUNCTIONS - Body may be edited by hand.

void CEF_CALLBACK domevent_listener_handle_event(
    struct _cef_domevent_listener_t* self, cef_domevent_t* event)
{
  DCHECK(self);
  if(!self)
    return;

  CefRefPtr<CefDOMEvent> eventPtr;
  if (event)
    eventPtr = CefDOMEventCToCpp::Wrap(event);

  CefDOMEventListenerCppToC::Get(self)->HandleEvent(eventPtr);
}


// CONSTRUCTOR - Do not edit by hand.

CefDOMEventListenerCppToC::CefDOMEventListenerCppToC(CefDOMEventListener* cls)
    : CefCppToC<CefDOMEventListenerCppToC, CefDOMEventListener,
        cef_domevent_listener_t>(cls)
{
  struct_.struct_.handle_event = domevent_listener_handle_event;
}

#ifdef _DEBUG
template<> long CefCppToC<CefDOMEventListenerCppToC, CefDOMEventListener,
    cef_domevent_listener_t>::DebugObjCt = 0;
#endif
