#include "OpenTracing.hpp"
#include "Logger.hpp"
#include <string_view>

namespace concordUtils {
void SpanWrapper::setTag(const std::string& name, const std::string& value) {
#ifdef USE_OPENTRACING
  LOG_INFO(GL, "Set tag: " << name << ":" << value);
  span_ptr_->SetTag(name, value);
#else
  LOG_INFO(GL, "Set tag ignored");
  (void)name;
  (void)value;
#endif
}

void SpanWrapper::Finish() {
#ifdef USE_OPENTRACING
  span_ptr_->Finish();
#endif
}

SpanWrapper startSpan(const std::string& operation_name) {
#ifdef USE_OPENTRACING
  auto tracer = opentracing::Tracer::Global();
  if (!tracer) {
    // Tracer is not initialized by the parent
    return SpanWrapper();
  } else {
    return SpanWrapper{opentracing::Tracer::Global()->StartSpan(operation_name)};
  }
#else
  (void)operation_name;
  return SpanWrapper();
#endif
}

SpanWrapper startChildSpan(const std::string& operation_name, const SpanWrapper& parent_span) {
#ifdef USE_OPENTRACING
  auto tracer = opentracing::Tracer::Global();
  if (!tracer) {
    // Tracer is not initialized by the parent
    return SpanWrapper();
  } else {
    return SpanWrapper{opentracing::Tracer::Global()->StartSpan(
        operation_name, {opentracing::ChildOf(&parent_span.span_impl()->context())})};
  }
#else
  (void)operation_name;
  (void)parent_span;
  return SpanWrapper();
#endif
}

SpanWrapper fromContext(const SpanContext& context, const std::string& child_operation_name) {
#ifdef USE_OPENTRACING
  auto tracer = opentracing::Tracer::Global();
  if (!tracer) {
    // Tracer is not initialized by the parent
    return SpanWrapper();
  }
  if (context.empty()) {
    return SpanWrapper();
  }
  std::istringstream context_stream{context};
  auto parent_span_context = tracer->Extract(context_stream);
  if (!parent_span_context) {
    std::ostringstream stream;
    stream << "Failed to extract span, error:" << parent_span_context.error();
    throw std::runtime_error(stream.str());
  }
  return SpanWrapper{tracer->StartSpan(child_operation_name, {opentracing::ChildOf(parent_span_context->get())})};
#else
  (void)context;
  (void)child_operation_name;
  return SpanWrapper();
#endif
}

SpanContext toContext(const SpanWrapper& wrapper) {
#ifdef USE_OPENTRACING
  auto tracer = opentracing::Tracer::Global();
  if (!tracer) {
    // Tracer is not initialized by the parent
    return SpanContext();
  }
  std::ostringstream context;
  wrapper.span_impl()->tracer().Inject(wrapper.span_impl()->context(), context);
  return SpanContext{context.str()};
#else
  (void)wrapper;
  return SpanContext();
#endif
}
}  // namespace concordUtils
