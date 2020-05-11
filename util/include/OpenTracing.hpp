#ifndef OPENTRACING_UTILS_HPP
#define OPENTRACING_UTILS_HPP

#ifdef USE_OPENTRACING
#include <opentracing/tracer.h>
#endif
#include <memory>
#include <string>
#include <string_view>
#include <strstream>

namespace concordUtils::opentracing {

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
  void setTag(std::string_view name, std::string_view value);
  void Finish();

  friend SpanContext toContext(const SpanWrapper& wrapper);

 private:
  SpanWrapper(SpanPtr&& span) : span_ptr_(std::move(span)) {}
  const SpanPtr& span_impl() const { return span_ptr_; }
  /* data */
  SpanPtr span_ptr_;
};

SpanWrapper startSpan(std::string_view operation_name);
SpanWrapper startChildSpan(std::string_view operation_name, const SpanWrapper& parent_span);
SpanWrapper fromContext(const SpanContext& context, std::string_view child_operation_name);
SpanContext toContext(const SpanWrapper& wrapper);
}  // namespace concordUtils::opentracing
#endif /* end of include guard: OPENTRACING_UTILS_HPP */
