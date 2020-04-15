// Concord
//
// Copyright (c) 2018-2019 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").  You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the sub-component's license, as noted in the LICENSE
// file.

#pragma once

#include "SysConsts.hpp"
#include "PrimitiveTypes.hpp"
#include "MsgCode.hpp"
#include "ReplicasInfo.hpp"

namespace bftEngine {
namespace impl {

template <typename MessageT>
size_t sizeOfHeader();

class MessageBase {
 public:
#pragma pack(push, 1)
  struct Header {
    MsgType msgType;
    SpanContextSize spanContextSize = 0u;
  };
#pragma pack(pop)

  static_assert(sizeof(Header) == 6, "MessageBase::Header is 6B");

  explicit MessageBase(NodeIdType sender);

  MessageBase(NodeIdType sender, MsgType type, MsgSize size);
  MessageBase(NodeIdType sender, MsgType type, SpanContextSize spanContextSize, MsgSize size);

  MessageBase(NodeIdType sender, Header *body, MsgSize size, bool ownerOfStorage);

  void acquireOwnership() { owner_ = true; }

  void releaseOwnership() { owner_ = false; }

  virtual ~MessageBase();

  virtual void validate(const ReplicasInfo &) const {}

  bool equals(const MessageBase &other) const;

  static size_t serializeMsg(char *&buf, const MessageBase *msg);
  static MessageBase *deserializeMsg(char *&buf, size_t bufLen, size_t &actualSize);

  MsgSize size() const { return msgSize_; }

  char *body() const { return (char *)msgBody_; }

  NodeIdType senderId() const { return sender_; }

  MsgType type() const { return msgBody_->msgType; }

  SpanContextSize spanContextSize() const { return msgBody_->spanContextSize; }

  template <typename MessageT>
  std::string spanContext() const {
    return std::string(body() + sizeOfHeader<MessageT>(), spanContextSize());
  }

  MessageBase *cloneObjAndMsg() const;

  size_t sizeNeededForObjAndMsgInLocalBuffer() const;
#ifdef DEBUG_MEMORY_MSG
  static void printLiveMessages();
#endif

 protected:
  void writeObjAndMsgToLocalBuffer(char *buffer, size_t bufferLength, size_t *actualSize) const;
  static MessageBase *createObjAndMsgFromLocalBuffer(char *buffer, size_t bufferLength, size_t *actualSize);
  void shrinkToFit();

  void setMsgSize(MsgSize size);

  MsgSize internalStorageSize() const { return storageSize_; }

 protected:
  Header *msgBody_ = nullptr;
  MsgSize msgSize_ = 0;
  MsgSize storageSize_ = 0;
  NodeIdType sender_;
  // true IFF this instance is not responsible for de-allocating the body:
  bool owner_ = true;
  static constexpr uint32_t magicNumOfRawFormat = 0x5555897BU;

  template <typename MessageT>
  friend size_t sizeOfHeader();

  template <typename MessageT>
  friend MsgSize maxMessageSize();

  static constexpr uint64_t SPAN_CONTEXT_MAX_SIZE{1024};
#pragma pack(push, 1)
  struct RawHeaderOfObjAndMsg {
    uint32_t magicNum;
    MsgSize msgSize;
    NodeIdType sender;
    // TODO(GG): consider to add checksum
  };
#pragma pack(pop)
};

template <typename MessageT>
size_t sizeOfHeader() {
  return sizeof(typename MessageT::Header);
}

template <typename MessageT>
MsgSize maxMessageSize() {
  return sizeOfHeader<MessageT>() + MessageBase::SPAN_CONTEXT_MAX_SIZE;
}

}  // namespace impl
}  // namespace bftEngine
