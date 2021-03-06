/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSheet.h"

#include "mozilla/css/Rule.h"
#include "mozilla/StyleBackendType.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoMediaList.h"
#include "mozilla/ServoCSSRuleList.h"
#include "mozilla/css/GroupRule.h"
#include "mozilla/dom/CSSRuleList.h"
#include "mozilla/dom/MediaList.h"

#include "mozAutoDocUpdate.h"

using namespace mozilla::dom;

namespace mozilla {

// -------------------------------
// CSS Style Sheet Inner Data Container
//

ServoStyleSheetInner::ServoStyleSheetInner(CORSMode aCORSMode,
                                           ReferrerPolicy aReferrerPolicy,
                                           const SRIMetadata& aIntegrity)
  : StyleSheetInfo(aCORSMode, aReferrerPolicy, aIntegrity)
{
}

size_t
ServoStyleSheetInner::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  // XXX: need to measure mSheet

  return n;
}

ServoStyleSheet::ServoStyleSheet(css::SheetParsingMode aParsingMode,
                                 CORSMode aCORSMode,
                                 net::ReferrerPolicy aReferrerPolicy,
                                 const dom::SRIMetadata& aIntegrity)
  : StyleSheet(StyleBackendType::Servo, aParsingMode)
{
  mInner = new ServoStyleSheetInner(aCORSMode, aReferrerPolicy, aIntegrity);
  mInner->AddSheet(this);
}

ServoStyleSheet::ServoStyleSheet(const ServoStyleSheet& aCopy,
                                 ServoStyleSheet* aParentToUse,
                                 css::ImportRule* aOwnerRuleToUse,
                                 nsIDocument* aDocumentToUse,
                                 nsINode* aOwningNodeToUse)
  : StyleSheet(aCopy, aDocumentToUse, aOwningNodeToUse)
{
  mParent = aParentToUse;
}

ServoStyleSheet::~ServoStyleSheet()
{
  UnparentChildren();

  DropRuleList();
}

// QueryInterface implementation for ServoStyleSheet
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServoStyleSheet)
NS_INTERFACE_MAP_END_INHERITING(StyleSheet)

NS_IMPL_ADDREF_INHERITED(ServoStyleSheet, StyleSheet)
NS_IMPL_RELEASE_INHERITED(ServoStyleSheet, StyleSheet)

NS_IMPL_CYCLE_COLLECTION_CLASS(ServoStyleSheet)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ServoStyleSheet)
  tmp->DropRuleList();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(StyleSheet)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServoStyleSheet, StyleSheet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRuleList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool
ServoStyleSheet::HasRules() const
{
  return Inner()->mSheet && Servo_StyleSheet_HasRules(Inner()->mSheet);
}

nsresult
ServoStyleSheet::ParseSheet(css::Loader* aLoader,
                            const nsAString& aInput,
                            nsIURI* aSheetURI,
                            nsIURI* aBaseURI,
                            nsIPrincipal* aSheetPrincipal,
                            uint32_t aLineNumber)
{
  MOZ_ASSERT_IF(mMedia, mMedia->IsServo());
  RefPtr<URLExtraData> extraData =
    new URLExtraData(aBaseURI, aSheetURI, aSheetPrincipal);

  NS_ConvertUTF16toUTF8 input(aInput);
  if (!Inner()->mSheet) {
    auto* mediaList = static_cast<ServoMediaList*>(mMedia.get());
    RawServoMediaList* media = mediaList ?  &mediaList->RawList() : nullptr;

    Inner()->mSheet =
      Servo_StyleSheet_FromUTF8Bytes(
          aLoader, this, &input, mParsingMode, media, extraData).Consume();
  } else {
    // TODO(emilio): Once we have proper inner cloning (which we don't right
    // now) we should update the mediaList here too, though it's slightly
    // tricky.
    Servo_StyleSheet_ClearAndUpdate(Inner()->mSheet, aLoader,
                                    this, &input, extraData);
  }

  Inner()->mURLData = extraData.forget();
  return NS_OK;
}

void
ServoStyleSheet::LoadFailed()
{
  Inner()->mSheet = Servo_StyleSheet_Empty(mParsingMode).Consume();
  Inner()->mURLData = URLExtraData::Dummy();
}

