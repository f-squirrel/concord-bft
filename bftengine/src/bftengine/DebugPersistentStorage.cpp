// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include "DebugPersistentStorage.hpp"

#include "messages/PrePrepareMsg.hpp"
#include "messages/SignedShareMsgs.hpp"
#include "messages/NewViewMsg.hpp"
#include "messages/FullCommitProofMsg.hpp"
#include "messages/CheckpointMsg.hpp"

namespace bftEngine {
namespace impl {

DebugPersistentStorage::DebugPersistentStorage(uint16_t fVal, uint16_t cVal)
    : fVal_{fVal}, cVal_{cVal}, config_{ReplicaConfig::instance()}, seqNumWindow(1), checkWindow(0) {}

uint8_t DebugPersistentStorage::beginWriteTran() { return ++numOfNestedTransactions; }

uint8_t DebugPersistentStorage::endWriteTran() {
  ConcordAssert(numOfNestedTransactions != 0);
  return --numOfNestedTransactions;
}

bool DebugPersistentStorage::isInWriteTran() const { return (numOfNestedTransactions != 0); }

void DebugPersistentStorage::setLastExecutedSeqNum(SeqNum seqNum) {
  ConcordAssert(setIsAllowed());
  ConcordAssert(lastExecutedSeqNum_ <= seqNum);
  lastExecutedSeqNum_ = seqNum;
}

void DebugPersistentStorage::setPrimaryLastUsedSeqNum(SeqNum seqNum) {
  ConcordAssert(nonExecSetIsAllowed());
  primaryLastUsedSeqNum_ = seqNum;
}

void DebugPersistentStorage::setStrictLowerBoundOfSeqNums(SeqNum seqNum) {
  ConcordAssert(nonExecSetIsAllowed());
  strictLowerBoundOfSeqNums_ = seqNum;
}

void DebugPersistentStorage::setLastViewThatTransferredSeqNumbersFullyExecuted(ViewNum view) {
  ConcordAssert(nonExecSetIsAllowed());
  ConcordAssert(lastViewThatTransferredSeqNumbersFullyExecuted_ <= view);
  lastViewThatTransferredSeqNumbersFullyExecuted_ = view;
}

void DebugPersistentStorage::setDescriptorOfLastExitFromView(const DescriptorOfLastExitFromView &prevViewDesc) {
  ConcordAssert(nonExecSetIsAllowed());
  ConcordAssert(prevViewDesc.view >= 0);

  // Here we assume that the first view is always 0
  // (even if we load the initial state from disk)
  ConcordAssert(hasDescriptorOfLastNewView_ || prevViewDesc.view == 0);

  ConcordAssert(!hasDescriptorOfLastExitFromView_ || prevViewDesc.view > descriptorOfLastExitFromView_.view);
  ConcordAssert(!hasDescriptorOfLastNewView_ || prevViewDesc.view == descriptorOfLastNewView_.view);
  ConcordAssert(prevViewDesc.lastStable >= lastStableSeqNum_);
  ConcordAssert(prevViewDesc.lastExecuted >= lastExecutedSeqNum_);
  ConcordAssert(prevViewDesc.lastExecuted >= prevViewDesc.lastStable);
  ConcordAssert(prevViewDesc.stableLowerBoundWhenEnteredToView >= 0 &&
                prevViewDesc.lastStable >= prevViewDesc.stableLowerBoundWhenEnteredToView);
  ConcordAssert(prevViewDesc.elements.size() <= kWorkWindowSize);
  ConcordAssert(hasDescriptorOfLastExitFromView_ || descriptorOfLastExitFromView_.elements.size() == 0);
  if (prevViewDesc.view > 0) {
    ConcordAssert(prevViewDesc.myViewChangeMsg != nullptr);
    ConcordAssert(prevViewDesc.myViewChangeMsg->newView() == prevViewDesc.view);
    // ConcordAssert(d.myViewChangeMsg->idOfGeneratedReplica() == myId); TODO(GG): add
    ConcordAssert(prevViewDesc.myViewChangeMsg->lastStable() <= prevViewDesc.lastStable);
  } else {
    ConcordAssert(prevViewDesc.myViewChangeMsg == nullptr);
  }

  std::vector<ViewsManager::PrevViewInfo> clonedElements(prevViewDesc.elements.size());

  for (size_t i = 0; i < prevViewDesc.elements.size(); i++) {
    const ViewsManager::PrevViewInfo &e = prevViewDesc.elements[i];
    ConcordAssert(e.prePrepare != nullptr);
    ConcordAssert(e.prePrepare->seqNumber() >= lastStableSeqNum_ + 1);
    ConcordAssert(e.prePrepare->seqNumber() <= lastStableSeqNum_ + kWorkWindowSize);
    ConcordAssert(e.prePrepare->viewNumber() == prevViewDesc.view);
    ConcordAssert(e.prepareFull == nullptr || e.prepareFull->viewNumber() == prevViewDesc.view);
    ConcordAssert(e.prepareFull == nullptr || e.prepareFull->seqNumber() == e.prePrepare->seqNumber());

    PrePrepareMsg *clonedPrePrepareMsg = nullptr;
    if (e.prePrepare != nullptr) {
      clonedPrePrepareMsg = (PrePrepareMsg *)e.prePrepare->cloneObjAndMsg();
      ConcordAssert(clonedPrePrepareMsg->type() == MsgCode::PrePrepare);
    }

    PrepareFullMsg *clonedPrepareFull = nullptr;
    if (e.prepareFull != nullptr) {
      clonedPrepareFull = (PrepareFullMsg *)e.prepareFull->cloneObjAndMsg();
      ConcordAssert(clonedPrepareFull->type() == MsgCode::PrepareFull);
    }

    clonedElements[i].prePrepare = clonedPrePrepareMsg;
    clonedElements[i].hasAllRequests = e.hasAllRequests;
    clonedElements[i].prepareFull = clonedPrepareFull;
  }

  ViewChangeMsg *clonedViewChangeMsg = nullptr;
  if (prevViewDesc.myViewChangeMsg != nullptr)
    clonedViewChangeMsg = (ViewChangeMsg *)prevViewDesc.myViewChangeMsg->cloneObjAndMsg();

  // delete messages from previous descriptor
  for (size_t i = 0; i < descriptorOfLastExitFromView_.elements.size(); i++) {
    const ViewsManager::PrevViewInfo &e = descriptorOfLastExitFromView_.elements[i];
    ConcordAssert(e.prePrepare != nullptr);
    delete e.prePrepare;
    if (e.prepareFull != nullptr) delete e.prepareFull;
  }
  if (descriptorOfLastExitFromView_.myViewChangeMsg != nullptr) delete descriptorOfLastExitFromView_.myViewChangeMsg;

  hasDescriptorOfLastExitFromView_ = true;
  descriptorOfLastExitFromView_ = DescriptorOfLastExitFromView{prevViewDesc.view,
                                                               prevViewDesc.lastStable,
                                                               prevViewDesc.lastExecuted,
                                                               clonedElements,
                                                               clonedViewChangeMsg,
                                                               prevViewDesc.stableLowerBoundWhenEnteredToView};
}

void DebugPersistentStorage::setDescriptorOfLastNewView(const DescriptorOfLastNewView &prevViewDesc) {
  ConcordAssert(nonExecSetIsAllowed());
  ConcordAssert(prevViewDesc.view >= 1);
  ConcordAssert(hasDescriptorOfLastExitFromView_);
  ConcordAssert(prevViewDesc.view > descriptorOfLastExitFromView_.view);

  ConcordAssert(prevViewDesc.newViewMsg != nullptr);
  ConcordAssert(prevViewDesc.newViewMsg->newView() == prevViewDesc.view);

  ConcordAssert(prevViewDesc.myViewChangeMsg == nullptr ||
                prevViewDesc.myViewChangeMsg->newView() == prevViewDesc.view);

  const size_t numOfVCMsgs = 2 * fVal_ + 2 * cVal_ + 1;

  ConcordAssert(prevViewDesc.viewChangeMsgs.size() == numOfVCMsgs);

  std::vector<ViewChangeMsg *> clonedViewChangeMsgs(numOfVCMsgs);

  for (size_t i = 0; i < numOfVCMsgs; i++) {
    const ViewChangeMsg *vc = prevViewDesc.viewChangeMsgs[i];
    ConcordAssert(vc != nullptr);
    ConcordAssert(vc->newView() == prevViewDesc.view);
    ConcordAssert(prevViewDesc.myViewChangeMsg == nullptr ||
                  prevViewDesc.myViewChangeMsg->idOfGeneratedReplica() != vc->idOfGeneratedReplica());

    Digest digestOfVCMsg;
    vc->getMsgDigest(digestOfVCMsg);
    ConcordAssert(prevViewDesc.newViewMsg->includesViewChangeFromReplica(vc->idOfGeneratedReplica(), digestOfVCMsg));

    ViewChangeMsg *clonedVC = (ViewChangeMsg *)vc->cloneObjAndMsg();
    ConcordAssert(clonedVC->type() == MsgCode::ViewChange);
    clonedViewChangeMsgs[i] = clonedVC;
  }

  // TODO(GG): check thay we a message with the id of the current replica

  NewViewMsg *clonedNewViewMsg = (NewViewMsg *)prevViewDesc.newViewMsg->cloneObjAndMsg();
  ConcordAssert(clonedNewViewMsg->type() == MsgCode::NewView);

  ViewChangeMsg *clonedMyViewChangeMsg = nullptr;
  if (prevViewDesc.myViewChangeMsg != nullptr)
    clonedMyViewChangeMsg = (ViewChangeMsg *)prevViewDesc.myViewChangeMsg->cloneObjAndMsg();

  if (hasDescriptorOfLastNewView_) {
    // delete messages from previous descriptor
    delete descriptorOfLastNewView_.newViewMsg;
    delete descriptorOfLastNewView_.myViewChangeMsg;

    ConcordAssert(descriptorOfLastNewView_.viewChangeMsgs.size() == numOfVCMsgs);

    for (size_t i = 0; i < numOfVCMsgs; i++) {
      delete descriptorOfLastNewView_.viewChangeMsgs[i];
    }
  }

  hasDescriptorOfLastNewView_ = true;
  descriptorOfLastNewView_ = DescriptorOfLastNewView{prevViewDesc.view,
                                                     clonedNewViewMsg,
                                                     clonedViewChangeMsgs,
                                                     clonedMyViewChangeMsg,
                                                     prevViewDesc.stableLowerBoundWhenEnteredToView,
                                                     prevViewDesc.maxSeqNumTransferredFromPrevViews};
}

void DebugPersistentStorage::setDescriptorOfLastExecution(const DescriptorOfLastExecution &prevViewDesc) {
  ConcordAssert(setIsAllowed());
  ConcordAssert(!hasDescriptorOfLastExecution_ ||
                descriptorOfLastExecution_.executedSeqNum < prevViewDesc.executedSeqNum);
  ConcordAssert(lastExecutedSeqNum_ + 1 == prevViewDesc.executedSeqNum);
  ConcordAssert(prevViewDesc.validRequests.numOfBits() >= 1);
  ConcordAssert(prevViewDesc.validRequests.numOfBits() <= maxNumOfRequestsInBatch);

  hasDescriptorOfLastExecution_ = true;
  descriptorOfLastExecution_ = DescriptorOfLastExecution{prevViewDesc.executedSeqNum, prevViewDesc.validRequests};
}

void DebugPersistentStorage::setLastStableSeqNum(SeqNum seqNum) {
  ConcordAssert(seqNum >= lastStableSeqNum_);
  lastStableSeqNum_ = seqNum;
  seqNumWindow.advanceActiveWindow(lastStableSeqNum_ + 1);
  checkWindow.advanceActiveWindow(lastStableSeqNum_);
}

void DebugPersistentStorage::DebugPersistentStorage::clearSeqNumWindow() {
  ConcordAssert(seqNumWindow.getBeginningOfActiveWindow() == lastStableSeqNum_ + 1);
  seqNumWindow.resetAll(seqNumWindow.getBeginningOfActiveWindow());
}

void DebugPersistentStorage::setPrePrepareMsgInSeqNumWindow(SeqNum seqNum, PrePrepareMsg *msg) {
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));
  SeqNumData &seqNumData = seqNumWindow.get(seqNum);
  ConcordAssert(!seqNumData.isPrePrepareMsgSet());
  seqNumData.setPrePrepareMsg(msg->cloneObjAndMsg());
}

