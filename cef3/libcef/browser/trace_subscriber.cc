// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/trace_subscriber.h"
#include "include/cef_trace.h"
#include "libcef/browser/thread_util.h"

#include "base/debug/trace_event.h"
#include "base/files/file_util.h"
#include "content/public/browser/tracing_controller.h"

namespace {

// Create the temporary file and then execute |callback| on the thread
// represented by |message_loop_proxy|. 
void CreateTemporaryFileOnFileThread(
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
    base::Callback<void(const base::FilePath&)> callback) {
  CEF_REQUIRE_FILET();
  base::FilePath file_path;
  if (!base::CreateTemporaryFile(&file_path))
    LOG(ERROR) << "Failed to create temporary file.";
  message_loop_proxy->PostTask(FROM_HERE, base::Bind(callback, file_path));
}

}  // namespace

using content::TracingController;

CefTraceSubscriber::CefTraceSubscriber()
    : collecting_trace_data_(false),
      weak_factory_(this) {
  CEF_REQUIRE_UIT();
}

CefTraceSubscriber::~CefTraceSubscriber() {
  CEF_REQUIRE_UIT();
  if (collecting_trace_data_)
    TracingController::GetInstance()->DisableRecording(NULL);
}

bool CefTraceSubscriber::BeginTracing(
    const std::string& categories,
    CefRefPtr<CefCompletionCallback> callback) {
  CEF_REQUIRE_UIT();

  if (collecting_trace_data_)
    return false;

  collecting_trace_data_ = true;

  TracingController::EnableRecordingDoneCallback done_callback;
  if (callback.get()) {
    done_callback =
        base::Bind(&CefCompletionCallback::OnComplete, callback.get());
  }

  TracingController::GetInstance()->EnableRecording(
      base::debug::CategoryFilter(categories),
      base::debug::TraceOptions(),
      done_callback);
  return true;
}

bool CefTraceSubscriber::EndTracing(
    const base::FilePath& tracing_file,
    CefRefPtr<CefEndTracingCallback> callback) {
  CEF_REQUIRE_UIT();

  if (!collecting_trace_data_)
    return false;

  if (tracing_file.empty()) {
    // Create a new temporary file path on the FILE thread, then continue.
    CEF_POST_TASK(CEF_FILET,
        base::Bind(CreateTemporaryFileOnFileThread,
            base::MessageLoop::current()->message_loop_proxy(),
            base::Bind(&CefTraceSubscriber::ContinueEndTracing,
                       weak_factory_.GetWeakPtr(), callback)));
    return true;
  }

  base::Closure result_callback;
  if (callback.get()) {
    result_callback =
        base::Bind(&CefTraceSubscriber::OnTracingFileResult,
                   weak_factory_.GetWeakPtr(), callback, tracing_file);
  }

  TracingController::GetInstance()->DisableRecording(
      TracingController::CreateFileSink(tracing_file, result_callback));
  return true;
}

void CefTraceSubscriber::ContinueEndTracing(
    CefRefPtr<CefEndTracingCallback> callback,
    const base::FilePath& tracing_file) {
  CEF_REQUIRE_UIT();
  if (!tracing_file.empty())
    EndTracing(tracing_file, callback);
}

void CefTraceSubscriber::OnTracingFileResult(
    CefRefPtr<CefEndTracingCallback> callback,
    const base::FilePath& tracing_file) {
  CEF_REQUIRE_UIT();

  collecting_trace_data_ = false;

  callback->OnEndTracingComplete(tracing_file.value());
}
