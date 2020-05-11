#ifndef OPENTRACING_UTILS_HPP
#define OPENTRACING_UTILS_HPP

#ifdef USE_OPENTRACING
#include <opentracing/tracer.h>
#endif
#include <memory>
#include <string>
#include <string_view>
#include <strstream>

namespace concordUtils {

using SpanContext = std::string;

class SpanWrapper {
#ifdef USE_OPENTRACING
  using SpanPtr = std::unique_ptr<opentracing::Span>;
#else
  using SpanPtr = void*;
#endif
 public:
  SpanWrapper() {}
  SpanWrapper(const SpanWrapper&) = delete;
  SpanWrapper& operator=(const SpanWrapper&) = delete;
  SpanWrapper(SpanWrapper&&) = default;
  SpanWrapper& operator=(SpanWrapper&&) = default;
  void setTag(const std::string& name, const std::string& value);
  void Finish();

  friend SpanWrapper startSpan(const std::string& operation_name);
  friend SpanWrapper startChildSpan(const std::string& operation_name, const SpanWrapper& parent_span);
  friend SpanWrapper fromContext(const SpanContext& context, const std::string& child_operation_name);
  friend SpanContext toContext(const SpanWrapper& wrapper);

 private:
  SpanWrapper(SpanPtr&& span) : span_ptr_(std::move(span)) {}
  const SpanPtr& span_impl() const { return span_ptr_; }
  /* data */
  SpanPtr span_ptr_;
};

SpanWrapper startSpan(const std::string& operation_name);
SpanWrapper startChildSpan(const std::string& operation_name, const SpanWrapper& parent_span);
SpanWrapper fromContext(const SpanContext& context, const std::string& child_operation_name);
SpanContext toContext(const SpanWrapper& wrapper);
}  // namespace concordUtils
#endif /* end of include guard: OPENTRACING_UTILS_HPP */