void DebugPersistentStorage::setSlowStartedInSeqNumWindow(SeqNum seqNum, bool slowStarted) {
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));
  SeqNumData &seqNumData = seqNumWindow.get(seqNum);
  seqNumData.setSlowStarted(slowStarted);
}

void DebugPersistentStorage::setFullCommitProofMsgInSeqNumWindow(SeqNum seqNum, FullCommitProofMsg *msg) {
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));
  SeqNumData &seqNumData = seqNumWindow.get(seqNum);
  ConcordAssert(!seqNumData.isFullCommitProofMsgSet());
  seqNumData.setFullCommitProofMsg(msg->cloneObjAndMsg());
}

void DebugPersistentStorage::setForceCompletedInSeqNumWindow(SeqNum seqNum, bool forceCompleted) {
  ConcordAssert(forceCompleted);
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));
  SeqNumData &seqNumData = seqNumWindow.get(seqNum);
  seqNumData.setForceCompleted(forceCompleted);
}

void DebugPersistentStorage::setPrepareFullMsgInSeqNumWindow(SeqNum seqNum, PrepareFullMsg *msg) {
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));
  SeqNumData &seqNumData = seqNumWindow.get(seqNum);
  ConcordAssert(!seqNumData.isPrepareFullMsgSet());
  seqNumData.setPrepareFullMsg(msg->cloneObjAndMsg());
}

