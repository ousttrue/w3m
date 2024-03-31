#include "event.h"

std::string stringify(ftxui::Event event) {
  std::string out;
  for (auto &it : event.input())
    out += " " + std::to_string((unsigned int)it);

  out = "(" + out + " ) -> ";
  if (event.is_character()) {
    out += "Event::Character(\"" + event.character() + "\")";
  } else if (event.is_mouse()) {
    out += "mouse";
    switch (event.mouse().button) {
    case ftxui::Mouse::Left:
      out += "_left";
      break;
    case ftxui::Mouse::Middle:
      out += "_middle";
      break;
    case ftxui::Mouse::Right:
      out += "_right";
      break;
    case ftxui::Mouse::None:
      out += "_none";
      break;
    case ftxui::Mouse::WheelUp:
      out += "_wheel_up";
      break;
    case ftxui::Mouse::WheelDown:
      out += "_wheel_down";
      break;
    }
    switch (event.mouse().motion) {
    case ftxui::Mouse::Pressed:
      out += "_pressed";
      break;
    case ftxui::Mouse::Released:
      out += "_released";
      break;
    }
    if (event.mouse().control)
      out += "_control";
    if (event.mouse().shift)
      out += "_shift";
    if (event.mouse().meta)
      out += "_meta";

    out += "(" + //
           std::to_string(event.mouse().x) + "," +
           std::to_string(event.mouse().y) + ")";
  } else if (event == ftxui::Event::ArrowLeft) {
    out += "Event::ArrowLeft";
  } else if (event == ftxui::Event::ArrowRight) {
    out += "Event::ArrowRight";
  } else if (event == ftxui::Event::ArrowUp) {
    out += "Event::ArrowUp";
  } else if (event == ftxui::Event::ArrowDown) {
    out += "Event::ArrowDown";
  } else if (event == ftxui::Event::ArrowLeftCtrl) {
    out += "Event::ArrowLeftCtrl";
  } else if (event == ftxui::Event ::ArrowRightCtrl) {
    out += "Event::ArrowRightCtrl";
  } else if (event == ftxui::Event::ArrowUpCtrl) {
    out += "Event::ArrowUpCtrl";
  } else if (event == ftxui::Event::ArrowDownCtrl) {
    out += "Event::ArrowDownCtrl";
  } else if (event == ftxui::Event::Backspace) {
    out += "Event::Backspace";
  } else if (event == ftxui::Event::Delete) {
    out += "Event::Delete";
  } else if (event == ftxui::Event::Escape) {
    out += "Event::Escape";
  } else if (event == ftxui::Event::Return) {
    out += "Event::Return";
  } else if (event == ftxui::Event::Tab) {
    out += "Event::Tab";
  } else if (event == ftxui::Event::TabReverse) {
    out += "Event::TabReverse";
  } else if (event == ftxui::Event::F1) {
    out += "Event::F1";
  } else if (event == ftxui::Event::F2) {
    out += "Event::F2";
  } else if (event == ftxui::Event::F3) {
    out += "Event::F3";
  } else if (event == ftxui::Event::F4) {
    out += "Event::F4";
  } else if (event == ftxui::Event::F5) {
    out += "Event::F5";
  } else if (event == ftxui::Event::F6) {
    out += "Event::F6";
  } else if (event == ftxui::Event::F7) {
    out += "Event::F7";
  } else if (event == ftxui::Event::F8) {
    out += "Event::F8";
  } else if (event == ftxui::Event::F9) {
    out += "Event::F9";
  } else if (event == ftxui::Event::F10) {
    out += "Event::F10";
  } else if (event == ftxui::Event::F11) {
    out += "Event::F11";
  } else if (event == ftxui::Event::F12) {
    out += "Event::F12";
  } else if (event == ftxui::Event::Home) {
    out += "Event::Home";
  } else if (event == ftxui::Event::End) {
    out += "Event::End";
  } else if (event == ftxui::Event::PageUp) {
    out += "Event::PageUp";
  } else if (event == ftxui::Event::PageDown) {
    out += "Event::PageDown";
  } else if (event == ftxui::Event::Custom) {
    out += "Resize ?";
  } else {
    out += "(special)";
  }
  return out;
}