// nsICSSLoaderObserver implementation
NS_IMETHODIMP
ServoStyleSheet::StyleSheetLoaded(StyleSheet* aSheet,
                                  bool aWasAlternate,
                                  nsresult aStatus)
{
  MOZ_ASSERT(aSheet->IsServo(),
             "why we were called back with a CSSStyleSheet?");

  ServoStyleSheet* sheet = aSheet->AsServo();
  if (sheet->GetParentSheet() == nullptr) {
    return NS_OK; // ignore if sheet has been detached already
  }
  NS_ASSERTION(this == sheet->GetParentSheet(),
               "We are being notified of a sheet load for a sheet that is not our child!");

  if (mDocument && NS_SUCCEEDED(aStatus)) {
    mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);
    NS_WARNING("stylo: Import rule object not implemented");
    mDocument->StyleRuleAdded(this, nullptr);
  }

  return NS_OK;
}

void
ServoStyleSheet::DropRuleList()
{
  if (mRuleList) {
    mRuleList->DropReference();
    mRuleList = nullptr;
  }
}

css::Rule*
ServoStyleSheet::GetDOMOwnerRule() const
{
  NS_ERROR("stylo: Don't know how to get DOM owner rule for ServoStyleSheet");
  return nullptr;
}

already_AddRefed<StyleSheet>
ServoStyleSheet::Clone(StyleSheet* aCloneParent,
                       css::ImportRule* aCloneOwnerRule,
                       nsIDocument* aCloneDocument,
                       nsINode* aCloneOwningNode) const
{
  RefPtr<StyleSheet> clone = new ServoStyleSheet(*this,
    static_cast<ServoStyleSheet*>(aCloneParent),
    aCloneOwnerRule,
    aCloneDocument,
    aCloneOwningNode);
  return clone.forget();
}

CSSRuleList*
ServoStyleSheet::GetCssRulesInternal(ErrorResult& aRv)
{
  if (!mRuleList) {
    RefPtr<ServoCssRules> rawRules =
      Servo_StyleSheet_GetRules(Inner()->mSheet).Consume();
    mRuleList = new ServoCSSRuleList(rawRules.forget());
    mRuleList->SetStyleSheet(this);
  }
  return mRuleList;
}

uint32_t
ServoStyleSheet::InsertRuleInternal(const nsAString& aRule,
                                    uint32_t aIndex, ErrorResult& aRv)
{
  // Ensure mRuleList is constructed.
  GetCssRulesInternal(aRv);

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);
  aRv = mRuleList->InsertRule(aRule, aIndex);
  if (aRv.Failed()) {
    return 0;
  }
  // XXX If the inserted rule is an import rule, we should only notify
  // the document if its associated child stylesheet has been loaded.
  if (mDocument) {
    // XXX We may not want to get the rule when stylesheet change event
    // is not enabled.
    mDocument->StyleRuleAdded(this, mRuleList->GetRule(aIndex));
  }
  return aIndex;
}

void
ServoStyleSheet::DeleteRuleInternal(uint32_t aIndex, ErrorResult& aRv)
{
  // Ensure mRuleList is constructed.
  GetCssRulesInternal(aRv);
  if (aIndex > mRuleList->Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);
  // Hold a strong ref to the rule so it doesn't die when we remove it
  // from the list. XXX We may not want to hold it if stylesheet change
  // event is not enabled.
  RefPtr<css::Rule> rule = mRuleList->GetRule(aIndex);
  aRv = mRuleList->DeleteRule(aIndex);
  MOZ_ASSERT(!aRv.ErrorCodeIs(NS_ERROR_DOM_INDEX_SIZE_ERR),
             "IndexSizeError should have been handled earlier");
  if (!aRv.Failed() && mDocument) {
    mDocument->StyleRuleRemoved(this, rule);
  }
}

nsresult
ServoStyleSheet::InsertRuleIntoGroupInternal(const nsAString& aRule,
                                             css::GroupRule* aGroup,
                                             uint32_t aIndex)
{
  auto rules = static_cast<ServoCSSRuleList*>(aGroup->CssRules());
  MOZ_ASSERT(rules->GetParentRule() == aGroup);
  return rules->InsertRule(aRule, aIndex);
}

size_t
ServoStyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = StyleSheet::SizeOfIncludingThis(aMallocSizeOf);
  const ServoStyleSheet* s = this;
  while (s) {
    // See the comment in CSSStyleSheet::SizeOfIncludingThis() for an
    // explanation of this.
    if (s->Inner()->mSheets.LastElement() == s) {
      n += s->Inner()->SizeOfIncludingThis(aMallocSizeOf);
    }

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - s->mRuleList

    s = s->mNext ? s->mNext->AsServo() : nullptr;
  }
  return n;
}

} // namespace mozilla