void DebugPersistentStorage::setCommitFullMsgInSeqNumWindow(SeqNum seqNum, CommitFullMsg *msg) {
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));
  SeqNumData &seqNumData = seqNumWindow.get(seqNum);
  ConcordAssert(!seqNumData.isCommitFullMsgSet());
  seqNumData.setCommitFullMsg(msg->cloneObjAndMsg());
}

void DebugPersistentStorage::setCheckpointMsgInCheckWindow(SeqNum s, CheckpointMsg *msg) {
  ConcordAssert(checkWindow.insideActiveWindow(s));
  CheckData &checkData = checkWindow.get(s);
  checkData.deleteCheckpointMsg();
  checkData.setCheckpointMsg(msg->cloneObjAndMsg());
}

void DebugPersistentStorage::setCompletedMarkInCheckWindow(SeqNum seqNum, bool mark) {
  ConcordAssert(mark == true);
  ConcordAssert(checkWindow.insideActiveWindow(seqNum));
  CheckData &checkData = checkWindow.get(seqNum);
  checkData.setCompletedMark(mark);
}

SeqNum DebugPersistentStorage::getLastExecutedSeqNum() {
  ConcordAssert(getIsAllowed());
  return lastExecutedSeqNum_;
}

SeqNum DebugPersistentStorage::getPrimaryLastUsedSeqNum() {
  ConcordAssert(getIsAllowed());
  return primaryLastUsedSeqNum_;
}

