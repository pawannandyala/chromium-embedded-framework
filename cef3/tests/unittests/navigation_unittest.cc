// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <list>

// Include this first to avoid type conflicts with CEF headers.
#include "tests/unittests/chromium_includes.h"

#include "include/base/cef_bind.h"
#include "include/cef_callback.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_closure_task.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tests/cefclient/client_app.h"
#include "tests/unittests/test_handler.h"
#include "tests/unittests/test_util.h"

namespace {

const char kHNav1[] = "http://tests-hnav/nav1.html";
const char kHNav2[] = "http://tests-hnav/nav2.html";
const char kHNav3[] = "http://tests-hnav/nav3.html";
const char kHistoryNavMsg[] = "NavigationTest.HistoryNav";

enum NavAction {
  NA_LOAD = 1,
  NA_BACK,
  NA_FORWARD,
  NA_CLEAR
};

typedef struct {
  NavAction action;     // What to do
  const char* target;   // Where to be after navigation
  bool can_go_back;     // After navigation, can go back?
  bool can_go_forward;  // After navigation, can go forward?
} NavListItem;

// Array of navigation actions: X = current page, . = history exists
static NavListItem kHNavList[] = {
                                      // kHNav1 | kHNav2 | kHNav3
  {NA_LOAD, kHNav1, false, false},    //   X
  {NA_LOAD, kHNav2, true, false},     //   .        X
  {NA_BACK, kHNav1, false, true},     //   X        .
  {NA_FORWARD, kHNav2, true, false},  //   .        X
  {NA_LOAD, kHNav3, true, false},     //   .        .        X
  {NA_BACK, kHNav2, true, true},      //   .        X        .
  // TODO(cef): Enable once ClearHistory is implemented
  // {NA_CLEAR, kHNav2, false, false},   //            X
};

#define NAV_LIST_SIZE() (sizeof(kHNavList) / sizeof(NavListItem))

bool g_history_nav_test = false;

// Browser side.
class HistoryNavBrowserTest : public ClientApp::BrowserDelegate {
 public:
  HistoryNavBrowserTest() {}

  virtual void OnBeforeChildProcessLaunch(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefCommandLine> command_line) OVERRIDE {
    if (!g_history_nav_test)
      return;

    // Indicate to the render process that the test should be run.
    command_line->AppendSwitchWithValue("test", kHistoryNavMsg);
  }

 protected:
  IMPLEMENT_REFCOUNTING(HistoryNavBrowserTest);
};

// Renderer side.
class HistoryNavRendererTest : public ClientApp::RenderDelegate,
                               public CefLoadHandler {
 public:
  HistoryNavRendererTest()
      : run_test_(false),
        nav_(0) {}

  virtual void OnRenderThreadCreated(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefListValue> extra_info) OVERRIDE {
    if (!g_history_nav_test) {
      // Check that the test should be run.
      CefRefPtr<CefCommandLine> command_line =
          CefCommandLine::GetGlobalCommandLine();
      const std::string& test = command_line->GetSwitchValue("test");
      if (test != kHistoryNavMsg)
        return;
    }

    run_test_ = true;
  }

  virtual CefRefPtr<CefLoadHandler> GetLoadHandler(
    CefRefPtr<ClientApp> app) OVERRIDE {
    if (!run_test_)
      return NULL;

    return this;
  }

  virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                    bool isLoading,
                                    bool canGoBack,
                                    bool canGoForward) OVERRIDE {
    const NavListItem& item = kHNavList[nav_];

    const std::string& url = browser->GetMainFrame()->GetURL();
    if (isLoading) {
      got_loading_state_start_.yes();

      EXPECT_STRNE(item.target, url.c_str());

      if (nav_ > 0) {
        const NavListItem& last_item = kHNavList[nav_ - 1];
        EXPECT_EQ(last_item.can_go_back, browser->CanGoBack());
        EXPECT_EQ(last_item.can_go_back, canGoBack);
        EXPECT_EQ(last_item.can_go_forward, browser->CanGoForward());
        EXPECT_EQ(last_item.can_go_forward, canGoForward);
      } else {
        EXPECT_FALSE(browser->CanGoBack());
        EXPECT_FALSE(canGoBack);
        EXPECT_FALSE(browser->CanGoForward());
        EXPECT_FALSE(canGoForward);
      }
    } else {
      got_loading_state_end_.yes();

      EXPECT_STREQ(item.target, url.c_str());

      EXPECT_EQ(item.can_go_back, browser->CanGoBack());
      EXPECT_EQ(item.can_go_back, canGoBack);
      EXPECT_EQ(item.can_go_forward, browser->CanGoForward());
      EXPECT_EQ(item.can_go_forward, canGoForward);

      SendTestResultsIfDone(browser);
    }
  }

  virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) OVERRIDE {
    const NavListItem& item = kHNavList[nav_];

    got_load_start_.yes();

    const std::string& url = frame->GetURL();
    EXPECT_STREQ(item.target, url.c_str());

    EXPECT_EQ(item.can_go_back, browser->CanGoBack());
    EXPECT_EQ(item.can_go_forward, browser->CanGoForward());
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    const NavListItem& item = kHNavList[nav_];

    got_load_end_.yes();

    const std::string& url = frame->GetURL();
    EXPECT_STREQ(item.target, url.c_str());

    EXPECT_EQ(item.can_go_back, browser->CanGoBack());
    EXPECT_EQ(item.can_go_forward, browser->CanGoForward());

    SendTestResultsIfDone(browser);
  }

  virtual bool OnBeforeNavigation(CefRefPtr<ClientApp> app,
                                  CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefRequest> request,
                                  cef_navigation_type_t navigation_type,
                                  bool is_redirect) OVERRIDE {
    if (!run_test_)
      return false;

    const NavListItem& item = kHNavList[nav_];

    std::string url = request->GetURL();
    EXPECT_STREQ(item.target, url.c_str());

    EXPECT_EQ(RT_SUB_RESOURCE, request->GetResourceType());
    EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());

    if (item.action == NA_LOAD)
      EXPECT_EQ(NAVIGATION_OTHER, navigation_type);
    else if (item.action == NA_BACK || item.action == NA_FORWARD)
      EXPECT_EQ(NAVIGATION_BACK_FORWARD, navigation_type);

    if (nav_ > 0) {
      const NavListItem& last_item = kHNavList[nav_ - 1];
      EXPECT_EQ(last_item.can_go_back, browser->CanGoBack());
      EXPECT_EQ(last_item.can_go_forward, browser->CanGoForward());
    } else {
      EXPECT_FALSE(browser->CanGoBack());
      EXPECT_FALSE(browser->CanGoForward());
    }

    return false;
  }

 protected:
  void SendTestResultsIfDone(CefRefPtr<CefBrowser> browser) {
    if (got_load_end_ && got_loading_state_end_)
      SendTestResults(browser);
  }

  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser) {
    EXPECT_TRUE(got_loading_state_start_);
    EXPECT_TRUE(got_loading_state_end_);
    EXPECT_TRUE(got_load_start_);
    EXPECT_TRUE(got_load_end_);

    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kHistoryNavMsg);
    CefRefPtr<CefListValue> args = return_msg->GetArgumentList();
    EXPECT_TRUE(args.get());
    EXPECT_TRUE(args->SetInt(0, nav_));
    EXPECT_TRUE(args->SetBool(1, result));
    EXPECT_TRUE(browser->SendProcessMessage(PID_BROWSER, return_msg));

    // Reset the test results for the next navigation.
    got_loading_state_start_.reset();
    got_loading_state_end_.reset();
    got_load_start_.reset();
    got_load_end_.reset();

    nav_++;
  }

  bool run_test_;
  int nav_;

  TrackCallback got_loading_state_start_;
  TrackCallback got_loading_state_end_;
  TrackCallback got_load_start_;
  TrackCallback got_load_end_;

  IMPLEMENT_REFCOUNTING(HistoryNavRendererTest);
};

