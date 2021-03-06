// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <map>

// Include this first to avoid type conflicts with CEF headers.
#include "tests/unittests/chromium_includes.h"

#include "include/base/cef_bind.h"
#include "include/cef_request.h"
#include "include/wrapper/cef_closure_task.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tests/cefclient/client_app.h"
#include "tests/unittests/test_handler.h"
#include "tests/unittests/test_util.h"

// Comment out this define to disable the unit test timeout.
#define TIMEOUT_ENABLED 1

// Verify Set/Get methods for CefRequest, CefPostData and CefPostDataElement.
TEST(RequestTest, SetGet) {
  // CefRequest CreateRequest
  CefRefPtr<CefRequest> request(CefRequest::Create());
  ASSERT_TRUE(request.get() != NULL);

  CefString url = "http://tests/run.html";
  CefString method = "POST";
  CefRequest::HeaderMap setHeaders, getHeaders;
  setHeaders.insert(std::make_pair("HeaderA", "ValueA"));
  setHeaders.insert(std::make_pair("HeaderB", "ValueB"));

  // CefPostData CreatePostData
  CefRefPtr<CefPostData> postData(CefPostData::Create());
  ASSERT_TRUE(postData.get() != NULL);

  // CefPostDataElement CreatePostDataElement
  CefRefPtr<CefPostDataElement> element1(CefPostDataElement::Create());
  ASSERT_TRUE(element1.get() != NULL);
  CefRefPtr<CefPostDataElement> element2(CefPostDataElement::Create());
  ASSERT_TRUE(element2.get() != NULL);

  // CefPostDataElement SetToFile
  CefString file = "c:\\path\\to\\file.ext";
  element1->SetToFile(file);
  ASSERT_EQ(PDE_TYPE_FILE, element1->GetType());
  ASSERT_EQ(file, element1->GetFile());

  // CefPostDataElement SetToBytes
  char bytes[] = "Test Bytes";
  element2->SetToBytes(sizeof(bytes), bytes);
  ASSERT_EQ(PDE_TYPE_BYTES, element2->GetType());
  ASSERT_EQ(sizeof(bytes), element2->GetBytesCount());
  char bytesOut[sizeof(bytes)];
  element2->GetBytes(sizeof(bytes), bytesOut);
  ASSERT_TRUE(!memcmp(bytes, bytesOut, sizeof(bytes)));

  // CefPostData AddElement
  postData->AddElement(element1);
  postData->AddElement(element2);
  ASSERT_EQ((size_t)2, postData->GetElementCount());

  // CefPostData RemoveElement
  postData->RemoveElement(element1);
  ASSERT_EQ((size_t)1, postData->GetElementCount());

  // CefPostData RemoveElements
  postData->RemoveElements();
  ASSERT_EQ((size_t)0, postData->GetElementCount());

  postData->AddElement(element1);
  postData->AddElement(element2);
  ASSERT_EQ((size_t)2, postData->GetElementCount());
  CefPostData::ElementVector elements;
  postData->GetElements(elements);
  CefPostData::ElementVector::const_iterator it = elements.begin();
  for (size_t i = 0; it != elements.end(); ++it, ++i) {
    if (i == 0)
      TestPostDataElementEqual(element1, (*it).get());
    else if (i == 1)
      TestPostDataElementEqual(element2, (*it).get());
  }

  // CefRequest SetURL
  request->SetURL(url);
  ASSERT_EQ(url, request->GetURL());

  // CefRequest SetMethod
  request->SetMethod(method);
  ASSERT_EQ(method, request->GetMethod());

  // CefRequest SetHeaderMap
  request->SetHeaderMap(setHeaders);
  request->GetHeaderMap(getHeaders);
  TestMapEqual(setHeaders, getHeaders, false);
  getHeaders.clear();

  // CefRequest SetPostData
  request->SetPostData(postData);
  TestPostDataEqual(postData, request->GetPostData());

  request = CefRequest::Create();
  ASSERT_TRUE(request.get() != NULL);

  // CefRequest Set
  request->Set(url, method, postData, setHeaders);
  ASSERT_EQ(url, request->GetURL());
  ASSERT_EQ(method, request->GetMethod());
  request->GetHeaderMap(getHeaders);
  TestMapEqual(setHeaders, getHeaders, false);
  getHeaders.clear();
  TestPostDataEqual(postData, request->GetPostData());
}