SeqNum DebugPersistentStorage::getStrictLowerBoundOfSeqNums() {
  ConcordAssert(getIsAllowed());
  return strictLowerBoundOfSeqNums_;
}

ViewNum DebugPersistentStorage::getLastViewThatTransferredSeqNumbersFullyExecuted() {
  ConcordAssert(getIsAllowed());
  return lastViewThatTransferredSeqNumbersFullyExecuted_;
}

bool DebugPersistentStorage::hasDescriptorOfLastExitFromView() {
  ConcordAssert(getIsAllowed());
  return hasDescriptorOfLastExitFromView_;
}

DescriptorOfLastExitFromView DebugPersistentStorage::getAndAllocateDescriptorOfLastExitFromView() {
  ConcordAssert(getIsAllowed());
  ConcordAssert(hasDescriptorOfLastExitFromView_);

  DescriptorOfLastExitFromView &d = descriptorOfLastExitFromView_;

  std::vector<ViewsManager::PrevViewInfo> elements(d.elements.size());

  for (size_t i = 0; i < elements.size(); i++) {
    const ViewsManager::PrevViewInfo &e = d.elements[i];
    elements[i].prePrepare = (PrePrepareMsg *)e.prePrepare->cloneObjAndMsg();
    elements[i].hasAllRequests = e.hasAllRequests;
    if (e.prepareFull != nullptr)
      elements[i].prepareFull = (PrepareFullMsg *)e.prepareFull->cloneObjAndMsg();
    else
      elements[i].prepareFull = nullptr;
  }

  ConcordAssert(d.myViewChangeMsg != nullptr || d.view == 0);
  ViewChangeMsg *myVCMsg = nullptr;
  if (d.myViewChangeMsg != nullptr) myVCMsg = (ViewChangeMsg *)d.myViewChangeMsg->cloneObjAndMsg();

  DescriptorOfLastExitFromView retVal{
      d.view, d.lastStable, d.lastExecuted, elements, myVCMsg, d.stableLowerBoundWhenEnteredToView};

  return retVal;
}