// Browser side.
class HistoryNavTestHandler : public TestHandler {
 public:
  HistoryNavTestHandler()
      : nav_(0),
        load_end_confirmation_(false),
        renderer_confirmation_(false) {}

  virtual void RunTest() OVERRIDE {
    // Add the resources that we will navigate to/from.
    AddResource(kHNav1, "<html>Nav1</html>", "text/html");
    AddResource(kHNav2, "<html>Nav2</html>", "text/html");
    AddResource(kHNav3, "<html>Nav3</html>", "text/html");

    // Create the browser.
    CreateBrowser(CefString());
  }

  void RunNav(CefRefPtr<CefBrowser> browser) {
    if (nav_ == NAV_LIST_SIZE()) {
      // End of the nav list.
      DestroyTest();
      return;
    }

    const NavListItem& item = kHNavList[nav_];

    // Perform the action.
    switch (item.action) {
    case NA_LOAD:
      browser->GetMainFrame()->LoadURL(item.target);
      break;
    case NA_BACK:
      browser->GoBack();
      break;
    case NA_FORWARD:
      browser->GoForward();
      break;
    case NA_CLEAR:
      // TODO(cef): Enable once ClearHistory is implemented
      // browser->GetHost()->ClearHistory();
      // Not really a navigation action so go to the next one.
      nav_++;
      RunNav(browser);
      break;
    default:
      break;
    }
  }

  void RunNextNavIfReady(CefRefPtr<CefBrowser> browser) {
    if (load_end_confirmation_ && renderer_confirmation_) {
      load_end_confirmation_ = false;
      renderer_confirmation_ = false;
      nav_++;
      RunNav(browser);
    }
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE {
    TestHandler::OnAfterCreated(browser);

    RunNav(browser);
  }

  virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              bool is_redirect) OVERRIDE {
    const NavListItem& item = kHNavList[nav_];

    got_before_browse_[nav_].yes();

    std::string url = request->GetURL();
    EXPECT_STREQ(item.target, url.c_str());

    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    if (item.action == NA_LOAD)
      EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());
    else if (item.action == NA_BACK || item.action == NA_FORWARD)
      EXPECT_EQ(TT_EXPLICIT | TT_FORWARD_BACK_FLAG, request->GetTransitionType());

    if (nav_ > 0) {
      const NavListItem& last_item = kHNavList[nav_ - 1];
      EXPECT_EQ(last_item.can_go_back, browser->CanGoBack());
      EXPECT_EQ(last_item.can_go_forward, browser->CanGoForward());
    } else {
      EXPECT_FALSE(browser->CanGoBack());
      EXPECT_FALSE(browser->CanGoForward());
    }

    return false;
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request) OVERRIDE {
    const NavListItem& item = kHNavList[nav_];

    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    if (item.action == NA_LOAD)
      EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());
    else if (item.action == NA_BACK || item.action == NA_FORWARD)
      EXPECT_EQ(TT_EXPLICIT | TT_FORWARD_BACK_FLAG, request->GetTransitionType());

    got_before_resource_load_[nav_].yes();

    std::string url = request->GetURL();
    if (url == item.target)
      got_correct_target_[nav_].yes();

    return false;
  }

  virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                    bool isLoading,
                                    bool canGoBack,
                                    bool canGoForward) OVERRIDE {
    const NavListItem& item = kHNavList[nav_];

    got_loading_state_change_[nav_].yes();

    if (item.can_go_back == canGoBack)
      got_correct_can_go_back_[nav_].yes();
    if (item.can_go_forward == canGoForward)
      got_correct_can_go_forward_[nav_].yes();
  }

  virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) OVERRIDE {
    if(browser->IsPopup() || !frame->IsMain())
      return;

    const NavListItem& item = kHNavList[nav_];

    got_load_start_[nav_].yes();

    std::string url1 = browser->GetMainFrame()->GetURL();
    std::string url2 = frame->GetURL();
    if (url1 == item.target && url2 == item.target)
      got_correct_load_start_url_[nav_].yes();
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    if (browser->IsPopup() || !frame->IsMain())
      return;

    const NavListItem& item = kHNavList[nav_];

    got_load_end_[nav_].yes();

    std::string url1 = browser->GetMainFrame()->GetURL();
    std::string url2 = frame->GetURL();
    if (url1 == item.target && url2 == item.target)
      got_correct_load_end_url_[nav_].yes();

    if (item.can_go_back == browser->CanGoBack())
      got_correct_can_go_back2_[nav_].yes();
    if (item.can_go_forward == browser->CanGoForward())
      got_correct_can_go_forward2_[nav_].yes();

    load_end_confirmation_ = true;
    RunNextNavIfReady(browser);
  }

  virtual bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE {
    if (message->GetName().ToString() == kHistoryNavMsg) {
      got_before_navigation_[nav_].yes();

      // Test that the renderer side succeeded.
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());
      EXPECT_EQ(nav_, args->GetInt(0));
      EXPECT_TRUE(args->GetBool(1));

      renderer_confirmation_ = true;
      RunNextNavIfReady(browser);
      return true;
    }

    // Message not handled.
    return false;
  }

  int nav_;
  bool load_end_confirmation_;
  bool renderer_confirmation_;

  TrackCallback got_before_browse_[NAV_LIST_SIZE()];
  TrackCallback got_before_navigation_[NAV_LIST_SIZE()];
  TrackCallback got_before_resource_load_[NAV_LIST_SIZE()];
  TrackCallback got_correct_target_[NAV_LIST_SIZE()];
  TrackCallback got_loading_state_change_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_back_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_forward_[NAV_LIST_SIZE()];
  TrackCallback got_load_start_[NAV_LIST_SIZE()];
  TrackCallback got_correct_load_start_url_[NAV_LIST_SIZE()];
  TrackCallback got_load_end_[NAV_LIST_SIZE()];
  TrackCallback got_correct_load_end_url_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_back2_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_forward2_[NAV_LIST_SIZE()];
};

}  // namespace