namespace {

void CreateRequest(CefRefPtr<CefRequest>& request) {
  request = CefRequest::Create();
  ASSERT_TRUE(request.get() != NULL);

  request->SetURL("http://tests/run.html");
  request->SetMethod("POST");

  CefRequest::HeaderMap headers;
  headers.insert(std::make_pair("HeaderA", "ValueA"));
  headers.insert(std::make_pair("HeaderB", "ValueB"));
  request->SetHeaderMap(headers);

  CefRefPtr<CefPostData> postData(CefPostData::Create());
  ASSERT_TRUE(postData.get() != NULL);

  CefRefPtr<CefPostDataElement> element1(
      CefPostDataElement::Create());
  ASSERT_TRUE(element1.get() != NULL);
  char bytes[] = "Test Bytes";
  element1->SetToBytes(sizeof(bytes), bytes);
  postData->AddElement(element1);

  request->SetPostData(postData);
}

class RequestSendRecvTestHandler : public TestHandler {
 public:
  RequestSendRecvTestHandler() {}

  virtual void RunTest() OVERRIDE {
    // Create the test request
    CreateRequest(request_);

    // Create the browser
    CreateBrowser("about:blank");
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE {
    TestHandler::OnAfterCreated(browser);

    // Load the test request
    browser->GetMainFrame()->LoadRequest(request_);
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request) OVERRIDE {
    // Verify that the request is the same
    TestRequestEqual(request_, request, true);
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    EXPECT_EQ(TT_LINK, request->GetTransitionType());

    got_before_resource_load_.yes();

    return false;
  }

  virtual CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) OVERRIDE {
    // Verify that the request is the same
    TestRequestEqual(request_, request, true);
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    EXPECT_EQ(TT_LINK, request->GetTransitionType());

    got_resource_handler_.yes();

    DestroyTest();

    // No results
    return NULL;
  }

  CefRefPtr<CefRequest> request_;

  TrackCallback got_before_resource_load_;
  TrackCallback got_resource_handler_;
};

}  // namespace

// Verify send and recieve
TEST(RequestTest, SendRecv) {
  CefRefPtr<RequestSendRecvTestHandler> handler =
      new RequestSendRecvTestHandler();
  handler->ExecuteTest();

  ASSERT_TRUE(handler->got_before_resource_load_);
  ASSERT_TRUE(handler->got_resource_handler_);
}

namespace {

const char kTypeTestCompleteMsg[] = "RequestTest.Type";
const char kTypeTestOrigin[] = "http://tests-requesttt.com/";

static struct TypeExpected {
  const char* file;
  bool browser_side;  // True if this expectation applies to the browser side.
  bool navigation; // True if this expectation represents a navigation.
  cef_transition_type_t transition_type;
  cef_resource_type_t resource_type;
  int expected_count;
} g_type_expected[] = {
  // Initial main frame load due to browser creation.
  {"main.html", true,  true, TT_EXPLICIT, RT_MAIN_FRAME,   1},
  {"main.html", false, true, TT_EXPLICIT, RT_SUB_RESOURCE, 1},

  // Sub frame load.
  {"sub.html", true,  true, TT_LINK,     RT_SUB_FRAME,    1},
  {"sub.html", false, true, TT_EXPLICIT, RT_SUB_RESOURCE, 1},

  // Stylesheet load.
  {"style.css", true, false, TT_LINK, RT_STYLESHEET, 1},

  // Script load.
  {"script.js", true, false, TT_LINK, RT_SCRIPT, 1},

  // Image load.
  {"image.png", true, false, TT_LINK, RT_IMAGE, 1},

  // Font load.
  {"font.ttf", true, false, TT_LINK, RT_FONT_RESOURCE, 1},

  // XHR load.
  {"xhr.html", true, false, TT_LINK, RT_XHR, 1},
};

class TypeExpectations {
 public:
  TypeExpectations(bool browser_side, bool navigation)
      : browser_side_(browser_side),
        navigation_(navigation) {
    // Build the map of relevant requests.
    for (int i = 0;
         i < static_cast<int>(sizeof(g_type_expected) / sizeof(TypeExpected));
         ++i) {
      if (g_type_expected[i].browser_side != browser_side_ ||
          (navigation_ && g_type_expected[i].navigation != navigation_))
        continue;

      request_count_.insert(std::make_pair(i, 0));
    }
  }