bool DebugPersistentStorage::hasDescriptorOfLastNewView() {
  ConcordAssert(getIsAllowed());
  return hasDescriptorOfLastNewView_;
}

DescriptorOfLastNewView DebugPersistentStorage::getAndAllocateDescriptorOfLastNewView() {
  ConcordAssert(getIsAllowed());
  ConcordAssert(hasDescriptorOfLastNewView_);

  DescriptorOfLastNewView &d = descriptorOfLastNewView_;

  NewViewMsg *newViewMsg = (NewViewMsg *)d.newViewMsg->cloneObjAndMsg();

  std::vector<ViewChangeMsg *> viewChangeMsgs(d.viewChangeMsgs.size());

  for (size_t i = 0; i < viewChangeMsgs.size(); i++) {
    viewChangeMsgs[i] = (ViewChangeMsg *)d.viewChangeMsgs[i]->cloneObjAndMsg();
  }

  ViewChangeMsg *myViewChangeMsg = nullptr;
  if (d.myViewChangeMsg != nullptr) myViewChangeMsg = (ViewChangeMsg *)d.myViewChangeMsg->cloneObjAndMsg();

  DescriptorOfLastNewView retVal{d.view,
                                 newViewMsg,
                                 viewChangeMsgs,
                                 myViewChangeMsg,
                                 d.stableLowerBoundWhenEnteredToView,
                                 d.maxSeqNumTransferredFromPrevViews};

  return retVal;
}

bool DebugPersistentStorage::hasDescriptorOfLastExecution() {
  ConcordAssert(getIsAllowed());
  return hasDescriptorOfLastExecution_;
}

DescriptorOfLastExecution DebugPersistentStorage::getDescriptorOfLastExecution() {
  ConcordAssert(getIsAllowed());
  ConcordAssert(hasDescriptorOfLastExecution_);

  DescriptorOfLastExecution &d = descriptorOfLastExecution_;

  return DescriptorOfLastExecution{d.executedSeqNum, d.validRequests};
}

SeqNum DebugPersistentStorage::getLastStableSeqNum() {
  ConcordAssert(getIsAllowed());
  return lastStableSeqNum_;
}

PrePrepareMsg *DebugPersistentStorage::getAndAllocatePrePrepareMsgInSeqNumWindow(SeqNum seqNum) {
  ConcordAssert(getIsAllowed());
  ConcordAssert(lastStableSeqNum_ + 1 == seqNumWindow.getBeginningOfActiveWindow());
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));

  PrePrepareMsg *orgMsg = seqNumWindow.get(seqNum).getPrePrepareMsg();
  if (orgMsg == nullptr) return nullptr;

  PrePrepareMsg *m = (PrePrepareMsg *)orgMsg->cloneObjAndMsg();
  ConcordAssert(m->type() == MsgCode::PrePrepare);
  return m;
}

bool DebugPersistentStorage::getSlowStartedInSeqNumWindow(SeqNum seqNum) {
  ConcordAssert(getIsAllowed());
  ConcordAssert(lastStableSeqNum_ + 1 == seqNumWindow.getBeginningOfActiveWindow());
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));
  bool b = seqNumWindow.get(seqNum).getSlowStarted();
  return b;
}