// Verify history navigation.
TEST(NavigationTest, History) {
  g_history_nav_test = true;
  CefRefPtr<HistoryNavTestHandler> handler =
      new HistoryNavTestHandler();
  handler->ExecuteTest();
  g_history_nav_test = false;

  for (size_t i = 0; i < NAV_LIST_SIZE(); ++i) {
    if (kHNavList[i].action != NA_CLEAR) {
      ASSERT_TRUE(handler->got_before_browse_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_before_navigation_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_before_resource_load_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_target_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_load_start_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_load_start_url_[i]) << "i = " << i;
    }

    ASSERT_TRUE(handler->got_loading_state_change_[i]) << "i = " << i;
    ASSERT_TRUE(handler->got_correct_can_go_back_[i]) << "i = " << i;
    ASSERT_TRUE(handler->got_correct_can_go_forward_[i]) << "i = " << i;

    if (kHNavList[i].action != NA_CLEAR) {
      ASSERT_TRUE(handler->got_load_end_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_load_end_url_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_can_go_back2_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_can_go_forward2_[i]) << "i = " << i;
    }
  }
}


namespace {

const char kRNav1[] = "http://tests/nav1.html";
const char kRNav2[] = "http://tests/nav2.html";
const char kRNav3[] = "http://tests/nav3.html";
const char kRNav4[] = "http://tests/nav4.html";

bool g_got_nav1_request = false;
bool g_got_nav3_request = false;
bool g_got_nav4_request = false;
bool g_got_invalid_request = false;

class RedirectSchemeHandler : public CefResourceHandler {
 public:
  RedirectSchemeHandler() : offset_(0), status_(0) {}

  virtual bool ProcessRequest(CefRefPtr<CefRequest> request,
                              CefRefPtr<CefCallback> callback) OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    std::string url = request->GetURL();
    if (url == kRNav1) {
      // Redirect using HTTP 302
      g_got_nav1_request = true;
      status_ = 302;
      location_ = kRNav2;
      content_ = "<html><body>Redirected Nav1</body></html>";
    } else if (url == kRNav3) {
      // Redirect using redirectUrl
      g_got_nav3_request = true;
      status_ = -1;
      location_ = kRNav4;
      content_ = "<html><body>Redirected Nav3</body></html>";
    } else if (url == kRNav4) {
      g_got_nav4_request = true;
      status_ = 200;
      content_ = "<html><body>Nav4</body></html>";
    }

    if (status_ != 0) {
      callback->Continue();
      return true;
    } else {
      g_got_invalid_request = true;
      return false;
    }
  }

  virtual void GetResponseHeaders(CefRefPtr<CefResponse> response,
                                  int64& response_length,
                                  CefString& redirectUrl) OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    EXPECT_NE(status_, 0);

    response->SetStatus(status_);
    response->SetMimeType("text/html");
    response_length = content_.size();

    if (status_ == 302) {
      // Redirect using HTTP 302
      EXPECT_GT(location_.size(), static_cast<size_t>(0));
      response->SetStatusText("Found");
      CefResponse::HeaderMap headers;
      response->GetHeaderMap(headers);
      headers.insert(std::make_pair("Location", location_));
      response->SetHeaderMap(headers);
    } else if (status_ == -1) {
      // Rdirect using redirectUrl
      EXPECT_GT(location_.size(), static_cast<size_t>(0));
      redirectUrl = location_;
    }
  }

  virtual void Cancel() OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
  }

  virtual bool ReadResponse(void* data_out,
                            int bytes_to_read,
                            int& bytes_read,
                            CefRefPtr<CefCallback> callback) OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    size_t size = content_.size();
    if (offset_ < size) {
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size - offset_));
      memcpy(data_out, content_.c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      return true;
    }

    return false;
  }

 protected:
  std::string content_;
  size_t offset_;
  int status_;
  std::string location_;

  IMPLEMENT_REFCOUNTING(RedirectSchemeHandler);
};

class RedirectSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  RedirectSchemeHandlerFactory() {}

  virtual CefRefPtr<CefResourceHandler> Create(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& scheme_name,
      CefRefPtr<CefRequest> request) OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    return new RedirectSchemeHandler();
  }

  IMPLEMENT_REFCOUNTING(RedirectSchemeHandlerFactory);
};

class RedirectTestHandler : public TestHandler {
 public:
  RedirectTestHandler() {}

  virtual void RunTest() OVERRIDE {
    // Create the browser.
    CreateBrowser(kRNav1);
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request) OVERRIDE {
    // Should be called for all but the second URL.
    std::string url = request->GetURL();

    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());

    if (url == kRNav1) {
      got_nav1_before_resource_load_.yes();
    } else if (url == kRNav3) {
      got_nav3_before_resource_load_.yes();
    } else if (url == kRNav4) {
      got_nav4_before_resource_load_.yes();
    } else {
      got_invalid_before_resource_load_.yes();
    }

    return false;
  }

  virtual void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  const CefString& old_url,
                                  CefString& new_url) OVERRIDE {
    // Should be called for each redirected URL.

    if (old_url == kRNav1 && new_url == kRNav2) {
      // Called due to the nav1 redirect response.
      got_nav1_redirect_.yes();

      // Change the redirect to the 3rd URL.
      new_url = kRNav3;
    } else if (old_url == kRNav1 && new_url == kRNav3) {
      // Called due to the redirect change above.
      got_nav2_redirect_.yes();
    } else if (old_url == kRNav3 && new_url == kRNav4) {
      // Called due to the nav3 redirect response.
      got_nav3_redirect_.yes();
    } else {
      got_invalid_redirect_.yes();
    }
  }

  virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) OVERRIDE {
    // Should only be called for the final loaded URL.
    std::string url = frame->GetURL();

    if (url == kRNav4) {
      got_nav4_load_start_.yes();
    } else {
      got_invalid_load_start_.yes();
    }
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    // Should only be called for the final loaded URL.
    std::string url = frame->GetURL();

    if (url == kRNav4) {
      got_nav4_load_end_.yes();
      DestroyTest();
    } else {
      got_invalid_load_end_.yes();
    }
  }

  TrackCallback got_nav1_before_resource_load_;
  TrackCallback got_nav3_before_resource_load_;
  TrackCallback got_nav4_before_resource_load_;
  TrackCallback got_invalid_before_resource_load_;
  TrackCallback got_nav4_load_start_;
  TrackCallback got_invalid_load_start_;
  TrackCallback got_nav4_load_end_;
  TrackCallback got_invalid_load_end_;
  TrackCallback got_nav1_redirect_;
  TrackCallback got_nav2_redirect_;
  TrackCallback got_nav3_redirect_;
  TrackCallback got_invalid_redirect_;
};

}  // namespace