  // Notify that a request has been received. Returns true if the request is
  // something we care about.
  bool GotRequest(CefRefPtr<CefRequest> request) {
    const std::string& url = request->GetURL();
    if (url.find(kTypeTestOrigin) != 0)
      return false;

    const std::string& file = url.substr(sizeof(kTypeTestOrigin)-1);
    cef_transition_type_t transition_type = request->GetTransitionType();
    cef_resource_type_t resource_type = request->GetResourceType();

    const int index = GetExpectedIndex(file, transition_type, resource_type);
    EXPECT_GE(index, 0)
        << "File: " << file.c_str()
        << "; Browser Side: " << browser_side_
        << "; Navigation: " << navigation_
        << "; Transition Type: " << transition_type
        << "; Resource Type: " << resource_type;

    RequestCount::iterator it = request_count_.find(index);
    EXPECT_TRUE(it != request_count_.end());

    const int actual_count = ++it->second;
    const int expected_count = g_type_expected[index].expected_count;
    EXPECT_LE(actual_count, expected_count)
        << "File: " << file.c_str()
        << "; Browser Side: " << browser_side_
        << "; Navigation: " << navigation_
        << "; Transition Type: " << transition_type
        << "; Resource Type: " << resource_type;

    return true;
  }

  // Test if all expectations have been met.
  bool IsDone(bool assert) {
    for (int i = 0;
         i < static_cast<int>(sizeof(g_type_expected) / sizeof(TypeExpected));
         ++i) {
      if (g_type_expected[i].browser_side != browser_side_ ||
          (navigation_ && g_type_expected[i].navigation != navigation_))
        continue;

      RequestCount::const_iterator it = request_count_.find(i);
      EXPECT_TRUE(it != request_count_.end());
      if (it->second != g_type_expected[i].expected_count) {
        if (assert) {
          EXPECT_EQ(g_type_expected[i].expected_count, it->second)
              << "File: " << g_type_expected[i].file
              << "; Browser Side: " << browser_side_
              << "; Navigation: " << navigation_
              << "; Transition Type: " << g_type_expected[i].transition_type
              << "; Resource Type: " << g_type_expected[i].resource_type;
        }
        return false;
      }
    }
    return true;
  }

 private:
  // Returns the index for the specified navigation.
  int GetExpectedIndex(const std::string& file,
                       cef_transition_type_t transition_type,
                       cef_resource_type_t resource_type) {
    for (int i = 0;
         i < static_cast<int>(sizeof(g_type_expected) / sizeof(TypeExpected));
         ++i) {
      if (g_type_expected[i].file == file &&
          g_type_expected[i].browser_side == browser_side_ &&
          (!navigation_ || g_type_expected[i].navigation == navigation_) &&
          g_type_expected[i].transition_type == transition_type &&
          g_type_expected[i].resource_type == resource_type) {
        return i;
      }
    }
    return -1;
  }

  bool browser_side_;
  bool navigation_;

  // Map of TypeExpected index to actual request count.
  typedef std::map<int, int> RequestCount;
  RequestCount request_count_;
};

// Renderer side.
class TypeRendererTest : public ClientApp::RenderDelegate {
 public:
  TypeRendererTest() :
      expectations_(false, true) {}

  virtual bool OnBeforeNavigation(CefRefPtr<ClientApp> app,
                                  CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefRequest> request,
                                  cef_navigation_type_t navigation_type,
                                  bool is_redirect) OVERRIDE {
    if (expectations_.GotRequest(request) && expectations_.IsDone(false))
      SendTestResults(browser);
    return false;
  }

 private:
  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser) {
    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kTypeTestCompleteMsg);
    CefRefPtr<CefListValue> args = return_msg->GetArgumentList();
    EXPECT_TRUE(args.get());
    EXPECT_TRUE(args->SetBool(0, result));
    EXPECT_TRUE(browser->SendProcessMessage(PID_BROWSER, return_msg));
  }

  TypeExpectations expectations_;

  IMPLEMENT_REFCOUNTING(TypeRendererTest);
};

// Browser side.
class TypeTestHandler : public TestHandler {
 public:
  TypeTestHandler() :
      browse_expectations_(true, true),
      load_expectations_(true, false),
      get_expectations_(true, false),
      completed_browser_side_(false),
      completed_render_side_(false),
      destroyed_(false) {}