FullCommitProofMsg *DebugPersistentStorage::getAndAllocateFullCommitProofMsgInSeqNumWindow(SeqNum seqNum) {
  ConcordAssert(getIsAllowed());
  ConcordAssert(lastStableSeqNum_ + 1 == seqNumWindow.getBeginningOfActiveWindow());
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));

  FullCommitProofMsg *orgMsg = seqNumWindow.get(seqNum).getFullCommitProofMsg();
  if (orgMsg == nullptr) return nullptr;

  FullCommitProofMsg *m = (FullCommitProofMsg *)orgMsg->cloneObjAndMsg();
  ConcordAssert(m->type() == MsgCode::FullCommitProof);
  return m;
}

bool DebugPersistentStorage::getForceCompletedInSeqNumWindow(SeqNum seqNum) {
  ConcordAssert(getIsAllowed());
  ConcordAssert(lastStableSeqNum_ + 1 == seqNumWindow.getBeginningOfActiveWindow());
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));
  bool b = seqNumWindow.get(seqNum).getForceCompleted();
  return b;
}

PrepareFullMsg *DebugPersistentStorage::getAndAllocatePrepareFullMsgInSeqNumWindow(SeqNum seqNum) {
  ConcordAssert(getIsAllowed());
  ConcordAssert(lastStableSeqNum_ + 1 == seqNumWindow.getBeginningOfActiveWindow());
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));

  PrepareFullMsg *orgMsg = seqNumWindow.get(seqNum).getPrepareFullMsg();
  if (orgMsg == nullptr) return nullptr;

  PrepareFullMsg *m = (PrepareFullMsg *)orgMsg->cloneObjAndMsg();
  ConcordAssert(m->type() == MsgCode::PrepareFull);
  return m;
}

CommitFullMsg *DebugPersistentStorage::getAndAllocateCommitFullMsgInSeqNumWindow(SeqNum seqNum) {
  ConcordAssert(getIsAllowed());
  ConcordAssert(lastStableSeqNum_ + 1 == seqNumWindow.getBeginningOfActiveWindow());
  ConcordAssert(seqNumWindow.insideActiveWindow(seqNum));

  CommitFullMsg *orgMsg = seqNumWindow.get(seqNum).getCommitFullMsg();
  if (orgMsg == nullptr) return nullptr;

  CommitFullMsg *m = (CommitFullMsg *)orgMsg->cloneObjAndMsg();
  ConcordAssert(m->type() == MsgCode::CommitFull);
  return m;
}

CheckpointMsg *DebugPersistentStorage::getAndAllocateCheckpointMsgInCheckWindow(SeqNum seqNum) {
  ConcordAssert(getIsAllowed());
  ConcordAssert(lastStableSeqNum_ == checkWindow.getBeginningOfActiveWindow());
  ConcordAssert(checkWindow.insideActiveWindow(seqNum));

  CheckpointMsg *orgMsg = checkWindow.get(seqNum).getCheckpointMsg();
  if (orgMsg == nullptr) return nullptr;

  CheckpointMsg *m = (CheckpointMsg *)orgMsg->cloneObjAndMsg();
  ConcordAssert(m->type() == MsgCode::Checkpoint);
  return m;
}

bool DebugPersistentStorage::getCompletedMarkInCheckWindow(SeqNum seqNum) {
  ConcordAssert(getIsAllowed());
  ConcordAssert(lastStableSeqNum_ == checkWindow.getBeginningOfActiveWindow());
  ConcordAssert(checkWindow.insideActiveWindow(seqNum));
  bool b = checkWindow.get(seqNum).getCompletedMark();
  return b;
}

bool DebugPersistentStorage::setIsAllowed() const { return isInWriteTran(); }

bool DebugPersistentStorage::getIsAllowed() const { return !isInWriteTran(); }

bool DebugPersistentStorage::nonExecSetIsAllowed() const {
  return setIsAllowed() &&
         (!hasDescriptorOfLastExecution_ || descriptorOfLastExecution_.executedSeqNum <= lastExecutedSeqNum_);
}

}  // namespace impl
}  // namespace bftEngine