// Verify frame names and identifiers.
TEST(NavigationTest, Redirect) {
  CefRegisterSchemeHandlerFactory("http", "tests",
      new RedirectSchemeHandlerFactory());
  WaitForIOThread();

  CefRefPtr<RedirectTestHandler> handler =
      new RedirectTestHandler();
  handler->ExecuteTest();

  CefClearSchemeHandlerFactories();
  WaitForIOThread();

  ASSERT_TRUE(handler->got_nav1_before_resource_load_);
  ASSERT_TRUE(handler->got_nav3_before_resource_load_);
  ASSERT_TRUE(handler->got_nav4_before_resource_load_);
  ASSERT_FALSE(handler->got_invalid_before_resource_load_);
  ASSERT_TRUE(handler->got_nav4_load_start_);
  ASSERT_FALSE(handler->got_invalid_load_start_);
  ASSERT_TRUE(handler->got_nav4_load_end_);
  ASSERT_FALSE(handler->got_invalid_load_end_);
  ASSERT_TRUE(handler->got_nav1_redirect_);
  ASSERT_TRUE(handler->got_nav2_redirect_);
  ASSERT_TRUE(handler->got_nav3_redirect_);
  ASSERT_FALSE(handler->got_invalid_redirect_);
  ASSERT_TRUE(g_got_nav1_request);
  ASSERT_TRUE(g_got_nav3_request);
  ASSERT_TRUE(g_got_nav4_request);
  ASSERT_FALSE(g_got_invalid_request);
}


namespace {

const char KONav1[] = "http://tests-onav/nav1.html";
const char KONav2[] = "http://tests-onav/nav2.html";
const char kOrderNavMsg[] = "NavigationTest.OrderNav";
const char kOrderNavClosedMsg[] = "NavigationTest.OrderNavClosed";

void SetOrderNavExtraInfo(CefRefPtr<CefListValue> extra_info) {
  // Arbitrary data for testing.
  extra_info->SetBool(0, true);
  CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
  dict->SetInt("key1", 5);
  dict->SetString("key2", "test string");
  extra_info->SetDictionary(1, dict);
  extra_info->SetDouble(2, 5.43322);
  extra_info->SetString(3, "some string");
}

bool g_order_nav_test = false;

// Browser side.
class OrderNavBrowserTest : public ClientApp::BrowserDelegate {
 public:
  OrderNavBrowserTest() {}

  virtual void OnBeforeChildProcessLaunch(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefCommandLine> command_line) OVERRIDE {
    if (!g_order_nav_test)
      return;

    // Indicate to the render process that the test should be run.
    command_line->AppendSwitchWithValue("test", kOrderNavMsg);
  }

  virtual void OnRenderProcessThreadCreated(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefListValue> extra_info) OVERRIDE {
    if (!g_order_nav_test)
      return;

    // Some data that we'll check for.
    SetOrderNavExtraInfo(extra_info);
  }

 protected:
  IMPLEMENT_REFCOUNTING(OrderNavBrowserTest);
};

class OrderNavLoadState {
 public:
  OrderNavLoadState(bool is_popup, bool browser_side)
      : is_popup_(is_popup),
        browser_side_(browser_side) {}

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) {
    if (isLoading) {
      EXPECT_TRUE(Verify(false, false, false, false));

      got_loading_state_start_.yes();
    } else {
      EXPECT_TRUE(Verify(true, false, true, false));

      got_loading_state_end_.yes();
    }
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) {
    EXPECT_TRUE(Verify(true, false, false, false));

    got_load_start_.yes();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) {
    EXPECT_TRUE(Verify(true, true, true, false));

    got_load_end_.yes();
  }

  bool IsStarted() {
    return got_loading_state_start_ ||
           got_loading_state_end_ ||
           got_load_start_ ||
           got_load_end_;
  }

  bool IsDone() {
    return got_loading_state_start_ &&
           got_loading_state_end_ &&
           got_load_start_ &&
           got_load_end_;
  }

 private:
  bool Verify(bool got_loading_state_start,
              bool got_loading_state_end,
              bool got_load_start,
              bool got_load_end) {
    EXPECT_EQ(got_loading_state_start, got_loading_state_start_)
      << "Popup: " << is_popup_
      << "; Browser Side: " << browser_side_;
    EXPECT_EQ(got_loading_state_end, got_loading_state_end_)
      << "Popup: " << is_popup_
      << "; Browser Side: " << browser_side_;
    EXPECT_EQ(got_load_start, got_load_start_)
      << "Popup: " << is_popup_
      << "; Browser Side: " << browser_side_;
    EXPECT_EQ(got_load_end, got_load_end_)
      << "Popup: " << is_popup_
      << "; Browser Side: " << browser_side_;

    return got_loading_state_start == got_loading_state_start_ &&
           got_loading_state_end == got_loading_state_end_ &&
           got_load_start == got_load_start_ &&
           got_load_end == got_load_end_;
  }

  bool is_popup_;
  bool browser_side_;

  TrackCallback got_loading_state_start_;
  TrackCallback got_loading_state_end_;
  TrackCallback got_load_start_;
  TrackCallback got_load_end_;
};