  virtual void RunTest() OVERRIDE {
    AddResource(std::string(kTypeTestOrigin) + "main.html",
        "<html>"
        "<head>"
        "<link rel=\"stylesheet\" href=\"style.css\" type=\"text/css\">"
        "<script type=\"text/javascript\" src=\"script.js\"></script>"
        "</head>"
        "<body><p>Main</p>"
        "<script>xhr = new XMLHttpRequest();"
        "xhr.open('GET', 'xhr.html', false);"
        "xhr.send();</script>"
        "<iframe src=\"sub.html\"></iframe>"
        "<img src=\"image.png\">"
        "</body></html>",
        "text/html");
    AddResource(std::string(kTypeTestOrigin) + "sub.html",
        "<html>Sub</html>",
        "text/html");
    AddResource(std::string(kTypeTestOrigin) + "style.css",
        "@font-face {"
        "  font-family: custom_font;"
        "  src: url('font.ttf');"
        "}"
        "p {"
        "  font-family: custom_font;"
        "}",
        "text/css");
    AddResource(std::string(kTypeTestOrigin) + "script.js",
        "<!-- -->",
        "text/javascript");
    AddResource(std::string(kTypeTestOrigin) + "image.png",
        "<!-- -->",
        "image/png");
    AddResource(std::string(kTypeTestOrigin) + "font.ttf",
        "<!-- -->",
        "font/ttf");
    AddResource(std::string(kTypeTestOrigin) + "xhr.html",
        "<html>XHR</html>",
        "text/html");

    CreateBrowser(std::string(kTypeTestOrigin) + "main.html");

#if defined(TIMEOUT_ENABLED)
    // Time out the test after a reasonable period of time.
    CefPostDelayedTask(TID_UI, base::Bind(&TypeTestHandler::DestroyTest, this),
                       2000);
#endif
  }

  virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              bool is_redirect) OVERRIDE {
    browse_expectations_.GotRequest(request);

    return false;
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request) OVERRIDE {
    load_expectations_.GotRequest(request);
    
    return false;
  }

  virtual CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) OVERRIDE {
    if (get_expectations_.GotRequest(request) &&
        get_expectations_.IsDone(false)) {
      completed_browser_side_ = true;
      // Destroy the test on the UI thread.
      CefPostTask(TID_UI,
          base::Bind(&TypeTestHandler::DestroyTestIfComplete, this));
    }

    return TestHandler::GetResourceHandler(browser, frame, request);
  }

  virtual bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE {
    const std::string& msg_name = message->GetName();
    if (msg_name == kTypeTestCompleteMsg) {
      // Test that the renderer side succeeded.
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());
      EXPECT_TRUE(args->GetBool(0));

      completed_render_side_ = true;
      DestroyTestIfComplete();
      return true;
    }

    // Message not handled.
    return false;
  }

 private:
  void DestroyTestIfComplete() {
    if (destroyed_)
      return;

    if (completed_browser_side_ && completed_render_side_)
      DestroyTest();
  }

  virtual void DestroyTest() OVERRIDE {
    if (destroyed_)
      return;
    destroyed_ = true;

    // Verify test expectations.
    EXPECT_TRUE(completed_browser_side_);
    EXPECT_TRUE(completed_render_side_);
    EXPECT_TRUE(browse_expectations_.IsDone(true));
    EXPECT_TRUE(load_expectations_.IsDone(true));
    EXPECT_TRUE(get_expectations_.IsDone(true));

    TestHandler::DestroyTest();
  }

  TypeExpectations browse_expectations_;
  TypeExpectations load_expectations_;
  TypeExpectations get_expectations_;

  bool completed_browser_side_;
  bool completed_render_side_;
  bool destroyed_;
};

}  // namespace

// Verify the order of navigation-related callbacks.
TEST(RequestTest, ResourceAndTransitionType) {
  CefRefPtr<TypeTestHandler> handler =
      new TypeTestHandler();
  handler->ExecuteTest();
}


// Entry point for creating request renderer test objects.
// Called from client_app_delegates.cc.
void CreateRequestRendererTests(ClientApp::RenderDelegateSet& delegates) {
  delegates.insert(new TypeRendererTest);
}
