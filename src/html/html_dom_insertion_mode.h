#pragma once
#include "html_dom.h"

inline InsertionMode getInsertionMode(intptr_t func) {
  if (func == (intptr_t)&initialInsertionMode) {
    return (InsertionMode)1;
  } else if (func == (intptr_t)&beforeHtmlMode) {
    return (InsertionMode)2;
  } else if (func == (intptr_t)&beforeHeadMode) {
    return (InsertionMode)3;
  } else if (func == (intptr_t)&inHeadMode) {
    return (InsertionMode)4;
  } else if (func == (intptr_t)&inHeadNoscriptMode) {
    return (InsertionMode)5;
  } else if (func == (intptr_t)&afterHeadMode) {
    return (InsertionMode)6;
  } else if (func == (intptr_t)&inBodyMode) {
    return (InsertionMode)7;
  } else if (func == (intptr_t)&textMode) {
    return (InsertionMode)8;
  } else if (func == (intptr_t)&tableMode) {
    return (InsertionMode)9;
  } else if (func == (intptr_t)&tableTextMode) {
    return (InsertionMode)10;
  } else if (func == (intptr_t)&captionMode) {
    return (InsertionMode)11;
  } else if (func == (intptr_t)&inColumnMode) {
    return (InsertionMode)12;
  } else if (func == (intptr_t)&inTableBodyMode) {
    return (InsertionMode)13;
  } else if (func == (intptr_t)&inRowMode) {
    return (InsertionMode)14;
  } else if (func == (intptr_t)&inCellMode) {
    return (InsertionMode)15;
  } else if (func == (intptr_t)&inSelectMode) {
    return (InsertionMode)16;
  } else if (func == (intptr_t)&inSelectInTableMode) {
    return (InsertionMode)17;
  } else if (func == (intptr_t)&inTemplateMode) {
    return (InsertionMode)18;
  } else if (func == (intptr_t)&afterBodyMode) {
    return (InsertionMode)19;
  } else if (func == (intptr_t)&inFramesetMode) {
    return (InsertionMode)20;
  } else if (func == (intptr_t)&afterFramesetMode) {
    return (InsertionMode)21;
  } else if (func == (intptr_t)&afterAfterBodyMode) {
    return (InsertionMode)22;
  } else if (func == (intptr_t)&afterAfterFramesetMode) {
    return (InsertionMode)23;
  }

  return {};
}