// Renderer side.
class OrderNavRendererTest : public ClientApp::RenderDelegate,
                             public CefLoadHandler {
 public:
  OrderNavRendererTest()
      : run_test_(false),
        browser_id_main_(0),
        browser_id_popup_(0),
        state_main_(false, false), 
        state_popup_(true, false) {}

  virtual void OnRenderThreadCreated(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefListValue> extra_info) OVERRIDE {
    if (!g_order_nav_test) {
      // Check that the test should be run.
      CefRefPtr<CefCommandLine> command_line =
          CefCommandLine::GetGlobalCommandLine();
      const std::string& test = command_line->GetSwitchValue("test");
      if (test != kOrderNavMsg)
        return;
    }

    run_test_ = true;

    EXPECT_FALSE(got_webkit_initialized_);

    got_render_thread_created_.yes();

    // Verify that |extra_info| transferred successfully.
    CefRefPtr<CefListValue> expected = CefListValue::Create();
    SetOrderNavExtraInfo(expected);
    TestListEqual(expected, extra_info);
  }

  virtual void OnWebKitInitialized(CefRefPtr<ClientApp> app) OVERRIDE {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);

    got_webkit_initialized_.yes();
  }

  virtual void OnBrowserCreated(CefRefPtr<ClientApp> app,
                                CefRefPtr<CefBrowser> browser) OVERRIDE {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    if (browser->IsPopup()) {
      EXPECT_FALSE(got_browser_created_popup_);
      EXPECT_FALSE(got_browser_destroyed_popup_);
      EXPECT_FALSE(state_popup_.IsStarted());

      got_browser_created_popup_.yes();
      browser_id_popup_ = browser->GetIdentifier();
      EXPECT_GT(browser->GetIdentifier(), 0);
    } else {
      EXPECT_FALSE(got_browser_created_main_);
      EXPECT_FALSE(got_browser_destroyed_main_);
      EXPECT_FALSE(state_main_.IsStarted());

      got_browser_created_main_.yes();
      browser_id_main_ = browser->GetIdentifier();
      EXPECT_GT(browser->GetIdentifier(), 0);

      browser_main_ = browser;
    }
  }

  virtual void OnBrowserDestroyed(CefRefPtr<ClientApp> app,
                                  CefRefPtr<CefBrowser> browser) OVERRIDE {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    if (browser->IsPopup()) {
      EXPECT_TRUE(got_browser_created_popup_);
      EXPECT_FALSE(got_browser_destroyed_popup_);
      EXPECT_TRUE(state_popup_.IsDone());

      got_browser_destroyed_popup_.yes();
      EXPECT_EQ(browser_id_popup_, browser->GetIdentifier());
      EXPECT_GT(browser->GetIdentifier(), 0);

      // Use |browser_main_| to send the message otherwise it will fail.
      SendTestResults(browser_main_, kOrderNavClosedMsg);
    } else {
      EXPECT_TRUE(got_browser_created_main_);
      EXPECT_FALSE(got_browser_destroyed_main_);
      EXPECT_TRUE(state_main_.IsDone());

      got_browser_destroyed_main_.yes();
      EXPECT_EQ(browser_id_main_, browser->GetIdentifier());
      EXPECT_GT(browser->GetIdentifier(), 0);

      browser_main_ = NULL;
    }
  }

  virtual CefRefPtr<CefLoadHandler> GetLoadHandler(
    CefRefPtr<ClientApp> app) OVERRIDE {
    if (!run_test_)
      return NULL;

    return this;
  }

  virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                    bool isLoading,
                                    bool canGoBack,
                                    bool canGoForward) OVERRIDE {
    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    if (browser->IsPopup()) {
      EXPECT_TRUE(got_browser_created_popup_);
      EXPECT_FALSE(got_browser_destroyed_popup_);

      state_popup_.OnLoadingStateChange(browser, isLoading, canGoBack,
                                        canGoForward);
    } else {
      EXPECT_TRUE(got_browser_created_main_);
      EXPECT_FALSE(got_browser_destroyed_main_);

      state_main_.OnLoadingStateChange(browser, isLoading, canGoBack,
                                       canGoForward);
    }

    if (!isLoading)
      SendTestResultsIfDone(browser);
  }

  virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) OVERRIDE {
    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    if (browser->IsPopup()) {
      EXPECT_TRUE(got_browser_created_popup_);
      EXPECT_FALSE(got_browser_destroyed_popup_);

      state_popup_.OnLoadStart(browser, frame);
    } else {
      EXPECT_TRUE(got_browser_created_main_);
      EXPECT_FALSE(got_browser_destroyed_main_);

      state_main_.OnLoadStart(browser, frame);
    }
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    if (browser->IsPopup()) {
      EXPECT_TRUE(got_browser_created_popup_);
      EXPECT_FALSE(got_browser_destroyed_popup_);

      state_popup_.OnLoadEnd(browser, frame, httpStatusCode);
    } else {
      EXPECT_TRUE(got_browser_created_main_);
      EXPECT_FALSE(got_browser_destroyed_main_);

      state_main_.OnLoadEnd(browser, frame, httpStatusCode);
    }

    SendTestResultsIfDone(browser);
  }

 protected:
  void SendTestResultsIfDone(CefRefPtr<CefBrowser> browser) {
    bool done = false;
    if (browser->IsPopup())
      done = state_popup_.IsDone();
    else
      done = state_main_.IsDone();

    if (done)
      SendTestResults(browser, kOrderNavMsg);
  }

  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser, const char* msg_name) {
    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(msg_name);
    CefRefPtr<CefListValue> args = return_msg->GetArgumentList();
    EXPECT_TRUE(args.get());
    EXPECT_TRUE(args->SetBool(0, result));
    if (browser->IsPopup())
      EXPECT_TRUE(args->SetInt(1, browser_id_popup_));
    else
      EXPECT_TRUE(args->SetInt(1, browser_id_main_));
    EXPECT_TRUE(browser->SendProcessMessage(PID_BROWSER, return_msg));
  }

  bool run_test_;

  int browser_id_main_;
  int browser_id_popup_;
  CefRefPtr<CefBrowser> browser_main_;
  TrackCallback got_render_thread_created_;
  TrackCallback got_webkit_initialized_;
  TrackCallback got_browser_created_main_;
  TrackCallback got_browser_destroyed_main_;
  TrackCallback got_browser_created_popup_;
  TrackCallback got_browser_destroyed_popup_;

  OrderNavLoadState state_main_;
  OrderNavLoadState state_popup_;

  IMPLEMENT_REFCOUNTING(OrderNavRendererTest);
};

// Browser side.
class OrderNavTestHandler : public TestHandler {
 public:
  OrderNavTestHandler()
      : browser_id_main_(0),
        browser_id_popup_(0),
        state_main_(false, true),
        state_popup_(true, true),
        got_message_(false) {}

  virtual void RunTest() OVERRIDE {
    // Add the resources that we will navigate to/from.
    AddResource(KONav1, "<html>Nav1</html>", "text/html");
    AddResource(KONav2, "<html>Nav2</html>", "text/html");

    // Create the browser.
    CreateBrowser(KONav1);
  }

