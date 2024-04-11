#pragma once
#include "html_parser.h"
#include <string_view>

inline std::string_view getParserStateName(intptr_t func) {

  if (func == (intptr_t)&dataState) {
    return "dataState";

  } else if (func == (intptr_t)&rcdataState) {
    return "rcdataState";
  } else if (func == (intptr_t)&rawtextState) {
    return "rawtextState";
  } else if (func == (intptr_t)&scriptDataState) {
    return "scriptDataState";
  } else if (func == (intptr_t)&plaintextState) {
    return "plaintextState";
  } else if (func == (intptr_t)&tagOpenState) {
    return "tagOpenState";
  } else if (func == (intptr_t)&endTagOpenState) {
    return "endTagOpenState";
  } else if (func == (intptr_t)&tagNameState) {
    return "tagNameState";
  } else if (func == (intptr_t)&beforeAttributeNameState) {
    return "beforeAttributeNameState";
  } else if (func == (intptr_t)&attributeNameState) {
    return "attributeNameState";
  } else if (func == (intptr_t)&afterAttributeNameState) {
    return "afterAttributeNameState";
  } else if (func == (intptr_t)&beforeAttributeValueState) {
    return "beforeAttributeValueState";
  } else if (func == (intptr_t)&attributeValueDoubleQuatedState) {
    return "attributeValueDoubleQuatedState";
  } else if (func == (intptr_t)&attributeValueSingleQuotedState) {
    return "attributeValueSingleQuotedState";
  } else if (func == (intptr_t)&attributeValueUnquotedState) {
    return "attributeValueUnquotedState";
  } else if (func == (intptr_t)&afterAttributeValueQuotedState) {
    return "afterAttributeValueQuotedState";
  // } else if (func == (intptr_t)&SelfClosingStartTagState) {
  //   return "SelfClosingStartTagState";
  } else if (func == (intptr_t)&selfClosingStartTagState) {
    return "selfClosingStartTagState";
  } else if (func == (intptr_t)&bogusCommentState) {
    return "bogusCommentState";
  } else if (func == (intptr_t)&markupDeclarationOpenState) {
    return "markupDeclarationOpenState";
  } else if (func == (intptr_t)&commentStartState) {
    return "commentStartState";
  } else if (func == (intptr_t)&commentStartDashState) {
    return "commentStartDashState";
  } else if (func == (intptr_t)&commentState) {
    return "commentState";
  } else if (func == (intptr_t)&commentEndDashState) {
    return "commentEndDashState";
  } else if (func == (intptr_t)&commentEndState) {
    return "commentEndState";
  } else if (func == (intptr_t)&doctypeState) {
    return "doctypeState";
  } else if (func == (intptr_t)&beforeDoctypeNameState) {
    return "beforeDoctypeNameState";
  } else if (func == (intptr_t)&doctypeNameState) {
    return "doctypeNameState";
  } else if (func == (intptr_t)&characterReferenceState) {
    return "characterReferenceState";
  } else if (func == (intptr_t)&namedCharacterReferenceState) {
    return "namedCharacterReferenceState";
  } else if (func == (intptr_t)&ambiguousAmpersandState) {
    return "ambiguousAmpersandState";
  }

  return "unknown parser state";
}