  void ContinueIfReady(CefRefPtr<CefBrowser> browser) {
    if (!got_message_)
      return;

    bool done = false;
    if (browser->IsPopup())
      done = state_popup_.IsDone();
    else
      done = state_main_.IsDone();
    if (!done)
      return;

    got_message_ = false;

    if (!browser->IsPopup()) {
      // Create the popup window.
      browser->GetMainFrame()->ExecuteJavaScript(
          "window.open('" + std::string(KONav2) + "');", CefString(), 0);
    } else {
      // Close the popup window.
      browser_popup_->GetHost()->CloseBrowser(false);
    }
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE {
    TestHandler::OnAfterCreated(browser);

    if (browser->IsPopup()) {
      browser_id_popup_ = browser->GetIdentifier();
      EXPECT_GT(browser_id_popup_, 0);
      browser_popup_ = browser;
    } else {
      browser_id_main_ = browser->GetIdentifier();
      EXPECT_GT(browser_id_main_, 0);
    }
  }

  virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              bool is_redirect) OVERRIDE {
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());

    if (browser->IsPopup()) {
      EXPECT_EQ(TT_LINK, request->GetTransitionType());
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_popup_, browser->GetIdentifier());
      got_before_browse_popup_.yes();
    } else {
      EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_main_, browser->GetIdentifier());
      got_before_browse_main_.yes();
    }

    std::string url = request->GetURL();
    if (url == KONav1)
      EXPECT_FALSE(browser->IsPopup());
    else if (url == KONav2)
      EXPECT_TRUE(browser->IsPopup());
    else
      EXPECT_TRUE(false);  // not reached

    return false;
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request) OVERRIDE {
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());

    if (browser->IsPopup()) {
      EXPECT_EQ(TT_LINK, request->GetTransitionType());
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_popup_, browser->GetIdentifier());
    } else {
      EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_main_, browser->GetIdentifier());
    }
    
    return false;
  }

  virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                    bool isLoading,
                                    bool canGoBack,
                                    bool canGoForward) OVERRIDE {
    if (browser->IsPopup()) {
      state_popup_.OnLoadingStateChange(browser, isLoading, canGoBack,
                                        canGoForward);
    } else {
      state_main_.OnLoadingStateChange(browser, isLoading, canGoBack,
                                       canGoForward);
    }

    if (!isLoading)
      ContinueIfReady(browser);
  }

  virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) OVERRIDE {
    if (browser->IsPopup()) {
      state_popup_.OnLoadStart(browser, frame);
    } else {
      state_main_.OnLoadStart(browser, frame);
    }
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    if (browser->IsPopup()) {
      state_popup_.OnLoadEnd(browser, frame, httpStatusCode);
    } else {
      state_main_.OnLoadEnd(browser, frame, httpStatusCode);
    }

    ContinueIfReady(browser);
  }

  virtual bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE {
    if (browser->IsPopup()) {
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_popup_, browser->GetIdentifier());
    } else {
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_main_, browser->GetIdentifier());
    }

    const std::string& msg_name = message->GetName();
    if (msg_name == kOrderNavMsg || msg_name == kOrderNavClosedMsg) {
      // Test that the renderer side succeeded.
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());
      EXPECT_TRUE(args->GetBool(0));

      if (browser->IsPopup()) {
        EXPECT_EQ(browser_id_popup_, args->GetInt(1));
      } else {
        EXPECT_EQ(browser_id_main_, args->GetInt(1));
      }

      if (msg_name == kOrderNavMsg) {
        // Continue with the test.
        got_message_ = true;
        ContinueIfReady(browser);
      } else {
        // Popup was closed. End the test.
        browser_popup_ = NULL;
        DestroyTest();
      }

      return true;
    }

    // Message not handled.
    return false;
  }

 protected:
  virtual void DestroyTest() OVERRIDE {
    // Verify test expectations.
    EXPECT_TRUE(got_before_browse_main_);
    EXPECT_TRUE(got_before_browse_popup_);

    TestHandler::DestroyTest();
  }

  int browser_id_main_;
  int browser_id_popup_;
  CefRefPtr<CefBrowser> browser_popup_;

  TrackCallback got_before_browse_main_;
  TrackCallback got_before_browse_popup_;

  OrderNavLoadState state_main_;
  OrderNavLoadState state_popup_;

  bool got_message_;
};

}  // namespace

// Verify the order of navigation-related callbacks.
TEST(NavigationTest, Order) {
  g_order_nav_test = true;
  CefRefPtr<OrderNavTestHandler> handler =
      new OrderNavTestHandler();
  handler->ExecuteTest();
  g_order_nav_test = false;
}


namespace {

const char kCrossOriginNav1[] = "http://tests-conav1/nav1.html";
const char kCrossOriginNav2[] = "http://tests-conav2/nav2.html";
const char kCrossOriginNavMsg[] = "NavigationTest.CrossOriginNav";

bool g_cross_origin_nav_test = false;

// Browser side.
class CrossOriginNavBrowserTest : public ClientApp::BrowserDelegate {
 public:
  CrossOriginNavBrowserTest() {}

  virtual void OnBeforeChildProcessLaunch(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefCommandLine> command_line) OVERRIDE {
    if (!g_cross_origin_nav_test)
      return;

    // Indicate to the render process that the test should be run.
    command_line->AppendSwitchWithValue("test", kCrossOriginNavMsg);
  }

 protected:
  IMPLEMENT_REFCOUNTING(CrossOriginNavBrowserTest);
};

// Renderer side.
class CrossOriginNavRendererTest : public ClientApp::RenderDelegate,
                                   public CefLoadHandler {
 public:
  CrossOriginNavRendererTest()
      : run_test_(false) {}
  virtual ~CrossOriginNavRendererTest() {
    EXPECT_TRUE(status_list_.empty());
  }

  virtual void OnRenderThreadCreated(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefListValue> extra_info) OVERRIDE {
    if (!g_cross_origin_nav_test) {
      // Check that the test should be run.
      CefRefPtr<CefCommandLine> command_line =
          CefCommandLine::GetGlobalCommandLine();
      const std::string& test = command_line->GetSwitchValue("test");
      if (test != kCrossOriginNavMsg)
        return;
    }

    run_test_ = true;

    EXPECT_FALSE(got_webkit_initialized_);

    got_render_thread_created_.yes();
  }

  virtual void OnWebKitInitialized(CefRefPtr<ClientApp> app) OVERRIDE {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);

    got_webkit_initialized_.yes();
  }

  virtual void OnBrowserCreated(CefRefPtr<ClientApp> app,
                                CefRefPtr<CefBrowser> browser) OVERRIDE {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    EXPECT_FALSE(GetStatus(browser));
    Status* status = AddStatus(browser);
    status->got_browser_created.yes();
  }

  virtual void OnBrowserDestroyed(CefRefPtr<ClientApp> app,
                                  CefRefPtr<CefBrowser> browser) OVERRIDE {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    Status* status = GetStatus(browser);
    EXPECT_TRUE(status);

    EXPECT_TRUE(status->got_browser_created);
    EXPECT_TRUE(status->got_loading_state_end);

    EXPECT_EQ(status->browser_id, browser->GetIdentifier());

    EXPECT_TRUE(RemoveStatus(browser));
  }

  virtual CefRefPtr<CefLoadHandler> GetLoadHandler(
    CefRefPtr<ClientApp> app) OVERRIDE {
    if (!run_test_)
      return NULL;

    return this;
  }

  virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                    bool isLoading,
                                    bool canGoBack,
                                    bool canGoForward) OVERRIDE {
    if (!isLoading) {
      EXPECT_TRUE(got_render_thread_created_);
      EXPECT_TRUE(got_webkit_initialized_);

      Status* status = GetStatus(browser);
      EXPECT_TRUE(status);

      EXPECT_TRUE(status->got_browser_created);
      EXPECT_FALSE(status->got_loading_state_end);

      status->got_loading_state_end.yes();

      EXPECT_EQ(status->browser_id, browser->GetIdentifier());

      SendTestResults(browser);
    }
  }

 protected:
  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser) {
    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kCrossOriginNavMsg);
    CefRefPtr<CefListValue> args = return_msg->GetArgumentList();
    EXPECT_TRUE(args.get());
    EXPECT_TRUE(args->SetBool(0, result));
    EXPECT_TRUE(args->SetInt(1, browser->GetIdentifier()));
    EXPECT_TRUE(browser->SendProcessMessage(PID_BROWSER, return_msg));
  }

  bool run_test_;

  TrackCallback got_render_thread_created_;
  TrackCallback got_webkit_initialized_;

  struct Status {
    Status() : browser_id(0) {}

    CefRefPtr<CefBrowser> browser;
    int browser_id;
    TrackCallback got_browser_created;
    TrackCallback got_loading_state_end;
  };
  typedef std::list<Status> StatusList;
  StatusList status_list_;

  Status* GetStatus(CefRefPtr<CefBrowser> browser) {
    StatusList::iterator it = status_list_.begin();
    for (; it != status_list_.end(); ++it) {
      Status& status = (*it);
      if (status.browser->IsSame(browser))
        return &status;
    }

    return NULL;
  }

  Status* AddStatus(CefRefPtr<CefBrowser> browser) {
    Status status;
    status.browser = browser;
    status.browser_id = browser->GetIdentifier();
    EXPECT_GT(status.browser_id, 0);
    status_list_.push_back(status);
    return &status_list_.back();
  }

  bool RemoveStatus(CefRefPtr<CefBrowser> browser) {
    StatusList::iterator it = status_list_.begin();
    for (; it != status_list_.end(); ++it) {
      Status& status = (*it);
      if (status.browser->IsSame(browser)) {
        status_list_.erase(it);
        return true;
      }
    }

    return false;
  }

  IMPLEMENT_REFCOUNTING(CrossOriginNavRendererTest);
};

// Browser side.
class CrossOriginNavTestHandler : public TestHandler {
 public:
  CrossOriginNavTestHandler()
      : browser_id_current_(0),
        got_message_(false),
        got_load_end_(false) {}

  virtual void RunTest() OVERRIDE {
    // Add the resources that we will navigate to/from.
    AddResource(kCrossOriginNav1, "<html>Nav1</html>", "text/html");
    AddResource(kCrossOriginNav2, "<html>Nav2</html>", "text/html");

    // Create the browser.
    CreateBrowser(kCrossOriginNav1);
  }

  void ContinueIfReady(CefRefPtr<CefBrowser> browser) {
    if (!got_message_ || !got_load_end_)
      return;

    got_message_ = false;
    got_load_end_ = false;

    std::string url = browser->GetMainFrame()->GetURL();
    if (url == kCrossOriginNav1) {
      // Load the next url.
      browser->GetMainFrame()->LoadURL(kCrossOriginNav2);
    } else {
      // Done with the test.
      DestroyTest();
    }
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE {
    TestHandler::OnAfterCreated(browser);

    EXPECT_EQ(browser_id_current_, 0);
    browser_id_current_ = browser->GetIdentifier();
    EXPECT_GT(browser_id_current_, 0);
  }

  virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              bool is_redirect) OVERRIDE {
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());

    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    return false;
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request) OVERRIDE {
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());

    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());
    
    return false;
  }

  virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) OVERRIDE {
    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    got_load_end_ = true;
    ContinueIfReady(browser);
  }

  virtual bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE {
    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    const std::string& msg_name = message->GetName();
    if (msg_name == kCrossOriginNavMsg) {
      // Test that the renderer side succeeded.
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());
      EXPECT_TRUE(args->GetBool(0));

      EXPECT_EQ(browser_id_current_, args->GetInt(1));

      // Continue with the test.
      got_message_ = true;
      ContinueIfReady(browser);

      return true;
    }

    // Message not handled.
    return false;
  }

 protected:
  int browser_id_current_;

  bool got_message_;
  bool got_load_end_;
};

}  // namespace

// Verify navigation-related callbacks when browsing cross-origin.
TEST(NavigationTest, CrossOrigin) {
  g_cross_origin_nav_test = true;
  CefRefPtr<CrossOriginNavTestHandler> handler =
      new CrossOriginNavTestHandler();
  handler->ExecuteTest();
  g_cross_origin_nav_test = false;
}


namespace {

const char kPopupNavPageUrl[] = "http://tests-popup/page.html";
const char kPopupNavPopupUrl[] = "http://tests-popup/popup.html";
const char kPopupNavPopupName[] = "my_popup";

// Browser side.
class PopupNavTestHandler : public TestHandler {
 public:
  explicit PopupNavTestHandler(bool allow)
      : allow_(allow) {}

  virtual void RunTest() OVERRIDE {
    // Add the resources that we will navigate to/from.
    std::string page = "<html><script>function doPopup() { window.open('" +
                       std::string(kPopupNavPopupUrl) + "', '" +
                       std::string(kPopupNavPopupName) +
                       "'); }</script>Page</html>";
    AddResource(kPopupNavPageUrl, page, "text/html");
    AddResource(kPopupNavPopupUrl, "<html>Popup</html>", "text/html");

    // Create the browser.
    CreateBrowser(kPopupNavPageUrl);
  }

  virtual bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             const CefString& target_url,
                             const CefString& target_frame_name,
                             const CefPopupFeatures& popupFeatures,
                             CefWindowInfo& windowInfo,
                             CefRefPtr<CefClient>& client,
                             CefBrowserSettings& settings,
                             bool* no_javascript_access) OVERRIDE {
    got_on_before_popup_.yes();

    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_STREQ(kPopupNavPageUrl, frame->GetURL().ToString().c_str());
    EXPECT_STREQ(kPopupNavPopupUrl, target_url.ToString().c_str());
    EXPECT_STREQ(kPopupNavPopupName, target_frame_name.ToString().c_str());
    EXPECT_FALSE(*no_javascript_access);

    return !allow_;
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    std::string url = frame->GetURL();
    if (url == kPopupNavPageUrl) {
      frame->ExecuteJavaScript("doPopup()", kPopupNavPageUrl, 0);

      if (!allow_) {
        // Wait a bit to make sure the popup window isn't created.
        CefPostDelayedTask(TID_UI,
            base::Bind(&PopupNavTestHandler::DestroyTest, this), 200);
      }
    } else if (url == kPopupNavPopupUrl) {
      if (allow_) {
        got_popup_load_end_.yes();
        browser->GetHost()->CloseBrowser(false);
        DestroyTest();
      } else {
        EXPECT_FALSE(true); // Not reached.
      }
    } else {
      EXPECT_FALSE(true); // Not reached.
    }
  }

 private:
  virtual void DestroyTest() OVERRIDE {
    EXPECT_TRUE(got_on_before_popup_);
    if (allow_)
      EXPECT_TRUE(got_popup_load_end_);
    else
      EXPECT_FALSE(got_popup_load_end_);

    TestHandler::DestroyTest();
  }

  bool allow_;

  TrackCallback got_on_before_popup_;
  TrackCallback got_popup_load_end_;
};

}  // namespace

// Test allowing popups.
TEST(NavigationTest, PopupAllow) {
  CefRefPtr<PopupNavTestHandler> handler = new PopupNavTestHandler(true);
  handler->ExecuteTest();
}

// Test denying popups.
TEST(NavigationTest, PopupDeny) {
  CefRefPtr<PopupNavTestHandler> handler = new PopupNavTestHandler(false);
  handler->ExecuteTest();
}


namespace {

const char kBrowseNavPageUrl[] = "http://tests-browsenav/nav.html";

// Browser side.
class BrowseNavTestHandler : public TestHandler {
 public:
  BrowseNavTestHandler(bool allow)
      : allow_(allow),
        destroyed_(false) {}

  virtual void RunTest() OVERRIDE {
   AddResource(kBrowseNavPageUrl, "<html>Test</html>", "text/html");

    // Create the browser.
    CreateBrowser(kBrowseNavPageUrl);

    // Time out the test after a reasonable period of time.
    CefPostDelayedTask(TID_UI,
        base::Bind(&BrowseNavTestHandler::DestroyTest, this),
        2000);
  }

  virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              bool is_redirect) OVERRIDE {
    const std::string& url = request->GetURL();
    EXPECT_STREQ(kBrowseNavPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());
    
    got_before_browse_.yes();
    
    return !allow_;
  }

  virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) OVERRIDE {
    const std::string& url = frame->GetURL();
    EXPECT_STREQ(kBrowseNavPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_load_start_.yes();
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    const std::string& url = frame->GetURL();
    EXPECT_STREQ(kBrowseNavPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_load_end_.yes();
    DestroyTestIfDone();
  }

  virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           ErrorCode errorCode,
                           const CefString& errorText,
                           const CefString& failedUrl) OVERRIDE {
    const std::string& url = frame->GetURL();
    EXPECT_STREQ("", url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    EXPECT_EQ(ERR_ABORTED, errorCode);
    EXPECT_STREQ(kBrowseNavPageUrl, failedUrl.ToString().c_str());

    got_load_error_.yes();
    DestroyTestIfDone();
  }

  virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                    bool isLoading,
                                    bool canGoBack,
                                    bool canGoForward) OVERRIDE {
    const std::string& url = browser->GetMainFrame()->GetURL();
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());

    if (isLoading) {
      EXPECT_STREQ("", url.c_str());

      got_loading_state_changed_start_.yes();
    } else {
      if (allow_)
        EXPECT_STREQ(kBrowseNavPageUrl, url.c_str());
      else
        EXPECT_STREQ("", url.c_str());

      got_loading_state_changed_end_.yes();
      DestroyTestIfDone();
    }
  }

 private:
  void DestroyTestIfDone() {
    if (destroyed_)
      return;

    if (got_loading_state_changed_end_) {
      if (allow_) {
        if (got_load_end_)
          DestroyTest();
      } else if (got_load_error_) {
        DestroyTest();
      }
    }
  }

  virtual void DestroyTest() OVERRIDE {
    if (destroyed_)
      return;
    destroyed_ = true;

    EXPECT_TRUE(got_before_browse_);
    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_TRUE(got_loading_state_changed_end_);

    if (allow_) {
      EXPECT_TRUE(got_load_start_);
      EXPECT_TRUE(got_load_end_);
      EXPECT_FALSE(got_load_error_);
    } else {
      EXPECT_FALSE(got_load_start_);
      EXPECT_FALSE(got_load_end_);
      EXPECT_TRUE(got_load_error_);
    }

    TestHandler::DestroyTest();
  }

  bool allow_;
  bool destroyed_;

  TrackCallback got_before_browse_;
  TrackCallback got_load_start_;
  TrackCallback got_load_end_;
  TrackCallback got_load_error_;
  TrackCallback got_loading_state_changed_start_;
  TrackCallback got_loading_state_changed_end_;
};

}  // namespace

// Test allowing navigation.
TEST(NavigationTest, BrowseAllow) {
  CefRefPtr<BrowseNavTestHandler> handler = new BrowseNavTestHandler(true);
  handler->ExecuteTest();
}

// Test denying navigation.
TEST(NavigationTest, BrowseDeny) {
  CefRefPtr<BrowseNavTestHandler> handler = new BrowseNavTestHandler(false);
  handler->ExecuteTest();
}


// Entry point for creating navigation browser test objects.
// Called from client_app_delegates.cc.
void CreateNavigationBrowserTests(ClientApp::BrowserDelegateSet& delegates) {
  delegates.insert(new HistoryNavBrowserTest);
  delegates.insert(new OrderNavBrowserTest);
  delegates.insert(new CrossOriginNavBrowserTest);
}

// Entry point for creating navigation renderer test objects.
// Called from client_app_delegates.cc.
void CreateNavigationRendererTests(ClientApp::RenderDelegateSet& delegates) {
  delegates.insert(new HistoryNavRendererTest);
  delegates.insert(new OrderNavRendererTest);
  delegates.insert(new CrossOriginNavRendererTest);
}
